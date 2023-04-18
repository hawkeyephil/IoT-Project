// Matthew Merritt, Michael Merritt, Philip Caldarella
// CSC375
// Final Part 2: M5 Core2 Receiver

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <M5Core2.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// WiFi data
String ssid;
String password = "123456789"; // assume this is the password
bool foundWifi = false;

// MQTT server data
const char* mqttIP = "192.168.3.1"; // this is given in class
int mqttPort = 1883;
const int timeout = 10000;

// Screen constants
const int MAX_X = 320;
const int MAX_Y = 240;
const int SPRITE_COLOR_DEPTH = 8;

// Grid constants
const int BOXES_PER_ROW = 3;
const int ROWS = 2;
const int BOX_WIDTH = MAX_X / BOXES_PER_ROW;
const int BOX_HEIGHT = MAX_Y / ROWS;

// Extra colors
const uint16_t LIGHT_GRAY = 0xC638;
const uint16_t DARK_GRAY = 0x4208;

// Character constants
const int LETTER_SIZE = 6;
const int FONT_SIZE = 2;

// Box data
const int ITEMS = 6;
int currentConnections = 0;
String macs[ITEMS];
Ticker activity[ITEMS];
bool states[ITEMS];

// Sprite data
TFT_eSprite img = TFT_eSprite(&M5.Lcd);
TFT_eSprite imgLocked = TFT_eSprite(&M5.Lcd);

// Clients for MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Zones for touchscreen
//Zone buttons[6];

// BLE beacon data
//Scan duration in seconds
int scanTime = 1; 
//Scanner
BLEScan* pBLEScan;
//Beacon name that unlocks device 
String beaconName = "Group3Beacon";
//Beacon name of identified devices 
String deviceName; 
//True if beacon is close enough
bool inRange = false;
String temp;
bool foundDevice;

// Helper function to build the zones for the given screen size
//void createZones() {
//  for (int i = 0; i < ITEMS; ++i) {
//    // grid location calculations
//    int x = i % BOXES_PER_ROW * BOX_WIDTH;
//    int y = i / BOXES_PER_ROW * BOX_HEIGHT;
//    buttons[i] = Zone (x, y, BOX_WIDTH, BOX_HEIGHT);
//  }
//}

// Helper function to find the index of a given MAC address in the list of addresses
int findMacIndex(String mac) {
  // linear search
  for (int i = 0; i < ITEMS; ++i) {
    if (macs[i] == mac) {
      return i;
    }
  }
  return -1;
}

// Ticker callback to disable boxes when not in use
void disableBox(int index) {
  drawBox(index, macs[index], 0, false);
}

// Function for drawing a box to the screen to show the 6 distance sensors
void drawBox(int index, String mac, int dist, bool active) {
  // grid location calculations
  int x = index % BOXES_PER_ROW * BOX_WIDTH;
  int y = index / BOXES_PER_ROW * BOX_HEIGHT;

  // color selection, assumes dist is in cm
  uint16_t color;
  if (!active) {
    color = LIGHT_GRAY;
    states[index] = false;
  }
  else {
    if (dist > 100) {
      color = GREEN;
    }
    else if (dist > 50) {
      color = YELLOW;
    }
    else {
      color = RED;
    }
    states[index] = true;
  }

  // drawing to screen image
  img.fillRect(x, y, BOX_WIDTH, BOX_HEIGHT, color);
  img.drawRect(x, y, BOX_WIDTH, BOX_HEIGHT, BLACK);
  // moving text cursor to center of box (then one char to the left and half a char up)
  img.setCursor(x + BOX_WIDTH/2 - (FONT_SIZE * LETTER_SIZE), y + BOX_HEIGHT/2 - (FONT_SIZE * LETTER_SIZE)/2);
  img.setTextColor(DARK_GRAY, color);
  img.setTextSize(FONT_SIZE);
  // displaying last 2 digits of mac address
  img.print(mac.substring(mac.length() - 2));
  
  // Update screen
  if (!inRange || !foundDevice) {
    imgLocked.pushSprite(0, 0);
  }
  else {
    img.pushSprite(0, 0);
  }
  M5.update();

  // Starting the timer to disable the box if it doesn't hear back in time
  // the callback has to take no parameters and return void, so we had to wrap it in a lambda expression
//  activity[index].attach_ms(timeout, [index]{disableBox(index);});
  // issue caused by the lambda: since we have to capture index, it can't be converted back to a function pointer
  // as such, we used this as an alternative
  if (active) {
    switch (index) {
      case 0:
        activity[index].once_ms(timeout, []{disableBox(0);});
        break;
      case 1:
        activity[index].once_ms(timeout, []{disableBox(1);});
        break;
      case 2:
        activity[index].once_ms(timeout, []{disableBox(2);});
        break;
      case 3:
        activity[index].once_ms(timeout, []{disableBox(3);});
        break;
      case 4:
        activity[index].once_ms(timeout, []{disableBox(4);});
        break;
      case 5:
        activity[index].once_ms(timeout, []{disableBox(5);});
        break;
    }
  }
  
}

