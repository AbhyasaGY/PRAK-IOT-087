#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "HOTSPOT-UNPAD"; 
const char* password = "ShidiqGanteng"; 

// MQTT broker credentials
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic = "rc/control";
const char* topic_pompa = "rc/pompa";
const char* topic_strobo = "rc/strobo";
const char* topic_waterlevel = "rc/waterlevel";
const char* topic_battery = "rc/baterailevel";
const char* topic_speed = "rc/speed";
const char* topic_notification = "rc/notification";

// Pin definitions for motor control
const int RPWM = 18;
const int LPWM = 19;
const int ENA = 14;
const int IN3 = 27;
const int IN4 = 26; 

const int Relay1 = 21; // Relay for pompa
const int Relay2 = 22; // Relay for strobo

// Pin definitions for Water level
const int trigPin = 5; 
const int echoPin = 4;

// Pin definitions for backward notfiication (ultrasonic)
const int trigPin2 = 15; // Pin baru untuk sensor kedua
const int echoPin2 = 16; // Pin baru untuk sensor kedua

// Pin for battery level sensor
const int batteryPin = 32; // ADC pin for battery voltage (gunakan GPIO 32)

// Pin for speed sensor (optocoupler)
const int speedSensorPin = 33; // Digital pin untuk sensor kecepatan

// Pin for servo
const int servoPin = 23;

// Variables for sensor data
volatile int pulseCount = 0; // Jumlah pulsa dari sensor
unsigned long lastSpeedMeasure = 0; // Timestamp terakhir pengukuran kecepatan
float speedKmPerHour = 0.0; // Variabel untuk menyimpan kecepatan dalam km/jam
const int intervalSpeedMeasure = 1000; // Interval pengukuran kecepatan (ms)

// Constants for wheel circumference
const float wheelCircumference = 0.28; // Keliling roda dalam meter (misal 50 cm = 0.5 meter)

// Servo control variables
Servo myServo; // Objek Servo
int currentAngle = 90; // Posisi awal servo di tengah (90 derajat)
bool stopCommand = false; // Flag untuk menghentikan gerakan servo

WiFiClient espClient;
PubSubClient client(espClient);

// API endpoints
const String apiBase = "https://api.shidiq.com/"; // Ganti dengan URL API yang sesuai
const String postEndpoint = "post?API-Key=apikeydarilangit"; // Endpoint lengkap untuk POST

void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void sendPost(float speed, float battery, float water_level);
void motorForward();
void motorBackward();
void motorStop();
void steerLeft();
void steerRight();
void stopSteer();
void relayOn(int relay);
void relayOff(int relay);
float getWaterLevel();
float getBatteryLevel();
void IRAM_ATTR countPulse();
float getBackwardDistance();
void moveServoSlowly(int fromAngle, int toAngle, int stepDelay);

void setup() {
  Serial.begin(115200);

  // Set the motor, steering control, and ultrasonic sensor pins as outputs or inputs
  pinMode(ENA, OUTPUT);
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin2, OUTPUT);  
  pinMode(echoPin2, INPUT);

  // Set speed sensor pin as input and attach interrupt
  pinMode(speedSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(speedSensorPin), countPulse, FALLING);

    // Initialize Servo
  myServo.attach(servoPin);
  myServo.write(currentAngle); // Set servo ke posisi awal
  Serial.print("Servo initialized at ");
  Serial.print(currentAngle);
  Serial.println(" degrees.");



  // Initialize WiFi and MQTT connection  
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup complete.");
}

void loop() {
  static unsigned long lastBatteryRead = 0; // Timestamp untuk pembacaan baterai
  unsigned long currentMillis = millis();  // Waktu saat ini

  // Ensure the client is connected to the MQTT broker
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Process incoming MQTT messages

  // Read water level and send to MQTT topic
  float waterPercentage = getWaterLevel();
  String waterPercentageStr = String(waterPercentage, 2); // Mengonversi nilai ke string dengan 2 desimal
  client.publish(topic_waterlevel, waterPercentageStr.c_str()); // Kirim data ke topik MQTT
  Serial.print("Water level percentage: ");
  Serial.println(waterPercentageStr);

  // Read battery level and send to MQTT topic every - minutes
  if (currentMillis - lastBatteryRead >= 100) {
    lastBatteryRead = currentMillis; // Perbarui timestamp
    float sensorBattery = getBatteryLevel();
    String batteryLevelStr = String(sensorBattery, 2); // Convert percentage to string
    if (client.publish(topic_battery, batteryLevelStr.c_str())) {
      Serial.print("Battery level (percentage) published: ");
      Serial.println(batteryLevelStr);
    } else {
      Serial.println("Failed to publish battery level.");
    }
  }

  // Measure speed (in km/h) every 1 second
  if (currentMillis - lastSpeedMeasure >= intervalSpeedMeasure) {
    lastSpeedMeasure = currentMillis; // Perbarui timestamp
    noInterrupts(); // Matikan interrupt sementara
    float rpm = (pulseCount * 60.0) / 5.0; // Hitung RPM (5 per rotasi)
    pulseCount = 0; // Reset jumlah 
    interrupts(); // Aktifkan kembali interrupt

    // Convert RPM to speed in km/h
    speedKmPerHour = (rpm * wheelCircumference * 60) / 1000; // Kecepatan dalam km/jam

    // Publikasikan nilai kecepatan dalam km/jam ke MQTT
    String speedStr = String(speedKmPerHour, 2); // Konversi kecepatan ke string
    if (client.publish(topic_speed, speedStr.c_str())) {
      Serial.print("Speed (km/h) published: ");
      Serial.println(speedStr);
    } else {
      Serial.println("Failed to publish speed (km/h).");
    }
    sendPost(speedKmPerHour, getBatteryLevel(), getWaterLevel()); 
  }

  // Tambahkan delay untuk menghindari penggunaan CPU yang terlalu tinggi
  delay(300000);
}

