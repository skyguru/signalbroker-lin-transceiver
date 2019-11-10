#pragma once

#include <Arduino.h>
#include <AsyncUDP.h>

#include "lin.h"
#include "Config.hpp"
#include "Common.hpp"

class LinUdpGateway
{
    enum DiagnosticFrameId : uint8_t
    {
        MasterRequest = 60,
        SlaveRequest = 61
    };

public:
    LinUdpGateway(HardwareSerial &serial, Config &config, Records &records);

    void init();

public:
    void readLinAndSendOnUdp(byte id);
    void writeHeader(byte id);
    uint8_t synchHeader();
    void sendOverUdp(byte id);
    void sendOverUdp(byte id, byte *payload, byte size);
    void sendOverSerial(Record *record);
    void cacheUdpMessage(Record *record);

    bool connected() { return _udpClient.connected(); }
    void run();

private:
    void runMaster();
    void runSlave();

private:
    AsyncUDP _udpClient;
    Lin _lin;
    Config *_config;
    Records *_records;

    std::array<uint8_t, UDP_TX_PACKET_MAX_SIZE_CUSTOM> _packetBuffer{};
    int packetBufferLength;

    static constexpr uint8_t BREAK = 0x00;
    static constexpr uint8_t SYN_FIELD = 0x55;

    // this is used by the master to send lin BREAK
    // on lin 14, 3 is fine. 2 results in some timeout
    static constexpr uint8_t TRAFFIC_TIMEOUT = 4;
    static constexpr uint8_t QUERY_SIGNAL_SERVER_TIMEOUT = 0;
    static constexpr uint16_t DEFAULT_LIN_TIMEOUT = 1000;

    bool newData = false;
};