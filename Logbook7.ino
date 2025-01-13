#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "HOTSPOT-UNPAD";          // Replace with your WiFi SSID
const char* password = "ShidiqGanteng";      // Replace with your WiFi password

// MQTT broker credentials
const char* mqtt_server = "broker.hivemq.com"; // HiveMQ public broker
const int mqtt_port = 1883;                    // Default MQTT port
const char* topic = "rc/control";             // Topic to subscribe

// Pin Definitions
const int ENA = 5;   // PWM pin for speed control

// Drive motor pins (Ban)
const int IN1 = 17;  // Direction control pin 1
const int IN2 = 16;  // Direction control pin 2

// Steering motor pins (Stir)
const int IN3 = 4;  // Direction control pin for left/right
const int IN4 = 2;  // Direction control pin for left/right

WiFiClient espClient;
PubSubClient client(espClient);

// Function Prototypes
void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void motorForward(int speed);
void motorBackward(int speed);
void motorStop();
void steerLeft(int duration = 1000);  // Time-based control with default duration
void steerRight(int duration = 1000); // Time-based control with default duration
void steerStraight();

void setup() {
  Serial.begin(115200);

  // Set the control pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Initialize WiFi and MQTT connection
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup complete.");
}

void loop() {
  // Ensure the client is connected to the broker
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Process incoming MQTT messages
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
  if (message == "forward") {
    motorForward(255);
    steerStraight();
  } else if (message == "backward") {
    motorBackward(255);
    steerStraight();
  } else if (message == "stop") {
    motorStop();
    steerStraight();
  } else if (message == "left") {
    steerLeft(1000);  // Steer left for 1 second
  } else if (message == "right") {
    steerRight(1000); // Steer right for 1 second
  } else if (message == "forward_left") {
    motorForward(255);
    steerLeft(1000);  // Steer left while moving forward
  } else if (message == "forward_right") {
    motorForward(255);
    steerRight(1000); // Steer right while moving forward
  } else if (message == "backward_left") {
    motorBackward(255);
    steerLeft(1000);  // Steer left while moving backward
  } else if (message == "backward_right") {
    motorBackward(255);
    steerRight(1000); // Steer right while moving backward
  } else {
    Serial.println("Unknown command.");
  }
}

// Function to reconnect to the MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ShidiqRCKontrol")) {
      Serial.println("Connected to MQTT broker.");
      client.subscribe(topic); // Subscribe to the control topic
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Function to rotate drive motor forward
void motorForward(int speed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, speed); // Set motor speed
  Serial.println("Driving forward...");
}

// Function to rotate drive motor backward
void motorBackward(int speed) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, speed); // Set motor speed
  Serial.println("Driving backward...");
}

// Function to stop the drive motor
void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0); // Stop sending PWM signal
  Serial.println("Stopping...");
}

// Function to steer left (with duration)
void steerLeft(int duration) {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  Serial.println("Steering left...");
}

// Function to steer right (with duration)
void steerRight(int duration) {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// Function to steer straight
void steerStraight() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

}