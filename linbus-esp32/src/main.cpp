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

// #include <Arduino.h>
#include <AsyncUDP.h>
#include <ETH.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include <EEPROM.h>

#include "lin.h"
#include "Config.hpp"
#include "Log.hpp"
#include "Record.hpp"

#define MAX_RECORD_ENTRIES 20
Record record_list[MAX_RECORD_ENTRIES];

// buffers for receiving and sending data
#define UDP_TX_PACKET_MAX_SIZE_CUSTOM 128
byte packetBuffer[UDP_TX_PACKET_MAX_SIZE_CUSTOM]; //buffer to hold incoming packet,

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12

// this is used by the master to send lin BREAK
#define BREAK 0x00
#define SYN_FIELD 0x55
#define RED_PIN 7
#define GREEN_PIN 8
#define BLUE_PIN 9

#define DEFAULT_LIN_TIMEOUT 1000
// on lin 14, 3 is fine. 2 results in some timeout
#define TRAFFIC_TIMEOUT 4
#define QUERY_SIGNAL_SERVER_TIMEOUT 0
// #define QUERY_CONFIG_TIMEOUT 1000

// dont turn this on unless you know what you are doing.
#define USE_DHCP true

Lin lin{Serial1};
AsyncUDP udp{};
AsyncUDP udpConfig{};
Config config{udpConfig};
Logger logger{Serial};
WiFiClient client{};

//If DHCP is NOT used the RIB will take as its IP the ip_base + its ribID.
IPAddress ip_base = {192, 168, 5, 50};
//Until the server adress is known we broadcast on the entire subnet
IPAddress ipserver = {255, 255, 255, 255};

byte mac[]{0x00, 0xA7, 0x0A, 0x12, 0x1B, 0x00}; //00A70A is VOLVOO upside down with some good will. 12 1B is RIB (obviously..)

void readLinAndSendOnUdp(byte id);
void writeHeader(byte id);
Record *getRecord(byte id);
byte consumeUdpMessage(byte *buffer, int temp_timeout);
void sendOverSerial(Record *record);
void sendOverUdp(byte id, byte *payload, byte size);
byte synchHeader();
void cacheUdpMessage(Record *record, byte id);
void connectEthernet();
void led_status_ok();
void WiFiEvent(WiFiEvent_t event);

void (*SWReset)(void) = 0; //declare reset function @ address 0

static bool eth_connected = false;
int record_entries;

void setup()
{
  Serial.begin(19200);
  config.init();
  mac[5] = config.ribID();

  // configure ethernet
  // if (!USE_DHCP || !Ethernet.begin(mac))
  // { //try DHCP, but if it doesn't work, use a static IP
  //   ip_base[3] += config.ribID;
  //   Ethernet.begin(mac, ip_base); //Static IP
  // }
  WiFi.onEvent(WiFiEvent);
  ETH.begin();

  // pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  udpConfig.listen(config.configPort());

  logger.info("LIN Debugging begins");
  lin.begin(19200);
  lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);

  if (udp.connect(ipserver, config.configPort()))
  {
    logger.debug("UDP connected");
  }
}

