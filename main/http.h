#ifndef _HTTP_H_
#define _HTTP_H_

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>

httpd_handle_t Start_webserver(void);

void Stop_webserver(httpd_handle_t server);

#endif // HTTP_H