// Interrupt Service Routine (ISR) untuk menghitung pulsa
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// Function to connect to WiFi
void setupWiFi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Function to handle incoming MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received: ");
  Serial.println(message);

  // Control based on received message
  if (String(topic) == "rc/control") {
    if (message == "forward") {
      motorForward();
      stopSteer();
    } else if (message == "backward") {
      motorBackward();
      stopSteer();
    } else if (message == "stop") {
      motorStop();
      stopSteer();
    } else if (message == "left") {
      steerLeft();
    } else if (message == "right") {
      steerRight();
    } else if (message == "forward_left") {
      motorForward();
      steerLeft();
    } else if (message == "forward_right") {
      motorForward();
      steerRight();
    } else if (message == "backward_left") {
      motorBackward();
      steerLeft();
    } else if (message == "backward_right") {
      motorBackward();
      steerRight();
    } else {
      Serial.println("Unknown command.");
    }
  } else if (String(topic) == "rc/pompa") {
    // Kontrol relay
    if (message == "pompaHidup") {
      relayOn(Relay1);
    } else if (message == "pompaMati") {
      relayOff(Relay1);
    }
    // Kontrol Servo
    else if (message == "kiri") {
      Serial.println("Menggerakkan servo ke kiri (180 derajat)");
      stopCommand = false; // Reset stop command
      moveServoSlowly(currentAngle, 180, 15); // Gerakkan ke kiri (180 derajat)
    } else if (message == "kanan") {
      Serial.println("Menggerakkan servo ke kanan (0 derajat)");
      stopCommand = false; // Reset stop command
      moveServoSlowly(currentAngle, 0, 15); // Gerakkan ke kanan (0 derajat)
    } else if (message == "tengah") {
      Serial.println("Menggerakkan servo ke tengah (90 derajat)");
      stopCommand = false; // Reset stop command
      moveServoSlowly(currentAngle, 90, 15); // Gerakkan ke tengah (90 derajat)
    } else if (message == "stop") {
      Serial.println("Menghentikan gerakan servo di posisi saat ini.");
      stopCommand = true; // Aktifkan perintah untuk menghentikan gerakan
    } else {
      Serial.println("Perintah tidak dikenali untuk rc/pompa. Gunakan 'pompaHidup', 'pompaMati', 'kiri', 'kanan', 'tengah', atau 'stop'.");
    }
  } else if (String(topic) == "rc/strobo") {
    if (message == "stroboHidup") {
      relayOn(Relay2);
    } else if (message == "stroboMati") {
      relayOff(Relay2);
    } else {
      Serial.println("Perintah tidak dikenali untuk rc/strobo. Gunakan 'stroboHidup' atau 'stroboMati'.");
  }
}
}

// Function to reconnect to the MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("PRAKTIKUM_EE_087")) {
      Serial.println("Connected to MQTT broker.");
      client.subscribe(topic);
      client.subscribe(topic_pompa);
      client.subscribe(topic_strobo);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Function to measure water level using ultrasonic sensor
float getWaterLevel() {
  long duration;
  float distance;

  // Trigger the ultrasonic pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the echo duration
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in cm
  distance = (duration * 0.034) / 2;

  // Aturan batas jarak minimal (misalnya 2 cm sebagai batas bawah)
  if (distance < 2.0) {
    distance = 2.0; // Set nilai minimum untuk menghindari pembacaan yang tidak akurat
  }

  float maxTankHeight = 40.0; // Sesuaikan tinggi tangki air Anda

  // Hitung level air berdasarkan jarak
  float waterLevel = maxTankHeight - distance;

  // Pastikan level air tidak melebihi batas (0% - 100%)
  if (waterLevel < 0) waterLevel = 0;
  if (waterLevel > maxTankHeight) waterLevel = maxTankHeight;

  // Hitung persentase air yang tersisa di tangki
  float waterPercentage = (waterLevel / maxTankHeight) * 100;

  // Kembalikan persentase level air
  return waterPercentage;
}


