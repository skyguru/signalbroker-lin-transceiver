#include "Config.hpp"

Config::Config(Records &records)
    : _records{&records}
{
}

/**
 * @breif: Init the config-udp
 * @param: deveice_identifier: this identifier needs for the serverside
 * */
void Config::init(uint8_t device_identifier)
{
    _ribID = device_identifier;

    _lastHash.u16 = 0xFFFF;
    _clientPortHash.u16 = 0;
    _hostPortHash.u16 = 0;
    _nodeModeHash.u16 = 0;
    _messageSizesHash.u16 = 0;

    if (_udp.listen(_configPort))
    {
        _udp.onPacket([&](AsyncUDPPacket packet) {
            std::array<char, 100> message;
            sprintf(message.data(), "From: %s: %d, To: %s: %d, Length: %d",
                    packet.remoteIP().toString().c_str(), packet.remotePort(),
                    packet.localIP().toString().c_str(), packet.localPort(), packet.length());

            log(message.data());

            if (packet.length() > packetBuffer.size())
            {
                memcpy(packetBuffer.data(), packet.data(), packetBuffer.size());
                _packetBufferLength = packetBuffer.size();
            }
            else
            {
                memcpy(packetBuffer.data(), packet.data(), packet.length());
                _packetBufferLength = packet.length();
            }

            if (!_lockIpAddress)
            {
                ipserver = packet.remoteIP();
                _lockIpAddress = true;
            }
            _newData = true;
        });
    }
}

/**
 * @breif: Sending heartbeat, parse server message & verify config
 * */
void Config::run()
{
    sendHeartbeat();
    parseServerMessage();
    verifyConfig();
}

void Config::sendHeartbeat()
{
    for (static ulong lastHeartbeat = 0; millis() > lastHeartbeat + _heartbeatPeriod; lastHeartbeat = millis())
    {
        AsyncUDPMessage heartbeat{};
        heartbeat.write(HEADER);
        heartbeat.write(_ribID);
        heartbeat.write(_lastHash.u8.high);
        heartbeat.write(_lastHash.u8.low);
        heartbeat.write(HEART_BEAT);

        //payload size
        heartbeat.write((uint8_t)0x00);
        heartbeat.write((uint8_t)21);

        DoubleByte local;

        local.u16 = _rxOverLin;
        heartbeat.write((uint8_t)HEART_BEAT_RX_LIN);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        local.u16 = _txOverLin;
        heartbeat.write((uint8_t)HEART_BEAT_TX_LIN);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        local.u16 = _rxOverUdp;
        heartbeat.write((uint8_t)HEART_BEAT_RX_UDP);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        local.u16 = _txOverUdp;
        heartbeat.write((uint8_t)HEART_BEAT_TX_UDP);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        //sync
        local.u16 = _synchCount;
        heartbeat.write((uint8_t)HEART_BEAT_SYNC_COUNT);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        local.u16 = _unsynchedPackages;
        heartbeat.write((uint8_t)HEART_BEAT_UNSYNCHED_PACKAGES);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        local.u16 = _synchedPackages;
        heartbeat.write((uint8_t)HEART_BEAT_SYNCHED_PACKAGES);
        heartbeat.write(local.u8.high);
        heartbeat.write(local.u8.low);

        _udp.broadcast(heartbeat);

        clearCounters();
    }
}

void Config::verifyConfig()
{
    bool goodConfig = false;

    while (!goodConfig)
    {
        goodConfig = true;
        if (_clientPortHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(CLIENT_PORT);
        }
        else if (_hostPortHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(HOST_PORT);
        }
        else if (_nodeModeHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(NODE_MODE);
        }
        else if (_messageSizesHash.u16 != _lastHash.u16)
        {
            goodConfig = false;
            requestConfigItem(MESSAGE_SIZES);
        }

        if (!goodConfig)
        {
            // Wait 500ms
            delay(500);
            parseServerMessage();
        }
    }
}

void Config::requestConfigItem(uint8_t item)
{
    std::array<char, 50> logMessage;
    sprintf(logMessage.data(), "Rerequesting config item 0x%x", item);
    log(logMessage.data());

    AsyncUDPMessage message{};

    message.write(HEADER);
    message.write(_ribID);
    message.write(_lastHash.u8.high);
    message.write(_lastHash.u8.low);
    message.write(item);
    message.write((uint8_t)0x00);
    message.write((uint8_t)0x00);
    _udp.broadcast(message);
}

