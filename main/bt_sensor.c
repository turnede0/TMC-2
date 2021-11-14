#include "bt_sensor.h"

#include "mesh_handler.h"

#define TAG "SENSOR CLIENT"

void example_ble_mesh_sensor_cli_cb(esp_ble_mesh_sensor_client_cb_event_t event,
                                    esp_ble_mesh_sensor_client_cb_param_t *param)
{
    uint32_t opcode = param->params->opcode;
    ESP_LOGI(TAG, "model id: 0x%04x", param->params->model->model_id);

    switch (event)
    {
    case ESP_BLE_MESH_SENSOR_CLIENT_TIMEOUT_EVT:
    {
        ESP_LOGW(TAG, "Timeout Received");
        example_ble_mesh_sensor_timeout(opcode);
        break;
    }
    case ESP_BLE_MESH_SENSOR_CLIENT_GET_STATE_EVT:
    {
        //padding will cause +1 padding simple fix by direct assign
        //FIXME: struct a uint8 staruct and use another to read

        esp_user_model_sensor_field_t *dev1 = param->status_cb.sensor_status.marshalled_sensor_data->data;
        esp_user_model_sensor_field_t *dev2 = (param->status_cb.sensor_status.marshalled_sensor_data->data + 3);

        //int data = (uint8_t) * (param->status_cb.sensor_status.marshalled_sensor_data->data + 3);

        //TODO: add sensor reading handling
        ESP_LOGW(TAG, "temp: %d, humidity: %d", dev1->value, dev2->value);

        send_sensor_data_to_ui(dev1->value, dev2->value);

        break;
    }

    default:
        ESP_LOGW(TAG, "received from sensor srv 0x%04x", event);
        break;
    }
}

static void example_ble_mesh_sensor_timeout(uint32_t opcode)
{
    switch (opcode)
    {
    case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
        ESP_LOGW(TAG, "Sensor Descriptor Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
        ESP_LOGW(TAG, "Sensor Cadence Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
        ESP_LOGW(TAG, "Sensor Cadence Set timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
        ESP_LOGW(TAG, "Sensor Settings Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
        ESP_LOGW(TAG, "Sensor Setting Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
        ESP_LOGW(TAG, "Sensor Setting Set timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_GET:
        ESP_LOGW(TAG, "Sensor Get timeout 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
        ESP_LOGW(TAG, "Sensor Column Get timeout, opcode 0x%04x", opcode);
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
        ESP_LOGW(TAG, "Sensor Series Get timeout, opcode 0x%04x", opcode);
        break;
    default:
        ESP_LOGE(TAG, "Unknown Sensor Get/Set opcode 0x%04x", opcode);
        return;
    }
}