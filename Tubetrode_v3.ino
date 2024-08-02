#include <Wire.h>
#include <SD.h>
#include <SPI.h>

#define OPCODE_SINGLE_REGISTER_READ 0b00010000
#define OPCODE_SINGLE_REGISTER_WRITE 0b00001000
#define OPCODE_SET_BIT 0b00011000
#define OPCODE_CLEAR_BIT 0b00100000
#define OPCODE_READ_CONTINUOUS_BLOCK 0b00110000
#define OPCODE_WRITE_CONTINUOUS_BLOCK 0b00101000

#define SYSTEM_STATUS 0x0
#define GENERAL_CFG 0x1
#define DATA_CFG 0x2
#define OSR_CFG 0x3
#define OPMODE_CFG 0x4
#define PIN_CFG 0x5
#define GPIO_CFG 0x7
#define GPO_DRIVE_CFG 0x9
#define GPO_VALUE 0xB
#define GPI_VALUE 0xD
#define SEQUENCE_CFG 0x10
#define CHANNEL_SEL 0x11
#define AUTO_SEQ_CH_SEL 0x12

#define LOOP_DELAY 1000  // ie, sampling rate

uint8_t ADDRESS_0 = 0x13;
uint8_t ADDRESS_1 = 0x14;
uint16_t buffer0[7];
uint16_t buffer1[7];
int bufferIndex = 0;

const int chipSelect = 4;  // SPI needs a "chip select" pin for each device, in this case the SD card
char filename[16];         // make a "char" type variable called "filename", containing 16 characters
float measuredvbat;

void enableAveragingFilter(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(OPCODE_SINGLE_REGISTER_WRITE);
  Wire.write(OSR_CFG);
  Wire.write(0b00000111);  // OSR=128
  Wire.endTransmission();
}

void setup() {
  Wire.begin();        // Join I2C bus as master
  Serial.begin(9600);  // Start serial communication for debugging
  long startTime = millis();
  while ((millis() - startTime) < 5)  // wait for 5 seconds
  {
    digitalWrite(LED_BUILTIN, ~digitalRead(LED_BUILTIN));  // toggle
    delay(100);
    if (Serial) {
      break;  // Exit the loop if a serial connection is established
    }
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Hello, Tubetrode v3.");

  enableAveragingFilter(ADDRESS_0);
  enableAveragingFilter(ADDRESS_1);

  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    digitalWrite(LED_BUILTIN, HIGH);  // Red LEDs on solid means no logging
  }
  Serial.println("SD card initialized.");
  int n = 0;
  snprintf(filename, sizeof(filename), "tube%03d.csv", n);  // includes a three-digit sequence number in the file name
  while (SD.exists(filename)) {
    n++;
    snprintf(filename, sizeof(filename), "tube%03d.csv", n);
  }
  Serial.print("New file created: ");
  Serial.println(filename);

  File dataFile = SD.open(filename, FILE_WRITE);                                 // open the file with the name "filename"
  if (dataFile) {                                                                // if the file is available...
    Serial.println("Millis,vBAT,Mag1,Mag2,Mag3,Mag4,Mag5,Mag6,Mag7,TrodeId");    // Write dataString to the SD card
    dataFile.println("Millis,vBAT,Mag1,Mag2,Mag3,Mag4,Mag5,Mag6,Mag7,TrodeId");  // Write dataString to the SD card
    dataFile.close();                                                            // Close the file
  }
}

bool readAndLogData(uint8_t address, uint16_t* bufferArray, uint8_t trodeId) {
  for (uint8_t CHANNEL = 0; CHANNEL <= 6; CHANNEL++) {
    // Begin I2C write operation
    Wire.beginTransmission(address);
    Wire.write(OPCODE_SINGLE_REGISTER_WRITE);
    Wire.write(CHANNEL_SEL);
    Wire.write(CHANNEL);
    uint8_t error = Wire.endTransmission();

    // Check for I2C transmission error
    if (error != 0) {
      Serial.print("I2C Transmission Error: ");
      Serial.println(error);
      return false;
    }

    // Request 2 bytes from the I2C device
    Wire.requestFrom(address, 2);
    uint16_t buffer = 0;
    if (Wire.available() == 2) {
      uint8_t msb = Wire.read();  // Read first byte (MSB)
      uint8_t lsb = Wire.read();  // Read second byte (LSB)
      buffer = (msb << 8) | lsb;  // Combine MSB and LSB
    } else {
      // Handle error (e.g., not enough data received)
      Serial.print("Error: Not enough data received from address ");
      Serial.println(address, HEX);
      return false;
    }

    // Store the received data in the global array
    bufferArray[CHANNEL] = buffer;

    // Print received data for debugging
    Serial.print(buffer);
    if (CHANNEL < 6) {
      Serial.print(", ");
    }
  }

  // Log data to SD card
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.print(measuredvbat, 2);
    dataFile.print(",");
    for (uint8_t i = 0; i < 7; i++) {
      dataFile.print(bufferArray[i]);
      if (i < 6) {
        dataFile.print(",");
      }
    }
    dataFile.print(",");
    dataFile.println(trodeId);
    dataFile.close();
  } else {
    Serial.println(", Error opening data file");
  }

  return true;
}

void loop() {
  bool gotData = false;
  if(readAndLogData(ADDRESS_0, buffer0, 0)) {
    Serial.println();
  }
  readAndLogData(ADDRESS_1, buffer1, 1);
  delay(LOOP_DELAY);  // Wait for 100 milliseconds before repeating
}


void ReadBatteryLevel() {
  measuredvbat = analogRead(A7);
  measuredvbat *= 2;     // we divided by 2, so multiply back
  measuredvbat *= 3.3;   // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024;  // convert to voltage
}