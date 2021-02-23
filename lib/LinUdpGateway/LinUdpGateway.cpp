#include "LinUdpGateway.hpp"

#include "Config.hpp"

LinUdpGateway::LinUdpGateway(HardwareSerial &serial, Config &config, Records &records)
        : m_lin(serial, TX1),
          m_config{&config},
          m_records{&records},
          m_packetBufferLength{} {}

/**
 * @brief Init the gateway, with lin-serialport and the udp-client config
 * */
void LinUdpGateway::init() {
    m_lin.begin();
//    m_lin.serial.setTimeout(DEFAULT_LIN_TIMEOUT);

    if (m_udpClient.connect(m_config->serverIp(), m_config->hostPort())) {
        Serial.printf("Connected to host port: %d\n", m_config->hostPort());
        // Callback method when package arrives on udp
        m_udpClient.onPacket([&](const AsyncUDPPacket &packet) {
        });
    }

    if (m_udpListen.listen(m_config->clientPort())) {
        Serial.printf("Connected to client port: %d\n", m_config->clientPort());
        // Callback method when package arrives on udp
        m_udpListen.onPacket([&](AsyncUDPPacket packet) {
            m_packetBufferLength = packet.length() > _packetBuffer.size() ? _packetBuffer.size() : packet.length();
            memcpy(&_packetBuffer, packet.data(), m_packetBufferLength);

            m_config->incrementRxOverUdp();
            newData = true;
        });
    }
}

/**
 * @brief Write LIN frame-header (includes break, synch & id)
 * @param id - Frame id on LIN-bus
 * */
void LinUdpGateway::writeHeader(uint8_t id) {
    std::array<uint8_t, 2> echo{};
    m_lin.serialBreak();                          // Generate the low signal that exceeds 1 char.
    m_lin.serial().write(SYN_FIELD);                // Sync byte
    m_lin.serial().write(Lin::addrParity(id) | id); // ID byte

    // lin transceiver will echo back, just consume the data.
    m_lin.serial().readBytes(echo.data(), echo.size());
}

/**
 *  @brief Find start position look for BREAK followed BY SYN_FIELD
 *          we expect this to be the header if it's not read bytes until conditions are met.
 *          we now should have BREAK, SYN_FIELD followed by ID.
 * */
uint8_t LinUdpGateway::synchHeader() {
    std::array<uint8_t, 3> synchBuffer{};

    auto bytesReceived = m_lin.serial().readBytes(synchBuffer.data(), synchBuffer.size());
//    Serial.printf("Received %d bytes\n", bytesReceived);

    int synchTries = 0;

    byte frameBreak = synchBuffer.at(0);
    byte frameSynch = synchBuffer.at(1);
    byte frameId = synchBuffer.at(2);

    boolean match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);

    while (!match) {
        int bytesExpected = 1;

        // align the buffer.
        frameBreak = frameSynch;
        frameSynch = frameId;
        bytesReceived = m_lin.serial().readBytes(&frameId, bytesExpected);

//        Serial.printf("FrameBreak: %d, FrameSynch: %d, FrameId: %d\n", frameBreak, frameSynch, frameId);

        if (bytesReceived != bytesExpected) {
            std::array<char, 100> message{};
            sprintf(message.data(), "Timeout2: Bytes expected: %d, bytes received: %d, synchTries: %d", bytesExpected,
                    bytesReceived, synchTries);
            m_config->log(message.data());
        }

        match = (frameBreak == BREAK) && (frameSynch == SYN_FIELD);
        synchTries++;
    }

    m_config->incrementSynchedPackages();

    if (synchTries > 0) {
        m_config->setSynchedCount(synchTries);
        m_config->incrementUnSynchedPackages();

        std::array<char, 100> message{};
//        sprintf(message.data(), "Synch count is %d frameId = 0x%d", synchTries, (frameId & 0x3fu));
//        m_config->log(message.data());
    }
    return frameId;
}

/**
 * @brief We are reading frame on LIN-bus and send the frame over UDP
 * @param id - Frame id on LIN-bus
 * */
