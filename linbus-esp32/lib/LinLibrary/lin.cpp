// #include "Arduino.h"
#include "lin.h"

Lin::Lin(LIN_SERIAL& ser,uint8_t TxPin)
  : serial(ser)
  , txPin(TxPin)
{
}

void Lin::begin(int speed)
{
  serialSpd = speed;
  serial.begin(serialSpd);
  serialOn  = 1;

  unsigned long int Tbit = 100000/serialSpd;  // Not quite in uSec, I'm saving an extra 10 to change a 1.4 (40%) to 14 below...
  unsigned long int nominalFrameTime = ((34*Tbit)+90*Tbit);  // 90 = 10*max # payload bytes + checksum (9).
  timeout = LIN_TIMEOUT_IN_FRAMES * 14 * nominalFrameTime;  // 14 is the specced addtl 40% space above normal*10 -- the extra 10 is just pulled out of the 1000000 needed to convert to uSec (so that there are no decimal #s).
}

// Generate a BREAK signal (a low signal for longer than a byte) across the serial line
void Lin::serialBreak(void)
{
  if (serialOn) {
    serial.flush();
    serial.end();
  }

  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);  // Send BREAK
  const unsigned long int brkend = (1000000UL/((unsigned long int)serialSpd));
  const unsigned long int brkbegin = brkend*LIN_BREAK_DURATION;
  if (brkbegin > 16383) delay(brkbegin/1000);  // delayMicroseconds unreliable above 16383 see arduino man pages
  else delayMicroseconds(brkbegin);

  digitalWrite(txPin, HIGH);  // BREAK delimiter

  if (brkend > 16383) delay(brkend/1000);  // delayMicroseconds unreliable above 16383 see arduino man pages
  else delayMicroseconds(brkend);

  serial.begin(serialSpd);
  serialOn = 1;
}

/* Lin defines its checksum as an inverted 8 bit sum with carry */
uint8_t Lin::dataChecksum(const uint8_t* message, char nBytes,uint16_t sum)
{
    while (nBytes-- > 0) sum += *(message++);
    // Add the carry
    while(sum>>8)  // In case adding the carry causes another carry
      sum = (sum&255)+(sum>>8);
    return (~sum);
}

/* Create the Lin ID parity */
#define BIT(data,shift) ((addr&(1<<shift))>>shift)
uint8_t Lin::addrParity(uint8_t addr)
{
  uint8_t p0 = BIT(addr,0) ^ BIT(addr,1) ^ BIT(addr,2) ^ BIT(addr,4);
  uint8_t p1 = ~(BIT(addr,1) ^ BIT(addr,3) ^ BIT(addr,4) ^ BIT(addr,5));
  return (p0 | (p1<<1))<<6;
}

int Lin::calulateChecksum(unsigned char data[], byte data_size){
  int suma = 0;
	for(int i=0;i<data_size;i++) {
		suma = suma + data[i];
	}
  byte v_checksum = (byte)~(suma % 0xff);
  return v_checksum;
}

// according to enhanced crc
boolean Lin::validateChecksum(unsigned char data[], byte data_size){
	byte checksum = data[data_size-1];
  byte v_checksum = calulateChecksum(data, data_size-1);

	if(checksum==v_checksum) {
		return true;
	}
	else {
		return false;
	}

}


/* Send a message across the Lin bus */
/*void Lin::send(uint8_t addr, const uint8_t* message, uint8_t nBytes,uint8_t proto)
{
  uint8_t addrbyte = (addr&0x3f) | addrParity(addr);
  uint8_t cksum = dataChecksum(message,nBytes,(proto==1) ? 0:addrbyte);
  serialBreak();       // Generate the low signal that exceeds 1 char.
  serial.write(0x55);  // Sync byte
  serial.write(addrbyte);  // ID byte
  serial.write(message,nBytes);  // data bytes
  serial.write(cksum);  // checksum
  serial.flush();
}*/


// uint8_t addr, const uint8_t* message,uint8_t nBytes
//void Lin::send(uint8_t nBytes)//(uint8_t addr, const uint8_t* message, uint8_t nBytes,uint8_t proto)
void Lin::send(uint8_t addr, const uint8_t* message,uint8_t nBytes)
{
  //byte message[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}; // LEDs ON
  //uint8_t addrbyte = (addr&0x3f) | addrParity(addr);
  //if (serialOn) serial.end();
//  serial.begin(serialSpd);
  //serialOn = 1;
  int i = 0;
  uint8_t cksum = dataChecksum(message,8,addr);
  //serialBreak();       // Generate the low signal that exceeds 1 char.
  //serial.write(0x55);  // Sync byte
  //serial.write(0x32);  // ID byte
  //for (i = 0; i < 8; i++)
  //i = Serial.availableForWrite();
  //if (i > 0) {
  serial.write(message, 8);  // data bytes
  //serial.flush();
  serial.write(cksum);  // checksum
  serial.flush();
  }


  //serial.flush();
  //serial.end();
//


uint8_t Lin::recv(uint8_t addr, uint8_t* message, uint8_t nBytes,uint8_t proto)
{
  uint8_t bytesRcvd=0;
  unsigned int timeoutCount=0;
  serialBreak();       // Generate the low signal that exceeds 1 char.
  serial.flush();
  serial.write(0x55);  // Sync byte
  uint8_t idByte = (addr&0x3f) | addrParity(addr);
  serial.write(idByte);  // ID byte
  bytesRcvd = 0xfd;
  do { // I hear myself
    while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
  } while(serial.read() != 0x55);
  bytesRcvd = 0xfe;
  do {
    while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
  } while(serial.read() != idByte);


  bytesRcvd = 0;
  for (uint8_t i=0;i<nBytes;i++)
  {
    // This while loop strategy does not take into account the added time for the logic.  So the actual timeout will be slightly longer then written here.
    while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
    message[i] = serial.read();
    bytesRcvd++;
  }
  while(!serial.available()) { delayMicroseconds(100); timeoutCount+= 100; if (timeoutCount>=timeout) goto done; }
  if (serial.available())
  {
    uint8_t cksum = serial.read();
    bytesRcvd++;
    if (proto==1) idByte = 0;  // Don't cksum the ID byte in LIN 1.x
    if (dataChecksum(message,nBytes,idByte) == cksum) bytesRcvd = 0xff;
  }

done:
  serial.flush();

  return bytesRcvd;
}



int Lin::readStream(byte data[],byte data_size){
  byte rec[data_size];
  //Serial.begin(19200);
  //if(ch==1){ // For LIN1 or Serial
    if(serial.read() != -1){ // Check if there is an event on LIN bus
      serial.readBytes(rec,data_size);
      for(int j=0;j<data_size;j++){
        data[j] = rec[j];
      }
      return 1;
    }/*
  }else if(ch==2){ // For LIN2 or Serial
    if(Serial.read() != -1){ // Check if there is an event on LIN bus
      Serial.readBytes(data,data_size);
      return 1;
    }
  }*/
  return 0;
}
