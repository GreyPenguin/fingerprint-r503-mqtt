#define ESP_DRD_USE_LITTLEFS true

#include "setup.h"
#include "config.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP_DoubleResetDetector.h>
#include <LittleFS.h>
#include "Adafruit_Fingerprint.h"

#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0

SoftwareSerial swSerial(SENSOR_TX, SENSOR_RX);
Adafruit_Fingerprint fingerSensor = Adafruit_Fingerprint(&swSerial);

String lastMessage = "";
String sensorMode = "reading";
String lastSensorMode = "";

String sensorState = "waiting";
String lastSensorState = "";

bool match = false;

uint8_t fingerprintId = 0;
uint8_t userId = 0;
uint8_t confidence = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
char mqttBuffer[MQTT_MAX_PACKET_SIZE];

char mqttHost[32] = "homeassistant.local";
char mqttPort[6] = "1883";
char mqttUsername[16] = "mqttuser";
char mqttPassword[16] = "mqttpass";
char deviceGateId[32] = "main";

DoubleResetDetector resetDetector(DRD_TIMEOUT, DRD_ADDRESS);

void resetMessage()
{
    sensorState = STATE_WAIT;
    sensorMode = MODE_READING;
    match = false;
    fingerprintId = 0;
    confidence = 0;
}

void mqttPublish(String message)
{
    if ((message != lastMessage) ||
        (sensorMode != lastSensorMode) ||
        (sensorState != lastSensorState))
    {

        lastMessage = message;
        lastSensorMode = sensorMode;
        lastSensorState = sensorState;

        DynamicJsonDocument mqttMessage(MQTT_MAX_PACKET_SIZE);

        mqttMessage["message"] = message;
        mqttMessage["state"] = sensorState;
        mqttMessage["mode"] = sensorMode;
        mqttMessage["match"] = match;
        mqttMessage["fingerprintId"] = fingerprintId;
        mqttMessage["userId"] = fingerprintId / 10;
        mqttMessage["confidence"] = confidence;
        mqttMessage["gate"] = deviceGateId;

        Serial.print("Message: ");
        Serial.println(message);

        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(TOPIC_STATUS, mqttBuffer, mqttMessageSize);
        Serial.println(mqttBuffer);
    }
}

void setupDevices()
{
    Serial.begin(9600);

    while (!Serial)
        ;
    delay(100);

    Serial.println("\n\nWelcome to Fingerprint-MQTT sensor");

    pinMode(LED_BUILTIN, OUTPUT);

    if (resetDetector.detectDoubleReset())
    {
        Serial.println("RESET DEVICE");

        digitalWrite(LED_BUILTIN, LOW);
        LittleFS.remove(CONFIG_FILE);
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        ESP.restart();
    }
    else
    {
        Serial.println("Device reset not detected.");
        digitalWrite(LED_BUILTIN, HIGH);
    }

    fingerSensor.begin(57600);
    delay(5);

    Serial.print("Looking for sensor...");

    if (fingerSensor.verifyPassword())
    {
        Serial.println("ok");
    }
    else
    {
        Serial.println("KO.\nSensor not found: check serial connection on green/yellow cables.");

        delay(3000);
        ESP.restart();
        delay(5000);
    }

    fingerSensor.getParameters();
    Serial.print(F("Status : 0x"));
    Serial.println(fingerSensor.status_reg, HEX);
    Serial.print(F("Sys ID : 0x"));
    Serial.println(fingerSensor.system_id, HEX);
    Serial.print(F("Capacity: "));
    Serial.println(fingerSensor.capacity);
    Serial.print(F("Security level: "));
    Serial.println(fingerSensor.security_level);
    Serial.print(F("Device address: "));
    Serial.println(fingerSensor.device_addr, HEX);
    Serial.print(F("Packet len: "));
    Serial.println(fingerSensor.packet_len);
    Serial.print(F("Baud rate: "));
    Serial.println(fingerSensor.baud_rate);

    fingerSensor.getTemplateCount();

    if (fingerSensor.templateCount == 0)
    {
        Serial.print("Sensor doesn't contain any fingerprint data");
    }
    else
    {
        Serial.println("Waiting for valid finger");
        Serial.print("Sensor contains ");
        Serial.print(fingerSensor.templateCount);
        Serial.println(" templates");
    }

    //pinMode(SENSOR_TOUCH, INPUT);
    //attachInterrupt(digitalPinToInterrupt(SENSOR_TOUCH), setupTouch, CHANGE);
}

ICACHE_RAM_ATTR void setupTouch()
{
    Serial.println("Touch!");
}

void mqttSetup(void (*callback)(char *topic, byte *payload, unsigned int length))
{
    client.setServer(mqttHost, atoi(mqttPort));
    client.setCallback(callback);

    mqttConnect();
}

void mqttConnect()
{
    while (!client.connected())
    {
        Serial.print("Connecting to MQTT ");
        Serial.print(mqttHost);
        Serial.print("...");

        if (client.connect(HOSTNAME, mqttUsername, mqttPassword, TOPIC_AVAILABLE, 1, true, "offline"))
        {
            Serial.println("connected");

            client.publish(TOPIC_AVAILABLE, "online");
            client.subscribe(TOPIC_LEARN);
            client.subscribe(TOPIC_READ);
            client.subscribe(TOPIC_DELETE);
        }
        else
        {
            Serial.print("failed, rc: ");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");

            delay(5000);
        }
    }
}

void localLoop()
{
    resetDetector.loop();

    mqttConnect();

    delay(200);

    client.loop();
}