void LinUdpGateway::readLinAndSendOnUdp(uint8_t id) {
    // read buffer  0  1  2        3        4       10
    //              id payload1 payload2...         crc
    // count        0        1        2
    std::array<uint8_t, 12> readBuffer{};

    m_lin.serial().setTimeout(TRAFFIC_TIMEOUT);
    readBuffer.at(0) = static_cast<uint8_t>((Lin::addrParity(id) | id));

    Record *record = m_records->getRecordById(id);

    // If the records couldn't be found, go out from here
    if (record == nullptr) {
        return;
    }

    // Bytes expected is record size + one byte for crc
    const int bytesExpected = record->size() + 1;

    // Reading payload and crc
    int bytesReceived = m_lin.serial().readBytes(&readBuffer.at(1), bytesExpected);

    // If these values doesn't match, return.
    if (bytesReceived != bytesExpected) {
        return;
    }

    // If the bytes received is the same as bytes expected, increment the counter
    m_config->incrementRxOverLin();

    // This value holds the comparison between the received crc on the bus & the calculated crc
    bool crc_valid;

    // In case the the frame is a diagnostic frame, calculate the crc without id (old LIN 1.0 way)
    if ((id == value(DiagnosticFrameId::MasterRequest)) || (id == value(DiagnosticFrameId::SlaveRequest))) {
        // calculate checksum in traditional way for diagnostic frames.
        crc_valid = Lin::validateChecksum(&readBuffer.at(1), bytesExpected);
    } else {
        // Checksum considering payload-size + id + checksum
        crc_valid = Lin::validateChecksum(&readBuffer.at(0), bytesExpected + 1);
    }

    // If the calculated crc is the same as the one received on the lin-bus, then let's send it to the server
    if (crc_valid) {
        sendOverUdp(id, &readBuffer.at(1), record->size());
    }

    // return to default timeout.
    m_lin.serial().setTimeout(DEFAULT_LIN_TIMEOUT);
}

/**
 * @brief Send lin-frame data on UDP
 * @param id - Frame id on lin-bus
 * @note We are sending a lin-id with empty bytes when we want to pull signals from a slave
 * */
void LinUdpGateway::sendOverUdp(uint8_t id) {
    constexpr uint8_t empty = 0;
    sendOverUdp(id, &empty, 0);
}

/**
 * @brief Send lin frame data on UDP
 * @param id - Frame id on LIN-bus
 * @param payload - Pointer to the buffer that's are going to send
 * @param size - Size of payload
 * */
void LinUdpGateway::sendOverUdp(uint8_t id, const uint8_t *payload, uint8_t size) {
    std::array<uint8_t, 13> toServer{};

    // Don't continue if payload-size is greater then total size of server
    if (size > 11) {
        return;
    }

    // The payload data starts at index 5
    uint8_t *payloadStart = &toServer.at(PACKET_BUFFER_PAYLOAD_POS);

    // Index 3 holds the id
    toServer.at(PACKET_BUFFER_ID_POS) = id;

    // Index 4 holds the payload size
    toServer.at(PACKET_BUFFER_SIZE_POS) = size;

    // Index 5 holds the payload
    memcpy(&toServer.at(PACKET_BUFFER_PAYLOAD_POS), payload, size);

    // or rotate the bytes (if needed)
//    for (int i = 0; i < size; i++) {
//        *(payloadStart + i) = payload[size - 1 - i];
//    }

    AsyncUDPMessage message{};
    message.write(toServer.data(), size + 5);
    m_udpClient.send(message);
    m_config->incrementTxOverUdp();
}

/**
 * @brief Send record info over serial (LIN-serialport)
 * @param record: which record you want to send
 * */
void LinUdpGateway::sendOverSerial(Record *record) {
    // The server expect the data is coming in this order
    // first byte is id, following by payload (5-7 bytes) and last the crc

    // With this buffer we arrange the data in the order the server expect it
    std::array<uint8_t, 10> sendBuffer{};

    uint8_t id = record->id();

    sendBuffer.at(0) = Lin::addrParity(id) | id;

    uint8_t *sendBufferPayload = &sendBuffer.at(1);
    memcpy(&sendBuffer.at(1), &record->writeCache(), record->size());

//    for (uint8_t i = 0; i < record->size(); i++) {
//        Serial.printf("is at %d, record_size %d \n", i, record->size());
//        sendBufferPayload[i] = record->writeCache().at(i);
//    }

//    for (uint8_t i = 0; i < record->size(); i++) {
//        sendBufferPayload[i] = record->writeCache().at(record->size() - 1 - i);
//    }

//    if (id == 0x24) {
//        Serial.println(sendBuffer.at(3) & 0x1);
//    }

    uint8_t *crc = &sendBuffer.at(record->size() + 1);

    if ((id == value(DiagnosticFrameId::MasterRequest))
        || (id == value(DiagnosticFrameId::SlaveRequest))) {
        // calculate checksum in traditional way for diagnostic frames.
        crc[0] = (uint8_t) Lin::calculateChecksum(&sendBuffer.at(1), record->size());
    } else {
        crc[0] = (uint8_t) Lin::calculateChecksum(sendBuffer.data(), record->size() + 1);
    }

    //this code fixes consequent synch issues.
    m_lin.serial().write(&sendBuffer.at(1), record->size() + 1);
    m_lin.serial().flush();
    m_config->incrementTxOverLin();
}

