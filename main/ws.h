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

// json handling
#include "cJSON.h"

void WS_send_JSON(const cJSON *data);

static void ws_async_send(void *arg);

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req);

static esp_err_t ws_handler(httpd_req_t *req);

void Register_ws_server(httpd_handle_t *server);

#endif // WS_H
