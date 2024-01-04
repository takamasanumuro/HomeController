
#include <WiFi.h>
#include <WiFiClient.h>
#include "blinker.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN GPIO_NUM_2
#endif

#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID       "TMPL2ToOnhl7Q"
#define BLYNK_TEMPLATE_NAME     "GardenController"
#define BLYNK_AUTH_TOKEN        "syLia4-5q6jyVtldVoPxiDXlPqpTcmoY"
#include <BlynkSimpleEsp32.h>

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

BlynkTimer timer;

// This function is a callback invoked when the remote virtual pin status is changed
BLYNK_WRITE(V0)
{
    // Set incoming value from pin V0 to a variable
    int value = param.asInt();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, value);

    DEBUG_PRINTF("Received Slider value: %d\n", value);
    // Update state
    Blynk.virtualWrite(V1, value);
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
    //Synchronize virtual pins
    Blynk.syncVirtual(V0);
}


// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent()
{
    // You can send any value at any time.
    // Please don't send more that 10 values per second.
    Blynk.virtualWrite(V2, millis() / 1000);
    Blink(100, 100, 1);

}

void setup() {
    // Debug console
    Serial.begin(115200);
    int statusPin = LED_BUILTIN;
    xTaskCreate(&BlinkerTask, "BlinkerTask", 4096, &statusPin, 1, NULL);
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    // Setup periodic functions
    timer.setInterval(1000L, myTimerEvent);
}

void loop() {
    Blynk.run();
    timer.run();
}
