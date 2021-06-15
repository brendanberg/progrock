#include <errno.h>
#include <limits.h>
#include <string.h>

#define BUF_LENGTH 1024

#define PIN_DATA 2
#define PIN_CLOCK 3
#define PIN_LATCH 4
#define PIN_WRITE 13
#define PIN_D0 5
#define PIN_D7 12

extern int errno;
int data = 0;

char *ok = "OK";
char *err = "ERROR";

byte ser_data[BUF_LENGTH + 1];
int start, bytelen;
char cmd;
  
/*
 * Shift 16 bits out over a data pin.
 * 
 * Parameters:
 *   - int value
 *     a 16-bit value to shift out
 */
void shiftWrite(int value) {
  digitalWrite(PIN_LATCH, LOW);
  
  shiftOut(PIN_DATA, PIN_CLOCK, LSBFIRST, value);
  shiftOut(PIN_DATA, PIN_CLOCK, LSBFIRST, value >> 8);
  
  digitalWrite(PIN_LATCH, HIGH);
}


/*
 * Shift an address out onto the address bus.
 * 
 * Parameters:
 *   - int address
 *     a 15-bit address (the 28C64 programmer ignores the highest two bits
 *   - bool outputEnable
 *     true when asserting an address in a read cycle, false otherwise
 * 
 * Note:
 *   The 'OE pin on the 28C64 is active low, so the bit associated with
 *   'OE in the shift register should be the inverse of the value passed
 *   to this function.
 */
void busAssertAddress(int address, bool outputEnable) {
  shiftWrite(outputEnable ? address & 0x7FFF : address | 0x8000);
}


/*
 * Returns a byte read from digital pins D0 through D7.
 */
byte busReadData() {
  byte data = 0;
  
  for (int pin = PIN_D7; pin >= PIN_D0; pin--) {
    data = (data << 1) | digitalRead(pin);
  }

  return data;
}


/*
 * Set pins to output and assert a value on digital pins D0 through D7.
 * 
 * Parameters:
 *   - byte data
 *     the value to be asserted on the digital pins
 */
void busAssertData(byte data) {
  unsigned long time = micros();
  for (int pin = PIN_D0; pin <= PIN_D7; pin++) {
    pinMode(pin, OUTPUT);
  }
  
  for (int pin = PIN_D0; pin <= PIN_D7; pin++) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
}


/*
 * Set digital pins D0 through D7 to input mode without reading a value
 */
void busReleaseData() {
  for (int pin = PIN_D0; pin <= PIN_D7; pin++) {
    pinMode(pin, INPUT);
  }
}


/*
 * Read the byte at the specified address of a 28C64 EEPROM.
 */
byte eepromReadByte(int address) {
  busReleaseData();
  busAssertAddress(address, true);
  return busReadData();
}


/*
 * Write a given byte to the specified address of a 28C64 EEPROM.
 */
void eepromWriteByte(int address, byte data) {
  busAssertAddress(address, false);
  digitalWrite(PIN_WRITE, LOW);
  busAssertData(data);
  digitalWrite(PIN_WRITE, HIGH);
  delay(10);
  busReleaseData();
}


/*
 * Read a sequence of bytes from the EEPROM starting at startAddress
 * and continuing for len bytes into the given buffer.
 * 
 * Parameters:
 *   - int startAddress
 *   - int len
 *   - byte *data
 */
void eepromReadRange(int startAddress, int len, byte *data) {
  for (int i = 0; i < len; i++) {
    data[i] = eepromReadByte(startAddress + i);
  }
}


/*
 * Write a sequence of bytes from the given buffer to the sequence
 * of EEPROM locations starting at startAddress and continuing for
 * len bytes.
 * 
 * Parameters:
 *   - int startAddress
 *   - int len
 *   - byte *data
 */
void eepromWriteRange(int startAddress, int len, byte *data) {
  for (int i = 0; i < len; i++) {
    eepromWriteByte(startAddress + i, data[i]);
  }
  //delay(500);
}


void serialParseCmd(byte *ser_data, byte *cmd, int *start, int *len) {
  // C string manipulation functions are fetid piles of decomposing
  // garbage, rife with all the associated microorganisms that thrive
  // in such filth. I hate this code with the entirity of my being.

  *cmd = ser_data[0];

  char *command = ser_data;
  char *token = strsep(&command, " ");

  if (token && strlen(token) == 1) {
    *cmd = token[0];
  } else {
    *cmd = '0';
    return;
  }

  char *endchr;
  unsigned long s;

  token = strsep(&command, " ");

  if (token) {
    s = strtoul(token, &endchr, 10);

    if (endchr == token) {
      // Couldn't parse a decimal number
      *cmd = '1';
      return;
    } else if ('\0' != *endchr) {
      // Extra characters at end of input
      *cmd = '2';
      return;
    } else if (errno == ERANGE || s > INT_MAX) {
      // Number out of range
      *cmd = '3';
      return;
    } else {
      *start = s;
    }
  }

  token = strsep(&command, " ");

  if (token) {
    s = strtoul(token, &endchr, 10);

    if (endchr == token) {
      *cmd = '4';
      return;
    } else if ('\0' != *endchr) {
      *cmd = '5';
      return;
    } else if (errno == ERANGE || s > INT_MAX) {
      *cmd = '6';
      return;
    } else {
      *len = s;
    }
  }
  
  return;
}


/************************************************************
 ************************************************************/


void setup() {
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_WRITE, HIGH);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_WRITE, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);

  // Set pins D0 through D7 to input
  busReleaseData();

  Serial.begin(115200);
}

void loop() {
  size_t len = Serial.readBytesUntil('\n', ser_data, BUF_LENGTH);
  
  if (len) {
    ser_data[len] = '\0';
    serialParseCmd(ser_data, &cmd, &start, &bytelen);

    switch (cmd) {
      case 'r': {
        if (bytelen > BUF_LENGTH) {
          Serial.print(err);
          Serial.println(" requested size exceeds buffer size");
          break;
        } else {
          Serial.println(ok);
        }

        eepromReadRange(start, bytelen, ser_data);
        Serial.write(ser_data, bytelen);
        Serial.println(ok);
        break;
      }

      case 'w': {
        if (bytelen > BUF_LENGTH) {
          Serial.print(err);
          Serial.println(" upload size exceeds buffer size");
          break;
        } else {
          Serial.println(ok);
        }
        
        // Read up to bytelen bytes from buffer
        len = Serial.readBytes(ser_data, bytelen);

        if (bytelen != len) {
          Serial.print(err);
          Serial.println(" received fewer bytes than expected");
          break;
        }

        eepromWriteRange(start, bytelen, ser_data);

        // Since 'CE is tied low, we need to bring 'OE high
        // so that a write won't happen when 'WE goes low
        // during an Arduino reset.
        busAssertAddress(0, true);

        Serial.println(ok);
        break;
      }

      default: {
        Serial.print(err);
        Serial.print(" unrecognized command: \"");
        Serial.write(ser_data, len);
        Serial.println("\"");
        break;
      }
    }
  }
}
