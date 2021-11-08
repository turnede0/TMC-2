#if !defined(WS_H)
#define WS_H

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include <esp_http_server.h>

//json handling
#include "cJSON.h"

void Register_ws_server(httpd_handle_t *server);

void WS_send_JSON(const cJSON *data);

#endif // WS_H
