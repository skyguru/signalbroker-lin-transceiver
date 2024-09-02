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

#pragma once

#include <cstdint>
#include <cstdlib>
#include "pins_arduino.h"

class HardwareSerial;

class Lin {
public:
    constexpr explicit Lin(HardwareSerial &ser, uint8_t txPin = TX1)
            : m_Serial{ser},
              m_TxPin{txPin} {};

    void serialBreak();

    void begin();

    static int calculateChecksum(const unsigned char data[], uint8_t data_size);

    static bool validateChecksum(const unsigned char data[], uint8_t data_size);

    HardwareSerial &serial() const { return m_Serial; }

    static uint8_t addrParity(uint8_t addr) {
        uint8_t p0 = BIT_SHIFT(addr, 0u) ^BIT_SHIFT(addr, 1u) ^BIT_SHIFT(addr, 2u) ^BIT_SHIFT(addr, 4u);
        uint8_t p1 = ~(BIT_SHIFT(addr, 1u) ^ BIT_SHIFT(addr, 3u) ^ BIT_SHIFT(addr, 4u) ^ BIT_SHIFT(addr, 5u));

        return (p0 | (p1 << 1u)) << 6u;
    };

private:
    static constexpr uint8_t BIT_SHIFT(uint8_t data, uint8_t shift) {
        return ((data & (1u << shift)) >> shift);
    }

private:
    HardwareSerial &m_Serial;
    static constexpr auto serialSpd = 19200;  // in bits/sec. Also called baud rate
    uint8_t m_TxPin;  // what pin # is used to transmit (needed to generate the BREAK signal)
};
