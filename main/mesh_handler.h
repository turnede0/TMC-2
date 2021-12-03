#if !defined(MESH_HANDLER_H)
#define MESH_HANDLER_H

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_provisioning_api.h"

//json handling
#include "cJSON.h"

#define LED_OFF 0x0
#define LED_ON 0x1

typedef struct NODE_DETAIL
{
    esp_ble_mesh_unprov_dev_add_t dev;
    struct NODE_DETAIL *next;
} node_detail;

//use data in bt example first
typedef struct
{
    uint8_t uuid[16];
    uint16_t unicast;
    uint8_t elem_num;
    uint8_t onoff;
    esp_ble_mesh_model_t *model;
    uint16_t model_id;
    esp_ble_mesh_msg_ctx_t *ctx;
} esp_ble_mesh_node_info_t;

extern esp_ble_mesh_node_info_t nodes[CONFIG_BLE_MESH_MAX_PROV_NODES];

int Connect_unprovisioned_device(char *mac);

void Provision_complete(const char *name, const char *uuid, uint16_t model_id);

esp_err_t Start_provisioner(void);

esp_err_t Stop_provisioner(void);

const node_detail *Retreive_dlist();

void Websocket_msg_handle(uint8_t *payload);

void send_unprov_device_mac_to_ui(uint8_t *data);

void send_sensor_data_to_ui(const uint8_t *name, const uint8_t temp, const uint8_t humid);

void Get_connected_nodes();

void Onoff_model(char *uuid, bool status);

void sensor_model(int index);

void get_status();

void Unprovisioned_advertise_pkt_receive(uint8_t dev_uuid[16], uint8_t addr[BD_ADDR_LEN],
                                         esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                         uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer);

#endif // MESH_HANDLER_H
