#include <Arduino.h>
#include "LHRP/LHRP.hpp"

#define LED_BUILTIN 2 // Define built-in LED pin for ESP32

#include "get-node-configuration.hpp"

#define CVG networkConfiguration1

/* create node with peers */
LHRP_Node net = getNodeConfiguration(CVG);

void blink()
{
  // led test
  pinMode(LED_BUILTIN, OUTPUT);
  analogWrite(LED_BUILTIN, 200);
  delay(100);
  analogWrite(LED_BUILTIN, 0);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("LHRP Node Starting...");

  blink();

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
    blink();
    Serial.println("Received pocket for Address:");
    Serial.println(pocket.address[0]);
    analogWrite(LED_BUILTIN, pocket.payload[0]);
    Serial.println("LED brightness set to " + String(pocket.payload[0]));
  });

  Serial.println(net.begin() ? "LHRP Node Started!" : "LHRP Node Failed to Start!");
}

void loop()
{
  if (isSender())
  {
    if (random(5) == 0)
      Serial.println(net.send(Pocket{CVG.node1, {(uint8_t)random(255)}}));
    if (random(5) == 0)
      Serial.println(net.send(Pocket{CVG.node2, {(uint8_t)random(255)}}));
    if (random(5) == 0)
      Serial.println(net.send(Pocket{CVG.node3, {(uint8_t)random(255)}}));
  }
  delay(1000);
}
