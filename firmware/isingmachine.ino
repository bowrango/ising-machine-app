// Ising Machine, ESP32 Feather Firmware
// Date: Oct 08, 2026

// References:
// [1] https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/22242A.pdf

#include <SPI.h>

// Microcontroller (ESP32 Feather V2)
// pin definitions
#define HSPI_MISO     12 // MISO1
#define HSPI_MOSI     13 // MOSI1
#define HSPI_MOSI_BK  32 // MOSI1_BK
#define HSPI_SCLK     14 // SCK
#define HSPI_SS       21 // CS1

// 3-8 Decoder for chip select (SN74HCS138DYYR)
// pin definitions
#define ADDR0   15
#define ADDR1   33
#define ADDR2   27
// Parallel-Serial Shift Register (SN74HCS165DYYR)
// pin definition
#define SH_LD   19

// The following constants are defined with the exact addresses given in the datasheet. The setDigiPot function adds the bitshifts to format the message.

// Digital Potentiometer (MCP4351-103E/ST 10K)
// wiper addresses (Table 4-2 [1])
#define WIPER0  0x00 // 0000
#define WIPER1  0x01 // 0001
#define WIPER2  0x06 // 0110
#define WIPER3  0x07 // 0111
static const uint16_t WIPER_ADDR[4] = {WIPER0, WIPER1, WIPER2, WIPER3};
// terminal control addresses (Table 4-2 [1])
#define TCON0   0x04 // 0100
#define TCON1   0x0A // 1010
// channel connections
// static const uint16_t TCON00 = 0b110001000; // 00 (neither connected)
// static const uint16_t TCON01 = 0b110001011; // 01 (channel 0 connected)
// static const uint16_t TCON10 = 0b110111000; // 10 (channel 1 connected)
// static const uint16_t TCONBW = 0b110111011; // 11 (both connected)
static const uint8_t TCON00 = 0b10001000;   // 00 – none
static const uint8_t TCON01 = 0b10001011;   // 01 – pot 0 only
static const uint8_t TCON10 = 0b10111000;   // 10 – pot 1 only
static const uint8_t TCONBW = 0b10111011;   // 11 – both
// read/write commands
static const uint16_t WRITE = 0b00;
static const uint16_t READ = 0b11;

static const int spiClk = 1000000; // 1MHz

// addr=0..6 maps to decoder Y0..Y6 (DP_CS0..DP_CS6) and digipots U16..U22.
static const char* DIGIPOT_LABEL[7] = {"U16","U17","U18","U19","U20","U21","U22"};

SPIClass *hspi = NULL;

void addrSelect(uint8_t address) {
  // Enable output of the 3-8 decoder for the desired digipot address 
  assert(address >= 0 && address <= 7);
  uint8_t A0 = address & 1;
  uint8_t A1 = (address >> 1) & 1;
  uint8_t A2 = (address >> 2) & 1;
  digitalWrite(ADDR0, A0);
  digitalWrite(ADDR1, A1);
  digitalWrite(ADDR2, A2);
}

void spi16Command(SPIClass *spi, uint8_t address, uint16_t data[], uint8_t size=6) {
  // Select the digipot
  addrSelect(address);
  // Send message
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(spi->pinSS(), LOW);  //pull SS slow to prep other end for transfer
  delayMicroseconds(5);
  for (uint8_t i = 0; i < size; i++) {
    spi->transfer16(data[i]);
  }
  digitalWrite(spi->pinSS(), HIGH);  //pull ss high to signify end of data transfer
  spi->endTransaction();
}

