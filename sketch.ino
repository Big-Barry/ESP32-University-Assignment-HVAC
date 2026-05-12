#include <DHT.h>
#include <Arduino.h>
#include <esp_task_wdt.h> // Required for Watchdog implementation

#define DHTTYPE DHT22

// --- 1. PIN DEFINITIONS ---
#define DHT_PIN 15
#define POT_PIN 34
#define RELAY_PIN 13
#define BUZZER_PIN 12
#define LED_PIN 14
#define BUTTON_PIN 27

// Watchdog Timeout in seconds
#define WDT_TIMEOUT 5 

// --- 2. SHARED DATA STRUCTURE ---
typedef struct {
    float temperature;
    float humidity;
    int rawADCValue;
    bool emergencyOverride; 
    bool sensorAnomaly; // Added for Sensor anomaly detection[cite: 1]
} SystemState;

// Create an instance of our struct to hold the data
SystemState hvacState = {0.0, 0.0, 0, false, false};

// --- 3. FREERTOS HANDLES ---
SemaphoreHandle_t stateMutex;

// --- INTERRUPT VARIABLES ---
volatile bool buttonTriggered = false; 
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 250; 

// --- 4. TASK PROTOTYPES ---
void TaskReadSensors(void *pvParameters);
void TaskReadADC(void *pvParameters);
void TaskSystemControl(void *pvParameters);

// --- INTERRUPT SERVICE ROUTINE (ISR) ---
void IRAM_ATTR handleEmergencyButton() {
    unsigned long currentTime = millis();
    
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        buttonTriggered = true; 
        lastDebounceTime = currentTime;
    }
}

// --- 5. SETUP (INITIALIZATION) ---
void setup() {
    Serial.begin(115200);
    
    // Pin Configurations
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); 
    
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleEmergencyButton, FALLING);
    Serial.println("System Booting...");

    // Initialize the Hardware Watchdog Timer[cite: 1]
    // Initialize the Hardware Watchdog Timer (Updated for ESP-IDF v5)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // Monitor all cores
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);

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

// --- 6. MAIN LOOP ---
void loop() {
    // Empty
}

// --- 7. TASK IMPLEMENTATIONS ---

void TaskReadSensors(void *pvParameters) {
    DHT dht(DHT_PIN, DHTTYPE);
    dht.begin();

    for (;;) {
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();
        bool anomaly = false;

        // Sensor anomaly detection: Check for failure or impossible industrial values[cite: 1]
        if (isnan(temp) || isnan(hum) || temp < -20.0 || temp > 80.0) {
            Serial.println("ERROR: Sensor Anomaly Detected!");
            anomaly = true;
        }

        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            hvacState.temperature = temp;
            hvacState.humidity = hum;
            hvacState.sensorAnomaly = anomaly; // Update the anomaly flag
            xSemaphoreGive(stateMutex); 
            
            if (!anomaly) {
                Serial.printf("Sensor Update -> Temp: %.1f°C, Hum: %.1f%%\n", temp, hum);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void TaskReadADC(void *pvParameters) {
    analogReadResolution(12); 

    for (;;) {
        long sum = 0;
        
        for(int i = 0; i < 10; i++) {
            sum += analogRead(POT_PIN);
            vTaskDelay(pdMS_TO_TICKS(5)); 
        }
        int smoothedADC = sum / 10;

        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            hvacState.rawADCValue = smoothedADC;
            xSemaphoreGive(stateMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

void TaskSystemControl(void *pvParameters) {
    unsigned long lastPrintTime = 0; 
    
    // Subscribe this critical safety task to the Watchdog Timer[cite: 1]
    esp_task_wdt_add(NULL); 

    for (;;) {
        // Feed the dog! If this loop freezes, the ESP32 will reboot[cite: 1]
        esp_task_wdt_reset(); 

        // 1. Thread-safe handling of the interrupt flag
        if (buttonTriggered) {
            if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
                hvacState.emergencyOverride = !hvacState.emergencyOverride; 
                xSemaphoreGive(stateMutex);
            }
            buttonTriggered = false; 
            Serial.println("\n!!! EMERGENCY OVERRIDE BUTTON PRESSED !!!\n");
        }

        // 2. Safely read the current state
        float currentTemp = 0.0;
        bool isEmergency = false;
        bool isAnomaly = false;
        int adcValue = 0;
        
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            currentTemp = hvacState.temperature;
            isEmergency = hvacState.emergencyOverride;
            isAnomaly = hvacState.sensorAnomaly;
            adcValue = hvacState.rawADCValue;
            xSemaphoreGive(stateMutex);
        }

        // 3. Calibration Equation
        float targetTemp = 15.0 + (adcValue * (20.0 / 4095.0));
        
        // Overheat shutdown threshold
        bool isOverheating = (currentTemp > 40.0); 

        // 4. Industrial Safety Logic
        unsigned long currentTime = millis();
        bool timeToPrint = (currentTime - lastPrintTime > 2000);

        // Fail-safe logic now includes the anomaly check[cite: 1]
        if (isEmergency || isOverheating || isAnomaly) {
            digitalWrite(RELAY_PIN, LOW);   
            digitalWrite(BUZZER_PIN, HIGH); 
            digitalWrite(LED_PIN, HIGH);    
            
            if (timeToPrint) {
                Serial.printf("STATUS: FAIL-SAFE! Overheat: %d, Emergency: %d, Anomaly: %d\n", isOverheating, isEmergency, isAnomaly);
                lastPrintTime = currentTime;
            }
        } else {
            // NORMAL OPERATION
            digitalWrite(BUZZER_PIN, LOW); 
            digitalWrite(LED_PIN, LOW);    
            
            if (currentTemp > targetTemp) {
                digitalWrite(RELAY_PIN, HIGH); 
                if (timeToPrint) {
                    Serial.printf("STATUS: AC ON  | Target: %.1f°C | Current: %.1f°C\n", targetTemp, currentTemp);
                    lastPrintTime = currentTime;
                }
            } else {
                digitalWrite(RELAY_PIN, LOW); 
                if (timeToPrint) {
                    Serial.printf("STATUS: AC OFF | Target: %.1f°C | Current: %.1f°C\n", targetTemp, currentTemp);
                    lastPrintTime = currentTime;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}