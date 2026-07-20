#include <Arduino.h> // Compatible with STM32duino environment

// -------------------------------------------------------------------
// Pin & Configuration Constants
// -------------------------------------------------------------------
const int VOLTAGE_PIN = PA0;      // Precision Voltage Transformer Analog input
const int CURRENT_PIN = PA1;      // Precision Current Transformer Analog input
const int STATUS_LED  = PC13;     // Onboard STM32 Status Indicator

const unsigned int SAMPLE_WINDOW = 200; // Sample over 200ms (10 full AC cycles at 50Hz)
const float V_CALIBRATION = 240.0 / 1.5; // Scale factor for voltage channel
const float I_CALIBRATION = 10.0 / 1.0;  // Scale factor for current channel

// -------------------------------------------------------------------
// Telemetry & NILM Calibration Variables
// -------------------------------------------------------------------
float V_rms = 0.0, I_rms = 0.0;
float activePower = 0.0, apparentPower = 0.0, powerFactor = 0.0;
float lastActivePower = 0.0;
unsigned long lastTelemetryTime = 0;

// Simplified NILM Baseline Signatures (Steady-State Active Power Steps in Watts)
const float SIGNATURE_REFRIGERATOR = 150.0;
const float SIGNATURE_MICROWAVE    = 800.0;
const float SIGNATURE_LIGHTING     = 40.0;
const float NILM_THRESHOLD         = 25.0; // Margin of error for step detection

void executeNILMEngine(float powerDelta);

void setup() {
    Serial.begin(115200); // UART Link out to MQTT Network Module (e.g., ESP8266)
    pinMode(VOLTAGE_PIN, INPUT);
    pinMode(CURRENT_PIN, INPUT);
    pinMode(STATUS_LED, OUTPUT);
    
    digitalWrite(STATUS_LED, HIGH); // Initialization complete
}

void loop() {
    long sumVoltageSq = 0;
    long sumCurrentSq = 0;
    long sumInstantaneousPower = 0;
    unsigned int sampleCount = 0;
    
    unsigned long startTime = millis();
    
    // ---------------------------------------------------------------
    // 1. High-Frequency Analog Sampling Window
    // ---------------------------------------------------------------
    while ((millis() - startTime) < SAMPLE_WINDOW) {
        // Read raw 12-bit ADC data (Offset centered around VCC/2 internally)
        int rawV = analogRead(VOLTAGE_PIN) - 2048;
        int rawI = analogRead(CURRENT_PIN) - 2048;
        
        sumVoltageSq += ((long)rawV * rawV);
        sumCurrentSq += ((long)rawI * rawI);
        sumInstantaneousPower += ((long)rawV * rawI);
        
        sampleCount++;
    }
    
    // Prevent Division-by-Zero errors
    if (sampleCount == 0) return;

    // ---------------------------------------------------------------
    // 2. Continuous Power Metric Integration
    // ---------------------------------------------------------------
    V_rms = sqrt(sumVoltageSq / sampleCount) * (3.3 / 4096.0) * V_CALIBRATION;
    I_rms = sqrt(sumCurrentSq / sampleCount) * (3.3 / 4096.0) * I_CALIBRATION;
    
    activePower = abs((float)sumInstantaneousPower / sampleCount * (3.3 / 4096.0) * (3.3 / 4096.0) * V_CALIBRATION * I_CALIBRATION);
    apparentPower = V_rms * I_rms;
    powerFactor = (apparentPower > 0) ? (activePower / apparentPower) : 0.0;
    
    if (powerFactor > 1.0) powerFactor = 1.0;

    // ---------------------------------------------------------------
    // 3. Steady-State NILM Step Detection Engine
    // ---------------------------------------------------------------
    float powerDelta = activePower - lastActivePower;
    if (abs(powerDelta) >= NILM_THRESHOLD) {
        executeNILMEngine(powerDelta);
        lastActivePower = activePower; // Reset step baseline
    }

    // ---------------------------------------------------------------
    // 4. Low-Latency MQTT Telemetry Packaging (<50ms pipeline)
    // ---------------------------------------------------------------
    if (millis() - lastTelemetryTime >= 1000) { // Transmit data packet every second
        digitalWrite(STATUS_LED, LOW); // Toggle activity LED
        
        // Output cleanly formatted JSON string over UART link to MQTT gateway broker
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

// -------------------------------------------------------------------
// Non-Intrusive Load Monitoring (NILM) Event Logic
// -------------------------------------------------------------------
void executeNILMEngine(float powerDelta) {
    bool isEdgePositive = (powerDelta > 0);
    float absoluteDelta = abs(powerDelta);
    String targetDevice = "Unknown Appliance";
    
    // Classify step delta against known appliance profiles
    if (abs(absoluteDelta - SIGNATURE_MICROWAVE) <= 75.0) {
        targetDevice = "Microwave Ovens";
    } else if (abs(absoluteDelta - SIGNATURE_REFRIGERATOR) <= 35.0) {
        targetDevice = "Refrigerator Unit";
    } else if (abs(absoluteDelta - SIGNATURE_LIGHTING) <= 10.0) {
        targetDevice = "Lighting Arrays";
    } else {
        return; // Filter out negligible load noise
    }
    
    // Print isolated NILM event output to stream to specific cloud topic
    Serial.print("NILM_EVENT -> ");
    Serial.print(targetDevice);
    Serial.println(isEdgePositive ? " turned ON State." : " turned OFF State.");
}
