#include <PubSubClient.h>
#include <WiFi.h>
const char* ssid = "R";
const char* pass = "24062003";
const char* server = "broker.hivemq.com";

WiFiClient espclient;
PubSubClient client(espclient);

void Connect(){
  WiFi.begin(ssid,pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connect wifi");
  connectMQTT();
  client.subscribe("Ping");
  client.setCallback(GetMessage);
}

void connectMQTT(){
  client.setServer(server,1883);
  client.connect("PRAKTIKUM_EE_087");  
  while(!client.connected()){
    if(  client.connect("PRAKTIKUM_EE_087")){
      Serial.println("Berhasil");
    }else{
      Serial.println(client.state());
    }
  }
  client.publish("Ping","TEST");
}

void GetMessage(char* topic, byte* message , unsigned int Length){
  Serial.println(topic);
  for (int i = 0 ; i < Length; i++){
    Serial.print((char)message[i]);
  }
}

void setup() {
  Serial.begin(9600);
  Connect ();
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();
}
