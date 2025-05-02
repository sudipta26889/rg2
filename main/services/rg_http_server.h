#include "esp_http_server.h"
#include <string>
#include <nlohmann/json.hpp>
#include "constants.h"

// Struct to hold sensor data
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    const char* ip_address;
} data_for_html_server_t;

static const std::string HTEMPTAG = std::string(DEVICE_NAME) + "-" + DEVICE_VERSION + "::HttpServer";
static const char *HTTP_TAG = HTEMPTAG.c_str();

static data_for_html_server_t shared_data; // Shared data instance

static esp_err_t my_get_handler(httpd_req_t *req){

	/* our custom page sits at /helloworld in this example */
	if(strcmp(req->uri, "/") == 0){

        // Larger response buffer
        char response[1024];

        // Generate HTML response with dynamic data
        snprintf(response, sizeof(response),
            "<html><body>"
            "<h1>%s-%s Sensor Data</h1>"
            "<p id=\"ip_address\">IP Address: %s</p>"
            "<p id=\"temperature\">Temperature: %.2f°C</p>"
            "<p id=\"humidity\">Humidity: %.2f%%</p>"
            "<p id=\"pressure\">Pressure: %.2f hPa</p>"
            "<a href=\"/settings/\">Settings</a>"
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
            DEVICE_NAME, DEVICE_VERSION, shared_data.ip_address, shared_data.temperature, shared_data.humidity, shared_data.pressure);
        // Set response type and send response
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response, strlen(response));
    }
    else if(strcmp(req->uri, "/data") == 0){
        // Create JSON response
        nlohmann::json json_data;
        json_data["temperature"] = shared_data.temperature;
        json_data["humidity"] = shared_data.humidity;
        json_data["pressure"] = shared_data.pressure;
        json_data["ip_address"] = shared_data.ip_address;

        // Set response type and send JSON response
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_data.dump().c_str(), json_data.dump().length());
    }
	else{
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}
