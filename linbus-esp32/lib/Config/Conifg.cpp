#include "Config.hpp"

#include <EEPROM.h>

Config::Config(AsyncUDP &udp)
    : _udp{udp}
{
}

void Config::init()
{
    _ribID = getRibID();

    _lastHash.u16 = 0xFFFF;
    _clientPortHash.u16 = 0;
    _hostPortHash.u16 = 0;
    _nodeModeHash.u16 = 0;
    _messageSizesHash.u16 = 0;

    _lastHeartbeat = 0;
}

void Config::sendHeartbeat(int globalSynchedCount, int globalUnsynchedPackages, int globalSynchedPackages)
{
    for (ulong lastHeartbeat = 0; millis() > lastHeartbeat + HEARTBEAT_PERIOD; lastHeartbeat = millis())
    {
        // flush_udp_buffer(_udp);
        // debugPrintln("Sending heartbeat with hash 0x%x", lastHash.u16);
        //  _udp.connect(ipserver, CONFIG_PORT);
        _udp.write(HEADER);
        _udp.write(_ribID);
        _udp.write(_lastHash.u8.high);
        _udp.write(_lastHash.u8.low);
        _udp.write(HEART_BEAT);

        //payload size
        _udp.write((byte)0x00);
        _udp.write((byte)21);

        DoubleByte local;

        // local.u16 = RxOverLin;
        _udp.write((byte)HEART_BEAT_RX_LIN);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        // local.u16 = TxOverLin;
        _udp.write((byte)HEART_BEAT_TX_LIN);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        // local.u16 = RxOverUdp;
        _udp.write((byte)HEART_BEAT_RX_UDP);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        // local.u16 = TxOverUdp;
        _udp.write((byte)HEART_BEAT_TX_UDP);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        //sync
        local.u16 = globalSynchedCount;
        _udp.write((byte)HEART_BEAT_SYNC_COUNT);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        local.u16 = globalUnsynchedPackages;
        _udp.write((byte)HEART_BEAT_UNSYNCHED_PACKAGES);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        local.u16 = globalSynchedPackages;
        _udp.write((byte)HEART_BEAT_SYNCHED_PACKAGES);
        _udp.write(local.u8.high);
        _udp.write(local.u8.low);

        //end

        _udp.close();

        // debugPrintln("RxOverLin: %d, TxOverLin: %d, RxOverUdp: %d, TxOverUdp: %d",
        //   RxOverLin,TxOverLin,RxOverUdp,TxOverUdp);
        // RxOverLin = TxOverLin = RxOverUdp = TxOverUdp = 0;

        // TODO: Move this code out from function
        // global_sync_count = global_unsynched_packages = global_synched_packages = 0;
    }
}

void Config::verifyConfig()
{
    bool goodConfig = true;
    unsigned long lastUpdate = millis();

    do
    {
        goodConfig = true;
        if (this->_clientPortHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(CLIENT_PORT);
        }
        else if (this->_hostPortHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(HOST_PORT);
        }
        else if (this->_nodeModeHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(NODE_MODE);
        }
        else if (this->_messageSizesHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(MESSAGE_SIZES);
        }

        if (!goodConfig)
        {
            while (millis() < lastUpdate + 500)
            {
            }
            lastUpdate = millis();
            // parse_server_message();
        }
    } while (!goodConfig);
}

void Config::requestConfigItem(uint8_t item)
{
    //flush_udp_buffer(&UdpConfig);
    // debugPrintln("Rerequesting config item 0x%x", item);
    // UdpConfig.connect(ipserver, CONFIG_PORT);
    // UdpConfig.write(HEADER);
    // UdpConfig.write(ribID);
    // UdpConfig.write(lastHash.u8.high);
    // UdpConfig.write(lastHash.u8.low);
    // UdpConfig.write(item);
    // UdpConfig.write((byte)0x00);
    // UdpConfig.write((byte)0x00);
    // UdpConfig.close();
}

uint8_t Config::getRibID()
{
    //address @0000-@0001 = 0x131B if an ID has been stored in EEPROM
    if (0x13 == EEPROM.read(0) && 0x1B == EEPROM.read(1))
    {
        return EEPROM.read(2); //next byte is the RibID
    }

    // Read jumper configuration
    // Pull any combination of pins 2,3 and 4 to LOW to set a RIB ID
    // Pins will initially have random values unless pulled to HIGH or LOW.
    // First pull the config pins to HIGH from SW and then check what pins have been pulled LOW by bridging to GND.
    int jumperCfg = 1; //one-based index
    const int firstCfgPin = 2;
    const int lastCfgPin = 4;
    for (int pin = firstCfgPin; pin <= lastCfgPin; ++pin)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        pinMode(pin, INPUT);
        if (LOW == digitalRead(pin))
        {
            jumperCfg += 1 << (pin - firstCfgPin);
        }
    }
    return jumperCfg;
}