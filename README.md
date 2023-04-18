# IoT-Project
Various Embedded Devices utilized to create a sensor system which displays captured data. 
These devices were programmed with the Arduino IDE using C. 
A Mosquitto MQTT Server was utilized to enable client devices to publish and subscribe.

An MQTT Server was runs so that the various embedded devices could publish and subscribe to sensor data. 
Core2 employed to display captured data by subscribing to the desired sensor messages. The device also could enable/disable the Firebeetle2's publishing when data collection was undesired. 
Firebeetle1 is used as an access point station for devices to connect to the network. 
Firebeetle2 contains the sensor array which collects and publishes data intermittently to the MQTT server hosted on the local network. (Multiple Firebeetle2's could be used to scale the data collection) 
Firebeetle3 is used as a BLE Beacon. Utilizing RSSI strength, if the beacon reaches a certain proximity the Core2 display will unlock. The principle of hysteresis is used to add a clean transition between locked and unlocked states.
