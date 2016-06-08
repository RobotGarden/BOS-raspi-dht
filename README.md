# BOS RASPI DHT

Pure C++ program to read a DHT sensor and return results over the network as part of a home automation system.

## MQTT Version
This branch contains a version which has been written to broadcast results over MQTT.

## History
This program is based on a DHT sensor reader published by Adafruit industries which I rewrote to publish over ROS. It has now been rewritten again to publish on MQTT instead because the existing state of ROS 1.x is too cumbersome. 
