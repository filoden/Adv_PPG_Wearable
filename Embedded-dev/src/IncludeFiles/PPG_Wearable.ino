#include "PPGHandler.h"
#include "AccHandler.h"

#define WAIT_INTERVAL 60


enum transition {Go_Sleep, Go_Wait, Go_Measure};

extern volatile uint8_t interrupt;
int nextMode = Go_Sleep;

static inline void led_set(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(LED_RED, 255 - r);
  analogWrite(LED_GREEN, 255 - g);
  analogWrite(LED_BLUE, 255 - b);
}


static inline void loff()     { led_set(0,   0,   0);   }
static inline void lred()     { led_set(255, 0,   0);   }
static inline void lgreen()   { led_set(0,   255, 0);   }
static inline void lblue()    { led_set(0,   0,   255); }
static inline void lyellow()  { led_set(255, 255, 0);   }
static inline void lcyan()    { led_set(0,   255, 255); }
static inline void lmagenta() { led_set(255, 0,   255); }
static inline void lwhite()   { led_set(255, 255, 255); }
static inline void lorange()  { led_set(255, 64,  0);   }
static inline void lpurple()  { led_set(128, 0,   128); }
static inline void lpink()    { led_set(255, 64,  128); }
static inline void lteal()    { led_set(0,   128, 128); }


bool timer(int Time = 1, bool set = false){
  static unsigned long startTime;
  if (set = true){
    startTime = millis();
    return false;
  }
  else {
    return millis() - startTime > 1000*Time;
  }
}

int sleepMode(){ // need accelermoeter double tap mode and PPG sleep mode
  lred();
  setAccMode(doubleTap);
  bool alarm = false;
  timer(10, true);
  while (1){
    alarm = timer(10);
    if (interrupt == 1 || alarm){
      interrupt = 0;
      return Go_Wait;
    }
    Serial.print("...zzz....zz");
    delay(1000);
  }
  
}

int waitMode(){ // need accelerometer single tap mode & sybc pprocess
  //Serial.println("\nWell, I'm waiting..");
  lyellow();
  setAccMode(singleTap);
  unsigned long startTime = millis();
  while (millis() - startTime < WAIT_INTERVAL*1000){ 
    if (interrupt == 1){
      interrupt = 0;
      delay(1000);
      return Go_Measure;
    }
  }
    return Go_Sleep;
}

int measureMode(){ // need accelerometer acceleration interrupt mode and PPG setup
  int success = 0;
  lgreen();
  setAccMode(measure);
  //testRun(BRIGHTNESS, SAMPLEAVE, LEDMODE, SAMPLERATE, PULSEWIDTH, ADCRANGE, TIME);
  //Serial.print("measuring");
  success = standardRun();
  if (success == 1){
    return Go_Sleep;
  }
  else if (success == 0){
    interrupt = 0;
    return Go_Wait;
  }
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  Serial.begin(115200);
  delay(20);
  Serial.println("Initializing...");

  // Initialize PPG sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  particleSensor.setup(BRIGHTNESS, SAMPLEAVE, LEDMODE, 50, PULSEWIDTH, ADCRANGE); //Configure sensor with these settings
  // initialize accelerometer:

  accelerometer_setup();
}

void loop(){
  if (nextMode == Go_Sleep){
    nextMode = sleepMode();
  }
  else if (nextMode == Go_Wait){
    nextMode = waitMode();
  }
  else if (nextMode == Go_Measure){
    nextMode = measureMode();
  }
  else{
    Serial.println("Error: No Mode");
    exit(1);
  }

  }


