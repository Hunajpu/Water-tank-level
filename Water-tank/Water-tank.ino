#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
//#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

//ADC_MODE(ADC_VCC); // TODO check if applays to ESP32

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

#define LED_BUILTIN 2
// Ultrasonic sensor
long duration, cm;
#define ECHO 4
#define TRIG 15

unsigned long lastMillis = 0;
time_t now;
time_t nowish = 1510592825;

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    //delay(500);
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(cacert);
  net.setCertificate(client_cert);
  net.setPrivateKey(privkey);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(MQTT_HOST, 8883);

  // Create a message handler
  //client.setCallback(messageHandler);


  Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    //delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }


  // Subscribe to a topic
  //client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println(" AWS IoT Connected!");
  //digitalWrite(LED_BUILTIN, HIGH);

}

unsigned long previousMillis = 0;
const long interval = 5000;

void publishMessage()
{
  StaticJsonDocument<200> doc;
  // Getting sensor data
  Serial.println(F("\nReading ultrasonic sensor"));
  duration, cm = 0;
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(2);
  digitalWrite(TRIG, LOW);
  duration = pulseIn(ECHO, HIGH);
  cm = duration / 29 / 2;
  Serial.print(cm);
  Serial.print("cm");
  Serial.println();

  doc["time"] = millis();
  doc["water-level"] = cm;
  //doc["voltage"]=ESP.getVcc();
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup()
{
  Serial.begin(115200);
  // Delay to start the serial monitor comment if your not testing
  //delay(1000);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  //pinMode(LED_BUILTIN, OUTPUT); 

  connectAWS();

  // Read sensor and publish to AWS
  publishMessage();
  
  // Deep Sleep 10 seconds
  deepSleep();
  //digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
}

void deepSleep() {
  Serial.println(F("\n          Timed Deep Sleep, wake in 10 seconds"));
  client.disconnect(); // Disconnect client from AWS
  WiFi.mode(WIFI_OFF);  // you must turn the modem off; using disconnect won't work
  printMillis();  // show millis() across sleep, including Serial.flush()

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}


void printMillis() {
  Serial.print(F("millis() = "));  // show that millis() isn't correct across most Sleep modes
  Serial.println(millis());
  Serial.flush();  // needs a Serial.flush() else it may not print the whole message before sleeping
}
