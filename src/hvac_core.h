#ifndef HVAC_CORE_H
#define HVAC_CORE_H

#include <DHT.h>
#include <Arduino.h>
#include <esp_task_wdt.h>

#define DHTTYPE DHT22

// --- PIN DEFINITIONS ---
#define DHT_PIN 15
#define POT_PIN 34
#define RELAY_PIN 13
#define BUZZER_PIN 12
#define LED_PIN 14
#define BUTTON_PIN 27

// Watchdog Timeout in seconds
#define WDT_TIMEOUT 5 

// --- SHARED DATA STRUCTURE ---
typedef struct {
    float temperature;
    float humidity;
    int rawADCValue;
    bool emergencyOverride; 
    bool sensorAnomaly; 
} SystemState;

// --- EXTERN GLOBAL VARIABLES ---
// 'extern' tells the compiler these variables exist, but are defined in the .ino file
extern SystemState hvacState;
extern SemaphoreHandle_t stateMutex;
extern volatile bool buttonTriggered;

// --- TASK & ISR PROTOTYPES ---
void IRAM_ATTR handleEmergencyButton();
void TaskReadSensors(void *pvParameters);
void TaskReadADC(void *pvParameters);
void TaskSystemControl(void *pvParameters);

#endif // HVAC_CORE_H