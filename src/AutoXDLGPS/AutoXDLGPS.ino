/*
  Heavily based on the example BLE notify code provided with ESP32 on Arduino IDE. 
  Modified to use TinyGPSPlus and to send a 20 byte packet from an external GPS module. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLE2901.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

// Change to give device custom name 
String deviceName = "DIY GPS";

// DO NOT CHANGE
#define SERVICE_UUID        "ECBB8159-FBA9-4123-AEF0-CA06E1D390D9"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define RXPin 16
#define TXPin 17 

#define GPS_BAUD 115200

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
BLE2901 *descriptor_2901 = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// GPS parsing 
TinyGPSPlus gps; 

// type defs for packing gps data into bytes for ble
union latBytes {
  double lat;
  uint8_t bytes[8];
} latUnion; 

union longBytes {
  double lng;
  uint8_t bytes[8];
} longUnion; 

union speedBytes {
  double speed;
  uint8_t bytes[8];
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
  Serial2.begin(GPS_BAUD, SERIAL_8N1, RXPin, TXPin);
  Serial.println("GPS Started!");

  BLEDevice::init(deviceName);

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
  while (Serial2.available()) {
      gps.encode(Serial2.read());
  }

  if (deviceConnected && gps.location.isUpdated()) {
    // parse GPS data
    latUnion.lat = gps.location.lat();
    longUnion.lng = gps.location.lng();
    speedUnion.speed = gps.speed.mps();
    yearUnion.year = gps.date.year();
    monthUnion.month = gps.date.month();
    dayUnion.day = gps.date.day();
    hourUnion.hour = gps.time.hour();
    minUnion.min = gps.time.minute();
    secUnion.sec = gps.time.second();
    centUnion.cent = gps.time.centisecond();

    // create data packet for BLE transfer
    uint8_t dataPacket[32];
    dataPacket[0] = latUnion.bytes[0];
    dataPacket[1] = latUnion.bytes[1];
    dataPacket[2] = latUnion.bytes[2];
    dataPacket[3] = latUnion.bytes[3];
    dataPacket[4] = latUnion.bytes[4];
    dataPacket[5] = latUnion.bytes[5];
    dataPacket[6] = latUnion.bytes[6];
    dataPacket[7] = latUnion.bytes[7];
    dataPacket[8] = longUnion.bytes[0];
    dataPacket[9] = longUnion.bytes[1];
    dataPacket[10] = longUnion.bytes[2];
    dataPacket[11] = longUnion.bytes[3];
    dataPacket[12] = longUnion.bytes[4];
    dataPacket[13] = longUnion.bytes[5];
    dataPacket[14] = longUnion.bytes[6];
    dataPacket[15] = longUnion.bytes[7];
    dataPacket[16] = speedUnion.bytes[0];
    dataPacket[17] = speedUnion.bytes[1];
    dataPacket[18] = speedUnion.bytes[2];
    dataPacket[19] = speedUnion.bytes[3];
    dataPacket[20] = speedUnion.bytes[4];
    dataPacket[21] = speedUnion.bytes[5];
    dataPacket[22] = speedUnion.bytes[6];
    dataPacket[23] = speedUnion.bytes[7];
    dataPacket[24] = yearUnion.bytes[0];
    dataPacket[25] = yearUnion.bytes[1];
    dataPacket[26] = monthUnion.bytes[0];
    dataPacket[27] = dayUnion.bytes[0];
    dataPacket[28] = hourUnion.bytes[0];
    dataPacket[29] = minUnion.bytes[0];
    dataPacket[30] = secUnion.bytes[0];
    dataPacket[31] = centUnion.bytes[0];
    
    // notify changed value
    if (deviceConnected) {
      pCharacteristic->setValue((uint8_t *)&dataPacket, 32);
      pCharacteristic->notify();
      delay(25);
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
    oldDeviceConnected = deviceConnected;
  }
}
