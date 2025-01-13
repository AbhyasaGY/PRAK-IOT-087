#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "ICT-LAB WORKSPACE";
const char* password = "ICTLAB2024";

// MQTT broker credentials
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic = "rc/control";

// Pin definitions for motor control
const int RPWM = 18;
const int LPWM = 19;
const int ENA = 14;
const int IN3 = 27;
const int IN4 = 26;
const int Relay = 21;

// API endpoints
const String apiBase = "http://192.168.1.226/api_project/";
const String postEndpoint = "post.php";

// Variables for sensor data
float sensorSpeed = 50.5;
float sensorBattery = 80.2;

WiFiClient espClient;
PubSubClient client(espClient);

// Function Prototypes
void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void sendPost(float speed, float battery);
void motorForward();
void motorBackward();
void motorStop();
void steerLeft();
void steerRight();
void stopSteer();
void relayOn();
void relayOff();

void setup() {
  Serial.begin(115200);

  // Set the motor and steering control pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(Relay, OUTPUT);

  // Initialize WiFi and MQTT connection
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup complete.");
}

void loop() {
  // Ensure the client is connected to the MQTT broker
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Process incoming MQTT messages
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
  } else if (message == "rlyon") {
    relayOn();
  } else if (message == "rlyoff") {
    relayOff();
  } else if (message == "get_data") {
    Serial.println("Sending data in response to 'get_data' command.");
    sendPost(sensorSpeed, sensorBattery);
    client.publish("rc/response", "Data sent successfully.");
  } else {
    Serial.println("Unknown command.");
  }
}

// Function to reconnect to the MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("PRAKTIKUM_EE_087")) {
      Serial.println("Connected to MQTT broker.");
      client.subscribe(topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Function to send POST request
void sendPost(float speed, float battery) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = apiBase + postEndpoint + "?speed=" + String(speed) + "&battery=" + String(battery);
    http.begin(url);

    int httpResponseCode = http.POST("");
    if (httpResponseCode > 0) {
      Serial.print("POST Response: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString());
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected. Unable to send data.");
  }
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
}

void motorStop() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  stopSteer();
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

void relayOn() {
  digitalWrite(Relay, HIGH);
}

void relayOff() {
  digitalWrite(Relay, LOW);
}