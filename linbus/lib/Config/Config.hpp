#pragma once

#include <AsyncUDP.h>
#include <array>

#include "Records.hpp"
#include "Common.hpp"

class Config
{
    union DoubleByte {
        uint16_t u16;
        struct U8
        {
            uint8_t low;
            uint8_t high;
        } u8;
    };

public:
    Config(Records &records);
    /**
    * "MASTER means that you need to reply to all ids, all except the master ids. For master id the data should be sent back.
    * 
    * "SLAVE" means that we send arbitration frames and master frames. Other responses should be sent back.
    * 
    * "UNDEFINED" means that you have choosed any mode yet
    */
    enum NodeModes : uint8_t
    {
        SLAVE = 0,
        MASTER = 1,
        UNDEFINED = 0xFF
    };

    enum HeartbeatModes : uint8_t
    {
        HEART_BEAT_TX_LIN = 1,
        HEART_BEAT_RX_LIN = 2,
        HEART_BEAT_TX_UDP = 3,
        HEART_BEAT_RX_UDP = 4,
        HEART_BEAT_SYNC_COUNT = 5,
        HEART_BEAT_UNSYNCHED_PACKAGES = 6,
        HEART_BEAT_SYNCHED_PACKAGES = 7
    };

    enum Offsets : uint8_t
    {
        HEADER_OFFSET = 0,
        RIB_ID_OFFSET = 1,
        HASH_HIGH_OFFSET = 2,
        HASH_LOW_OFFSET = 3,
        IDENTIFIER_OFFSET = 4,
        PAYLOAD_SIZE_HIGH_OFFSET = 5,
        PAYLOAD_SIZE_LOW_OFFSET = 6,
        PAYLOAD_START_OFFSET = 7
    };

public:
    void init(uint8_t device_identifier);
    void run();

    void requestConfigItem(uint8_t item);

    void incrementTxOverLin() { _txOverLin++; }
    void incrementTxOverUdp() { _txOverUdp++; }
    void incrementRxOverLin() { _rxOverLin++; }
    void incrementRxOverUdp() { _rxOverUdp++; }

    void setSynchedCount(int count) { _synchCount = count; }
    void incrementSynchedPackages() { _synchedPackages++; }
    void incrementUnsynchedPackages() { _unsynchedPackages++; }

    IPAddress serverIp() { return ipserver; }
    bool getLockIpAddress() { return _lockIpAddress; }

public:
    uint8_t nodeMode() const { return _nodeMode; }
    uint8_t ribID() const { return _ribID; }
    uint16_t hostPort() const { return _hostPort; }
    uint16_t clientPort() const { return _clientPort; }
    void log(const char *message);

private:
    void sendHeartbeat();
    void parseServerMessage();
    void verifyConfig();
    void logToServer(const char *message);
    void clearCounters();

private:
    static constexpr int _configPort = 4000;
    static constexpr int _heartbeatPeriod = 3000;
    static constexpr uint8_t HEADER = 0x03;
    static constexpr uint8_t HOST_PORT = (1 << 0);      // 1
    static constexpr uint8_t CLIENT_PORT = (1 << 1);    // 2
    static constexpr uint8_t MESSAGE_SIZES = (1 << 2);  // 4
    static constexpr uint8_t NODE_MODE = (1 << 3);      // 8
    static constexpr uint8_t HEART_BEAT = (1 << 4);     // 16
    static constexpr uint8_t REASSIGN_RIBID = (1 << 5); // 32
    static constexpr uint8_t LOGGER = 0x60;             // 96

    uint8_t _ribID; // This is calculated locally and part of the message header
    AsyncUDP _udp;  // Udp-client

    uint16_t _hostPort; // hostport to be used for udp-client in LinUpdGateway
    DoubleByte _hostPortHash;

    uint16_t _clientPort;
    DoubleByte _clientPortHash;

    DoubleByte _messageSizesHash;

    NodeModes _nodeMode = NodeModes::UNDEFINED;
    DoubleByte _nodeModeHash;

    std::array<uint8_t, UDP_TX_PACKET_MAX_SIZE_CUSTOM> _packetBuffer{};
    int _packetBufferLength;
    bool _newData = false;

    int _txOverLin = 0;
    int _rxOverLin = 0;
    int _txOverUdp = 0;
    int _rxOverUdp = 0;
    int _synchCount = 0;
    int _synchedPackages = 0;
    int _unsynchedPackages = 0;

    IPAddress ipserver = {255, 255, 255, 255}; // Until the server adress is known we broadcast on the entire subnet
    bool _lockIpAddress = false;               // Until we have recieved an ipaddress from the server, we have this set to false

    DoubleByte _lastHash; // Last server message had this config version (or config hash) and all config fields should match.
    Records *_records; // In here we store all out records
};