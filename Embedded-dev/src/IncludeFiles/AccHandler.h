// Basic demo for tap/doubletap readings from Adafruit LIS3DH
#ifndef ACCHANDLER_H
  #define ACCHANDLER_H
#endif 

#ifndef ARDUINO_H
  #include <Arduino.h>
  #define ARDUINO_H
#endif 

#ifndef WIRE_H
  #include <Wire.h>
  #define WIRE_H
#endif

#ifndef SPI_H
  #include <SPI.h>
  #define SPI_H
#endif

#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

extern Adafruit_LIS3DH lis; 
extern volatile uint8_t interrupt;

enum AccMode {doubleTap, singleTap, measure};

// Used for software SPI
#define LIS3DH_CLK 13
#define LIS3DH_MISO 12
#define LIS3DH_MOSI 11
// Used for hardware & software SPI
#define LIS3DH_CS 10


#define MODE 1                  // 0 = turn off click detection & interrupt, 1 = single click only interrupt output, 2 = double click only interrupt output, detect single click
#define CLICKTHRESHOLD 80      // Adjust threshhold, higher numbers are less sensitive - highly dependent on range 0-127
#define TIMELIMIT 10            // Maximum time a pulse can last before it must return below the threshold to be interpreted as a tap
#define LATENCY 20              // minimum time between the end of the first pulse and the begining of the second to be counted as a double tap event
#define WINDOW 255             // maximum time between the end of the first pulse and the begining of the second to be counted as a double tap event
#define INTERRUPT_PIN D2
#define LIS3DH_ADDRESS 0x18      // I2C address of your device
#define INT1_CFG 0x30

#define MEASURETHRESHOLD 0x04



void accelerometer_setup();
void setAccMode(int mode);

bool writeToReg(uint8_t chipAdd, uint8_t regAdd, uint8_t data);

bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t &val);

void isr();

template <typename T>
inline void printVarPer(T var, unsigned int period, char str[]= ""){ // prints a variable at a given period with the option to include a string
  static int x = 0;
  if (((signed long)millis() - x*1000 ) > 0){
    Serial.print(str);
    Serial.println(var);
    x=x+period;
    }
}
