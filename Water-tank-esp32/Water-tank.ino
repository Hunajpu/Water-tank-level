// Se cambio el esp8266 por un esp32

//#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <coredecls.h>         // crc32()
#include <PolledTimeout.h>
#include <include/WiFiState.h>
#include "secrets.h"

#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
//#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

ADC_MODE(ADC_VCC);

WiFiClientSecure net;

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);


PubSubClient client(net);

// Ultrasonic sensor
long duration, cm;
#define ECHO 2
#define TRIG 0

unsigned long lastMillis = 0;
time_t now;
time_t nowish = 1510592825;

void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void connectAWS()
{
  
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  NTPConnect();

  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);
  //client.setCallback(messageReceived);
  

  Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }


  // Subscribe to a topic
  //client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println(" AWS IoT Connected!");

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
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  connectAWS();
}

void loop()
{
  now = time(nullptr);
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 5000)
    {
      lastMillis = millis();
      publishMessage();
    }
  }
  // Deep Sleep 10 seconds
  lightSleep();
}

void lightSleep() {
  Serial.println(F("\n          Timed Light Sleep, wake in 10 seconds"));
  client.disconnect(); // Disconnect client from AWS
  WiFi.mode(WIFI_OFF);  // you must turn the modem off; using disconnect won't work
  readVoltage();  // read internal VCC
  printMillis();  // show millis() across sleep, including Serial.flush()
  extern os_timer_t *timer_list;
  timer_list = nullptr;  // stop (but don't disable) the 4 OS timers
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  // only LOLEVEL or HILEVEL interrupts work, no edge, that's an SDK or CPU limitation
  wifi_fpm_set_wakeup_cb(wakeupCallback); // set wakeup callback
  // the callback is optional, but without it the modem will wake in 10 seconds then delay(10 seconds)
  // with the callback the sleep time is only 10 seconds total, no extra delay() afterward
  wifi_fpm_open();
  wifi_fpm_do_sleep(10E6);  // Sleep range = 10000 ~ 268,435,454 uS (0xFFFFFFE, 2^28-1)
  delay(10e3 + 1); // delay needs to be 1 mS longer than sleep or it only goes into Modem Sleep
  Serial.println(F("Woke up!"));  // the interrupt callback hits before this is executed
}

void readVoltage() { // read internal VCC
  float volts = ESP.getVcc();
  Serial.printf("The internal VCC reads %1.2f volts\n", volts / 1000);
}

void printMillis() {
  Serial.print(F("millis() = "));  // show that millis() isn't correct across most Sleep modes
  Serial.println(millis());
  Serial.flush();  // needs a Serial.flush() else it may not print the whole message before sleeping
}
