#include <Arduino.h>

// --- Pin Definitions ---
const int VOLTAGE_PIN = PA0;      // ZMPT101B Analog input
const int CURRENT_PIN = PA1;      // ACS712 Analog input
const int STATUS_LED  = PC13;     // Onboard STM32 Status Indicator

// --- Sampling Configurations ---
const unsigned int SAMPLE_WINDOW = 200; 
const float V_CALIBRATION = 240.0 / 1.5; 
const float I_CALIBRATION = 10.0 / 1.0;  

// --- Global Telemetry Variables ---
float V_rms = 0.0, I_rms = 0.0;
float activePower = 0.0, apparentPower = 0.0, powerFactor = 0.0;
float lastActivePower = 0.0;
unsigned long lastTelemetryTime = 0;

// --- Dynamic NILM Storage Framework ---
struct Appliance {
    char name[16];
    float powerRating;
    bool isActive;
};

// Provision to hold up to 10 user-defined dashboard appliances
Appliance userAppliances[10];
int totalApplianceCount = 0;
const float NILM_THRESHOLD = 25.0; // Minimum delta to trigger scan

// --- Function Prototypes ---
void executeDynamicNILM(float powerDelta);
void checkIncomingDashboardConfigs();

void setup() {
    Serial.begin(115200); // UART Serial Link linking to ESP8266 Gateway
    
    pinMode(VOLTAGE_PIN, INPUT);
    pinMode(CURRENT_PIN, INPUT);
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH); 
    
    // Seed default baseline profile into index slot 0
    strcpy(userAppliances[0].name, "Base Load");
    userAppliances[0].powerRating = 40.0;
    userAppliances[0].isActive = false;
    totalApplianceCount = 1;
}

void loop() {
    // 1. Check for new dynamic equipment updates added via the user dashboard
    checkIncomingDashboardConfigs();

    long sumVoltageSq = 0;
    long sumCurrentSq = 0;
    long sumInstantaneousPower = 0;
    unsigned int sampleCount = 0;
    
    unsigned long startTime = millis();
    
    // 2. High-Frequency Waveform Integration
    while ((millis() - startTime) < SAMPLE_WINDOW) {
        int rawV = analogRead(VOLTAGE_PIN) - 2048;
        int rawI = analogRead(CURRENT_PIN) - 2048;
        
        sumVoltageSq += ((long)rawV * rawV);
        sumCurrentSq += ((long)rawI * rawI);
        sumInstantaneousPower += ((long)rawV * rawI);
        sampleCount++;
    }
    
    if (sampleCount == 0) return;

    V_rms = sqrt(sumVoltageSq / sampleCount) * (3.3 / 4096.0) * V_CALIBRATION;
    I_rms = sqrt(sumCurrentSq / sampleCount) * (3.3 / 4096.0) * I_CALIBRATION;
    
    activePower = abs((float)sumInstantaneousPower / sampleCount * (3.3 / 4096.0) * (3.3 / 4096.0) * V_CALIBRATION * I_CALIBRATION);
    apparentPower = V_rms * I_rms;
    
    powerFactor = (apparentPower > 0) ? (activePower / apparentPower) : 0.0;
    if (powerFactor > 1.0) powerFactor = 1.0;

    // 3. Process Transients Using Dynamically Added Signatures
    float powerDelta = activePower - lastActivePower;
    if (abs(powerDelta) >= NILM_THRESHOLD) {
        executeDynamicNILM(powerDelta);
        lastActivePower = activePower; 
    }

    // 4. Dispatch System Stream Metrics Out to Dashboard Gateway (1 Hz)
    if (millis() - lastTelemetryTime >= 1000) { 
        digitalWrite(STATUS_LED, LOW); 
        
        Serial.print("{\"V_rms\":"); Serial.print(V_rms, 2);
        Serial.print(",\"I_rms\":"); Serial.print(I_rms, 3);
        Serial.print(",\"ActivePower\":"); Serial.print(activePower, 2);
        Serial.print(",\"ApparentPower\":"); Serial.print(apparentPower, 2);
        Serial.print(",\"PowerFactor\":"); Serial.print(powerFactor, 2);
        Serial.println("}");
        
        digitalWrite(STATUS_LED, HIGH);
        lastTelemetryTime = millis();
    }
}

// --- Parse Incoming Dashboard Instructions ---
// Handles incoming string format: "ADD:DeviceName,Rating" 
// Example string sent from cloud: "ADD:Microwave,800"
void checkIncomingDashboardConfigs() {
    if (Serial.available() > 0) {
        String incomingStr = Serial.readStringUntil('\n');
        incomingStr.trim();
        
        if (incomingStr.startsWith("ADD:") && totalApplianceCount < 10) {
            int colonIdx = incomingStr.indexOf(':');
            int commaIdx = incomingStr.indexOf(',');
            
            if (commaIdx > colonIdx) {
                String devName = incomingStr.substring(colonIdx + 1, commaIdx);
                String devRating = incomingStr.substring(commaIdx + 1);
                
                // Add the user payload definition directly into local search profiles
                devName.toCharArray(userAppliances[totalApplianceCount].name, 16);
                userAppliances[totalApplianceCount].powerRating = devRating.toFloat();
                userAppliances[totalApplianceCount].isActive = false;
                
                totalApplianceCount++;
            }
        }
    }
}

// --- Dynamic Matching Execution Engine ---
void executeDynamicNILM(float powerDelta) {
    bool isEdgePositive = (powerDelta > 0);
    float absoluteDelta = abs(powerDelta);
    int matchedIndex = -1;
    float closestTolerance = 99999.0;
    
    // Cycle through array configurations up to the current total counter metric limit
    for (int i = 0; i < totalApplianceCount; i++) {
        float difference = abs(absoluteDelta - userAppliances[i].powerRating);
        
        // Dynamic adaptive window scaling depending on magnitude size
        float calculatedToleranceWindow = userAppliances[i].powerRating * 0.15; 
        if (calculatedToleranceWindow < 15.0) calculatedToleranceWindow = 15.0; 
        
        if (difference <= calculatedToleranceWindow && difference < closestTolerance) {
            closestTolerance = difference;
            matchedIndex = i;
        }
    }
    
    // If a signature match matches the parameters, alert backend dashboard log channels
    if (matchedIndex != -1) {
        userAppliances[matchedIndex].isActive = isEdgePositive;
        
        Serial.print("{\"NILM_Event\":\"");
        Serial.print(userAppliances[matchedIndex].name);
        Serial.println(isEdgePositive ? "_ON\"}" : "_OFF\"}");
    }
}