// Function to measure distance while moving backward
float getBackwardDistance() {
  long duration;
  float distance;
  // Trigger the ultrasonic pulse
  digitalWrite(trigPin2, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin2, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin2, LOW);
  // Measure the echo duration
  duration = pulseIn(echoPin2, HIGH);
  // Calculate the distance in cm
  distance = (duration * 0.034) / 2;
  return distance; // Return distance in cm
}

// Function to measure battery level as a percentage
float getBatteryLevel() {
  int rawValue = analogRead(batteryPin); // Read raw ADC value
  float voltage = rawValue * (3.3 / 4095.0); // Convert ADC value to voltage (ESP32 ADC resolution is 12-bit)
  float batteryVoltage = voltage * 5.0; // Adjust based on the voltage divider (assuming divider ratio is 1:5)

  // Define the voltage range for the battery
  const float minVoltage = 3.0; // Minimum voltage (0% battery)
  const float maxVoltage = 12.0; // Maximum voltage (100% battery)

  // Calculate the battery percentage
  float batteryPercentage = ((batteryVoltage - minVoltage) / (maxVoltage - minVoltage)) * 100;

  // Constrain the percentage between 0% and 100%
  if (batteryPercentage < 0) batteryPercentage = 0;
  if (batteryPercentage > 100) batteryPercentage = 100;

  return batteryPercentage;
}

// Motor control functions
void motorForward() {
  analogWrite(RPWM, 100);
  analogWrite(LPWM, 0);
  Serial.println("Driving forward...");
}

void motorBackward() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 100);
  Serial.println("Driving backward...");

  // Measure backward distance
  float distance = getBackwardDistance();
  if (distance <= 50.0) { // Jika jarak kurang dari atau sama dengan 50 cm
    client.publish(topic_notification, "WARNING");
    motorStop(); // Berhenti otomatis untuk menghindari tabrakan
    } else { // Jika jarak lebih dari atau sama dengan 50 cm
    client.publish(topic_notification, "NO WARNING");
  }
}

void motorStop() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  Serial.println("Stopping...");
}

void steerLeft() {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(ENA, HIGH);
  Serial.println("Steering left...");
}

void steerRight() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  digitalWrite(ENA, HIGH);
  Serial.println("Steering right...");
}

void stopSteer() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void relayOn(int relay) {
  digitalWrite(relay, HIGH);
  Serial.print("Relay ");
  Serial.print(relay);
  Serial.println(" ON.");
}

void relayOff(int relay) {
  digitalWrite(relay, LOW);
  Serial.print("Relay ");
  Serial.print(relay);
  Serial.println(" OFF.");
}

// Fungsi untuk menggerakkan servo secara perlahan
void moveServoSlowly(int fromAngle, int toAngle, int stepDelay) {
  if (fromAngle < toAngle) {
    for (int angle = fromAngle; angle <= toAngle; angle++) {
      if (stopCommand) {
        Serial.println("Servo movement stopped.");
        client.publish(topic_notification, "Servo movement stopped.");
        break; // Jika ada perintah stop, hentikan gerakan
      }
      myServo.write(angle);
      currentAngle = angle;
      delay(stepDelay);
    }
  } else {
    for (int angle = fromAngle; angle >= toAngle; angle--) {
      if (stopCommand) {
        Serial.println("Servo movement stopped.");
        client.publish(topic_notification, "Servo movement stopped.");
        break; // Jika ada perintah stop, hentikan gerakan
      }
      myServo.write(angle);
      currentAngle = angle;
      delay(stepDelay);
    }
  }
  Serial.print("Servo moved to ");
  Serial.print(currentAngle);
  Serial.println(" degrees.");
  String servoStatus = "Servo moved to " + String(currentAngle) + " degrees.";
  client.publish(topic_notification, servoStatus.c_str());
}

// Function to send data via POST request
void sendPost(float speed, float battery, float water_level) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Menggabungkan URL dengan data
    String url = apiBase + postEndpoint + "?speed=" + String(speed) +
                 "&battery=" + String(battery) +
                 "&water_level=" + String(water_level);
    
    http.begin(url); // Memulai koneksi HTTP ke URL

    // Mengirim POST request tanpa body (hanya menggunakan query string)
    int httpResponseCode = http.POST("");

    if (httpResponseCode > 0) {
      // Respons berhasil diterima
      Serial.print("POST Response: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString());
    } else {
      // Respons gagal
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end(); // Menutup koneksi HTTP
  } else {
    Serial.println("WiFi not connected. Unable to send data.");
  }
}