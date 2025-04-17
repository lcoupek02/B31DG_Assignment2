#include <B31DGMonitor.h>

#include <Ticker.h>

#define SIG_ONE 16
#define SIG_TWO 4
#define FREQUENCY_ONE 18
#define FREQUENCY_TWO 17
#define LED_SUM 23
#define BUTTON 22
#define LED_TOGGLE 15

B31DGCyclicExecutiveMonitor monitor(2478);

// Global Variables
float f1, f2;
bool state = false; 

// RTOS Variables
SemaphoreHandle_t xF1Mute;
SemaphoreHandle_t xF2Mute;
SemaphoreHandle_t buttonMutex;
TaskHandle_t Task1Handler,Task2Handler, Task3Handler,Task4Handler,Task5Handler, TaskSumHandler, TaskButtonHandler;

//Function Prototypes
void JobTask1();
void JobTask2();
void JobTask3();
void JobTask4();
void JobTask5();


void IRAM_ATTR ISR_TOGGLE() {
    xSemaphoreGiveFromISR(buttonMutex, NULL); //Release semaphore so TaskButton can run
}

// LED or Oscilloscope output 
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
  
   int high = pulseIn(signal, HIGH, timeout);  // Measure HIGH of the pulse - 50% duty 
   
   if(task == 3){
    if (high == 0 || high > 750 || high < 500) { //invalid measurement 
       monitor.jobEnded(task);
       return f1; // Set as old frequency since invalid
    }
     }
    else if(task == 4){
      if (high == 0 || high > 600 || high < 333 ) {
         monitor.jobEnded(task);
         return f2;  // Set as old frequency since invalid
      }
    }

    float frequency = (1000000.0 / (float)(high*2)) ;   // Calculate new frequency
    
    monitor.jobEnded(task);
    return frequency;
}

// Tasks to be done
void Task1(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(4); // Tasks to run every 4 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        generateSignal(SIG_ONE, 250, 50, 300, 1); // Call task to be done 
    }
}

void Task2(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(3); // Tasks to run every 3 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        generateSignal(SIG_TWO, 100, 50, 200, 2); // Call task to be done 
    }
}

void Task3(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Tasks to run every 10 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        float tempF = measureFrequency(FREQUENCY_ONE, 3, 1500); // Call task to be done 
        
        xSemaphoreTake(xF1Mute, portMAX_DELAY); //Protect f1 while updating to new value
        f1 = tempF;
        xSemaphoreGive(xF1Mute);
    }
}

void Task4(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Tasks to run every 10 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        float tempF = measureFrequency(FREQUENCY_TWO, 4, 1200); // Call task to be done 
     
        xSemaphoreTake(xF2Mute, portMAX_DELAY); //Protect f2 while updating to new value
        f2 = tempF;
        xSemaphoreGive(xF2Mute);
    }
}

void Task5(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5); // Tasks to run every 5 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        monitor.jobStarted(5);
        monitor.doWork();
        monitor.jobEnded(5);
    }
}

void TaskSum(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(60); // Tasks to run every 60 ms 
    xLastWakeTime = xTaskGetTickCount();
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay next run until next period
        xSemaphoreTake(xF2Mute, portMAX_DELAY);  // Critical area of f1 and f2
        xSemaphoreTake(xF1Mute, portMAX_DELAY);
        int sum = (int)f1 + (int)f2; // Get sum
       
        xSemaphoreGive(xF2Mute);
        xSemaphoreGive(xF1Mute); //End of critical area for f1 and f2
    
        if (sum > 1500) { //if over 1500 turn LED on
            digitalWrite(LED_SUM, HIGH);
        } else {
            digitalWrite(LED_SUM, LOW);
        }
    }
}

void TaskButton(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(60);

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    if (xSemaphoreTake(buttonMutex, portMAX_DELAY) == pdTRUE) {   //If button has been pressed Semaphore can be taken
      monitor.doWork();
      state = !state;
      digitalWrite(LED_TOGGLE, state);
    }
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

    attachInterrupt(digitalPinToInterrupt(BUTTON), ISR_TOGGLE, RISING);
   
    xF1Mute = xSemaphoreCreateMutex();
    xF2Mute = xSemaphoreCreateMutex();
    buttonMutex = xSemaphoreCreateBinary();
  
    xTaskCreate(TaskSum, "TaskSum", 2048, NULL, 0, &TaskSumHandler);
    xTaskCreate(TaskButton, "TaskButton", 4096, NULL, 0, &TaskButtonHandler);
    xTaskCreate(Task1, "Task1", 4096, NULL, 3, &Task1Handler);
    xTaskCreate(Task2, "Task2", 4096, NULL, 3, &Task2Handler);
    xTaskCreate(Task3, "Task3", 4096, NULL,1, &Task3Handler);
    xTaskCreate(Task4, "Task4", 4096, NULL, 1, &Task4Handler);
    xTaskCreate(Task5, "Task5", 2048, NULL, 3, &Task5Handler);
   

    monitor.startMonitoring();
}

void loop(){}
