/**
 * @file mesh_handler.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-11-12
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <esp_system.h>
#include "esp_log.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "mesh_handler.h"

#include "ws.h"

#include "bt_provisioner.h"

#define TAG "MESH Handler"

#define MODEL_SENSOR_STR "Sensor Model"
#define MODEL_ONOFF_STR "ONOFF Model"
#define MODEL_UNKNOWN_STR "Unknown Model"

#define PROVISION_QUEUE_SIZE 3 //TODO: limit the queue size ?

esp_ble_mesh_node_info_t nodes[CONFIG_BLE_MESH_MAX_PROV_NODES] = {
    [0 ...(CONFIG_BLE_MESH_MAX_PROV_NODES - 1)] = {
        .unicast = ESP_BLE_MESH_ADDR_UNASSIGNED,
        .elem_num = 0,
        .onoff = LED_OFF,
        .model = NULL,
        .ctx = NULL,
    }};

static node_detail *discovered = NULL;

static bool provisioning = false;

static bool provisioned = false;

typedef struct QUEUE_UNPROV_DEV_STRUCT
{
    uint8_t *mac;
    struct QUEUE_UNPROV_DEV_STRUCT *next;
} queue_unprov_dev;

static queue_unprov_dev *queue_unprov = NULL;

const char *retrive_model_str(int model_id)
{
    switch (model_id)
    {
    case ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV:
        return MODEL_ONOFF_STR;
    case ESP_BLE_MESH_MODEL_ID_SENSOR_SRV:
        return MODEL_SENSOR_STR;
    default:
        return MODEL_UNKNOWN_STR;
    }
}

uint8_t *StrtoHex(char *string)
{
    uint8_t *mac = malloc(6 * sizeof(uint8_t));
    uint8_t *ptr = mac;
    uint8_t tmp;
    for (char *i = string; i != string + 12; i += 2) // TODO: add int for any length string
    {
        if (i != string - 2)
        {
            tmp = *(i + 2);
            *(i + 2) = 0;
        }

        *ptr = strtol(i, NULL, 16);

        if (i != string - 2)
            *(i + 2) = tmp;

        ptr++;
    }

    return mac;
}

// aka: unpair device found
void Unprovisioned_advertise_pkt_receive(uint8_t dev_uuid[16], uint8_t addr[BD_ADDR_LEN],
                                         esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                         uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{ // TODO: update websocket from the function
    ESP_LOGI(TAG, "Device: %s", bt_hex(dev_uuid, 16));
    queue_unprov_dev *ptr = queue_unprov;
    queue_unprov_dev *prev = NULL;
    while (ptr)
    {
        if (memcmp(ptr->mac, addr, BD_ADDR_LEN) == 0)
        { //if queue unprov dev found
            esp_ble_mesh_unprov_dev_add_t dev;
            memcpy(dev.addr, addr, BD_ADDR_LEN);
            memcpy(dev.uuid, dev_uuid, 16);
            dev.addr_type = (uint8_t)addr_type;
            dev.oob_info = oob_info;
            dev.bearer = (uint8_t)bearer;

            int err = esp_ble_mesh_provisioner_add_unprov_dev(&dev,
                                                              ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);

            // if failed
            if (err)
            {
                ESP_LOGE(TAG, "%s: Add unprovisioned device into queue failed", __func__);
                return;
            }

            if (ptr->next && prev != NULL)
                prev->next = ptr->next;
            else if (ptr->next && ptr == queue_unprov)
                queue_unprov = ptr->next;
            else if (ptr == queue_unprov)
                queue_unprov = NULL;
            else
                prev->next = NULL;

            free(ptr);

            return;
        }
        prev = ptr;
        ptr = ptr->next;
    }

    return;
}

int Connect_unprovisioned_device(char *mac)
{
    ESP_LOGI(TAG, "ready to connect: %s", mac);
    uint8_t *hex = StrtoHex(mac);

    ESP_LOGI(TAG, "added to queue: %s", bt_hex(hex, 6));

    queue_unprov_dev *dev = malloc(sizeof(queue_unprov_dev));
    dev->mac = hex;
    dev->next = NULL;

    queue_unprov_dev *ptr = queue_unprov;
    if (queue_unprov != NULL)
    {
        while (queue_unprov->next != NULL)
            ptr = queue_unprov->next;
        ptr->next = dev;
    }
    else
    {
        queue_unprov = dev;
    }

    return 0;
}

// aka: scan for device
esp_err_t Start_provisioner(void)
{
    esp_err_t err = ESP_OK;
    err = esp_ble_mesh_provisioner_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable mesh provisioner (err %d)", err);
        return err;
    }

    if (!provisioned)
    {
        provisioned = true;
        Ble_mesh_app_key_install();
    }

    provisioning = true;

    return err;
}

const node_detail *Retreive_dlist()
{
    return discovered;
}

/**
 * aka: connected status
 * TODO: device type and device uuid ?
 */