// Function that handles connecting to MQTT server
void reconnect() {
  // keep trying connection until success
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Group3Core2")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("CSC375/dist");
//      client.subscribe("CSC375/control");
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
  if (String(topic) == "CSC375/dist") {
    Serial.println("Distance message received.");

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, messageString);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else {
      String mac = doc["MAC"];
      int dist = doc["dist"];

      Serial.print("MAC: ");
      Serial.print(mac);
      Serial.printf(", Distance: %i\n", dist);

      int index = findMacIndex(mac);
      if (index != -1) {
        Serial.println("Device already in MAC list.");
        drawBox(index, mac, dist, true);
      }
      else {
        // add device to list if there is still room
        if (currentConnections < ITEMS) {
          macs[currentConnections] = mac;
          drawBox(currentConnections, mac, dist, true);
          currentConnections++;
          Serial.println("Device added to MAC list.");
        }
      }
    }
  }
  else {
    Serial.println("Message topic did not match known topics.");
  }
}

// Callback function for touchscreen events
//void eventDisplay(Event &e) {
//  // Only run when user releases press (only run once per press)
//  if (e.typeName() == "E_RELEASE") {
//    // Identify release coordinates and turn them into a point 
//    int x = e.to.x;
//    int y = e.to.y;
//    Point pressedLocation(x, y);
//    // Serial.println(pressedLocation);
//
//    // finding the pressed region and sending control update message
//    // assumption: if the box is inactive, the control is off
//    for (int i = 0; i < ITEMS; ++i) {
//      if (buttons[i].contains(pressedLocation)) {
//        // serialize info as json to publish
//        StaticJsonDocument<64> msg_json;
//        msg_json["MAC"] = macs[i];
//        // control value currently depends on the devices state
//        msg_json["control"] = states[i] ? 0 : 1;
//        String msg;
//        serializeJsonPretty(msg_json, msg);
//        client.publish("CSC375/control", msg.c_str()); 
//      }
//    }
//  }
//}

// Function to contain all the code for the BLE beacon check all in one place
void loopBLE() {
  //Stores scan results 
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
//  Serial.print("Devices found: ");
  //Prints number of devices detected
//  Serial.println(foundDevices.getCount());
  int numDevices = foundDevices.getCount();
  int i; 
  foundDevice = false; // need to re-find device
  //Iterate through the detected devices and if the beacon is one of them and it has a strong enough RSSI unlock the device 
  for(i = 0; i < numDevices; i++) {
    //printf("value of i: %d\n", i);
    //Serial.printf("Advertised Device: %s \n", foundDevices.getDevice(i).getName().c_str());
    temp = foundDevices.getDevice(i).getName().c_str();
    if(temp == beaconName) {
      foundDevice = true;
      //Already in range 
      if(inRange == true) {
        //RSSI of -70
        if(foundDevices.getDevice(i).getRSSI() > -70) {
          inRange = true;
        }
        else {
          inRange = false;
          Serial.println("Bluetooth signal lost, bring device closer again.");
        }
      }
      //Not in range 
      else {
        //RSSI of -50
        if(foundDevices.getDevice(i).getRSSI() > -50) {
          inRange = true;
          Serial.println("Found bluetooth signal.");
        }
        else {
          inRange = false;
        }
      }
    }
  }

  if (!inRange || !foundDevice) {
    imgLocked.pushSprite(0, 0);
  }
  else {
    img.pushSprite(0, 0);
  }
  M5.update();
  
//  Serial.println("Scan done!");
  //Clear the scan results so that it can be done again 
  pBLEScan->clearResults();  
}

void setup() {
  Serial.begin(115200);

  M5.begin();
  
  img.setColorDepth(SPRITE_COLOR_DEPTH);
  img.createSprite(MAX_X, MAX_Y);
  
  while (!Serial) continue; // wait for serial to start up

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
          Serial.print("Connecting... ");
          do {
            Serial.print("... ");
            WiFi.disconnect(true);
            WiFi.begin(ssid.c_str(), password.c_str());
            WiFi.waitForConnectResult(5000);
          } while (WiFi.status() != WL_CONNECTED);
          Serial.println("Connected to WiFi");
      
          // output address
          Serial.print("Device IP: ");
          Serial.println(WiFi.localIP());

          foundWifi = true; // end WiFi search
  
          // Part 2: Configure MQTT server connection
          client.setServer(mqttIP, mqttPort);
          client.setCallback(callback);
  
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

  // Part 6
  //Creates sprite 
  imgLocked.setColorDepth(SPRITE_COLOR_DEPTH);
  imgLocked.createSprite(MAX_X, MAX_Y);
  imgLocked.fillSprite(BLACK); 
  imgLocked.setTextSize(4);
  imgLocked.drawString("LOCKED", 70, 90, 2);

  //Initializes BLE Device
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  // Extra credit: Setting up touch screen listeners
//  createZones();
//  M5.Buttons.addHandler(eventDisplay, E_ALL - E_MOVE);
}

void loop() {
  // Part 2: Connecting to the MQTT server
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Part 6: BLE Beacon
  loopBLE();

  // Extra credit: Sending control messages
//  M5.update();
}
