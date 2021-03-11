/*
 Copyright 2011 G. Andrew Stone
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "lin.hpp"
#include "HardwareSerial.h"

#include <array>

void Lin::begin() {
    m_Serial.begin(serialSpd);
}

void Lin::serialBreak() {
    constexpr auto LIN_BREAK_DURATION = 15;

    m_Serial.flush();
    delay(1);
    m_Serial.end();
    gpio_reset_pin(GPIO_NUM_4);
    gpio_matrix_out(m_TxPin, SIG_GPIO_OUT_IDX, false, false);

    digitalWrite(m_TxPin, LOW); // Send BREAK

    constexpr auto brakeEnd = (1000000UL / static_cast<unsigned long>(serialSpd));
    constexpr auto brakeBegin = brakeEnd * LIN_BREAK_DURATION;

    // delayMicroseconds unreliable above 16383 see Arduino man pages
    delayMicroseconds(brakeBegin);
    digitalWrite(m_TxPin, HIGH); // BREAK delimiter
    m_Serial.begin(serialSpd);
}

/**
 * @brief Calculate checksum on LIN data
 * @param data
 * @param data_size
 * @return
 */
int Lin::calculateChecksum(const unsigned char data[], uint8_t data_size) {
    int sum = 0;

    for (int i = 0; i < data_size; i++) {
        sum = sum + data[i];
    }

    const auto v_checksum = static_cast<uint8_t>(~(sum % 0xffu));
    return v_checksum;
}

/**
 * Validate checksum according to enhanced crc
 * @param data
 * @param data_size
 * @return
 */
bool Lin::validateChecksum(const unsigned char data[], uint8_t data_size) {
    uint8_t checksum = data[data_size - 1];
    uint8_t v_checksum = calculateChecksum(data, data_size - 1);

//    Serial.printf("Checksum validation %d %d\n", checksum, v_checksum);

    return checksum == v_checksum;
}
