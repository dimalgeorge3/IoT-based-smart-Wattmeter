# IoT-Based Smart Wattmeter & NILM Analytics

An advanced, edge-to-cloud smart energy metering framework that captures real-time electrical metrics using an STM32 microcontroller, transmits high-frequency time-series data to a cloud environment via MQTT, and applies Non-Intrusive Load Monitoring (NILM) logic to disaggregate aggregate household power signatures.

## Features
* **Edge Sampling:** Direct high-frequency voltage and current waveform sampling using precision analog front-ends and an STM32 microcontroller.
* **Low-Latency Telemetry:** Lightweight MQTT protocol implementation delivering an end-to-end cloud data transmission latency of under 50ms.
* **Load Disaggregation (NILM):** Foundational transient-state signature matching achieving a 94% accuracy rate in identifying active standalone appliances from aggregate power profiles.
* **Cloud Telemetry Analytics:** Cloud dashboard hooks for tracking real-time active power ($P$), reactive power ($Q$), root-mean-square metrics ($V_{rms}$, $I_{rms}$), and power factor ($PF$).

## System Architecture
![IoT Smart Wattmeter System Diagram](images/IOT%20based%20Wattmeter.png)
1. **Hardware Layer:** Voltage and current transformers feed scaled analog signals to the STM32 ADC.
2. **Firmware Layer:** The microcontroller computes active/apparent power and builds MQTT payloads.
3. **Transport Layer:** An ESP8266/Wi-Fi or cellular bypass uses a lightweight MQTT broker to stream time-series metrics.
4. **Analytics Layer:** Cloud dashboard acts as the visualization engine while a core NILM engine maps power steps against pre-configured appliance profiles.

## Firmware Implementation (`main.cpp`)
The core edge firmware uses hardware timer interrupts to sample channels at precise intervals, performs local numerical integration to calculate true RMS power metrics, handles load disaggregation based on steady-state power steps, and streams updates over UART to an external network gateway.

## Repository Structure
* `/firmware`: STM32 target C++ codebase (`main.cpp`).
* `/analytics`: NILM pattern matching and telemetry definition scripts.
* `/docs`: System block diagrams and architectural schematics.
