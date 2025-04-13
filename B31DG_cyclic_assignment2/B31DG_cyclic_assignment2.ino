#include <B31DGMonitor.h>
#include "Ticker.h"

#define SIG_ONE 16
#define SIG_TWO 4
#define FREQUENCY_ONE 18
#define FREQUENCY_TWO 17
#define LED_SUM 23
#define BUTTON 22
#define LED_TOGGLE 15

B31DGCyclicExecutiveMonitor monitor(1922);
Ticker Cycle;


void frame();
void JobTask1();
void JobTask2();
void JobTask3();
void JobTask4();
void JobTask5();
void GenerateSignal(int signal, int time1, int time2, int time3, int task);
float measureFrequency(int signal, int task, int timeout);
void sumFrequencies();

// Global Variables
int state = 0;
float f1;
float f2;

// Interrupt variables 
volatile unsigned long lastButtonTime1 = 0;
volatile bool stateChanged = false;
volatile int frameCounter = 0;
const unsigned long debounceDelay = 25000;

// Button ISR
void IRAM_ATTR ISR_TOGGLE() {
    unsigned long currentTime = micros();
    if (currentTime - lastButtonTime1 > debounceDelay) {  //Check for debounce 
        state = !state; //Toggle LED state 
        stateChanged = true;  //Set flag to indicate button press
        lastButtonTime1 = currentTime;
    }
}

void setup() {
    Serial.begin(9600);

    pinMode(SIG_ONE, OUTPUT);
    pinMode(SIG_TWO, OUTPUT);
    pinMode(LED_TOGGLE, OUTPUT);
    pinMode(LED_SUM, OUTPUT);
    pinMode(FREQUENCY_ONE, INPUT);
    pinMode(FREQUENCY_TWO, INPUT);
    pinMode(BUTTON, INPUT_PULLUP);

    attachInterrupt(BUTTON, ISR_TOGGLE, RISING);

    Cycle.attach_ms(2, frame);  // Call frame() every 2ms

    monitor.startMonitoring();  // Start monitor library 
}

void loop() {}

// LED or Osclicope output 
void generateSignal(int signal, int time1, int time2, int time3, int task) {
    monitor.jobStarted(task); 

    digitalWrite(signal, HIGH);
    delayMicroseconds(time1);
    digitalWrite(signal, LOW);
    delayMicroseconds(time2);
    digitalWrite(signal, HIGH);
    delayMicroseconds(time3);
    digitalWrite(signal, LOW);

    monitor.jobEnded(task);
}

float measureFrequency(int signal, int task, int timeout) {
    monitor.jobStarted(task);

    int high = pulseIn(signal, HIGH, timeout);  // Measure HIGH of the pulse - 50% duty cycle SO HALF the signal

    if (task == 3) {
        if (high == 0 || high > 750 || high < 500) {  // Invalid measurement
            monitor.jobEnded(task);
            return f1;   // Set as old frequency since invalid
        }
    } else if (task == 4) {  // Invalid measurement
        if (high == 0 || high > 600 || high < 333) {
            monitor.jobEnded(task);
            return f2;  // Set as old frequency since invalid
        }
    }

   
    float frequency = (1000000.0 / (float)(high * 2));  // Calculate new frequency 

    monitor.jobEnded(task);
    return frequency;
}

// Tasks to be done
void JobTask1() { generateSignal(SIG_ONE, 250, 50, 300, 1); }

void JobTask2() { generateSignal(SIG_TWO, 100, 50, 200, 2); }

void JobTask3() { f1 = measureFrequency(FREQUENCY_ONE, 3, 1500); }

void JobTask4() { f2 = measureFrequency(FREQUENCY_TWO, 4, 1200); }

void JobTask5() {
    monitor.jobStarted(5);
    monitor.doWork();
    monitor.jobEnded(5);
}

// Calculate sum of the two frequencies 
void sumFrequencies() {
    int sum = (int)f1 + (int)f2;
    if (sum > 1500) { //if over 1500 turn LED on
        digitalWrite(LED_SUM, HIGH);
    } else {
        digitalWrite(LED_SUM, LOW);
    }
}

// Cyclic execution frame - 2ms long 
void frame() {
    unsigned int slot = frameCounter % 30;

    // Only toggle LED if state changed AND it's not frame 25 AND frame 5 (no slack)
    if (stateChanged && slot != 25 && slot != 5) {
        digitalWrite(LED_TOGGLE, state);
        monitor.doWork();
        stateChanged = false;  // Reset the flag after toggling
    }

    // Call tasks based on which frame is currently running 
    switch (slot) {
        case 0: JobTask1(); JobTask2(); JobTask5(); break;
        case 1: JobTask3(); break;
        case 2: JobTask1(); JobTask2(); break;
        case 3: JobTask5(); JobTask4(); JobTask2(); break;
        case 4: JobTask1(); break;
        case 5: JobTask2(); JobTask3(); break;
        case 6: JobTask1(); JobTask2(); JobTask5(); break;
        case 7: JobTask4(); break;
        case 8: JobTask1(); JobTask2(); JobTask5(); break;
        case 9: JobTask2(); break;
        case 10: JobTask1(); JobTask5(); break;
        case 11: JobTask2(); JobTask3(); break;
        case 12: JobTask1(); JobTask2(); break;
        case 13: JobTask5(); JobTask4(); break;
        case 14: JobTask1(); JobTask2(); break;
        case 15: JobTask2(); JobTask3(); break;
        case 16: JobTask1(); JobTask5(); break;
        case 17: JobTask2(); JobTask4(); break;
        case 18: JobTask1(); JobTask2(); JobTask5(); break;
        case 19: sumFrequencies(); break; //Empty frame so sum frequencies 
        case 20: JobTask1(); JobTask2(); JobTask5(); break;
        case 21: JobTask2(); JobTask3(); break;
        case 22: JobTask1(); JobTask4(); break;
        case 23: JobTask2(); JobTask5(); break;
        case 24: JobTask1(); JobTask2(); break;
        case 25: JobTask3(); JobTask5(); break;
        case 26: JobTask1(); JobTask2(); break;
        case 27: JobTask2(); JobTask4(); break;
        case 28: JobTask1(); JobTask5(); break;
        case 29: JobTask2(); break;
    }

    frameCounter++; // Next frame

    if (frameCounter >= 30) { 
        frameCounter = 0;  // Reset after completing the cycle
    }
}
