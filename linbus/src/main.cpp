// Copyright 2019 Volvo Cars
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// ”License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// “AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

/**
 * @author Alvaro Alonso & Niclas Lind
 * @note This project is based on a project from Aleksandar Filipov
 *          https://github.com/volvo-cars/signalbroker-lin-transceiver
 * 
 * @version 0.0.1
 * */

#include <Arduino.h>
#include <ETH.h>

#include "Config.hpp"
#include "Records.hpp"
#include "LinUdpGateway.hpp"

#define hostAddress 192, 168, 5, 1   // Host/Server address
#define esp32Address 192, 168, 5, 50 // Local IPAddress
#define subnetAddress 255, 255, 255, 0

constexpr uint8_t rib_id= 1;  
constexpr bool dhcp_enabled = true;
static bool eth_connected = false;

Records records{};
Config config{records};
LinUdpGateway linUdpGateway{Serial1, config, records};

void connectEthernet();
void WiFiEvent(WiFiEvent_t event);

void setup()
{
  Serial.begin(115200);
  Serial1.begin(19200);
  WiFi.onEvent(WiFiEvent);
  ETH.begin();

  if (!dhcp_enabled)
    ETH.config(IPAddress(esp32Address), IPAddress(hostAddress), IPAddress(subnetAddress));

  config.init(rib_id);

  Serial.print("Connecting");
  while (!eth_connected)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop()
{
  // Get configuration from server and send heartbeat
  config.run();

  if (!linUdpGateway.connected())
  {
    if (config.getLockIpAddress())
      linUdpGateway.init();
  }
  else
  {
    linUdpGateway.run();
  }
}

/**
 * Eventhandler to ethernet client
 * */
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_ETH_START:
  {
    config.log("WiFiEvent: ETH Started");
    ETH.setHostname("esp32-ethernet");
  }
  break;
  case SYSTEM_EVENT_ETH_CONNECTED:
  {
    config.log("WiFiEvent: ETH Connected");
  }
  break;
  case SYSTEM_EVENT_ETH_GOT_IP:
  {
    // Get MacAddress & IPAddress
    // auto macAddress = ETH.macAddress().c_str();
    // auto ipv4Address = ETH.localIP().toString().c_str();

    // std::array<char, 100> logMessage{};
    // sprintf(logMessage.data(), "WifiEvent: MacAddress: %s, IPAddress: %s", macAddress, ipv4Address);
    // config.log(logMessage.data());
    eth_connected = true;
  }
  break;
  case SYSTEM_EVENT_ETH_DISCONNECTED:
  {
    config.log("WiFiEvent: ETH Disconnected");
    eth_connected = false;
  }
  break;
  case SYSTEM_EVENT_ETH_STOP:
  {
    config.log("WiFiEvent: ETH Stopped");
    eth_connected = false;
  }
  break;
  default:
    break;
  }
}