/**
 * @brief
 * @param record A record pointer
 * */
void LinUdpGateway::cacheUdpMessage(Record *record) {
    // If we don't have received any new data or the packet buffer length is lesser then 4 bytes
    if (!newData || (m_packetBufferLength < 4 && (m_packetBufferLength != 0))) {
//        m_config->log("Signal server payload too short or no new data has been received");
        return;
    }

    // If we ends up here, we have new valid data and we set newData to false before we consume the data
    newData = false;

    Record *updateRecord = record;
    const uint8_t ID = _packetBuffer.at(PACKET_BUFFER_ID_POS);

    if (ID != updateRecord->id()) {
        updateRecord = m_records->getRecordById(ID);
    }

    if (updateRecord == nullptr) {
        std::array<char, 100> logMessage{};
        sprintf(logMessage.data(), "Miss match id: 0x%d does not exits", ID);
        m_config->log(logMessage.data());
    } else {
        memcpy(&updateRecord->writeCache(), &_packetBuffer.at(PACKET_BUFFER_PAYLOAD_POS), updateRecord->size());
        updateRecord->setCacheValid(true);
    }
}

/**
 * @brief Running the selected node continuously
 * */
void LinUdpGateway::run() {
    switch (m_config->nodeMode()) {
        case Config::NodeModes::MASTER:
            runMaster();
            break;
        case Config::NodeModes::SLAVE:
            runSlave();
            break;
        default:
            m_config->log("You haven't specified node, please select node in the signal broker and restart.");
            delay(500);
            break;
    }
}

/**
 * @brief Running node as a master
 * */
void LinUdpGateway::runMaster() {
    constexpr static int minPacketBufferLength = 5;

    if (!newData) {
        return;
    }

    newData = false;

    // Wait until UDP-message payload is greater then 5
    if (m_packetBufferLength < minPacketBufferLength) {
        return;
    }

    uint8_t id = _packetBuffer.at(PACKET_BUFFER_ID_POS);

    // Find record with the id from packet buffer
    Record *record = m_records->getRecordById(id);

    Serial.printf("Record found with id %d\n", record->id());

    bool validPayload = ((m_packetBufferLength == minPacketBufferLength) ||
                         (m_packetBufferLength == (minPacketBufferLength + record->size())));

    if (!validPayload) {
        m_config->log("Run Master: Incorrect data received over udp id");
        return;
    }

    if (m_packetBufferLength == minPacketBufferLength) {
        // this is an arbitration frame
        if (!record->master()) {
//            Serial.printf("This is not a master frame %d\n", record->id());
            writeHeader(id);
            // now consume the result
            readLinAndSendOnUdp(id);
        }
    } else {
        Serial.printf("This is a master frame %d\n", record->id());
        // this a master frame. Send it all..
        writeHeader(id);
        memcpy(&record->writeCache(), &_packetBuffer.at(PACKET_BUFFER_PAYLOAD_POS), record->size());
        sendOverSerial(record);
    }
}

/**
 * @brief Running node as a slave
 * */
void LinUdpGateway::runSlave() {
    if (m_lin.serial().available()) {
        // Get id including parity bits
        uint8_t idByte = synchHeader();

        // To check the parity we remove the parity bits and then add them again
        uint8_t id = idByte & 0x3fu; // Remove parity bits

        if ((Lin::addrParity(id) | id) == idByte) // Add them again and see if it match
        {
            // send this arbitration frame to the server.
            Record *record = m_records->getRecordById(id);

            if (record->master() || (!record->cacheValid())) {
                sendOverUdp(id);
                readLinAndSendOnUdp(id);
            } else {
                // signal-server should response with the expected payload
                if (record->cacheValid()) {
                    sendOverSerial(record);
                    // this is intentionally after writing to serial which might be counterintuitive,
                    // in order to meet timing req for short messages
                    sendOverUdp(id);
                }
            }
            cacheUdpMessage(record);
        } else {
            m_config->log("Run Slave: Parity failure - do nothing ");
        }
    }
}