void loop()
{
  if (eth_connected)
  {
    connectEthernet();
  }

  // TODO: Uncomment this when ready
  // RIB configuration
  // config.send_heartbeat();
  // config.parse_server_message();
  // config.verify_config();

  led_status_ok();
  // measureSignalBrokerBouce();

  //flush_udp_buffer(&Udp);

  // Actual LIN traffic
  if (config.modeMaster() == config.nodeMode())
  {
    // wait for server/automatic or manually to write arbitration frame
    int bytes = 0;
    bytes = consumeUdpMessage(packetBuffer, QUERY_SIGNAL_SERVER_TIMEOUT);
    if (bytes >= 5)
    {
      byte id = packetBuffer[3];
      Record *record = getRecord(id);
      boolean validpayload = ((bytes == 5) || (bytes == (5 + record->size())));
      if (validpayload)
      {
        if (bytes == 5)
        {
          //this is an arbiration frame
          if (!record->master())
          {
            writeHeader(id);
            // now consume the result
            readLinAndSendOnUdp(id);
          }
          else
          {
            // ignore this - a full frame will arrive on upd. Once it does we write it (length > 5)
          }
        }
        else
        {
          // this a master frame. Send it all..
          writeHeader(id);
          memcpy(record->writeCache(), &packetBuffer[5], record->size());
          sendOverSerial(record);
        }
      }
      else
      {
        std::string data = "incorrect data received over udp id ";
        data += id;
        data += " ";
        data += bytes;

        logger.error(data.c_str());
      }
    }
  }
  else if (config.modeSlave() == config.nodeMode())
  {
    if (lin.serial.available())
    {
      //find startposition look for BREAK followed BY SYN_FIELD
      // we expect this to be the header if it's not read bytes until conditions are met.
      byte id;
      //we now should have BREAK, SYN_FIELD followed by ID.
      //Now check parity
      //remove parity bits and then add them again. Check if it mathes whats expected
      byte empty = 0;
      byte idbyte = synchHeader();
      id = idbyte & 0x3f;
      if ((lin.addrParity(id) | id) == idbyte)
      {
        // send this arbitration frame to the server.
        Record *record = getRecord(id);
        // this works!!
        // if ((id != 0x10) || record->master) {
        if (record->master() || (!record->cacheValid()))
        {
          // if (true) {
          // if (record != NULL && ((boolean)record->master)) {
          // the master is sending data, just consume and report back.
          sendOverUdp(id, &empty, 0);
          // if (record->master) {
          readLinAndSendOnUdp(id);
          // }
        }
        else
        {
          // signalserver should response with the expected payload
          if (record->cacheValid())
          {
            sendOverSerial(record);
            // this is intetionally after writing to serial which might be counterintuitive,
            // in order to meet timing req for short messages
            sendOverUdp(id, &empty, 0);
          }
        }
        cacheUdpMessage(record, id);
      }
      else
      {
        std::string data = "parity failure - do nothing (id %d)", id;
        logger.error(data.c_str());
      }
    }
  }
  else
  {
    //We are neither Master nor Slave. ie. unconfigured.
  }

  logger.debug("no serial activity");
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_ETH_START:
    Serial.println("ETH Started");
    //set eth hostname here
    ETH.setHostname("esp32-ethernet");
    break;
  case SYSTEM_EVENT_ETH_CONNECTED:
    Serial.println("ETH Connected");
    break;
  case SYSTEM_EVENT_ETH_GOT_IP:
    Serial.print("ETH MAC: ");
    Serial.print(ETH.macAddress());
    Serial.print(", IPv4: ");
    Serial.print(ETH.localIP());
    if (ETH.fullDuplex())
    {
      Serial.print(", FULL_DUPLEX");
    }
    Serial.print(", ");
    Serial.print(ETH.linkSpeed());
    Serial.println("Mbps");
    eth_connected = true;
    break;
  case SYSTEM_EVENT_ETH_DISCONNECTED:
    Serial.println("ETH Disconnected");
    eth_connected = false;
    break;
  case SYSTEM_EVENT_ETH_STOP:
    Serial.println("ETH Stopped");
    eth_connected = false;
    break;
  default:
    break;
  }
}

void connectEthernet()
{
  while (!client.connect(ipserver, config.hostPort()))
  {
    logger.warning("Connection failed");
    delay(1000);
  }

  logger.info("Connected...");
}

