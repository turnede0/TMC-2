#if !defined(BT_PROVISIONER_H)
#define BT_PROVISIONER_H

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

#include "ble_mesh_example_init.h"
#include "mesh_handler.h"
//TODO: move back ?
#include "ws.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

typedef struct
{
    u_int16_t pid; // Property ID
    uint8_t value; // value
} esp_user_model_sensor_field_t;

extern esp_ble_mesh_client_t sensor_client;
extern esp_ble_mesh_client_t onoff_client;

esp_err_t ble_mesh_init(void);

esp_err_t Ble_mesh_app_key_install();

esp_err_t example_ble_mesh_set_msg_common(esp_ble_mesh_client_common_param_t *common,
                                          esp_ble_mesh_node_info_t *node,
                                          esp_ble_mesh_model_t *model, uint32_t opcode);

#endif // BT_PROVISIONER_H
