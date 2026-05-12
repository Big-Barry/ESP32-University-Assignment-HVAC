#include "hvac_core.h"

// --- GLOBAL VARIABLE DEFINITIONS ---
SystemState hvacState = {0.0, 0.0, 0, false, false};
SemaphoreHandle_t stateMutex;
volatile bool buttonTriggered = false; 

// --- SETUP (INITIALIZATION) ---
void setup() {
    Serial.begin(115200);
    
    // Pin Configurations
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); 
    
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleEmergencyButton, FALLING);
    Serial.println("System Booting...");

    // Initialize the Hardware Watchdog Timer
    esp_task_wdt_init(WDT_TIMEOUT, true);

    // Initialize the Mutex
    stateMutex = xSemaphoreCreateMutex();

    if (stateMutex != NULL) {
        xTaskCreate(TaskSystemControl, "Control Task", 2048, NULL, 3, NULL); 
        xTaskCreate(TaskReadSensors,   "Sensor Task",  2048, NULL, 2, NULL); 
        xTaskCreate(TaskReadADC,       "ADC Task",     2048, NULL, 1, NULL); 
    } else {
        Serial.println("Mutex creation failed!");
    }
}

// --- MAIN LOOP ---
void loop() {
    // Empty - FreeRTOS handles execution via Tasks
}