# 🏭 Industrial Machine Health Monitoring System  
### *(6th Semester IoT Lab Project)*

An IoT-based system that monitors machine parameters such as **temperature, humidity, current, and vibration** in real time using ESP32. The system sends data to Firebase via REST APIs and performs **automatic fault detection with relay-based shutdown**.  

🌐 The monitoring dashboard webpage is **hosted using GitHub Pages**.

---

## 🚀 Features

- 🌡️ Temperature & Humidity Monitoring (DHT11)  
- ⚡ Current Monitoring (ACS712)  
- 📳 Vibration Monitoring (MPU9250/6500)  
- ☁️ Real-time Cloud Sync (Firebase)  
- 🌐 Web dashboard hosted on GitHub Pages  
- 🚨 Fault Detection (Normal / Warning / Critical)  
- 🔌 Automatic Machine Shutdown (Relay)  
- 🔄 Remote Restart (Webpage + Button)  
- 📊 Live Data Monitoring Dashboard  

---

## 🧰 Components Used

- ESP32 Development Board  
- DHT11 Sensor  
- ACS712 Current Sensor (30A)  
- MPU9250 / MPU6500 IMU Sensor  
- Relay Module  
- Push Button  
- Jumper Wires  
- Breadboard  
- Power Supply / Battery  

---

## 🔌 Circuit Connections

### 📍 Pin Configuration

| Component | ESP32 Pin |
|----------|----------|
| DHT11 DATA | GPIO 4 |
| ACS712 OUT | GPIO 34 / 35 |
| MPU SDA | GPIO 21 |
| MPU SCL | GPIO 22 |
| Relay IN | GPIO 26 |
| LED | GPIO 2 |
| Button | GPIO 13 |

---

### 🖼️ Circuit Diagram

<img width="2000" height="1600" alt="`" src="https://github.com/user-attachments/assets/2dbd12d1-98cf-4fd8-94a9-f321224d7962" />

---

## 🌐 Webpage Overview (Hosted on GitHub Pages)

### 🔥 Firebase Database
<img width="1383" height="502" alt="Screenshot 2026-05-01 081930" src="https://github.com/user-attachments/assets/33d57e46-b44d-4111-8d68-503ba7ac2e0a" />


---



### 📊 Dashboard Overview
<img width="1906" height="950" alt="Screenshot 2026-05-01 081609" src="https://github.com/user-attachments/assets/0122d6bd-2e01-446d-a761-4b710113ca38" />


---

### 🚨 Alert Notification

<img width="1898" height="953" alt="Screenshot 2026-05-01 081521" src="https://github.com/user-attachments/assets/2c10ece9-28d3-4401-83f2-5b4e1572a69b" />



### 🛑 Machine Cutoff Notification
<img width="1917" height="947" alt="Screenshot 2026-05-01 081626" src="https://github.com/user-attachments/assets/58036453-0d46-4884-ad43-2dcf9911dafa" />


---


### 📈 Real-time Graph
<img width="1841" height="721" alt="Screenshot 2026-05-01 081653" src="https://github.com/user-attachments/assets/613bc863-38fe-4f31-8471-bbf68391729b" />

---

### 📥 Downloaded Report
you can view downloaded report above in the file section

[Click here to see the webpage](https://kishankulal987.github.io/iot-dashboard/)

---

## ⚙️ Working Principle

1. Sensors collect real-time data from the machine  
2. ESP32 processes and analyzes data  
3. Data is sent to Firebase using REST API  
4. The GitHub-hosted webpage fetches and displays data  
5. System checks thresholds:
   - Normal → No action  
   - Warning → LED indication  
   - Critical → Relay OFF (machine shutdown)  
6. User can monitor and control system remotely  

---

## 🧠 Technologies Used

- ESP32 (WiFi-enabled microcontroller)  
- Firebase Realtime Database  
- REST API (HTTP)  
- GitHub Pages (Web hosting)  
- Embedded C (Arduino IDE)  

---

## 📷 Prototype

<img width="2000" height="1500" alt="toggle switch (1)" src="https://github.com/user-attachments/assets/bfd1f54d-ac0a-45c2-bbd3-5c2921502a73" />




---

## 🎯 Future Improvements

- 📱 SMS / Email alerts (Twilio integration)  
- 📊 Advanced analytics dashboard  
- 🤖 AI-based predictive maintenance  
- 🔋 Improved power management  

---

## 👨‍💻 Author

**Kishan K Kulal**  


## TEAM MEMBERS
Kishan K Kulal


Kratika V Ulman


Kirthan Shenoy

---
B.Tech Electronics & Communication Engineering  
