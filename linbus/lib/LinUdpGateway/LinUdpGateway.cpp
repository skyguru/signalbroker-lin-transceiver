#include "LinUdpGateway.hpp"

LinUdpGateway::LinUdpGateway(HardwareSerial &serial, Config &config, Records &records)
    : _lin(serial, TX1),
      _config(&config), _records(&records)
{
}

/**
 * @brief Init the gateway, with lin-serialport and the udp-client conifg 
 * */
void LinUdpGateway::init()
{
    _lin.begin(19200);
    _lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);

    if (_udpClient.connect(_config->serverIp(), _config->hostPort()))
    {
        Serial.printf("Connected to listenport: %d\n", _config->hostPort());
        // Callback method when package arrives on udp
        _udpClient.onPacket([&](AsyncUDPPacket packet) {
            // Serial.print("UDP Packet Type: ");
            // Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
            // Serial.print(", From: ");
            // Serial.print(packet.remoteIP());
            // Serial.print(":");
            // Serial.print(packet.remotePort());
            // Serial.print(", To: ");
            // Serial.print(packet.localIP());
            // Serial.print(":");
            // Serial.print(packet.localPort());
            // Serial.print(", Length: ");
            // Serial.print(packet.length());
            // Serial.print(", Data: ");
            // Serial.write(packet.data(), packet.length());
            // Serial.println();
        });
    }

    if (_udpListen.listen(_config->clientPort()))
    {
        Serial.printf("Connected to listenport: %d\n", _config->clientPort());
        // Callback method when package arrives on udp
        _udpListen.onPacket([&](AsyncUDPPacket packet) {
            // Serial.print("UDP Packet Type: ");
            // Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
            // Serial.print(", From: ");
            // Serial.print(packet.remoteIP());
            // Serial.print(":");
            // Serial.print(packet.remotePort());
            // Serial.print(", To: ");
            // Serial.print(packet.localIP());
            // Serial.print(":");
            // Serial.print(packet.localPort());
            // Serial.print(", Length: ");
            // Serial.print(packet.length());
            // Serial.print(", Data: ");
            // Serial.write(packet.data(), packet.length());
            // Serial.println();

            _packetBufferLength = packet.length() > _packetBuffer.size() ? _packetBuffer.size() : packet.length();
            memcpy(&_packetBuffer, packet.data(), _packetBufferLength);

            _config->incrementRxOverUdp();
            newData = true;
        });
    }
}

/**
 * @brief Write linframe-header (includes break, synch & id)
 * @param id - Frame id on linbus
 * */
void LinUdpGateway::writeHeader(uint8_t id)
{
    std::array<uint8_t, 2> echo{};
    _lin.serialBreak();                          // Generate the low signal that exceeds 1 char.
    _lin.serial.write(SYN_FIELD);                // Sync byte
    _lin.serial.write(_lin.addrParity(id) | id); // ID byte

    // lin transiver will echo back, just consume the data.
    _lin.serial.readBytes(echo.data(), echo.size());
}

/**
 *  @brief Find startposition look for BREAK followed BY SYN_FIELD
 *          we expect this to be the header if it's not read bytes until conditions are met.
 *          we now should have BREAK, SYN_FIELD followed by ID.
 * */
uint8_t LinUdpGateway::synchHeader()
{
    std::array<uint8_t, 3> synchBuffer;

    int bytesReceived = _lin.serial.readBytes(synchBuffer.data(), synchBuffer.size());
    int synchTries = 0;

    byte frameBreak = synchBuffer[0];
    byte frameSynch = synchBuffer[1];
    byte frameId = synchBuffer[2];

    boolean match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);

    while (!match)
    {
        int bytesExpected = 1;

        // align the buffer.
        frameBreak = frameSynch;
        frameSynch = frameId;
        bytesReceived = _lin.serial.readBytes(&frameId, bytesExpected);

        Serial.printf("FrameBreak: %d, FrameSynch: %d, FrameId: %d\n", frameBreak, frameSynch, frameId);

        if (bytesReceived != bytesExpected)
        {
            std::array<char, 100> message;
            sprintf(message.data(), "Timeout2: Bytes expected: %d, bytes recieved: %d, synchTries: %d", bytesExpected, bytesReceived, synchTries);
            _config->log(message.data());
        }

        match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);
        synchTries++;
    }

    _config->incrementSynchedPackages();

    if (synchTries > 0)
    {
        _config->setSynchedCount(synchTries);
        _config->incrementUnsynchedPackages();

        std::array<char, 100> message;
        sprintf(message.data(), "Synch count is %d frameId = 0x%d", synchTries, (frameId & 0x3F));
        _config->log(message.data());
    }
    return frameId;
}

