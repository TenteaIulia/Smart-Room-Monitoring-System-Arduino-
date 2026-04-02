#  Smart Room Monitoring System (Arduino)

##  Overview
This project implements a smart embedded system for monitoring and controlling the **temperature** and **light intensity** inside a room using Arduino.

The system automatically adjusts lighting and simulates a heating system based on sensor data, maintaining a stable and comfortable environment.

---

##  Features
-  Light intensity monitoring using **GY-302 (BH1750) sensor**
-  Automatic brightness control using **PWM**
-  Temperature monitoring using **TMP102 sensor**
-  Thermostat functionality with configurable temperature
-  Storage of temperature setpoint in **EEPROM (25AA040A)**
-  Automatic restoration of saved settings on startup
-  Real-time system response
-  Serial communication for debugging and monitoring

---

##  System Logic

###  Light Control
- Reads ambient light intensity using GY-302
- Adjusts LED brightness using PWM to maintain constant light level
- If ambient light decreases → LED brightness increases
- If ambient light increases → LED brightness decreases

---

###  Temperature Control (Thermostat)
- Reads temperature from TMP102 sensor
- User-defined temperature is stored in EEPROM
- On startup, the system loads the saved value

#### Control Logic:
- If temperature < setpoint → heating ON (LED ON)
- If temperature > setpoint + 0.5°C → heating OFF (LED OFF)

 Hysteresis is used to prevent frequent switching

---

##  Hardware Components
- Arduino board (Uno / Nano)
- GY-302 (BH1750) Light Sensor
- TMP102 Temperature Sensor
- EEPROM 25AA040A
- LEDs (for light and heating simulation)
- Resistors
- Breadboard & wires

---

##  Communication Protocols
- I2C → GY-302, TMP102
- SPI → EEPROM (25AA040A)
- UART (Serial) → Debugging / Monitoring

---

##  How It Works
1. System initializes and reads saved temperature from EEPROM  
2. Sensors continuously provide real-time data  
3. Control logic adjusts:
   - LED brightness (PWM)
   - Heating state (ON/OFF)  
4. System reacts dynamically to environmental changes  

---

##  Demo  
- Circuit setup  
- Working system  
- Serial monitor output  

---

##  Possible Improvements
- Implement mobile/web dashboard  
- Add WiFi module (ESP32)  
- Improve control algorithm (PID controller)  
- Add multiple rooms support  

---

##  Technologies Used
- Arduino (C/C++)
- Embedded Systems Programming
- PWM Control
- I2C, SPI Communication
- EEPROM Memory Management

---

