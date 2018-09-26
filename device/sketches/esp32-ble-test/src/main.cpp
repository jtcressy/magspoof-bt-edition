/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
*/
#include <Arduino.h>
#include "mbedtls/aes.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "OneButton.h"
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/


BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
BLEAdvertising *pAdvertising;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};


bool state = LOW;
#define BUTTON_PIN 0
OneButton button(BUTTON_PIN, true);

void button_pushed() {
    state = !state;
    digitalWrite(LED_BUILTIN, state);
    if (state) {
        pServer->getAdvertising()->start();
        Serial.println("started advertising");
    }
    else {
        pServer->getAdvertising()->stop();
        Serial.println("stopped advertising");
    }
}


void setup() {
    Serial.begin(9600);
    Serial.println("Starting BLE work!");

    pinMode(LED_BUILTIN, OUTPUT);
    button.attachClick(button_pushed);

    BLEDevice::init("MagSpoofBT");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    //pServer->getAdvertising()->start(); // replaced with button press
    Serial.println("Waiting a client connection to notify...");
}

void loop() {
    button.tick();
    if (deviceConnected) {
        pTxCharacteristic->setValue(&txValue, 1);
        pTxCharacteristic->notify();
        txValue++;
		delay(10); // bluetooth stack will go into congestion, if too many packets are sent
	}

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}