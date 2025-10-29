#include "PPGHandler.h"

int x = 0;
int secondsperphase = 5;
MAX30105 particleSensor;
extern volatile uint8_t interrupt;

// Constructor
waveformPkg::waveformPkg(){
    startNode = new waveNode;
    realTime = currentTime();
    if (startNode == NULL){
        LOGERR("Fatal: unable to allocate start node\n");
        exit(-1);
    }
    currentNode = startNode;
    currentNode->length = 0;
    wavePtr = currentNode->wave;
    timePtr = currentNode->offset;
    currentNode->baseTime = static_cast<u32>(currentTime()-realTime);
}

void waveformPkg::enqueue(u16 waveIn){
    u32 timeIn = currentTime() - realTime;
    if (noEntryCnt<10){ // prevents the first 10 entries from being logged
        noEntryCnt++;
    }
    else if (isFull()){
        currentNode->next = new waveNode;
        if (currentNode->next == NULL){
            LOGERR("Fatal: unable to allocate start node\n");
            exit(-1);
        }
        currentNode = currentNode->next;
        currentNode->length = 1;
        wavePtr = currentNode->wave;
        timePtr = currentNode->offset;
        *(wavePtr) = waveIn;
        currentNode->baseTime = timeIn;
        *(timePtr) = static_cast<u32>(timeIn - static_cast<u32>(currentNode->baseTime));
        wavePtr++;
        timePtr++;
    }
    else{
        *(wavePtr) = waveIn;
        *(timePtr) = static_cast<u32>(timeIn - static_cast<u32>(currentNode->baseTime));
        currentNode->length++;
        wavePtr++;
        timePtr++;
        
    }
}

// Destructor
waveformPkg::~waveformPkg(){
    int x = 1;
    while (startNode != NULL){
        waveNode* tempNode = startNode->next;
        delete startNode;
        startNode = tempNode;
        delay(100);
    }
    delay(100);currentNode = NULL; // this pointer is set to the current node 
    wavePtr = NULL;
    timePtr = NULL;;
    cursor = 0;
    noEntryCnt = 0;
}

wavelet waveformPkg::peek(){
    if (isArrEmpty()){
        LOGERR("Error: peeked an empty structure");
        exit(-1);
    }
    else{
        wavelet item;
        item.wave = *(wavePtr-1);
        item.offset = *(timePtr-1);
        return item;
    }
}

wavelet waveformPkg::dequeue(){
    //LOGLN();
    //LOG("Cursor: ");
    //LOG(cursor);
    //LOG(" Dequeue Length: ");
    //LOG(startNode->length);
    
    if (isArrEmpty()){
        LOGERR("Error: Dequeued an empty structure");
        exit(-1);
    }
    
    else if (startNode == NULL){
        LOGERR("Error: Dequeued an empty structure: 3");
        exit(-1);
    }
    else if (startNode != NULL && (cursor == MAXSIZE || cursor == startNode->length)){
        if (startNode->next != NULL){
            waveNode* temp = startNode->next;
            delete startNode;
            startNode = temp;
            cursor = 0; 
        }
        else{
            LOGERR("Error: Dequeued an empty structure: 2");
            exit(-1);
        }
    }
    if (startNode != NULL){
        wavelet item;
        //LOGLN();
        //LOG("  Cursor: ");
        //LOGLN(cursor);
        item.wave = *(startNode->wave+cursor); // for edge cases first and last entry in array, cursor should be 0 and MAXSIZE - 1 respectively
        item.time = static_cast<u32>(*(startNode->offset+cursor)+startNode->baseTime)+realTime;
        cursor++;
        return item;
    }
    else{
        LOGERR("Error: Dequeued an empty structure: 1");
        exit(-1);
    }
    
}

void waveformPkg::dequeueToCSV(){
    wavelet item;
    printInternals();
    LOGLN("Time, Waveform");
    while(startNode->next != NULL || cursor < startNode->length){
        item = dequeue();
        LOG(item.time);
        LOG(",");
        LOGLN(item.wave);
    }
    noEntryCnt = 0;
}

unsigned long int waveformPkg::getBaseTime(){
    return baseTime;
}
bool waveformPkg::isArrEmpty(){
    return (currentNode->length == 0);
}
bool waveformPkg::isListEmpty(){
    return (cursor == MAXSIZE && startNode->next == NULL);
}
bool waveformPkg::isFull(){
    return (currentNode->length == MAXSIZE);
}
void waveformPkg::setInternals(u16 br, u8 sAve, u8 mode, u16 rate, u16 pWidth, u16 range){ // set sample rate in Hz for residual calculation
    //differential = (1/rate)*10000;
    brightness = br; 
    sampleAve = sAve;
    ledMode = mode;
    sampleRate = rate;
    pulseWidth = pWidth;
    adcRange = range;
}
void waveformPkg::setID(u8 identity){
    id = identity;
}
void waveformPkg::printInternals(){
    Serial.println("**Begin Metadata**");
    Serial.println("key, value");
    Serial.print("Run ID,");
    Serial.println(id);
    Serial.print("Brightness,");
    Serial.println(brightness);
    Serial.print("Sample Average,");
    Serial.println(sampleAve);
    Serial.print("Led Mode,");
    Serial.println(ledMode);
    Serial.print("Sample Rate,");
    Serial.println(sampleRate);
    Serial.print("Pulse Width,");
    Serial.println(pulseWidth);
    Serial.print("ADC range,");
    Serial.println(adcRange);
    Serial.println("**Begin Waveform**");
}

void waveformPkg::makeEmpty(){ // essentially a copy of destructor which retains the startNode
    int x = 1;
    delay(100);
    while (startNode->next != NULL){
        Serial.print(x++);
        waveNode* tempNode = startNode->next;
        delete startNode;
        startNode = tempNode;
        delay(100);
    }
    startNode->length = 0;
    delay(100);
    currentNode = NULL; 
    wavePtr = NULL;
    timePtr = NULL;;
    cursor = 0;
    noEntryCnt = 0;
}

// Global instance and testRun

int testRun(u16 br, u8 sAve, u8 mode, u16 rate, u16 pWidth, u16 range, u8 time){
    waveformPkg pk1 = *(new waveformPkg);
    pk1.setInternals(br, sAve, mode, rate, pWidth, range);
    particleSensor.setup(br, sAve, mode, rate, pWidth, range);
    u32 startTime = millis();
    bool failure = false;
    while (millis()-startTime < time*1000){
        u16 in = u16(particleSensor.getIR());
        pk1.enqueue(in);
        if (interrupt == 1){
            interrupt = 0;
            failure = true;
            delay(1000);
            break;
        }
    }
    
    if (!(failure)){
        pk1.dequeueToCSV();
        return 1;
    }
    else {
        return 0;
    }    
}

int standardRun(){
    return testRun(BRIGHTNESS, SAMPLEAVE, LEDMODE, SAMPLERATE, PULSEWIDTH, ADCRANGE, TIME);
}