void Provision_complete(const char *name, const char *uuid, uint16_t model_id)
{
    ESP_LOGE(TAG, "name: %s, uuid %s, %d", name, uuid, model_id);
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "function", "device_update");

    cJSON_AddStringToObject(data, "name", name);
    cJSON_AddStringToObject(data, "uuid", uuid);
    cJSON_AddStringToObject(data, "model", retrive_model_str(model_id));

    cJSON_AddItemToObject(root, "data", data);
    WS_send_JSON(root);
}

#define BADKEY -1
#define CONNECT 1
#define LIST_CONNECTED 2
#define ENABLE_PROVISION 3
#define DISABLE_PROVISION 4
#define TEMP 5
#define ONOFF 7
#define INITED 8

typedef struct
{
    char *key;
    int val;
} t_symstruct;

static t_symstruct lookuptable[] = {
    {"node_connect", CONNECT},
    {"list_connected", LIST_CONNECTED},
    {"temp", TEMP},
    {"onoff", ONOFF},
    {"inited", INITED}};

#define NKEYS (sizeof(lookuptable) / sizeof(t_symstruct))

int keyfromstring(char *key)
{
    int i;
    for (i = 0; i < NKEYS; i++)
    {
        t_symstruct *sym = &lookuptable[i];
        if (strcmp(sym->key, key) == 0)
            return sym->val;
    }
    return BADKEY;
}

// this is use for parsing json and doing relative action
void Websocket_msg_handle(uint8_t *payload)
{
    // TODO: testing

    cJSON *root = cJSON_Parse((const char *)payload);

    if (!root)
    {
        return;
        // TODO: error handling
    }

    cJSON *res = cJSON_GetObjectItem(root, "function");
    if (res != NULL)
    {
        switch (keyfromstring(res->valuestring))
        {
        case CONNECT:
        {
            cJSON *value = cJSON_GetObjectItem(root, "data");
            if (value != NULL)
            {
                Connect_unprovisioned_device(value->valuestring);
            }
            break;
        }
        case INITED:
            get_status(ESP_OK);
            break;
        case LIST_CONNECTED:
            Get_connected_nodes();
            break;
        case TEMP:
            sensor_model(0);
            break;
        case ONOFF:
        {
            cJSON *val = cJSON_GetObjectItem(root, "data");
            if (val != NULL)
            {
                cJSON *uuid = cJSON_GetObjectItem(val, "uuid");
                cJSON *status = cJSON_GetObjectItem(val, "status");
                if (uuid != NULL && status != NULL)
                    Onoff_model(uuid->valuestring);
            }

            break;
        }
        default:
            break;
        }
    }
    cJSON_Delete(root);
}

void send_unprov_device_mac_to_ui(uint8_t *data)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *dev = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "function", "provision_update");
    cJSON_AddStringToObject(dev, "mac", bt_hex(data, BD_ADDR_LEN));
    cJSON *device = cJSON_CreateArray();
    cJSON_AddItemToArray(device, dev);
    cJSON_AddBoolToObject(dev, "add", true);
    cJSON_AddItemToObject(root, "data", device);

    WS_send_JSON(root);
}

void send_sensor_data_to_ui(const uint8_t *name, const uint8_t temp, const uint8_t humid)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "function", "sensor_update");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "node", (const char *)name);

    if (temp == 0xFF || humid == 0xFF)
    { //if it is invalid temp and humid
        cJSON_AddNumberToObject(data, "error", 1.0);
    }
    else
    {
        cJSON_AddNumberToObject(data, "error", 0.0);
        cJSON_AddNumberToObject(data, "temp", temp);
        cJSON_AddNumberToObject(data, "humid", humid);
    }

    cJSON_AddItemToObject(root, "data", data);
    WS_send_JSON(root);
}

