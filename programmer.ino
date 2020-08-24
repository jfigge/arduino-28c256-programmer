/*
 * Arduino Nano based programmer for 28C256 EEPROM.
 * Writen by Jason Figge, 2020
 * 
 *       /=== Data>   /============= Address ===========\
 *      //////       G////A      H////A      A0\\\\\\\\A14
 *  +----------+   +-------+   +-------+   +---------------+
 *  |  Nano    |   | sr-hi |   | sr-lo |   | 28C256 EEPROM |
 *  +----------+   +-------+   +-------+   +---------------+
 *    \   \   \\     //   \---<---/ //        /////   /   /
 *     WE> OE> \\---//--------->---//   <Data====/ <OE <WE
 * 
 * 
 * Wiring:  
 *  low word shift-register (sr-lo):
 *    sr-lo.SER => Nano.A3
 *    sr-lo.clockPin => Nano.A4
 *    sr-lo.latchPin =>Nano. A5
 *    sr-lo.Qa-Qh => EEPROM.A0-A7
 *  
 *  high word shift-register (sr-hi):
 *    sr-hi.SER => lo.QH' (overflow)
 *    sr-hi.clockPin => Nano.A4
 *    sr-hi.latchPin => Nano.A2
 *    sr-hi.Qa-Qg => EEPROM.A8-A14
 *    sr-hi.Qh => NC
 *   
 *  Nano.11 => EEPROM.OE
 *  Nano.12 => EEPROM.WE
 *  Nano.2-9  => EEPROM.I/O0-7
 *  
 * Warning: Changing pin assignments with require significant
 * reworking of all port logic.
 * 
 * This code was constructed after watching Ben Eater's videos
 * on how to build a 6502 computer, not wanting to pay $57 for 
 * a programmer, and because I received an SDP enabled chip 
 * from JameCo.com. Credit to Bread80 for the inspiration to 
 * switch to port manipulation.
 * 
 * After writing a program using standard arduino high level 
 * functions I was able to read from the EEPROM, but due to 
 * the SDP nothing I did would allow me to write any data.  
 * Even after trying the unlock sequence. After connecting 
 * my o-scope I found the time to write a single byte far 
 * exceeded the max time allowed to perform a page write.  
 * 
 * Summizing the lock sequence had to be completed within a 
 * page, I set about using registry manipulation to speed up
 * the process.  Now byte writes are in the micro-second 
 * range and I can unlock the SDP 
 */
#include <Arduino.h>

// This method pushes the address bits into two shift registers.
// More often than not we can skip pushing the high order bits 
// and save some time.  So long as we don't latch sr-hi it does
// not matter that the unlatched bits are from the previous low
// word.  This saves time and allows us to meet page load times.
void setAddress(const uint16_t address) {
  static byte lastHigh = 0xFF;

  // shift low, and optionally high, word(s) into registers
  byte high = address >> 8;
  uint16_t bit = (lastHigh - high) ? 0x8000 : 0x80;  // Select all 16 bits, or just lower 8
  while (bit) {
    PORTC = (PORTC & B11110111) | (((address & bit) == bit) ? B00011000 : B00010000); // set dataPin and toggle clockPin high
    PORTC &= B11101111; // toggle clockPin low
    bit = bit >> 1;
  }

  // Latch the new values
  if (high != lastHigh) {
    PORTC |= B00100100; // Set latchPinHi & latchPinLo to high
    PORTC &= B11011011; // Set latchPinHi & latchPinLo to low
    lastHigh = high;
  } else {
    PORTC |= B00100000; // Set latchPinLo to high
    PORTC &= B11011111; // Set latchPinLo to low
  }
}

// Set the direction of data flow by adjusting both
// the EEPROM and Nano I/O pins.  Always set the prior output 
// pins to input before toggle the existing input to output
// This avoid head to head outputs and the chance of shorting
void setDataDirection(uint8_t direction) {
  static uint8_t lastDirection = 2;

  if (lastDirection != direction) {
    if (direction == INPUT) {
      DDRD  &= B00000011; // 2-7 as input
      DDRB  &= B11111100; // 8-9 as input
      PORTB &= B11110111; // Pin 11 low (enable EEPROM output)
    } else {
      PORTB |= B00001000; // Pin 11 high (disable EEPROM output)
      DDRD  |= B11111100; // 2-7 as output
      DDRB  |= B00000011; // 8-9 as output
    }
    lastDirection = direction;
  }
}

// Copy the bits of the data word across registers D & B
// Remember the last I/O7 bit for data polling - waitOnWrite()
uint8_t waitBit = 0;
void setData(byte data) {
  setDataDirection(OUTPUT);
  PORTD = (PORTD & 0x03) | (data << 2); // Port D pins 2-7 receive lower 6 bits
  PORTB = (PORTB & 0xFC) | (data >> 6); // Port B pins 8-9 receive upper 2 bits
  waitBit = PORTB;
}

// Extract the input pins.  Note these come from PINx, not 
// PORTx registry as PINx reflects the current values, while 
// PORTx can include updated values we've set.
byte getData() {
  setDataDirection(INPUT);
  return (PIND >> 2) | (PINB << 6);
}

// This method sets the registry and data pins, and then
// briefly pulses the WE pin high to trigger the update
void writeEEPROM(uint16_t address, byte data) {
  // Set the data pins
  setAddress(address);
  setData(data);
  
  // Briefly pulse the EEPROM write enable pin high
  PORTB &= B11101111;
  PORTB |= B00010000;
}

// Set the address lines and fetch the data byte from the EEPROM
byte readEEPROM(uint16_t address) {
  // Set the data pins
  setAddress(address);
  // Return the pin data
  return getData();
}

void waitOnWrite() {
  setDataDirection(INPUT);
  while ((PINB & 0x02) != (waitBit & 0x02)) {
    delayMicroseconds(1);
  }
}

// Extract 256 bytes, starting at the supplied address, and display
// in a formatted hex output.
void dump(const uint16_t address) {
  char text[80];
  Serial.println("       ===================== Read =====================");
  for (int i = 0; i < 256; i += 16) {
    byte data[16];
    for (int offset = 0; offset < 16; offset++) {
      data[offset] = readEEPROM(address + i + offset);
    }
    sprintf(text, "%04x   %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x", address + i, 
                  data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], 
                  data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.println(text);
  }
  Serial.println("       ===================== Done =====================");
}

// Send the codes necessary to disable Software Data Projection
void disableSDP() {
  // three-write command prefix
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55); 
  writeEEPROM(0x5555, 0x80);

  // three-write command to disable protection
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x20);
}

// Send the codes necessary to enable Software Data Projection
void enableSDP() {
  // three-write command prefix
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0xA0);
}

void setup() {

  // Set shift register pins
  PORTC &= B11000011; // Set latchPinHi, latchPinLo, clockPin, and dataPin all to low
  DDRC  |= B00111100; // Set latchPinHi, latchPinLo, clockPin, and dataPin all to output

  // set EEPROM control pins
  PORTB |= B00010000; // Set writePin to high
  DDRB  |= B00011000; // Set writePin and outputPin to output
  
  // set default data direction pins
  setDataDirection(INPUT);

  // Seed the high order word so we don't inadvertently skip
  // setting the initial values
  setAddress(0x0000);

  // Start the serial port
  Serial.begin(57600);
  while (!Serial); 
}

void loop() {
}
