#include "PPGHandler.h"


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  particleSensor.setup(BRIGHTNESS, SAMPLEAVE, LEDMODE, SAMPLERATE, PULSEWIDTH, ADCRANGE); //Configure sensor with these settings

  //Arduino plotter auto-scales annoyingly. To get around this, pre-populate
  //the plotter with 500 of an average reading from the sensor

  //Take an average of IR readings at power up
  const byte avgAmount = 64;
  long baseValue = 0;
  for (byte x = 0 ; x < avgAmount ; x++)
  {
    baseValue += particleSensor.getIR(); //Read the IR value
  }
  baseValue /= avgAmount;

  //Pre-populate the plotter so that the Y scale is close to IR values
  for (int x = 0 ; x < 500 ; x++)
    Serial.println(baseValue);
}

void loop() {
  analogWrite(LED_RED, 0xff);
  analogWrite(LED_GREEN, 0xff);
  analogWrite(LED_BLUE, 0xff);
  while (!Serial.available()){
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  }
  while (Serial.available()){
  Serial.read();
  }
  analogWrite(LED_RED, 0xaf);

  //testRun(BRIGHTNESS, SAMPLEAVE, LEDMODE, SAMPLERATE, PULSEWIDTH, ADCRANGE, TIME);
  // To run a measurement, call the above function as is with the desired parameters.
  // It will repeat, asking for a keypress to start each time.
  // if no parameters are given, the defaults defined at the top of PPGHandler.h file will be used.
  // The last parameter is the time in seconds to measure for, total measurement time over 3 minutes has not been tested.
  // After the last measurement is complete the data will be printed to serial in CSV format.
  // The data can be copied from the serial monitor and pasted into a text file, or logged via a program such as teraterm.


  }