void setDigiPot(uint8_t address, int16_t values[]) {
  // values is a length-4 vector of wiper positions: 
  // -1 := not connected
  // 0 := lowest resistance
  // 256 := highest resistance

  // determine connection masks for TCON0 (channel 0–1) and TCON1 (channel 2–3)
  uint8_t conn01 = (values[0] >= 0) | ((values[1] >= 0) << 1);
  uint8_t conn23 = (values[2] >= 0) | ((values[3] >= 0) << 1);
  uint16_t mask[4] = { TCON00, TCON01, TCON10, TCONBW };
  uint16_t tcon0 = mask[conn01]; // 8-bit data for channel 0–1
  uint16_t tcon1 = mask[conn23]; // 8-bit data for channel 2–3

  // assemble the six 16-bit data packets by shifting the commands
  uint16_t data[6];
  // 2 connection masks
  data[0] = (TCON0 << 12) | (WRITE << 10) | (tcon0 << 2);
  data[1] = (TCON1 << 12) | (WRITE << 10) | (tcon1 << 2);
  // Set the wiper command for each channel
  for (int i = 0; i < 4; i++) {
    uint8_t w = (values[i] >= 0 && values[i] <= 255) ? values[i] : 0xFF;
    // address | command | data
    data[i+2] = (WIPER_ADDR[i] << 12) | (WRITE << 10) | (w << 2);
  }
  spi16Command(hspi, address, data, 6);
}

uint8_t readShiftRegister(SPIClass *spi) {
  // The SN74HCS165 shifts on rising edges; mode 2 samples before each shift.
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE2));
  digitalWrite(spi->pinSS(), HIGH);
  addrSelect(7);
  digitalWrite(SH_LD, LOW);
  digitalWrite(spi->pinSS(), LOW);
  delayMicroseconds(1);  // Allow the parallel inputs to load and QH to settle.
  digitalWrite(SH_LD, HIGH);
  delayMicroseconds(1);  // Meet load-to-clock setup time.
  const uint8_t result = spi->transfer(0);  // H..A map to bits 7..0.
  digitalWrite(spi->pinSS(), HIGH);
  digitalWrite(SH_LD, LOW);
  spi->endTransaction();

  return result;
}

int16_t readDigiPot(SPIClass *spi, uint8_t address, uint8_t channel) {
  // Returns the wiper position (0–255) for the given channel.
  // Uses the same bit encoding as setDigiPot: value occupies bits [9:2].
  uint16_t cmd = (WIPER_ADDR[channel] << 12) | (READ << 10) | 0x1FF;
  addrSelect(address);
  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(spi->pinSS(), LOW);
  delayMicroseconds(5);
  uint16_t response = spi->transfer16(cmd);
  digitalWrite(spi->pinSS(), HIGH);
  spi->endTransaction();
  // If raw == cmd the digipot is not responding (MISO floating or loopback still connected).
  // A real response has upper bits ~0 with wiper value in bits [9:2].
  Serial.printf("  [readDigiPot] %s ch=%d  sent=0x%04X  raw=0x%04X  val=%d%s\n",
                DIGIPOT_LABEL[address], channel, cmd, response, (response >> 2) & 0xFF,
                response == cmd ? "  *** LOOPBACK/FLOATING ***" : "");
  return (response >> 2) & 0xFF;
}

// Write three distinct values to every channel of addr=0 and read back.
// If raw==cmd was seen in readDigiPot prints, MISO is still floating.
// If val != written value, the write path or TCON is the issue.
// **testWriteReadBack partial results:**
// val=128 and val=200 read back correctly (PASS). val=50 reads back as 178 (FAIL). Pattern: 178 = 50 + 128, meaning bit 7 of the returned value is always set. In the 16-bit write packet, wiper byte `w` is placed at bits [9:2] via `w << 2`. For val < 128, bit 7 of w = 0, requiring MOSI to drive bit 9 of the packet LOW.
void testWriteReadBack() {
  Serial.println("=== testWriteReadBack ===");
  const uint8_t testVals[] = {1, 25, 50, 64, 100, 127, 128, 150, 200, 255};
  for (uint8_t v = 0; v < sizeof(testVals); v++) {
    int16_t vals[4] = {(int16_t)testVals[v], (int16_t)testVals[v],
                       (int16_t)testVals[v], (int16_t)testVals[v]};
    setDigiPot(0, vals);
    Serial.printf("--- wrote %d to addr=0 ---\n", testVals[v]);
    bool pass = true;
    for (uint8_t ch = 0; ch < 4; ch++) {
      int16_t got = readDigiPot(hspi, 0, ch);
      if (got != testVals[v]) {
        Serial.printf("  FAIL ch=%d: expected %d got %d\n", ch, testVals[v], got);
        pass = false;
      }
    }
    if (pass) Serial.printf("  PASS: all channels read back %d\n", testVals[v]);
  }
  Serial.println("=== end testWriteReadBack ===");
}

