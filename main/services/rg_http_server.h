#include "esp_http_server.h"
#include <string>
#include <nlohmann/json.hpp>

// Struct to hold sensor data
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    const char* ip_address;
} data_for_html_server_t;

// HTTP GET handler
static esp_err_t http_get_handler(httpd_req_t *req)
{
    // Retrieve sensor data from user_ctx
    data_for_html_server_t *data = (data_for_html_server_t *)req->user_ctx;

    // Larger response buffer
    char response[1024];

    // Generate HTML response with dynamic data
    snprintf(response, sizeof(response),
             "<html><body>"
             "<h1>RoomGuardian V2 Sensor Data</h1>"
             "<p id=\"ip_address\">IP Address: %s</p>"
             "<p id=\"temperature\">Temperature: %.2f°C</p>"
             "<p id=\"humidity\">Humidity: %.2f%%</p>"
             "<p id=\"pressure\">Pressure: %.2f hPa</p>"
             "<script>"
             "setInterval(() => {"
             "    fetch('/data').then(response => response.json()).then(data => {"
             "        document.getElementById('ip_address').innerText = 'IP Address: ' + data.ip_address;"
             "        document.getElementById('temperature').innerText = 'Temperature: ' + data.temperature.toFixed(2) + '°C';"
             "        document.getElementById('humidity').innerText = 'Humidity: ' + data.humidity.toFixed(2) + '%%';" // Double %% escapes the %
             "        document.getElementById('pressure').innerText = 'Pressure: ' + data.pressure.toFixed(2) + ' hPa';"
             "    });"
             "}, 5000);"
             "</script>"
             "</body></html>",
             data->ip_address, data->temperature, data->humidity, data->pressure);

    // Set response type and send response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// HTTP GET handler for sensor data
static esp_err_t http_get_data_handler(httpd_req_t *req)
{
    // Retrieve sensor data from user_ctx
    data_for_html_server_t *data = (data_for_html_server_t *)req->user_ctx;

    // Create JSON response
    nlohmann::json json_data;
    json_data["temperature"] = data->temperature;
    json_data["humidity"] = data->humidity;
    json_data["pressure"] = data->pressure;
    json_data["ip_address"] = data->ip_address;

    std::string response = json_data.dump();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response.c_str(), response.size());
    return ESP_OK;
}

// Start the HTTP server
static httpd_handle_t start_webserver(data_for_html_server_t *shared_data)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register handler for the main page
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = shared_data
        };
        httpd_register_uri_handler(server, &uri_get);

        // Register handler for sensor data
        httpd_uri_t uri_data_get = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = http_get_data_handler,
            .user_ctx = shared_data
        };
        httpd_register_uri_handler(server, &uri_data_get);
    }
    return server;
}

// Stop the HTTP server
static void stop_webserver(httpd_handle_t server, data_for_html_server_t *data)
{
    if (server) {
        httpd_stop(server);
    }
    if (data) {
        free(data);
    }
}