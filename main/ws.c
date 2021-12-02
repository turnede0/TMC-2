/**
 * @file ws.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-11-12
 *
 * @copyright
 * Simple HTTP + SSL + WS Server Example
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 *
 */

#include "ws.h"

#include "esp_ble_mesh_defs.h"

// TODO: merge shit back to handler
#include "mesh_handler.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#define WEBSOCKET_MSG_SIZE 256

static const char *TAG = "Websocket";

static httpd_handle_t *httpd_server = NULL;
static int soc_fd = 0;

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
};

struct async_mac_tran
{
    httpd_handle_t hd;
    int fd;
    uint8_t data[6];
};

struct async_json_tran
{
    httpd_handle_t hd;
    int fd;
    cJSON *root;
};

// async func for send binary
static void queue_send_JSON(void *arg)
{
    struct async_json_tran *resp_arg = arg;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // serialize json
    char *buf = cJSON_Print(resp_arg->root);

    ws_pkt.payload = (uint8_t *)buf;
    ws_pkt.len = strlen(buf);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);

    cJSON_Delete(resp_arg->root);
    free(arg);
}

/**
 * @brief Send JSON to ui
 * Note: data should not be freed after passed to this function
 * it will handle free after transmitted*
 * @param data cJSON object
 */
void WS_send_JSON(const cJSON *data)
{
    if (httpd_server == NULL || soc_fd == 0 || !data) // if server not init or no data
        return;

    struct async_json_tran *d = malloc(sizeof(struct async_json_tran));
    d->fd = soc_fd;
    d->hd = httpd_server;
    d->root = data;
    httpd_queue_work(httpd_server, queue_send_JSON, d);
}

/**
 * @brief  async send function, which we put into the httpd work queue
 *
 * @param arg
 */
static void ws_async_send(void *arg)
{
    static const char *data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

/**
 * @brief  trigger a async call to send msg from ws
 *
 * @param handle
 * @param req
 * @return esp_err_t
 */
static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

/**
 * @brief Handle WebSocket Message
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 * @param req  request receive from websocket
 * @return esp_err_t  error code if error
 */
static esp_err_t ws_handler(httpd_req_t *req)
{
    // TODO: put back start provision as function
    // TODO: Add ping pong to detect client disconnect
    // add to receiver fd
    if (soc_fd == 0) // TODO: disconnect start provision
    {
    }
    soc_fd = httpd_req_to_sockfd(req);
    httpd_server = req->handle;

    // cJSON_Parse(monitor);
    uint8_t buf[WEBSOCKET_MSG_SIZE] = {0};
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // assign buffer
    ws_pkt.payload = (uint8_t *)&buf;

    // FIXME: wait ESP-IDF 5.0 update for dynamic allocation of mem for msg size
    /**
     * NOTE: seem this function hv bug originally it should be able to get msg len when len = 0 and return to pkt.len
     * currently it only support write to preallocated array
     * second calling of the function will return WS Message too long =w=
     * hopefully this should got fix in 5.0
     */

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, WEBSOCKET_MSG_SIZE);

    switch (ws_pkt.type)
    {
    case HTTPD_WS_TYPE_TEXT:
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
        Websocket_msg_handle(ws_pkt.payload); // TODO: async here later ?
        break;
    case HTTPD_WS_TYPE_PONG:
        ESP_LOGD(TAG, "Received PONG message");
        break;
    case HTTPD_WS_TYPE_PING:
        ESP_LOGD(TAG, "Received PING message");
        break;
    case HTTPD_WS_TYPE_CLOSE:
        ESP_LOGD(TAG, "Received CLOSE message");
        break;
    case HTTPD_WS_TYPE_CONTINUE:
        ESP_LOGD(TAG, "Received CONTINUE message");
        break;
    default:
        ESP_LOGD(TAG, "Received def message");
        break;
    }

    if (ws_pkt.len)
    {
        ESP_LOGI(TAG, "pkt len: %d", ws_pkt.len);
        /*
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf; //bind buffer to payload storage
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        */
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);

        return ret;
    }

    return ret;
}

static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true};

void Register_ws_server(httpd_handle_t *server)
{
    httpd_register_uri_handler(*server, &ws);
}

/*
static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
    }
}

*/