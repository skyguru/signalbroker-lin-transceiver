#include "LinUdpGateway.hpp"

LinUdpGateway::LinUdpGateway(HardwareSerial &serial, Config &config, Records &records)
    : _lin(serial),
      _config(&config), _records(&records)
{
}

void LinUdpGateway::init()
{
    _lin.begin(19200);
    _lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);

    if (_udpClient.connect(_config->serverIp(), _config->hostPort()))
    {
        // Callback method when package arrives on udp
        _udpClient.onPacket([&](AsyncUDPPacket packet) {
            Serial.print("UDP Packet Type: ");
            Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
            Serial.print(", From: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", To: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", Length: ");
            Serial.print(packet.length());
            Serial.print(", Data: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();

            if (packet.length() > _packetBuffer.size())
            {
                memcpy(_packetBuffer.data(), packet.data(), _packetBuffer.size());
                packetBufferLength = _packetBuffer.size();
            }
            else
            {
                memcpy(_packetBuffer.data(), packet.data(), packet.length());
                packetBufferLength = packet.length();
            }

            _config->incrementRxOverUdp();

            newData = true;
        });
    }
}

void LinUdpGateway::writeHeader(uint8_t id)
{
    uint8_t echo[2];
    _lin.serialBreak();                          // Generate the low signal that exceeds 1 char.
    _lin.serial.write(0x55);                     // Sync byte
    _lin.serial.write(_lin.addrParity(id) | id); // ID byte

    // lin transiver will echo back, just consume the data.
    _lin.serial.readBytes(echo, 2);
}

/**
 *  find startposition look for BREAK followed BY SYN_FIELD
 *  we expect this to be the header if it's not read bytes until conditions are met.
 *  we now should have BREAK, SYN_FIELD followed by ID.
 * */
uint8_t LinUdpGateway::synchHeader()
{
    int bytesExpected = 3;
    uint8_t synchbuffer[bytesExpected];

    int bytesReceived = _lin.serial.readBytes(synchbuffer, bytesExpected);
    int synchTries = 0;

    uint8_t frameBreak = synchbuffer[0];
    uint8_t frameSynch = synchbuffer[1];
    uint8_t frameId = synchbuffer[2];

    if (bytesReceived != bytesExpected)
    {
        std::array<char, 100> message;
        sprintf(message.data(), "Timeout1: Bytes expected: %d, bytes recieved: %d\n", bytesExpected, bytesReceived);
        _config->log(message.data());
    }

    boolean match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);

    while (!match)
    {
        bytesExpected = 1;
        // align the buffer.
        frameBreak = frameSynch;
        frameSynch = frameId;
        bytesReceived = _lin.serial.readBytes(&frameId, bytesExpected);

        if (bytesReceived != bytesExpected)
        {
            char message[100];
            sprintf(message, "Timeout2: Bytes expected: %d, bytes recieved: %d, synchTries: %d\n", bytesExpected, bytesReceived, synchTries);
            _config->log(message);
        }
        match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);
        synchTries++;
    }

    _config->incrementSynchedPackages();

    if (synchTries > 0)
    {
        _config->setSynchedCount(synchTries);
        _config->incrementUnsynchedPackages();

        std::array<char,100> message;
        sprintf(message.data(), "Synch count is %d frameId = 0x%d\n", synchTries, (frameId & 0x3F));
        _config->log(message.data());
    }
    return frameId;
}