void Abort()
{
  // pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 10; ++i)
  {
    // digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    // digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  SWReset();
}

int TxOverLin = 0;
int RxOverLin = 0;
int TxOverUdp = 0;
int RxOverUdp = 0;
int global_sync_count = 0;
int global_unsynched_packages = 0;
int global_synched_packages = 0;

void led_status_ok()
{
  // digitalWrite(LED_BUILTIN, HIGH);

  // external led
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  if (config.modeMaster() == config.nodeMode())
  {
    digitalWrite(GREEN_PIN, HIGH);
  }
  else if (config.modeSlave() == config.nodeMode())
  {
    digitalWrite(BLUE_PIN, HIGH);
  }
}

void red()
{
  // digitalWrite(LED_BUILTIN, LOW);

  // external led
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(BLUE_PIN, LOW);
}

// void flush_udp_buffer(EthernetUDP* flushUdp) {
//   int packetSize = 0;
//   do {
//     packetSize = flushUdp->parsePacket();
//   } while (packetSize != 0);
// }

// psudo code for simulating slave. Master sends arbritration messages.
// we respond appropriate. This means that slaves must be disconnected
// to avoid bus craziness.
// example frames..
//  3    D0 8 C0 B  6  0  8 BC   8D
// 47    BB F BB F 80 BB 6F   77 (where 77 is checksum and 47 is id)

Record *getRecord(byte id)
{
  Record *record = NULL;
  for (int i = 0; i < record_entries; i++)
  {
    // TODO: make a record_list
    // if (record_list[i].id == id)
    // {
    //   record = &record_list[i];
    //   break;
    // }
  }
  return record;
}
byte readbuffer[10];

void readLinAndSendOnUdp(byte id)
{
  // readbuffer   0  1  2        3        4       10
  //              id payload1 payload2...         crc
  // count        0        1        2
  boolean crc_valid = false;
  byte payload_size = 0;

  lin.serial.setTimeout(TRAFFIC_TIMEOUT);
  readbuffer[0] = (byte)(lin.addrParity(id) | id);

  Record *record = getRecord(id);
  if (record != NULL)
  {
    int verify = 0;
    payload_size = record->size();
    // adding one for crc.
    verify = lin.serial.readBytes(&readbuffer[1], payload_size + 1);
    if (verify == (payload_size + 1))
    {
      if ((id == 60) || (id == 61))
      {
        // calculate checksum in traditional way for diagnstic frames.
        crc_valid = lin.validateChecksum(&readbuffer[1], payload_size + 1);
      }
      else
      {
        crc_valid = lin.validateChecksum(&readbuffer[0], payload_size + 2);
      }
      RxOverLin++;
    }
    else
    {
    }
  }
  else
  {
    red();
  }
  // return to default timeout.
  lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);

  if (crc_valid)
  {
    led_status_ok();
    std::string data = "++crc match for id: ";
    data += id;
    data += "length is: ";
    data += payload_size;
    logger.debug(data.c_str());
    for (int i = 0; i < payload_size + 1; i++)
    {
    }
    sendOverUdp(id, &readbuffer[1], payload_size);
  }
  else
  {
    // red();
    std::string data = "--crc failed for id: 0x";
    data += id;
    data += "count is: ";
    data += payload_size;

    logger.debug(data.c_str());
  }
}