void send_onoff_data_to_ui(bool stataus)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "model", (float)ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV);
}

void Get_connected_nodes()
{
    cJSON *root = cJSON_CreateArray();
    int i = 0;
    while (nodes[i].unicast != ESP_BLE_MESH_ADDR_UNASSIGNED)
    {
        ESP_LOGI(TAG, "device uuid: %s", bt_hex(nodes[i].uuid, 16));
        cJSON *dev = cJSON_CreateObject();
        cJSON_AddStringToObject(dev, "uuid", bt_hex(nodes[i].uuid, 16));
        cJSON_AddStringToObject(dev, "name", esp_ble_mesh_provisioner_get_node_name(i));

        cJSON_AddItemToArray(root, dev);
        i++;
    }

    WS_send_JSON(root);
}

// if index = -1 the function will broadcast
//TODO: =w=  broadcast
void Onoff_model(char *uuid)
{

    //setup basic information
    //TODO: maybe send json from here if error ?
    /*
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "function", "onoff");

        cJSON *data = cJSON_CreateObject();
        */

    //here it will take from index of all ONOFF server model
    esp_ble_mesh_node_info_t *node = NULL;
    for (size_t i = 0; i < CONFIG_BLE_MESH_MAX_PROV_NODES || nodes[i].unicast != ESP_BLE_MESH_ADDR_UNASSIGNED; i++)
    {
        if (nodes[i].model_id == BLE_MESH_MODEL_ID_GEN_ONOFF_SRV && strcmp(uuid, bt_hex(nodes[i].uuid, 16)) == 0)

        {
            node = &nodes[i];
            break;
        }
    }

    if (!node)
    {
        ESP_LOGE(TAG, "%s: node not found!", __func__);
        //cJSON_AddNumberToObject(data, "error", 1);
        return;
    }
    ESP_LOGI(TAG, "Current Status: %d", node->onoff);

    esp_ble_mesh_client_common_param_t common = {0};

    esp_ble_mesh_generic_client_get_state_t get_state = {0};
    ESP_LOGI(TAG, "Status to be update: %d", node->onoff);
    example_ble_mesh_set_msg_common(&common, node, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET);
    esp_err_t error = esp_ble_mesh_generic_client_get_state(&common, &get_state);
    ESP_LOGW(TAG, "esp_ble_mesh_generic_client_get_state: %d", error);
}

void sensor_model(int index)
{
    if (index == -1)
    {
    }
    {

        esp_ble_mesh_node_info_t *node = NULL;
        int type_counter = 0;
        for (size_t i = 0; i < CONFIG_BLE_MESH_MAX_PROV_NODES || nodes[i].unicast != ESP_BLE_MESH_ADDR_UNASSIGNED; i++)
        {
            if (nodes[i].model_id == BLE_MESH_MODEL_ID_SENSOR_SRV)
            {
                if (type_counter != index)
                {
                    type_counter++;
                    continue;
                }
                else
                {
                    node = &nodes[i];
                    break;
                }
            }
        }

        if (!node)
        {
            ESP_LOGE(TAG, "%s: node not found!", __func__);
            //cJSON_AddNumberToObject(data, "error", 1);
            return;
        }

        esp_ble_mesh_client_common_param_t common = {0};
        esp_ble_mesh_sensor_client_get_state_t get_state = {0};
        example_ble_mesh_set_msg_common(&common, node, sensor_client.model, ESP_BLE_MESH_MODEL_OP_SENSOR_GET);
        esp_err_t error = esp_ble_mesh_sensor_client_get_state(&common, &get_state);
        ESP_LOGW(TAG, "esp_ble_mesh_sensor_client_get_state: %d", error);
    }
}

void get_status(esp_err_t err)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "function", "status_update");
    cJSON *data = cJSON_CreateObject();

    cJSON_AddBoolToObject(data, "provision_status", provisioning);
    cJSON_AddNumberToObject(data, "error", err);

    cJSON_AddItemToObject(root, "data", data);

    WS_send_JSON(root);
}

/**
 * Server and client transfer using json with format
 * function: string
 * data: any
 * 
 * 
 * model data
 * model: string ?
 * data any 
 * 
 */