void LinUdpGateway::readLinAndSendOnUdp(uint8_t id)
{
    std::array<uint8_t, 10> readbuffer;
    // readbuffer   0  1  2        3        4       10
    //              id payload1 payload2...         crc
    // count        0        1        2
    boolean crc_valid = false;
    uint8_t payload_size = 0;

    _lin.serial.setTimeout(TRAFFIC_TIMEOUT);
    readbuffer[0] = (uint8_t)(_lin.addrParity(id) | id);

    Record *record = _records->getRecordById(id);

    if (record != nullptr)
    {
        payload_size = record->size();
        // adding one for crc.
        int bytesReceived = _lin.serial.readBytes(&readbuffer[1], payload_size + 1);

        if (bytesReceived == (payload_size + 1))
        {
            if ((id == DiagnosticFrameId::MasterRequest) || (id == DiagnosticFrameId::SlaveRequest))
            {
                // calculate checksum in traditional way for diagnstic frames.
                crc_valid = _lin.validateChecksum(&readbuffer[1], payload_size + 1);
            }
            else
            {
                // Checksum considering payload-size + id + checksum
                crc_valid = _lin.validateChecksum(&readbuffer[0], payload_size + 2);
            }
            _config->incrementRxOverLin();

            if (crc_valid)
            {
                Serial.write(readbuffer.data(), readbuffer.size());
                Serial.println();
                sendOverUdp(id, &readbuffer[1], payload_size);
            }
        }
    }

    // return to default timeout.
    _lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);
}

void LinUdpGateway::sendOverUdp(uint8_t id)
{
    sendOverUdp(id, 0, 0);
}

void LinUdpGateway::sendOverUdp(uint8_t id, uint8_t *payload, uint8_t size)
{
    uint8_t toServer[]{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // If payload-size is bigger then total size of server
    if (size > 11)
    {
        return;
    }

    uint8_t *payloadStart = &toServer[5];
    toServer[3] = id;
    toServer[4] = size;
    // or rotate the bytes (if needed)
    for (int i = 0; i < size; i++)
    {
        *(payloadStart + i) = payload[size - 1 - i];
        // *(payloadStart + i) = payload[i];
    }

    AsyncUDPMessage message{};
    message.write(toServer, 5 + size);
    _udpClient.send(message);
    _config->incrementTxOverUdp();
}

void LinUdpGateway::sendOverSerial(Record *record)
{
    std::array<uint8_t, 10> sendbuffer;

    uint8_t id = record->id();
    sendbuffer[0] = _lin.addrParity(id) | id;
    uint8_t *sendbuffer_payload = &sendbuffer[1];
    for (uint8_t i = 0; i < record->size(); i++)
    {
        // sendbuffer_payload[i] = *(record->writeCache() + (record->size() - 1 - i));
        sendbuffer_payload[i] = record->writeCache()->at(record->size() - 1 - i);
     }
    uint8_t *crc = &sendbuffer[1 + record->size()];
    if ((id == 60) || (id == 61))
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

void LinUdpGateway::cacheUdpMessage(Record *record)
{
    do
    {
        if (packetBufferLength >= 4)
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
            if ((packetBufferLength < 4) && (packetBufferLength != 0))
            {
                _config->log("Signalserver payload too short");
            }
        }
    } while (packetBufferLength != 0);
}

void LinUdpGateway::run()
{
    switch (_config->nodeMode())
    {
    case Config::NodeModes::MASTER:
    {
        runMaster();
    }
    break;
    case Config::NodeModes::SLAVE:
    {
        runSlave();
    }
    break;
    default:
        break;
    }
}

void LinUdpGateway::runMaster()
{
    // wait for server/automatic or manually to write arbitration frame
    if (packetBufferLength >= 5)
    {
        uint8_t id = _packetBuffer[3];
        Record *record = _records->getRecordById(id);
        boolean validpayload = ((packetBufferLength == 5) || (packetBufferLength == (5 + record->size())));
        if (validpayload)
        {
            if (packetBufferLength == 5)
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
                memcpy(record->writeCache(), &_packetBuffer[5], record->size());
                sendOverSerial(record);
            }
        }
        else
        {
            _config->log("Incorrect data recieved over udp id");
        }
    }
}

void LinUdpGateway::runSlave()
{
    if (_lin.serial.available())
    {
        uint8_t idbyte = synchHeader();

        //Now check parity
        //remove parity bits and then add them again. Check if it mathes whats expected
        uint8_t id = idbyte & 0x3f;
        if ((_lin.addrParity(id) | id) == idbyte)
        {
            // send this arbitration frame to the server.
            Record *record = _records->getRecordById(id);
            // this works!!
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
            _config->log("parity failure - do nothing ");
        }
    }
}