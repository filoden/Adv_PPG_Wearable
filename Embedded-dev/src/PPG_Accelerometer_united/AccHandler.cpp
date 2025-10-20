 #include "AccHandler.h"


Adafruit_LIS3DH lis = Adafruit_LIS3DH();
volatile uint8_t interrupt = 0;

// Read 1 byte from an I2C register
bool i2cRead(uint8_t dev, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start, no STOP
  if (Wire.requestFrom((int)dev, 1) != 1) return false;
  val = Wire.read();
  return true;
}

bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t s = Wire.endTransmission(false);   // repeated START, no STOP
  if (s != 0) return false;
  if (Wire.requestFrom((int)addr, 1) != 1) return false;
  val = Wire.read();
  return true;
}

// Write 1 byte to an I2C register
bool i2cWrite(uint8_t dev, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// Update specific bits in a register (Linux "update_bits" style)
bool i2cUpdateBits(uint8_t dev, uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t v;
  if (!i2cRead(dev, reg, v)) return false;
  v = (v & ~mask) | (value & mask);
  return i2cWrite(dev, reg, v);
}

// Convenience for a single bit
bool i2cSetBit(uint8_t dev, uint8_t reg, uint8_t bitIndex, bool on) {
  uint8_t mask = (uint8_t)(1u << bitIndex);
  return i2cUpdateBits(dev, reg, mask, on ? mask : 0);
}

void setAccMode(int mode){
  switch (mode) {
    case doubleTap:
      writeToReg(LIS3DH_ADDRESS, 0x20/*CTRL1*/,     0x5F); 
      i2cSetBit(LIS3DH_ADDRESS, 0x24, 3, true);
      writeToReg(LIS3DH_ADDRESS, 0x22 /*CTRL3*/, 0b10000000);
      writeToReg(LIS3DH_ADDRESS, 0x21/*CTRL2*/,     0x08); 
      writeToReg(LIS3DH_ADDRESS, 0x30 /* INT1_CFG  */, 0x0);
      writeToReg(LIS3DH_ADDRESS, 0x38 /* CLICK_CFG  */, 0b00101010); 
      break;
    case singleTap:
      writeToReg(LIS3DH_ADDRESS, 0x20/*CTRL1*/,     0x5F); 
      i2cSetBit(LIS3DH_ADDRESS, 0x24, 3, true);
      writeToReg(LIS3DH_ADDRESS, 0x22 /*CTRL3*/, 0b10000000);
      writeToReg(LIS3DH_ADDRESS, 0x21/*CTRL2*/,     0x08); 
      writeToReg(LIS3DH_ADDRESS, 0x30 /* INT1_CFG  */, 0x0); 
      writeToReg(LIS3DH_ADDRESS, 0x38 /* CLICK_CFG  */, 0b00010101);
      break;
    case measure:
      writeToReg(LIS3DH_ADDRESS, 0x20/*CTRL1*/,     0x57); 
      writeToReg(LIS3DH_ADDRESS, 0x21/*CTRL2*/,     0x01);      //
      writeToReg(LIS3DH_ADDRESS, 0x22 /* CTRL_REG3  */, 0x40);  //
      i2cSetBit(LIS3DH_ADDRESS, 0x24, 3, false);
      writeToReg(LIS3DH_ADDRESS, 0x30 /* INT1_CFG  */, 0b01111111);   // movement recognition (6D disabled)
      writeToReg(LIS3DH_ADDRESS, 0x32 /* INT1_THS  */, MEASURETHRESHOLD);   // threshold 
      writeToReg(LIS3DH_ADDRESS, 0x33 /* INT1_CFG  */, 0x01);   // duration LSB = 1/ODR seconds
      writeToReg(LIS3DH_ADDRESS, 0x38 /* CLICK_CFG  */, 0x00);
      uint8_t dummy; i2cReadReg(LIS3DH_ADDRESS, 0x26 /*REFERENCE*/, dummy);
      break;
  }

}

bool writeToReg(uint8_t chipAdd, uint8_t regAdd, uint8_t data){ // Write to a given register over I2C
  bool success = false;
  bool limit = false;
  uint8_t x = 0;
  while ( !success && !(x > 5) ) {
    Wire.beginTransmission(chipAdd);
    Wire.write(regAdd);
    Wire.write(data);
    byte status = Wire.endTransmission(); // 0 indicates success
    if (status == 0) {
      success = true;
      return true;
    } 
    else {
      Serial.print("Write failed with status: ");
      Serial.println(status);
      x++;
      success == false;
    }
  }
  return false;
}

void accelerometer_setup(){
  #ifndef ESP8266
    while (!Serial) yield();     // will pause Zero, Leonardo, etc until serial console opens
  #endif

  Wire.begin();

  // set range 0=2g, 1 = 4g, 2 = 8g, 3 = 16g
  i2cSetBit(LIS3DH_ADDRESS, 0x23, 5, false); 
  i2cSetBit(LIS3DH_ADDRESS, 0x23, 4, false);

  // click settings when in use: 
  writeToReg(LIS3DH_ADDRESS, 0x3A /* CLICK_THS  */, (CLICKTHRESHOLD & 0x7F)); // set threshold + latch
  writeToReg(LIS3DH_ADDRESS, 0x3B /* TIME_LIMIT */, TIMELIMIT);                               // ~10*1/ODR
  writeToReg(LIS3DH_ADDRESS, 0x3C /* TIME_LAT   */, LATENCY);
  writeToReg(LIS3DH_ADDRESS, 0x3D /* TIME_WIND  */, WINDOW);

  // set interrupt to the interrupt pin to trigger when rising
  pinMode(INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), isr, RISING);

  return;
}


void isr(){ // interrupt service routine
  interrupt = 1;
  return;
}