void setup() {

  // Set baud rate
  Serial.begin(115200);
  // Serial.begin(921600);

  delay(500);

  pinMode(ADDR0, OUTPUT);
  digitalWrite(ADDR0, LOW);

  hspi = new SPIClass(HSPI);
  // initialize selected GPIO pins
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI_BK, HSPI_SS);
  // set slave select pin to output as the Arduino API
  // doesn't handle automatically pulling SS low
  pinMode(hspi->pinSS(), OUTPUT);
  // initialize HSPI SS as high
  digitalWrite(hspi->pinSS(), HIGH);

  pinMode(ADDR0, OUTPUT);
  pinMode(ADDR1, OUTPUT);
  pinMode(ADDR2, OUTPUT);

  pinMode(SH_LD, OUTPUT);
  // initialize parallel load shift register into LD mode
  digitalWrite(SH_LD, LOW);
}

// Sweep through all 7 digipot addresses. For each, assert SS for 5s so you can
// probe the decoder outputs with a multimeter.
// U13 (SN74HCS138DYYR) output pins: Y0=15, Y1=14, Y2=13, Y3=12, Y4=11, Y5=10, Y6=9.
// Selected output → 0V; all others → 3.3V.
void testDecoderSweep() {
  static const uint8_t Y_PIN[7] = {15, 14, 13, 12, 11, 10, 9};
  Serial.println("=== testDecoderSweep ===");
  for (uint8_t addr = 0; addr < 7; addr++) {
    addrSelect(addr);
    int a0 = digitalRead(ADDR0), a1 = digitalRead(ADDR1), a2 = digitalRead(ADDR2);
    digitalWrite(hspi->pinSS(), LOW);
    Serial.printf("[addr=%d %s] GPIO%d(A0)=%d GPIO%d(A1)=%d GPIO%d(A2)=%d → U13 pin %d (Y%d) should be 0V (5s)...\n",
                  addr, DIGIPOT_LABEL[addr],
                  ADDR0, a0, ADDR1, a1, ADDR2, a2,
                  Y_PIN[addr], addr);
    delay(5000);
    digitalWrite(hspi->pinSS(), HIGH);
  }
  Serial.println("=== end testDecoderSweep ===");
}

void Test1() {
  int16_t resistor_values[28] = {
    127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127,
    127, 127, 127, 127,
    127, 127, 127, 
    127, 127,
    127
  };

  // Set each digipot. This should pass a block of 4 values at each iteration
  for (uint8_t addr=0; addr < 7; addr++) {
    setDigiPot(addr, &(resistor_values[addr*4]));
  }

  // Verify digipot settings
  bool pass = true;
  for (uint8_t addr = 0; addr < 7; addr++) {
    for (uint8_t ch = 0; ch < 4; ch++) {
      int16_t expected = resistor_values[addr*4 + ch];
      if (expected < 0) continue;
      int16_t actual = readDigiPot(hspi, addr, ch);
      if (actual != expected) {
        Serial.printf("FAIL digipot %d ch %d: expected %d got %d\n", addr, ch, expected, actual);
        pass = false;
      }
    }
  }
  if (pass) Serial.println("All digipots OK");

  // Wait for synchronization
  delay(500);

  // Read phase results
  const int numSamples = 10;
  byte results[numSamples];
  for (int i=0; i<numSamples; i++) {
    results[i] = readShiftRegister(hspi);
    delayMicroseconds(10);
  }

  // Print completion
  Serial.print("Results: ");
  for (int i=0; i<numSamples; i++) {
    Serial.println(results[i], BIN);
  }
  Serial.print("\r\n");
}

void loop() {
  delay(1000);
    testDecoderSweep(); // input and output voltages looks good
  //testWriteReadBack();
}