/**
 * @brief We are reading frame on linbus and send the frame over UDP
 * @param id - Frame id on linbus
 * */
void LinUdpGateway::readLinAndSendOnUdp(uint8_t id)
{
    // readbuffer   0  1  2        3        4       10
    //              id payload1 payload2...         crc
    // count        0        1        2
    std::array<uint8_t, 10> readbuffer{};

    _lin.serial.setTimeout(TRAFFIC_TIMEOUT);
    readbuffer.at(0) = (uint8_t)(_lin.addrParity(id) | id);

    Record *record = _records->getRecordById(id);
    const int bytesExpected = record->size() + 1;

    if (record != nullptr)
    {
        // adding one for crc.
        int bytesReceived = _lin.serial.readBytes(&readbuffer[1], bytesExpected);
        Serial.printf("Bytes expected %d, bytes received: %d\n", bytesExpected, bytesReceived);

        bool crc_valid = false;

        if (bytesReceived == (bytesExpected))
        {
            if ((id == DiagnosticFrameId::MasterRequest) || (id == DiagnosticFrameId::SlaveRequest))
            {
                // calculate checksum in traditional way for diagnstic frames.
                crc_valid = _lin.validateChecksum(&readbuffer[1], bytesExpected);
            }
            else
            {
                // Checksum considering payload-size + id + checksum
                crc_valid = _lin.validateChecksum(&readbuffer[0], bytesExpected + 1);
            }
            _config->incrementRxOverLin();

            if (crc_valid)
            {
                sendOverUdp(id, &readbuffer[1], record->size());
            }
        }
    }

    // return to default timeout.
    _lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);
}

/**
 * @brief Send linframe data on UDP
 * @param id - Frame id on linbus
 * @note We are sending a lin-id with empty bytes when we want to pull signals from a slave
 * */
void LinUdpGateway::sendOverUdp(uint8_t id)
{
    uint8_t empty = 0;
    sendOverUdp(id, &empty, 0);
}

/**
 * @brief Send linframe data on UDP
 * @param id - Frame id on linbus
 * @param payload - Pointer to the buffer that's are going to send
 * @param size - Size of payload 
 * */
void LinUdpGateway::sendOverUdp(uint8_t id, uint8_t *payload, uint8_t size)
{
    std::array<uint8_t, 13> toServer{};

    // If payload-size is greater then total size of server
    if (size > 11)
    {
        return;
    }

    // The payload data starts at index 5
    uint8_t *payloadStart = &toServer[5];

    // Index 3 holds the id
    toServer[3] = id;

    // Index 4 holds the payload size
    toServer[4] = size;

    // or rotate the bytes (if needed)
    for (int i = 0; i < size; i++)
    {
        *(payloadStart + i) = payload[size - 1 - i];
    }

    AsyncUDPMessage message{};
    message.write(toServer.data(), size + 5);
    _udpClient.send(message);
    _config->incrementTxOverUdp();
}

/**
 * @brief Send record info over serial (LIN-serialport)
 * @param record: which record you want to send
 * */
