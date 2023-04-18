// Matthew Merritt, Michael Merritt, Philip Caldarella
// CSC375
// Final Part 2: FireBeetle with Distance Sensor

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// WiFi data
String ssid;
String password = "123456789"; // assume this is the password
bool foundWifi = false;

// MQTT server data
const char* mqttIP = "192.168.3.1"; // this is given in class
int mqttPort = 1883;

// Clients for MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// publish ticker
Ticker publishTicker;

// status (changed by messages on CSC375/control)
int active = 1;

// LED Pin
const int ledPin = 2;

// Variables for distance (from https://wiki.dfrobot.com/URM09_Ultrasonic_Sensor_Gravity_Trig_SKU_SEN0388)
// The ultrasonic velocity (cm/us) compensated by temperature
#define VELOCITY_TEMP(temp) ( ( 331.5 + 0.6 * (float)( temp ) ) * 100 / 1000000.0 ) 
int16_t sensorPin = D11;
uint16_t distance;
uint32_t pulseWidthUs;

// Function to find distance
void publishDistance() {
  int16_t dist, temp;
  pinMode(sensorPin, OUTPUT);
  digitalWrite(sensorPin, LOW);

  digitalWrite(sensorPin, HIGH); // Set the trig pin High
  delayMicroseconds(10);
  digitalWrite(sensorPin, LOW); // Set the trig pin Low

  pinMode(sensorPin, INPUT); // Set the pin to input mode
  // Detect the high level time on the echo pin, the output high level time 
  // represents the ultrasonic flight time (unit: us)
  pulseWidthUs = pulseIn(sensorPin, HIGH);

  // The distance can be calculated according to the flight time of ultrasonic wave,
  // and the ultrasonic sound speed can be compensated according to the actual ambient temperature
  distance = pulseWidthUs * VELOCITY_TEMP(20) / 2.0; 

  // serialize info as json to publish
  StaticJsonDocument<64> msg_json;
  msg_json["dist"] = floor(distance);
  msg_json["MAC"] = WiFi.macAddress();
  String msg;
  serializeJsonPretty(msg_json, msg);

  // publish if the control value is set to 1 (the device is "active")
  if(active == 1) {
    client.publish("CSC375/dist", msg.c_str()); 
  }
}

// Function that handles connecting to MQTT server
void reconnect() {
  // keep trying connection until success
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Group3FireBeetle")) {
      Serial.println("connected");
      // Subscribe
      // client.subscribe("CSC375/dist");
      client.subscribe("CSC375/control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Callback function that receives messages from the MQTT server
void callback(char* topic, byte* message, unsigned int length) {
  // constructing message from bytes array
  String messageString;
  for (int i = 0; i < length; i++) {
    messageString += (char)message[i];
  }

  // handling behavior based on topic given
  if (String(topic) == "CSC375/control") {
    Serial.println("Control message recieved.");

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, messageString);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else {
      String mac = doc["MAC"];
      int control = doc["control"];

      Serial.print("MAC: ");
      Serial.print(mac);
      Serial.printf(", Control: %i\n", control);

      // if the control message is for this device, change active to be the new value
      if(mac == WiFi.macAddress()) {
        active = control;
        Serial.print("Device control set to ");
        Serial.println(active);
      }
    }
  } 
  else {
    Serial.println("Message topic did not match known topics.");
  }
}

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Part 1: Connect to network
  // Scan for networks
  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0) {
    Serial.println("No networks found");
  } 
  else {
    while (!foundWifi) {
      // find the Omega SSID
      for (int i = 0; i < numNetworks; ++i) {
        ssid = WiFi.SSID(i);
        String omega = "Omega";
  
        // should be 0 if the WiFi SSID starts with Omega
        int compare = strncmp(ssid.c_str(), omega.c_str(), 5);
        Serial.printf("Network: ");
        Serial.println(ssid);
  
        // connect to WiFi with Omega
        if (!compare) {   
          Serial.printf("Omega network found, connecting to %s\n", ssid);
          // wait for connection to be successful
          // now with disconnecting if not connected
//          Serial.print("Connecting... ");
//          WiFi.begin(ssid.c_str(), password.c_str()); // TODO: remove
//          do {
//            Serial.print("... ");
//            WiFi.disconnect(true);
//            WiFi.begin(ssid.c_str(), password.c_str());
//            WiFi.waitForConnectResult(1000);
//            delay(2000); // TODO: fix
//          } while (WiFi.status() != WL_CONNECTED);
          do {
            Serial.println("Connecting...");
            WiFi.disconnect();
            WiFi.begin(ssid.c_str(), password.c_str());
            delay(5000);
          } while (WiFi.status() != WL_CONNECTED);
          Serial.println("Connected to WiFi");
      
          // output address
          Serial.print("Device IP: ");
          Serial.println(WiFi.localIP());

          foundWifi = true; // end WiFi search
  
          // Part 2: Configure MQTT server connection
          client.setServer(mqttIP, mqttPort);
          client.setCallback(callback);

          // Publishing distances every 2000ms
          publishTicker.attach_ms(2000, publishDistance);

          // LED
          pinMode(ledPin, OUTPUT);
  
          // break at end, in case of more Omega networks
          break;
        }
      }
      // scan again if no connection made
      if (!foundWifi) {
        Serial.println("Couldn't find a suitable network, attempting to scan again.");
        delay(100);
        int numNetworks = WiFi.scanNetworks();
      }
    }
  }
}

void loop() {
  // Part 2: Connecting to the MQTT server
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // turn LED on or off based on control
  if(active == 1) {
    digitalWrite(ledPin, HIGH);        
  }
  else {
    digitalWrite(ledPin, LOW); 
  }
}
