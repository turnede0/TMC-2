#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <esp_system.h>
#include "esp_log.h"

#include "mesh_handler.h"

#include "ws.h"

#include "bt_provisioner.h"

#define TAG "MESH Handler"

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

//aka: unpair device found
void Unprovisioned_advertise_pkt_receive(uint8_t dev_uuid[16], uint8_t addr[BD_ADDR_LEN],
                                         esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                         uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{ //TODO: update websocket from the function

    if (discovered == NULL)
    {
        discovered = blank_node_detail();

        memcpy(discovered->dev.addr, addr, BD_ADDR_LEN);
        discovered->dev.addr_type = (uint8_t)addr_type;
        memcpy(discovered->dev.uuid, dev_uuid, 16);
        discovered->dev.oob_info = oob_info;
        discovered->dev.bearer = (uint8_t)bearer;

        //add device to UI
        Send_device_to_ui(addr);

        //TODO: connect to device
        //Connect_unprovisioned_device(0);
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

            //add device to UI
            Send_device_to_ui(addr);
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
            //if exceed list
        }
        if (index < 0)
            break;
        prev = ptr;

        ptr = ptr->next;
    }
    int err = esp_ble_mesh_provisioner_add_unprov_dev(&(ptr->dev),
                                                      ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);

    //if failed
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

//aka: scan for device
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

//aka: stop scanning
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
void Provision_complete()
{
}

#define BADKEY -1
#define CONNECT 1
#define LIST_CONNECTED 2
#define ENABLE_PROVISION 3
#define DISABLE_PROVISION 4
#define TEST 5

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
    {"test", TEST}};

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

//this is use for parsing json and doing relative action
void Websocket_msg_handle(uint8_t *payload)
{
    cJSON *root = cJSON_Parse((const char *)payload);
    if (!root)
    {
        return;
        //TODO: error handling
    }

    cJSON *res = cJSON_GetObjectItem(root, "connect");
    if (res)
    { //this lib return 0 for get number if not found =w= so convert from string here
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
        case TEST:
            Onoff_model(-1);
            break;
        default:
            break;
        }
    }
}

void Send_device_to_ui(uint8_t *data)
{
    cJSON *root = cJSON_CreateArray();

    cJSON *dev = cJSON_CreateObject();

    cJSON_AddStringToObject(dev, "Device", bt_hex(data, BD_ADDR_LEN));
    cJSON_AddItemToArray(root, dev);

    WS_send_JSON(root);
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

//if index = -1 the function will broadcast
void Onoff_model(int index)
{

    ESP_LOGI(TAG, "onoff sent");
    if (index == -1)
    {
        esp_ble_mesh_node_info_t *node = &nodes[0];
        esp_ble_mesh_client_common_param_t common = {0};
        esp_ble_mesh_generic_client_set_state_t set_state = {0};
        ESP_LOGI(TAG, "Current Status: %d", node->onoff);

        esp_ble_mesh_generic_client_get_state_t get_state = {0};
        example_ble_mesh_set_msg_common(&common, node, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET);
        esp_err_t error = esp_ble_mesh_generic_client_get_state(&common, &get_state);
        //example_ble_mesh_set_msg_common(&common, node, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET);

        /*
        set_state.onoff_set.op_en = false;
        set_state.onoff_set.onoff = !node->onoff;
        set_state.onoff_set.tid = 1;

        ESP_LOGE(TAG, "model: %p", node->model);
        ESP_LOGE(TAG, "model: %p", onoff_client.model);
        //ESP_LOGE(TAG,"");

        espblemeshgeneric

            esp_err_t error = esp_ble_mesh_generic_client_set_state(&common, &set_state);

        */
        ESP_LOGW(TAG, "esp_ble_mesh_generic_client_set_state: %d", error);
        /*
        esp_ble_mesh_model_publish(nodes[0].model,
                                   ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET,
                                   sizeof(tmp), &tmp, ROLE_PROVISIONER);
                                   */
    }
}