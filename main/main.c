#include <stdio.h>
#include <string.h>

#include "bt_provisioner.h"
#include "http.h"
#include "wifi.h"
#include "ws.h"

#define TAG "Main"

static uint8_t dev_uuid[16];

void app_main(void)
{
    esp_err_t err;

    ESP_LOGE(TAG, "esp_user_model_sensor_field_t: %d", sizeof(esp_user_model_sensor_field_t));
    ESP_LOGI(TAG, "Initializing...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err)
    {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    //init wifi
    wifi_connect();

    //init http server
    httpd_handle_t server = NULL;
    server = Start_webserver();
    Register_ws_server(&server);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err)
    {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
}
