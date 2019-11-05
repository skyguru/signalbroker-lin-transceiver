// #define soft

#include <inttypes.h>
#include "Arduino.h"
#ifdef soft
#include <SoftwareSerial.h>
#define LIN_SERIAL            SoftwareSerial
#else
#include <HardwareSerial.h>
#define LIN_SERIAL            HardwareSerial
#endif

#define LIN_BREAK_DURATION    15    // Number of bits in the break.
#define LIN_TIMEOUT_IN_FRAMES 2     // Wait this many max frame times before declaring a read timeout.

enum
  {
    Lin1Frame = 0,
    Lin2Frame = 1,
    LinWriteFrame  = 0,
    LinReadFrame   = 2,
  };

class Lin
{
protected:
  // void serialBreak(void);
  // For Lin 1.X "start" should = 0, for Lin 2.X "start" should be the addr byte.

public:
  static boolean validateChecksum(unsigned char data[], byte data_size);
  static uint8_t dataChecksum(const uint8_t* message, char nBytes,uint16_t start=0);
  static uint8_t addrParity(uint8_t addr);
  static int calulateChecksum(unsigned char data[], byte data_size);
  void serialBreak(void);
  Lin(LIN_SERIAL& ser,uint8_t txPin=1);
  LIN_SERIAL& serial;
  uint8_t txPin;               //  what pin # is used to transmit (needed to generate the BREAK signal)
  int     serialSpd;           //  in bits/sec. Also called baud rate
  uint8_t serialOn;            //  whether the serial port is "begin"ed or "end"ed.  Optimization so we don't begin twice.
  unsigned long int timeout;   //  How long to wait for a slave to fully transmit when doing a "read".  You can modify this after calling "begin"
  void begin(int speed);

  // Send a message right now, ignoring the schedule table.
  //void send(uint8_t addr, const uint8_t* message,uint8_t nBytes,uint8_t proto=2);
  void send(uint8_t addr, const uint8_t* message,uint8_t nBytes);
  //void send(uint8_t nBytes);
  int readStream(byte data[], byte data_size); // read data from LIN bus
  // Receive a message right now, returns 0xff if good checksum, # bytes received (including checksum) if checksum is bad.
  uint8_t recv(uint8_t addr, uint8_t* message, uint8_t nBytes,uint8_t proto=2);

};
