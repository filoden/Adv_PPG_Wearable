#ifndef PPGHANDLER_H
#define PPGHANDLER_H

#include <Arduino.h>
#include <stdint.h>
#include <Wire.h>
#include "MAX30105.h"

#define MAXSIZE 200
#define LOG(x) (Serial.print(x))
#define LOGLN(x) (Serial.println(x))
#define LOGERR(x) (Serial.println(x))
#define BRIGHTNESS 170  //Options: 0=Off to 255=50mA
#define SAMPLEAVE 1
#define LEDMODE 2
#define SAMPLERATE 200  //Options: 50, 100, 200, 400, 800, 1000, 1600, 320
#define PULSEWIDTH 215  //Options: 69, 118, 215, 411
#define ADCRANGE 16384  //Options: 2048, 4096, 8192, 16384
#define MICROS() (micros()) // Arduino provides micros() which returns unsigned long; expose a no-arg macro for parity with host
#define TIME 5

using u16 = uint16_t;
using u32 = uint32_t;
using u8 = uint8_t;

// return tenths of a millisecond (100 us) rounded to nearest, using integer arithmetic
inline unsigned long int currentTime(){
    return static_cast<u32>((MICROS() + 50u) / 100u); // returns time in tenths of a ms since program launch
}

struct wavelet{
    unsigned long int time;
    u16 wave;
    u32 offset;
};

struct waveNode{
    u32 baseTime;          // offset from which the time of each item will be calculated
    u32 offset[MAXSIZE];
    u16 wave[MAXSIZE];
    u8 length;
    waveNode* next  = NULL;
};

class waveformPkg{

public:
    waveformPkg();
    void enqueue(u16 waveIn);
    ~waveformPkg();
    wavelet peek();
    wavelet dequeue();
    void dequeueToCSV();
    unsigned long int getBaseTime();
    bool isArrEmpty();
    bool isListEmpty();
    bool isFull();
    void setInternals(u16 br, u8 sAve, u8 mode, u16 rate, u16 pWidth, u16 range);
    void setID(u8 identity);
    void printInternals();
    void makeEmpty();

private:
    waveNode* startNode = NULL; // this pointer is set to the start of the linked list
    waveNode* currentNode = NULL; // this pointer is set to the current node 
    u16 * wavePtr = NULL;
    unsigned int baseTime; // This is the base time reference for an individual node 
    u32 * timePtr = NULL;;
    int cursor = 0;
    unsigned long int realTime; // This is the base time reference for all nodes in the array
    u16 brightness; 
    u8 sampleAve;
    u8 ledMode;
    u16 sampleRate;
    u16 pulseWidth;
    u16 adcRange;
    u8 id = 0;
    u8 noEntryCnt = 0;
};

int testRun(u16 br, u8 sAve, u8 mode, u16 rate, u16 pWidth, u16 range, u8 time);
int standardRun();

extern MAX30105 particleSensor;

#endif // PPGHANDLER_H
