#include "Config.hpp"

Config::Config(Records &records)
    : _records{&records}
{
}

/**
 * @brief Init the config-udp
 * @param deveice_identifier: Set identifier so the server knows who it's talking to
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

            auto remoteIp = packet.remoteIP().toString().c_str();
            auto localIp = packet.localIP().toString().c_str();

            sprintf(message.data(), "From: %s: %d, To: %s: %d, Length: %d",
                    remoteIp, packet.remotePort(),
                    localIp, packet.localPort(), packet.length());

            log(message.data());

            _packetBufferLength = packet.length() > _packetBuffer.size() ? _packetBuffer.size() : packet.length();
            memcpy(_packetBuffer.data(), packet.data(), _packetBufferLength);

            if (!_lockIpAddress)
            {
                ipserver = packet.remoteIP();
                _lockIpAddress = true;

                std::array<char, 50> debugMessage;
                sprintf(debugMessage.data(), "Locked IPAddress: %d", _lockIpAddress);
                log(debugMessage.data());
            }
            _newData = true;
        });
    }
}

/**
 * @brief Sending heartbeat, parse server message & verify config
 * */
void Config::run()
{
    sendHeartbeat();
    parseServerMessage();
    verifyConfig();
}

/**
 * @brief Sending heartbeat to server every X millis sec
 * */
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

/**
 * @brief Verifying that the data we got from server seems valid
 * */
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
            delay(200);
            parseServerMessage();
        }
    }
}

/**
 * @brief Requesting item from server
 * @param item: Object we want to have info about
 * */
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

/**
 * @brief Parse the message comming from the server
 * */
void Config::parseServerMessage()
{
    // We return if we don't have any new data
    if (!_newData)
    {
        return;
    }

    _newData = false;

    if (HEADER != _packetBuffer[HEADER_OFFSET])
    {
        return;
    }

    if (_ribID != _packetBuffer[RIB_ID_OFFSET])
    {
        //This packet was intended for another RIB. If broadcasting is used that is not an error, but we are done.
        return;
    }

    _lastHash.u8.high = _packetBuffer[HASH_HIGH_OFFSET];
    _lastHash.u8.low = _packetBuffer[HASH_LOW_OFFSET];

    if (0 == _packetBuffer[IDENTIFIER_OFFSET])
    {
        return;
    }

    int message_size = (_packetBuffer[PAYLOAD_SIZE_HIGH_OFFSET] << 8) | _packetBuffer[PAYLOAD_SIZE_LOW_OFFSET];

    if ((_packetBufferLength - PAYLOAD_START_OFFSET) != message_size)
    {
        if (message_size > (_packetBuffer.size() - PAYLOAD_START_OFFSET))
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

    switch (_packetBuffer[IDENTIFIER_OFFSET])
    {
    case HOST_PORT:
        if (2 != message_size)
        {
            return;
        }
        _hostPort = (_packetBuffer[PAYLOAD_START_OFFSET] << 8) + _packetBuffer[PAYLOAD_START_OFFSET + 1];
        Serial.printf("Hostport: %d\n", _hostPort);
        _hostPortHash = _lastHash;
        break;

    case CLIENT_PORT:
        if (2 != message_size)
        {
            return;
        }
        _clientPort = (_packetBuffer[PAYLOAD_START_OFFSET] << 8) + _packetBuffer[PAYLOAD_START_OFFSET + 1];
        Serial.printf("Clientport: %d\n", _clientPort);
        _clientPortHash = _lastHash;
        break;

    case MESSAGE_SIZES:
    {
        int record_entries = (message_size) / 3;

        int count = PAYLOAD_START_OFFSET;

        for (int i = 0; i < record_entries; i++)
        {
            Record record{};
            int id = _packetBuffer[count++];
            int size = _packetBuffer[count++];
            int master = _packetBuffer[count++];

            record.setId(id);
            record.setSize(size);
            record.setMaster(master);
            record.setCacheValid(false);
            _records->add(record);

            std::array<char, 100> logMessage;
            sprintf(logMessage.data(), "Id: %d, size: %d, master: %d",
                    _records->getLastElement()->id(),
                    _records->getLastElement()->size(),
                    _records->getLastElement()->master());

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
        _nodeMode = (NodeModes)_packetBuffer[PAYLOAD_START_OFFSET];
        _nodeModeHash = _lastHash;
        break;

    default:
        break;
    }
}

/**
 * @brief: Depending on the state of LOG_TO_SERIAL we either log to serialport or udp 
 * @param message: The message that will be printed in the log
 * */
void Config::log(const char *message)
{
    if (LOG_TO_SERIAL)
    {
        Serial.println(message);
    }
    else
    {
        logToServer(message);
    }
}

/**
 * @brief Sending log message to Udp-client instead of Serialport
 * @param message: The message that will be printed in the log
 * */
void Config::logToServer(const char *message)
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

/**
 * @brief Clear all counters 
 * */
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