#include "hvac_core.h"

// ISR-specific variables (kept local to this file to prevent namespace clutter)
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 250; 

// --- INTERRUPT SERVICE ROUTINE (ISR) ---
void IRAM_ATTR handleEmergencyButton() {
    unsigned long currentTime = millis();
    
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        buttonTriggered = true; 
        lastDebounceTime = currentTime;
    }
}

// --- TASK IMPLEMENTATIONS ---
void TaskReadSensors(void *pvParameters) {
    DHT dht(DHT_PIN, DHTTYPE);
    dht.begin();

    for (;;) {
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();
        bool anomaly = false;

        // DHT22 valid temperature range: -40°C to 80°C
        if (isnan(temp) || isnan(hum) || temp < -40.0 || temp > 80.0) { //This is used to filter out anomolies outside of the temperature rnage of the DHT22
            Serial.println("ERROR: Sensor Anomaly Detected!");
            anomaly = true;
        }

        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            hvacState.temperature = temp;
            hvacState.humidity = hum;
            hvacState.sensorAnomaly = anomaly; 
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
    
    esp_task_wdt_add(NULL); 

    for (;;) {
        esp_task_wdt_reset(); 

        if (buttonTriggered) {
            if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
                hvacState.emergencyOverride = !hvacState.emergencyOverride; 
                xSemaphoreGive(stateMutex);
            }
            buttonTriggered = false; 
            Serial.println("\n!!! EMERGENCY OVERRIDE BUTTON PRESSED !!!\n");
        }

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

        float targetTemp = 15.0 + (adcValue * (20.0 / 4095.0));
        bool isOverheating = (currentTemp > 40.0); 
        unsigned long currentTime = millis();
        bool timeToPrint = (currentTime - lastPrintTime > 2000);

        if (isEmergency || isOverheating || isAnomaly) {
            digitalWrite(RELAY_PIN, LOW);   
            digitalWrite(BUZZER_PIN, HIGH); 
            digitalWrite(LED_PIN, HIGH);    
            
            if (timeToPrint) {
                Serial.printf("STATUS: FAIL-SAFE! Overheat: %d, Emergency: %d, Anomaly: %d\n", isOverheating, isEmergency, isAnomaly);
                lastPrintTime = currentTime;
            }
        } else {
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