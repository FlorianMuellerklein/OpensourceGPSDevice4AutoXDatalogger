/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect handler associated with the server starts a background task that performs notification
   every couple of seconds.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLE2901.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
BLE2901 *descriptor_2901 = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "ECBB8159-FBA9-4123-AEF0-CA06E1D390D9"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define RXPin 16
#define TXPin 17 

#define GPS_BAUD 115200

// hardware serial UART2
HardwareSerial gpsSerial(1); 
// GPS parsing 
TinyGPSPlus gps; 

// type defs for packing gps data for ble
union latBytes {
  float lat;
  uint8_t bytes[4];
} latUnion; 

union longBytes {
  float lng;
  uint8_t bytes[4];
} longUnion; 

union speedBytes {
  float speed;
  uint8_t bytes[4];
} speedUnion; 

union yearBytes {
  int year;
  uint8_t bytes[2];
} yearUnion; 

union monthBytes {
  int month;
  uint8_t bytes[1];
} monthUnion; 

union dayBytes {
  int day;
  uint8_t bytes[1];
} dayUnion; 

union hourBytes {
  int hour;
  uint8_t bytes[1];
} hourUnion; 

union minBytes {
  int min;
  uint8_t bytes[1];
} minUnion; 

union secBytes {
  int sec;
  uint8_t bytes[1];
} secUnion; 

union centBytes {
  int cent;
  uint8_t bytes[1];
} centUnion; 

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};


void setup() {
  Serial.begin(115200);

  // start reading from the gps module
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXPin, TXPin);
  Serial.println("GPS Started!");

  // Create the BLE Device
  BLEDevice::init("DIYAutoXGPS");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Creates BLE Descriptor 0x2902: Client Characteristic Configuration Descriptor (CCCD)
  pCharacteristic->addDescriptor(new BLE2902());
  // Adds also the Characteristic User Description - 0x2901 descriptor
  descriptor_2901 = new BLE2901();
  descriptor_2901->setDescription("GPS Data");
  descriptor_2901->setAccessPermissions(ESP_GATT_PERM_READ);  // enforce read only - default is Read|Write
  pCharacteristic->addDescriptor(descriptor_2901);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

}


void loop() {
  while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
  }

  if (deviceConnected && gps.location.isUpdated()) {
    // Serial.print("LAT: ");  Serial.println(gps.location.lat(), 6);
    // Serial.print("LONG: "); Serial.println(gps.location.lng(), 6);
    // Serial.print("SPD: ");  Serial.println(gps.speed.mps());
    // Serial.print("hour: "); Serial.println(gps.time.hour());
    // Serial.print("min: "); Serial.println(gps.time.minute());
    // Serial.print("sec: "); Serial.println(gps.time.second());
    // Serial.print("nano: "); Serial.println(gps.time.centisecond()); // hundredth of a second
    // Serial.print("year: "); Serial.println(gps.date.year());
    // Serial.print("month: "); Serial.println(gps.date.month());
    // Serial.print("date: "); Serial.println(gps.date.day());
  
    latUnion.lat = gps.location.lat(); // 32.79075; // 
    longUnion.lng = gps.location.lng(); // -96.83435; // 
    speedUnion.speed = gps.speed.mps(); // 12.3; // 
    yearUnion.year = gps.date.year();
    monthUnion.month = gps.date.month();
    dayUnion.day = gps.date.day();
    hourUnion.hour = gps.time.hour();
    minUnion.min = gps.time.minute();
    secUnion.sec = gps.time.second();
    centUnion.cent = gps.time.centisecond();

    uint8_t dataPacket[20];
    dataPacket[0] = latUnion.bytes[0];
    dataPacket[1] = latUnion.bytes[1];
    dataPacket[2] = latUnion.bytes[2];
    dataPacket[3] = latUnion.bytes[3];
    dataPacket[4] = longUnion.bytes[0];
    dataPacket[5] = longUnion.bytes[1];
    dataPacket[6] = longUnion.bytes[2];
    dataPacket[7] = longUnion.bytes[3];
    dataPacket[8] = speedUnion.bytes[0];
    dataPacket[9] = speedUnion.bytes[1];
    dataPacket[10] = speedUnion.bytes[2];
    dataPacket[11] = speedUnion.bytes[3];
    dataPacket[12] = yearUnion.bytes[0];
    dataPacket[13] = yearUnion.bytes[1];
    dataPacket[14] = monthUnion.bytes[0];
    dataPacket[15] = dayUnion.bytes[0];
    dataPacket[16] = hourUnion.bytes[0];
    dataPacket[17] = minUnion.bytes[0];
    dataPacket[18] = secUnion.bytes[0];
    dataPacket[19] = centUnion.bytes[0];
    
    // notify changed value
    if (deviceConnected) {
      pCharacteristic->setValue((uint8_t *)&dataPacket, 20);
      // pCharacteristic->setValue(lat);
      pCharacteristic->notify();
      // delay(50);
    }
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }

  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  // delay(100);
}
