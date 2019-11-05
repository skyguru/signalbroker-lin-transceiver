#pragma once

#include <cstdint>
#include <AsyncUDP.h>

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
    Config(AsyncUDP &udp);

public:
    void init();
    void sendHeartbeat(int globalSynchedCount, int globalUnsynchedPackages, int globalSynchedPackeges);
    // void parse_server_message();
    void verifyConfig();
    void requestConfigItem(uint8_t item);
    // void send_logg_to_server(const char *message);

public:
    uint8_t nodeMode() const { return _nodeMode; }
    uint8_t ribID() const { return _ribID; }

public:
    static constexpr int configPort() { return CONFIG_PORT; }
    static constexpr int debugPortBase() { return DEBUG_PORT_BASE; }
    static constexpr int heartBeatPeriod() { return HEARTBEAT_PERIOD; }
    static constexpr uint8_t header() { return HEADER; }
    static constexpr uint8_t hostPort() { return HOST_PORT; }
    static constexpr uint8_t clientPort() { return CLIENT_PORT; }
    static constexpr uint8_t modeMaster() { return MODE_MASTER; }
    static constexpr uint8_t modeSlave() { return MODE_SLAVE; }

private:
    uint8_t getRibID();

private:
    static constexpr int CONFIG_PORT = 4000;
    static constexpr int DEBUG_PORT_BASE = 3000;
    static constexpr int HEARTBEAT_PERIOD = 3000;
    static constexpr uint8_t HEADER = 0x03;
    static constexpr uint8_t HOST_PORT = (1 << 0);      // 1
    static constexpr uint8_t CLIENT_PORT = (1 << 1);    // 2
    static constexpr uint8_t MESSAGE_SIZES = (1 << 2);  // 4
    static constexpr uint8_t NODE_MODE = (1 << 3);      // 8
    static constexpr uint8_t REASSIGN_RIBID = (1 << 5); // 32
    static constexpr uint8_t LOGGER = 0x60;             // 96

    // Heartbeat
    static constexpr uint8_t HEART_BEAT = (1 << 4);
    static constexpr uint8_t HEART_BEAT_TX_LIN = 1;
    static constexpr uint8_t HEART_BEAT_RX_LIN = 2;
    static constexpr uint8_t HEART_BEAT_TX_UDP = 3;
    static constexpr uint8_t HEART_BEAT_RX_UDP = 4;
    static constexpr uint8_t HEART_BEAT_SYNC_COUNT = 5;
    static constexpr uint8_t HEART_BEAT_UNSYNCHED_PACKAGES = 6;
    static constexpr uint8_t HEART_BEAT_SYNCHED_PACKAGES = 7;

    // LIN-Mode
    static constexpr uint8_t MODE_SLAVE = 0;
    static constexpr uint8_t MODE_MASTER = 1;
    static constexpr uint8_t MODE_NONE = 0xFF;

    // Offset
    static constexpr uint8_t HEADER_OFFSET = 0;
    static constexpr uint8_t RIB_ID_OFFSET = 1;
    static constexpr uint8_t HASH_HIGH_OFFSET = 2;
    static constexpr uint8_t HASH_LOW_OFFSET = 3;
    static constexpr uint8_t IDENTIFIER_OFFSET = 4;
    static constexpr uint8_t PAYLOAD_SIZE_HIGH_OFFSET = 5;
    static constexpr uint8_t PAYLOAD_SIZE_LOW_OFFSET = 6;
    static constexpr uint8_t PAYLOAD_START_OFFSET = 7;

    uint8_t _ribID; //This is calculated locally and part of the message header
    AsyncUDP _udp;

    uint16_t _hostPort;
    DoubleByte _hostPortHash;
    uint16_t _clientPort;
    DoubleByte _clientPortHash;
    //size_t messageSizes;
    DoubleByte _messageSizesHash;
    // "MODE_MASTER means that you need to reply to all ids, all except the master ids. For master id the data should be sent back.
    // "MODE_SLAVE" means that we send arbitration frames and master frames. Other responses should be sent back.
    uint8_t _nodeMode;
    DoubleByte _nodeModeHash;

    DoubleByte _lastHash; //Last server message had this config version (or config hash) and all config fields should match.
    unsigned long _lastHeartbeat;
};