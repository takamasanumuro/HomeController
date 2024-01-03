#include <Arduino.h>
#include <WiFi.h>
#include "UbidotsEsp32Mqtt.h"
#include <map>
#include <string>

#ifndef LED_BUILTIN
    #define LED_BUILTIN GPIO_NUM_2
#endif

int publishFrequency = 15000; // Update rate in milliseconds

EventGroupHandle_t systemEventGroup = xEventGroupCreate();
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT BIT1 // Required due to event group only being able to wait for bits to be set, not cleared

struct BlinkCommand {
    long offTime;
    long onTime;
    int repeats;
};

QueueHandle_t blinkerQueue = xQueueCreate(10, sizeof(BlinkCommand));

Ubidots mqttClient(UBIDOTS_TOKEN);

std::string ParseVariableFromTopic(std::string topic, std::string device) {
    std::string delimiter = device;
    delimiter.at(0) = std::tolower(delimiter.at(0));

    size_t pos = topic.find(delimiter);
    if (pos != std::string::npos) {
        std::string variable = topic.substr(pos + delimiter.length() + 1);
        delimiter = "/";
        pos = variable.find(delimiter);
        if (pos != std::string::npos) {
            variable = variable.substr(0, pos);
        }
        return variable;
    }
    return "";
}

void ubidotsCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    //Message arrived [/v2.0/devices/garden/sprinkler/lv] 0.0

    std::string message = std::string(topic);
    std::string delimiter = DEVICE_LABEL;
    std::string variable_label = VARIABLE_LABEL;
    // Guaranteeing that the first letter is lowercase
    delimiter.at(0) = std::tolower(delimiter.at(0));
    variable_label.at(0) = std::tolower(variable_label.at(0));

    std::string variable = ParseVariableFromTopic(message, DEVICE_LABEL);
    if (variable == "") {
        Serial.printf("Could not parse variable from topic %s\n", topic);
        return;
    } else if (variable != variable_label) {
        Serial.printf("Variable %s does not match %s\n", variable.c_str(), variable_label.c_str());
        return;
    }

    float value = atof((char*)payload);
    Serial.printf("Updating %s to %.2f\n", variable.c_str(), value);

}

void BlinkerTask(void* pvParameters) {
    BlinkCommand command;
    while (true) {
        int previousState;
        if (xQueueReceive(blinkerQueue, &command, portMAX_DELAY)) {
            previousState = digitalRead(LED_BUILTIN);
            for (int i = 0; i < command.repeats; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(command.onTime));
                digitalWrite(LED_BUILTIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(command.offTime));
            }
            digitalWrite(LED_BUILTIN, previousState);
        }
    }
}

void WifiConnectionTask(void* pvParameters) {

    std::map<const char*, const char*> wifiCredentials = {
        {WIFI_SSID, WIFI_PASS},
        {WIFI_SSID_BACKUP, WIFI_PASS_BACKUP}
    };

    //Use WiFi events to check the connection status
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case SYSTEM_EVENT_STA_START:
                Serial.printf("WiFi client started\n");
                break;
            case SYSTEM_EVENT_STA_CONNECTED:
                xEventGroupSetBits(systemEventGroup, WIFI_CONNECTED_BIT);
                xEventGroupClearBits(systemEventGroup, WIFI_DISCONNECTED_BIT);
                digitalWrite(LED_BUILTIN, HIGH);
                Serial.printf("Connected to access point\n");
                break;
            case SYSTEM_EVENT_STA_GOT_IP:
                Serial.printf("Obtained IP address: %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
                xEventGroupSetBits(systemEventGroup, WIFI_DISCONNECTED_BIT);
                xEventGroupClearBits(systemEventGroup, WIFI_CONNECTED_BIT);
                digitalWrite(LED_BUILTIN, LOW);
                Serial.printf("Disconnected from WiFi access point\n");
                break;
        }
    });

    while (true) {

        for (auto& wifi : wifiCredentials) {

            WiFi.begin(wifi.first, wifi.second);
            Serial.printf("\nConnecting to %s\n", wifi.first);

            long timeout = millis() + 5000;
            while ((WiFi.status() != WL_CONNECTED) && (millis() < timeout)) {
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (WiFi.status() == WL_CONNECTED) {
                break;
            }
        }          
        
        xEventGroupWaitBits(systemEventGroup, WIFI_DISCONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
}

void PublisherTask(void *pvParameters) {

    xEventGroupWaitBits(systemEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(1000);
    Serial.printf("Configuring Ubidots MQTT client...\n");

    mqttClient.setCallback(ubidotsCallback);
    mqttClient.setup();
    mqttClient.reconnect();
    mqttClient.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL);
    Serial.printf("Ubidots MQTT client configured\n");

    while (true) {
        xEventGroupWaitBits(systemEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        if (!mqttClient.connected()) {
            mqttClient.reconnect();
        }


        BlinkCommand blinkParams = {offTime: 200, onTime: 200, repeats: 1};
        xQueueSend(blinkerQueue, &blinkParams, portMAX_DELAY);
        float value = random(0, 2);
        mqttClient.add(VARIABLE_LABEL, value);
        mqttClient.publish(DEVICE_LABEL);
        Serial.printf("Published %.2f to %s\n", value, VARIABLE_LABEL);
        mqttClient.loop(); // Required to keep the connection alive
        vTaskDelay(pdMS_TO_TICKS(publishFrequency));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Env Name: %s\nBUILD: %s %s\n", ENV_NAME, __DATE__, __TIME__);
    vTaskDelay(pdMS_TO_TICKS(1000));

    pinMode(LED_BUILTIN, OUTPUT);

    xTaskCreate(&BlinkerTask, "BlinkerTask", 4096, NULL, 1, NULL);
    xTaskCreate(&WifiConnectionTask, "WifiConnectionTask", 4096, NULL, 1, NULL);
    //xTaskCreate(&PublisherTask, "PublisherTask", 4096, NULL, 1, NULL);
}

void loop() {
    vTaskDelete(NULL);
}