byte toServerPayload[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void sendOverUdp(byte id, byte *payload, byte size)
{
  byte *payloadLocal = &toServerPayload[5];
  toServerPayload[3] = id;
  toServerPayload[4] = size;
  // or rotate the bytes (if needed)
  for (int i = 0; i < size; i++)
  {
    payloadLocal[i] = payload[size - 1 - i];
  }
  if (!udp.connect(ipserver, config.hostPort()))
  {
    std::string data = "!!! Failed to open UDP socket: ";
    data += ipserver[0];
    data += " ";
    data += ipserver[1];
    data += " ";
    data += ipserver[2];
    data += " ";
    data += ipserver[3];
    data += " ";
    data += config.hostPort();
    data = +" for send";

    logger.error(data.c_str());
  }
  else
  {
    udp.write(toServerPayload, 5 + size);
    udp.close();
    TxOverUdp++;
  }
}

byte sendbuffer[10];
void sendOverSerial(Record *record)
{
  byte id = record->id();
  sendbuffer[0] = lin.addrParity(id) | id;
  byte *sendbuffer_payload = &sendbuffer[1];
  for (uint8_t i = 0; i < record->size(); i++)
  {
    // TODO: Create a writeCahace function

    // sendbuffer_payload[i] = (record->writeCache() - 1) - i; ///(record->size() - 1) - i);
  }
  byte *crc = &sendbuffer[1 + record->size()];
  if ((id == 60) || (id == 61))
  {
    // calculate checksum in traditional way for diagnstic frames.
    crc[0] = (byte)lin.calulateChecksum(&sendbuffer[1], record->size());
  }
  else
  {
    crc[0] = (byte)lin.calulateChecksum(sendbuffer, record->size() + 1);
  }
  //this code fixes consequent synch isseus.
  lin.serial.write(&sendbuffer[1], record->size() + 1);
  lin.serial.flush();
  lin.serial.end();
  TxOverLin++;
  lin.serial.begin(19200);
}

byte consumeUdpMessage(byte *buffer, int temp_timeout)
{
  int packetSize = 0;
  int bytes = 0;
  unsigned long endtime = millis() + temp_timeout;
  do
  {
    // TODO: Check how to handle parsepacket and timeout
    // packetSize = Udp.parsePacket();
    if (packetSize)
    {
      // Udp.setTimeout(temp_timeout);
      // bytes = Udp.read(buffer, UDP_TX_PACKET_MAX_SIZE_CUSTOM);
      // Udp.setTimeout(DEFAULT_LIN_TIMEOUT);
      RxOverUdp++;
    }
  } while ((packetSize == 0) && (millis() < endtime));
  return bytes;
}

void cacheUdpMessage(Record *record, byte id)
{
  int bytes;
  do
  {
    bytes = consumeUdpMessage(packetBuffer, QUERY_SIGNAL_SERVER_TIMEOUT);
    // fake packetbuffer
    // int bytes = 13;
    // for (int i = 0; i < 13; i++) {
    //   packetBuffer[i] = hardcoded[i];
    // }
    // id plus size shold aways be at least 5 bytes.
    if (bytes >= 4)
    {
      Record *updateRecord = record;
      if (packetBuffer[3] != id)
      {
        updateRecord = getRecord(packetBuffer[3]);
      }
      if (updateRecord == NULL)
      {
        logger.info("missmatch id does not exist");
      }
      else
      {
        memcpy(updateRecord->writeCache(), &packetBuffer[5], updateRecord->size());
        updateRecord->setCacheValid(true);
      }
    }
    else
    {
      if ((bytes < 4) && (bytes != 0))
      {
        logger.warning("Signalserver payload to short");
        red();
      }
    }
  } while (bytes != 0);
}

// ////////////////////////////////////////////////////////////////////////////////

// ///      //  ///  //
// ///      //  ///  //
// ///      //  //// //
// ///      //  // ////
// ///      //  //  ///
// //////// //  //   //

// ////////////////////////////////////////////////////////////////////////////////

// #ifndef NO_LIN

byte synchbuffer[3];
byte synchHeader()
{
  int verify = 0;
  boolean match = false;
  int synchCount = 0;
  verify = lin.serial.readBytes(synchbuffer, 3);
  if (verify != 3)
  {
    red();
    logger.info("timeout1");
  }
  match = (synchbuffer[0] == (uint8_t)BREAK) && (synchbuffer[1] == (uint8_t)SYN_FIELD);
  logger.debug("already synched...");
  if (match)
    global_synched_packages++;
  // match = (data[0] == (byte)BREAK) && (data[1] == (byte)SYN_FIELD && (data[2] == 0x03));

  while (!match)
  {
    red();
    logger.debug("synching");
    // align the buffer.
    synchbuffer[0] = synchbuffer[1];
    synchbuffer[1] = synchbuffer[2];
    verify = lin.serial.readBytes(&synchbuffer[2], 1);
    if (verify != 1)
    {
      red();
      logger.info("timeout2");
    }
    // match = (synchbuffer[0] == (byte)BREAK) && (synchbuffer[1] == (byte)SYN_FIELD && (synchbuffer[2] == 0x03));
    match = (synchbuffer[0] == (byte)BREAK) && (synchbuffer[1] == (byte)SYN_FIELD);
    synchCount++;
  }
  if (synchCount > 0)
  {
    global_sync_count += synchCount;
    global_unsynched_packages++;

    std::string data = "synch count is ";
    data += synchCount;
    data += " synchbuffer[2] & 0x3F) = 0x";
    data += (synchbuffer[2] & 0x3F);

    logger.info(data.c_str());
  }
  led_status_ok();
  return synchbuffer[2];
}

void writeHeader(byte id)
{
  byte echo[2];
  lin.serialBreak();                         // Generate the low signal that exceeds 1 char.
  lin.serial.write(0x55);                    // Sync byte
  lin.serial.write(lin.addrParity(id) | id); // ID byte
  // lin transiver will echo back, just consume the data.
  lin.serial.readBytes(echo, 2);
}

// #ifndef SAVE_MEM
// void Config::send_logg_to_server(const char *message)
// {
//   udpConfig.connect(ipserver, CONFIG_PORT);
//   udpConfig.write(HEADER);
//   udpConfig.write(config.ribID);
//   udpConfig.write(lastHash.u8.high);
//   udpConfig.write(lastHash.u8.low);
//   udpConfig.write(LOGGER);
//   udpConfig.write((byte)0x00);
//   udpConfig.write((byte)strlen(message));
//   // TODO: Cast this to preffered type
//   // udpConfig.write(message);
//   udpConfig.close();
// }
// #else
// #define send_log_to_server(X)
// #endif

// #endif //NO_LIN

// void measureSignalBrokerBouce() {
//   for (;;) {
//     unsigned long t = micros();
//     byte empty = 0;
//     sendOverUdp(0x24, &empty, 0);
//     int packetSize;
//     int bytes;
//     UNUSED(bytes);

//     //wait for signalserver to respond
//     do {
//       packetSize = Udp.parsePacket();
//       if (packetSize) {
//         bytes = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE_CUSTOM);
//       }
//     } while (packetSize == 0);

//   }
// }
// #endif //NO_LIN

// void Config::send_heartbeat()
// {
//   if (millis() > lastHeartbeat + HEARTBEAT_PERIOD)
//   {
//     //flush_udp_buffer(&udpConfig);
//     udpConfig.connect(ipserver, CONFIG_PORT);
//     udpConfig.write(HEADER);
//     udpConfig.write(ribID);
//     udpConfig.write(lastHash.u8.high);
//     udpConfig.write(lastHash.u8.low);
//     udpConfig.write(HEART_BEAT);

//     //payload size
//     udpConfig.write((byte)0x00);
//     udpConfig.write((byte)21);

//     DoubleByte local;

//     local.u16 = RxOverLin;
//     udpConfig.write((byte)HEART_BEAT_RX_LIN);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     local.u16 = TxOverLin;
//     udpConfig.write((byte)HEART_BEAT_TX_LIN);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     local.u16 = RxOverUdp;
//     udpConfig.write((byte)HEART_BEAT_RX_UDP);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     local.u16 = TxOverUdp;
//     udpConfig.write((byte)HEART_BEAT_TX_UDP);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     //sync
//     local.u16 = global_sync_count;
//     udpConfig.write((byte)HEART_BEAT_SYNC_COUNT);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     local.u16 = global_unsynched_packages;
//     udpConfig.write((byte)HEART_BEAT_UNSYNCHED_PACKAGES);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     local.u16 = global_synched_packages;
//     udpConfig.write((byte)HEART_BEAT_SYNCHED_PACKAGES);
//     udpConfig.write(local.u8.high);
//     udpConfig.write(local.u8.low);

//     //end

//     udpConfig.close();

//     lastHeartbeat = millis();

//     RxOverLin = TxOverLin = RxOverUdp = TxOverUdp = 0;
//     global_sync_count = global_unsynched_packages = global_synched_packages = 0;
//   }
// }

// void Config::request_config_item(byte item)
// {
//   //flush_udp_buffer(&udpConfig);
//   udpConfig.connect(ipserver, CONFIG_PORT);
//   udpConfig.write(HEADER);
//   udpConfig.write(ribID);
//   udpConfig.write(lastHash.u8.high);
//   udpConfig.write(lastHash.u8.low);
//   udpConfig.write(item);
//   udpConfig.write((byte)0x00);
//   udpConfig.write((byte)0x00);
//   udpConfig.close();
// }

// void Config::parse_server_message()
// {
//   // TODO: Convert this to AsyncUDP Packet info
//   int packetSize();
//   // int packetSize = udpConfig.parsePacket();
//   if (0 == packetSize)
//     return;

//   // TODO: Check how to read a packet from AsyncUDP
//   // udpConfig.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE_CUSTOM);

//   if (HEADER != packetBuffer[HEADER_OFFSET])
//   {
//     return;
//   }
//   if (ribID != packetBuffer[RIB_ID_OFFSET])
//   {
//     //This packet was intended for another RIB. If broadcasting is used that is not an error, but we are done.
//     return;
//   }

//   //If we are in broadcast mode we can switch to unicast since we now know that the sender is the server.
//   if (255 == ipserver[0])
//   {
//     ipserver = udpConfig.listenIP();
//   }

//   lastHash.u8.high = packetBuffer[HASH_HIGH_OFFSET];
//   lastHash.u8.low = packetBuffer[HASH_LOW_OFFSET];

//   if (0 == packetBuffer[IDENTIFIER_OFFSET])
//   {
//     return;
//   }

//   int message_size = (packetBuffer[PAYLOAD_SIZE_HIGH_OFFSET] << 8) | packetBuffer[PAYLOAD_SIZE_LOW_OFFSET];

//   // TODO: Check this
//   // if ((packetSize - PAYLOAD_START_OFFSET) != message_size) {
//   //   if (message_size > (UDP_TX_PACKET_MAX_SIZE_CUSTOM - PAYLOAD_START_OFFSET)) {
//   //   } else {
//   //   }
//   //   return;
//   // }

//   switch (packetBuffer[IDENTIFIER_OFFSET])
//   {
//   case HOST_PORT:
//     if (2 != message_size)
//     {
//       return;
//     }
//     hostPort = (packetBuffer[PAYLOAD_START_OFFSET] << 8) + packetBuffer[PAYLOAD_START_OFFSET + 1];
//     hostPortHash = lastHash;
//     break;

//   case CLIENT_PORT:
//     if (2 != message_size)
//     {
//       return;
//     }
//     clientPort = (packetBuffer[PAYLOAD_START_OFFSET] << 8) + packetBuffer[PAYLOAD_START_OFFSET + 1];
//     Udp.close();
//     if (!Udp.connect(ipserver, clientPort))
//     {
//       return;
//     }
//     clientPortHash = lastHash;
//     break;

//   case MESSAGE_SIZES:
//   {
//     record_entries = (message_size) / 3;
//     if (record_entries > MAX_RECORD_ENTRIES)
//     {
//
//       return;
//     }
//     int count = PAYLOAD_START_OFFSET;
//     for (int i = 0; i < record_entries; i++)
//     {
//       record_list[i].id = packetBuffer[count++];
//       record_list[i].size = packetBuffer[count++];
//       record_list[i].master = packetBuffer[count++];
//       record_list[i].cache_valid = false;
//     }
//   }
//     messageSizesHash = lastHash;
//     break;

//   case NODE_MODE:
//     if (1 != message_size)
//     {
//       return;
//     }
//     nodeMode = packetBuffer[PAYLOAD_START_OFFSET];
//     nodeModeHash = lastHash;
//     break;

//   case REASSIGN_RIBID:
//     if (1 != message_size)
//     {
//       return;
//     }
//     EEPROM.write(0, 0x13); //address @0000-@0001 = 0x131B if an ID has been stored in EEPROM
//     EEPROM.write(1, 0x1B);
//     EEPROM.write(2, packetBuffer[PAYLOAD_START_OFFSET]); //next byte is the RibID
//     Abort();
//     break;

//   default:
//     break;
//   }
// }

// void Config::verify_config()
// {
//   bool goodConfig = true;
//   unsigned long lastUpdate = millis();

//   do
//   {
//     goodConfig = true;
//     if (clientPortHash.u16 != lastHash.u16)
//     {
//       goodConfig = false;
//       request_config_item(CLIENT_PORT);
//     }
//     else if (hostPortHash.u16 != lastHash.u16)
//     {
//       goodConfig = false;
//       request_config_item(HOST_PORT);
//     }
//     else if (nodeModeHash.u16 != lastHash.u16)
//     {
//       goodConfig = false;
//       request_config_item(NODE_MODE);
//     }
//     else if (messageSizesHash.u16 != lastHash.u16)
//     {
//       goodConfig = false;
//       request_config_item(MESSAGE_SIZES);
//     }

//     if (!goodConfig)
//     {
//       while (millis() < lastUpdate + 500)
//       {
//       }
//       lastUpdate = millis();
//       parse_server_message();
//     }
//   } while (!goodConfig);
// }