void Config::sendLogToServer(const char *message)
{
    AsyncUDPMessage logMessage{};

    logMessage.write(HEADER);
    logMessage.write(_ribID);
    logMessage.write(_lastHash.u8.high);
    logMessage.write(_lastHash.u8.low);
    logMessage.write(LOGGER);
    logMessage.write((uint8_t)0x00);
    logMessage.write((uint8_t)strlen(message));
    logMessage.print(message);

    _udp.broadcast(logMessage);
}

void Config::parseServerMessage()
{
    // We return if we don't have any new data
    if (!_newData)
    {
        return;
    }

    _newData = false;

    if (HEADER != packetBuffer[HEADER_OFFSET])
    {
        return;
    }

    if (_ribID != packetBuffer[RIB_ID_OFFSET])
    {
        //This packet was intended for another RIB. If broadcasting is used that is not an error, but we are done.
        return;
    }

    _lastHash.u8.high = packetBuffer[HASH_HIGH_OFFSET];
    _lastHash.u8.low = packetBuffer[HASH_LOW_OFFSET];

    if (0 == packetBuffer[IDENTIFIER_OFFSET])
    {
        return;
    }

    int message_size = (packetBuffer[PAYLOAD_SIZE_HIGH_OFFSET] << 8) | packetBuffer[PAYLOAD_SIZE_LOW_OFFSET];

    if ((_packetBufferLength - PAYLOAD_START_OFFSET) != message_size)
    {
        if (message_size > (packetBuffer.size() - PAYLOAD_START_OFFSET))
        {
            const char *message = "Udp_TX_PACKET_MAX_SIZE_CUSTOM is smaller then message size";
            log(message);
        }
        else
        {
            const char *message = "payload size missmatch";
            log(message);
        }
        return;
    }

    switch (packetBuffer[IDENTIFIER_OFFSET])
    {
    case HOST_PORT:
        if (2 != message_size)
        {
            return;
        }
        _hostPort = (packetBuffer[PAYLOAD_START_OFFSET] << 8) + packetBuffer[PAYLOAD_START_OFFSET + 1];
        Serial.printf("Hostport: %d\n", _hostPort);
        _hostPortHash = _lastHash;
        break;

    case CLIENT_PORT:
        if (2 != message_size)
        {
            return;
        }
        _clientPort = (packetBuffer[PAYLOAD_START_OFFSET] << 8) + packetBuffer[PAYLOAD_START_OFFSET + 1];
        Serial.printf("Clientport: %d\n", _clientPort);
        _clientPortHash = _lastHash;
        break;

    case MESSAGE_SIZES:
    {
        int record_entries = (message_size) / 3;
        if (record_entries > _records->recordEntries())
        {
            return;
        }

        int count = PAYLOAD_START_OFFSET;
        for (int i = 0; i < record_entries; i++)
        {
            Record *record = _records->getRecord(i);
            int id = packetBuffer[count++];
            int size = packetBuffer[count++];
            int master = packetBuffer[count++];

            record->setId(id);
            record->setSize(size);
            record->setMaster(master);
            record->setCacheValid(false);

            std::array<char, 100> logMessage;
            sprintf(logMessage.data(), "At address: %p, Id: %d, size: %d, master: %d\n", record, record->id(), record->size(), record->master());
            log(logMessage.data());
        }
    }
        _messageSizesHash = _lastHash;
        break;

    case NODE_MODE:
        if (1 != message_size)
        {
            return;
        }
        _nodeMode = (NodeModes)packetBuffer[PAYLOAD_START_OFFSET];
        _nodeModeHash = _lastHash;
        break;

    default:
        break;
    }
}

void Config::log(const char *message)
{
    if (LOG_TO_SERIAL)
    {
        Serial.println(message);
    }
    else
    {
        LogToServer(message);
    }
}

void Config::LogToServer(const char *message)
{
    AsyncUDPMessage udpMessage{};
    udpMessage.write(HEADER);
    udpMessage.write(_ribID);
    udpMessage.write(_lastHash.u8.high);
    udpMessage.write(_lastHash.u8.low);
    udpMessage.write(LOGGER);
    udpMessage.write((byte)0x00);
    udpMessage.write((byte)strlen(message));
    udpMessage.println(message);

    _udp.broadcast(udpMessage);
}

void Config::clearCounters()
{
    _rxOverLin = 0;
    _txOverLin = 0;
    _rxOverUdp = 0;
    _txOverUdp = 0;
    _synchCount = 0;
    _synchedPackages = 0;
    _unsynchedPackages = 0;
}