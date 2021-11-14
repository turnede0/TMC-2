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

esp_ble_mesh_node_info_t nodes[CONFIG_BLE_MESH_MAX_PROV_NODES] = {
    [0 ...(CONFIG_BLE_MESH_MAX_PROV_NODES - 1)] = {
        .unicast = ESP_BLE_MESH_ADDR_UNASSIGNED,
        .elem_num = 0,
        .onoff = LED_OFF,
        .model = NULL,
        .ctx = NULL,
    }};

static node_detail *discovered = NULL;

node_detail *blank_node_detail()
{
    node_detail *blank = malloc(sizeof(node_detail));
    blank->next = NULL;
    ESP_LOGI(TAG, "1 device found...");

    /*
        ESP_LOGI(TAG, "address: %s, address type: %d, adv type: %d", discovered->mac, addr_type, adv_type);
        ESP_LOGI(TAG, "device uuid: %s", bt_hex(dev_uuid, 16));
        ESP_LOGI(TAG, "oob info: %d, bearer: %s", oob_info, (bearer & ESP_BLE_MESH_PROV_ADV) ? "PB-ADV" : "PB-GATT");
        */

    return blank;
}

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

// aka: unpair device found
void Unprovisioned_advertise_pkt_receive(uint8_t dev_uuid[16], uint8_t addr[BD_ADDR_LEN],
                                         esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                         uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{ // TODO: update websocket from the function

    if (discovered == NULL)
    {
        discovered = blank_node_detail();
        memcpy(discovered->dev.addr, addr, BD_ADDR_LEN);
        discovered->dev.addr_type = (uint8_t)addr_type;
        memcpy(discovered->dev.uuid, dev_uuid, 16);
        discovered->dev.oob_info = oob_info;
        discovered->dev.bearer = (uint8_t)bearer;

        // add device to UI
        send_unprov_device_mac_to_ui(addr);

        // TODO: connect to device

        // Connect_unprovisioned_device(0);
    }
    else
    {
        node_detail *list_ptr = discovered;
        bool found = false;
        node_detail *last_ele = NULL;
        while (list_ptr != NULL)
        {
            if (memcmp(addr, list_ptr->dev.addr, 6) >= 0)
            {
                found = true;
                break;
            }
            last_ele = list_ptr;
            list_ptr = list_ptr->next;
        }
        if (!found)
        {
            last_ele->next = blank_node_detail();

            node_detail *current = last_ele->next;

            memcpy(current->dev.addr, addr, BD_ADDR_LEN);
            current->dev.addr_type = (uint8_t)addr_type;
            memcpy(current->dev.uuid, dev_uuid, 16);
            current->dev.oob_info = oob_info;
            current->dev.bearer = (uint8_t)bearer;

            // add device to UI
            send_unprov_device_mac_to_ui(addr);
        }
    }

    return;
}

int Connect_unprovisioned_device(int index)
{
    node_detail *ptr = discovered;

    node_detail *prev = NULL;
    while (ptr != NULL)
    {

        index--;

        if (ptr == NULL && index != -1)
        {
            ESP_LOGE(TAG, "no device in the list!");
            return -1;
            // if exceed list
        }
        if (index < 0)
            break;
        prev = ptr;

        ptr = ptr->next;
    }
    int err = esp_ble_mesh_provisioner_add_unprov_dev(&(ptr->dev),
                                                      ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);

    // if failed
    if (err)
    {
        ESP_LOGE(TAG, "%s: Add unprovisioned device into queue failed", __func__);
        return -1;
    }

    if (ptr->next != NULL)
    {
        if (prev != NULL)
        {
            prev->next = ptr->next;
        }
        else
        {
            discovered = ptr->next;
        }
    }

    if (ptr == discovered)
    {
        discovered = NULL;
    }
    free(ptr);

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

    Ble_mesh_app_key_install();

    return err;
}

// aka: stop scanning
esp_err_t Stop_provisioner(void)
{
    esp_err_t err = ESP_OK;

    err = esp_ble_mesh_provisioner_prov_disable(ESP_BLE_MESH_PROV_ADV);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disable mesh provisioner (err %d)", err);
        return err;
    }

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
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "uuid", uuid);
    cJSON_AddStringToObject(root, "model", retrive_model_str(model_id));
    WS_send_JSON(root);
}

#define BADKEY -1
#define CONNECT 1
#define LIST_CONNECTED 2
#define ENABLE_PROVISION 3
#define DISABLE_PROVISION 4
#define TEMP 5
#define PROVISION 6
#define ONOFF 7

typedef struct
{
    char *key;
    int val;
} t_symstruct;

static t_symstruct lookuptable[] = {
    {"connect", CONNECT},
    {"list_connected", LIST_CONNECTED},
    {"enable_provision", ENABLE_PROVISION},
    {"disable_provision", DISABLE_PROVISION},
    {"temp", TEMP},
    {"onoff", ONOFF},
    {"provision", PROVISION}};

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

    cJSON *res = cJSON_GetObjectItem(root, "connect");
    if (res)
    { // this lib return 0 for get number if not found =w= so convert from string here
        int device = atoi(res->valuestring);
        Connect_unprovisioned_device(device);
        return;
    }

    res = cJSON_GetObjectItem(root, "function");
    if (res != NULL)
    {
        switch (keyfromstring(res->valuestring))
        {
        case LIST_CONNECTED:
            Get_connected_nodes();
            break;
        case TEMP:
            sensor_model(0);
            break;
        case ONOFF:
            Onoff_model(0);
            break;
        case PROVISION:
        {
            cJSON *value = cJSON_GetObjectItem(root, "data");
            if (value != NULL)
            {
                if (value->valueint)
                {
                    Start_provisioner();
                }
                else
                {
                    Stop_provisioner();
                }
            }

            break;
        }
        default:
            break;
        }
    }
}

void send_unprov_device_mac_to_ui(uint8_t *data)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *dev = cJSON_CreateObject();

    cJSON_AddStringToObject(dev, "mac", bt_hex(data, BD_ADDR_LEN));
    cJSON *device = cJSON_CreateArray();
    cJSON_AddItemToArray(device, dev);
    cJSON_AddItemToObject(root, "device", device);

    WS_send_JSON(root);
}

void send_sensor_data_to_ui(uint8_t temp, uint8_t humid)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "model", (float)ESP_BLE_MESH_MODEL_ID_SENSOR_SRV);

    cJSON *data = cJSON_CreateObject();
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
void Onoff_model(int index)
{
    if (index == -1)
    {
    }
    else
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
        int type_counter = 0;
        for (size_t i = 0; i < CONFIG_BLE_MESH_MAX_PROV_NODES || nodes[i].unicast != ESP_BLE_MESH_ADDR_UNASSIGNED; i++)
        {
            if (nodes[i].model_id == BLE_MESH_MODEL_ID_GEN_ONOFF_SRV)
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
        ESP_LOGI(TAG, "Current Status: %d", node->onoff);

        esp_ble_mesh_client_common_param_t common = {0};

        esp_ble_mesh_generic_client_get_state_t get_state = {0};
        example_ble_mesh_set_msg_common(&common, node, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET);
        esp_err_t error = esp_ble_mesh_generic_client_get_state(&common, &get_state);
        ESP_LOGW(TAG, "esp_ble_mesh_generic_client_get_state: %d", error);
    }
}

void sensor_model(int index)
{
    if (index == -1)
    {
    }
    else
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