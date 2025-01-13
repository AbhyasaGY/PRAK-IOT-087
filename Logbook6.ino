#include <WiFi.h>
#include <PubSubClient.h>

// WiFi and MQTT Settings
const char* ssid = "HOTSPOT-UNPAD";
const char* pass = "ShidiqGanteng";
const char* server = "broker.hivemq.com";
const char* mqtt_client_id = "PraktikumIoT_MotorClient";

// MQTT topic for motor control
const char* topic_motor_control = "rc/control";

WiFiClient espclient;
PubSubClient client(espclient);

// Motor A Pins
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 14;

// PWM properties
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;

void Connect() {
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Set up MQTT server and connect
  client.setServer(server, 1883);
  connectMQTT();
  client.subscribe(topic_motor_control); // Subscribe to the motor control topic
  client.setCallback(GetMessage);        // Set callback for MQTT messages
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected to MQTT");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

void GetMessage(char* topic, byte* message, unsigned int length) {
  String receivedMessage;
  for (int i = 0; i < length; i++) {
    receivedMessage += (char)message[i];
  }

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(receivedMessage);

  // Control motor based on received message
  if (receivedMessage == "w") {
    // Move forward
    Serial.println("Moving Forward");
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
    ledcWrite(enable1Pin, dutyCycle);
  } 
  else if (receivedMessage == "s") {
    // Move backward
    Serial.println("Moving Backward");
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
    ledcWrite(enable1Pin, dutyCycle);
  } 
  else if (receivedMessage == "x") {
    // Stop the motor
    Serial.println("Motor stopped");
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    ledcWrite(enable1Pin, 0); // Stop PWM signal
  }
}

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);

  // Set motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  
  // Configure LEDC PWM
  ledcAttachChannel(enable1Pin, freq, resolution, pwmChannel);

  // Connect to WiFi and MQTT
  Connect();
}

void loop() {
  if (!client.connected()) {
    connectMQTT(); // Reconnect if connection is lost
  }
  client.loop(); // Ensure MQTT client remains active
}