void LinUdpGateway::sendOverSerial(Record *record)
{
    std::array<uint8_t, 10> sendbuffer{};

    uint8_t id = record->id();
    sendbuffer[0] = _lin.addrParity(id) | id;
    uint8_t *sendbuffer_payload = &sendbuffer[1];

    for (uint8_t i = 0; i < record->size(); i++)
    {
        sendbuffer_payload[i] = record->writeCache()->at(record->size() - 1 - i);
    }

    uint8_t *crc = &sendbuffer[record->size() + 1];

    if ((id == MasterRequest) || (id == SlaveRequest))
    {
        // calculate checksum in traditional way for diagnstic frames.
        crc[0] = (uint8_t)_lin.calulateChecksum(&sendbuffer[1], record->size());
    }
    else
    {
        crc[0] = (uint8_t)_lin.calulateChecksum(sendbuffer.data(), record->size() + 1);
    }
    //this code fixes consequent synch isseus.
    _lin.serial.write(&sendbuffer[1], record->size() + 1);
    _lin.serial.flush();
    _config->incrementTxOverLin();
}

/**
 * @brief
 * @param record:
 * */
void LinUdpGateway::cacheUdpMessage(Record *record)
{
    if (_packetBufferLength >= 4)
    {
        Record *updateRecord = record;
        if (_packetBuffer[3] != updateRecord->id())
        {
            updateRecord = _records->getRecordById(_packetBuffer[3]);
        }
        if (updateRecord == nullptr)
        {
            std::array<char, 100> logMessage{};
            sprintf(logMessage.data(), "Missmatch id: 0x%d does not exits", _packetBuffer[3]);
            _config->log(logMessage.data());
        }
        else
        {
            memcpy(updateRecord->writeCache(), &_packetBuffer[5], updateRecord->size());
            updateRecord->setCacheValid(true);
        }
    }
    else
    {
        if ((_packetBufferLength < 4) && (_packetBufferLength != 0))
        {
            _config->log("Signalserver payload too short");
        }
    }
}

/**
 * @brief Running the selected node continuously
 * */
void LinUdpGateway::run()
{
    switch (_config->nodeMode())
    {
    case Config::NodeModes::MASTER:
        runMaster();
        break;
    case Config::NodeModes::SLAVE:
        runSlave();
        break;
    default:
        _config->log("You heaven't specified node, please select node in the signalbroker and restart.");
        delay(500);
        break;
    }
}

/**
 * @brief Running node as a master
 * */
void LinUdpGateway::runMaster()
{
    // wait for server/automatic or manually to write arbitration frame
    if (_packetBufferLength >= 5)
    {
        // The id of linframe is stored in byte 3 of packetBuffer
        uint8_t id = _packetBuffer[3];

        // Find record with the id from packetbuffer
        Record *record = _records->getRecordById(id);

        boolean validpayload = ((_packetBufferLength == 5) || (_packetBufferLength == (5 + record->size())));

        if (validpayload)
        {
            if (_packetBufferLength == 5)
            {
                // this is an arbitration frame
                if (!record->master())
                {
                    writeHeader(id);
                    // now consume the result
                    readLinAndSendOnUdp(id);
                }
            }
            else
            {
                // this a master frame. Send it all..
                writeHeader(id);
                memcpy(record->writeCache(), &_packetBuffer[5], record->size());
                sendOverSerial(record);
            }
        }
        else
        {
            _config->log("Run Master: Incorrect data recieved over udp id");
        }
    }
}

/**
 * @brief Running node as a slave 
 * */
void LinUdpGateway::runSlave()
{
    if (_lin.serial.available())
    {
        // Get id including parity bits
        uint8_t idbyte = synchHeader();

        // To check the parity we remove the parity bits and then add them again
        uint8_t id = idbyte & 0x3f; // Remove parity bits

        if ((_lin.addrParity(id) | id) == idbyte) // Add them again and see if it match
        {
            // send this arbitration frame to the server.
            Record *record = _records->getRecordById(id);

            if (record->master() || (!record->cacheValid()))
            {
                sendOverUdp(id);
                readLinAndSendOnUdp(id);
            }
            else
            {
                // signalserver should response with the expected payload
                if (record->cacheValid())
                {
                    sendOverSerial(record);
                    // this is intetionally after writing to serial which might be counterintuitive,
                    // in order to meet timing req for short messages
                    sendOverUdp(id);
                }
            }
            cacheUdpMessage(record);
        }
        else
        {
            _config->log("Run Slave: Parity failure - do nothing ");
        }
    }
}