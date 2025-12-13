#include <Arduino.h>
#include "LHRP/LHRP.hpp"

#define LED_BUILTIN 2 // Define built-in LED pin for ESP32

/* optional test address */
LHRP_Peer peer1 = {{0x88, 0x13, 0xBF, 0x0B, 0xA6, 0x6C}, {1}};
LHRP_Peer peer2 = {{0x88, 0x13, 0xBF, 0x0B, 0x62, 0x18}, {2}};
LHRP_Peer peer3 = {{0xA0, 0xB7, 0x65, 0x2C, 0x5A, 0x18}, {3}};

/* create node with peers */
LHRP_Node net({
    peer2,
    peer1,
    peer3,
});

void setup()
{
  Serial.begin(115200);
  Serial.println("LHRP Node Starting...");

  // led test
  pinMode(LED_BUILTIN, OUTPUT);
  analogWrite(LED_BUILTIN, 200);
  delay(100);
  analogWrite(LED_BUILTIN, 0);

  // print the esps mac
  Serial.print("MAC Address: {");
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] < 16)
      Serial.print("0");
    Serial.print("0x");
    Serial.print(mac[i], HEX);
    if (i < 5)
      Serial.print(", ");
  }
  Serial.println("}");

  delay(1000);

  net.onPocketReceive([&](const Pocket &pocket) { // handle received pocket
    Serial.println("Received pocket for Address:");
    Serial.println(pocket.address[0]);
    analogWrite(LED_BUILTIN, pocket.payload[0]);
    Serial.println("LED brightness set to " + String(pocket.payload[0]));
  });

  Serial.println(net.begin() ? "LHRP Node Started!" : "LHRP Node Failed to Start!");
}

void loop()
{
  // Serial.println(net.send(Pocket{{2}, {(uint8_t)random(255)}})); // send test pocket to address 2
  // Serial.println(net.send(Pocket{{3}, {(uint8_t)random(255)}})); // send test pocket to address 3
  // Serial.println(net.send(Pocket{{2, 1}, {(uint8_t)random(255)}})); // send test pocket to address 2 (with subadress 1)
  delay(1000);
}
