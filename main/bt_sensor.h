/**
 * @file bt_sensor.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-11-13
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#if !defined(BT_SENSSOR_H)
#define BT_SENSSOR_H

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

#include "ble_mesh_example_init.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "bt_provisioner.h"

/**
 * @brief callback for sensor model 
 * 
 * @param event event emitted apon receive 
 * @param param 
 */
void example_ble_mesh_sensor_cli_cb(esp_ble_mesh_sensor_client_cb_event_t event,
                                    esp_ble_mesh_sensor_client_cb_param_t *param);

static void example_ble_mesh_sensor_timeout(uint32_t opcode);
#endif // BT_SENSSOR_H
