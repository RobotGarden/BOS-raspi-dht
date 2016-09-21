//  ROS Building Operating System digital temperature and humidity
//  sensor driver based on:
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//


// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <bcm2835.h>
#include <unistd.h>
#include <std::string>

#include <mosquittopp.h>

#define MAXTIMINGS 100

//#define DEBUG

#define DHT11 11
#define DHT22 22
#define AM2302 22

class BosDhtNode : public mosqpp::mosquittopp
{
public:
  BosDhtNode(const int sensorType, const int dhtPin, const std::string& topicStem, const int sampleIntervalSeconds=30, const char* id=NULL, const char* host="127.0.0.1", const int port=1883, const int keepAlive=120);
  ~BosDhtNode();
  
  void on_connect(int rc);
  void on_disconnect(int rc);
  void on_publish(int mid);
  
  int run();
  
  typedef enum {
    initalized,
    connected,
    failedToConnect,
    disconnected,
    sampling,
    publishTmp,
    publishHum,
    waiting,
  } BDNState;

private:
  void publishSample(const std::string& topic, const float sample);
  int  readDHT();
  
  BDNState state;
  const int sensorType;
  const int type;
  const int pin;
  int msgCount;
  const std::string temperatureTopic;
  const std::string humidityTopic;
  float temperatureSample;
  float humiditySample;
  struct timeval lastSampleTime;
};

BosDhtNode::BosDhtNode(const int sensorType, const int dhtPin, const std::string& topicStem, const int sampleIntervalSeconds, const char* id, const char* host, const int port, const int keepAlive) :
  mosquittopp(id),
  state(initalized),
  type(sensorType),
  pin(dhtPin),
  interval(sampleIntervalSeconds),
  msgCount(0),
  temperatureTopic(topicStem + std::string("/temperature")),
  humidityTopic(topicStemp + std::string("/humidity")) {
  mosqpp::lib_init();
  connect_async(host, port, keepAlive);
  //loop_start();
}

BosDhtNode::~BosDhtNode() {
  //loop_stop();
  mosqpp::lib_cleanup();
}

void BosDhtNode::on_connect(int rc)
{
  if (rc == 0) {
    printf("Connected to broker\r\n");
    state = connected;
  }
  else {
    printf("Couldn't connect to broker: %d\r\n", rc);
    state = failedToConnect;
  }
}

void BosDhtNode::on_disconnect(int rc)
{
  state = disconnected;
  printf("Disconnected from broker: %d\r\n", rc);
}

void BosDhtNode::on_publish(int mid)
{
  if (mid != msgCount) {
    printf("Unexpected published callback for mid %d while in state %d\r\n", mid, state);
  }
  else if (state == publishTmp) {
    state = publishHum;
    msgCount++;
    publishSample(humidityTopic, humiditySample);
  }
  else if (state == publishHum) {
    state = waiting;
    msgCount++;
  }
  else {
    printf("Unexpected state when receiving published callback: %d, mid=%d\r\n", state, mid);
    state = waiting;
    msgCount++;
  }
}

int BosDhtNode::run()
{
  while (true) {
    loop();
    switch (state) {
      case initalized:
        continue;
      case connected:
        state = sampling;
        continue;
      case failedToConnect:
      case disconnected:
        return -1;
      case sampling:
      {
        if (readDHT()) {
          gettimeofday(&lastSampleTime, NULL);
          state = publishTmp;
          publishSample(temperatureTopic, temperatureSample);
        }
        continue;
      }
      case publishTmp:
      case publishHum:
        continue;
      case waiting:
      {
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > (lastSampleTime.tv_sec + interval)) state = sampling;
        continue;
      }
    }
  }
}

void BosDhtNode::publishSample(const std::string& topic, const float sample)
{
  const size_t maxDataLen = 1024;
  char jsonData[maxDataLen];
  int jsonLen = snprintnf(jsonData, maxDataLen, "%f", sample);
  publish(&msgCount, topic.c_str(), jsonLen, jsonData, 0, true);
}

int BosDhtNode::readDHT() {
  XXX Reimport adafruit library
}


int main(int argc, char **argv) {

  if (!bcm2835_init()) return 1;

  int sensorType = -1;
  int dhtPin = -1;
  char* topicStem = "dht";
  int sampleInterval = 30;
  char* id = argv[0];
  char* host = "127.0.0.1";
  int port = 1883;
  int keepAlive = 120;
  



  if (argc < 3) {
    printf("usage: %s {11|22|2302} GPIOpin# [ROS Parameters]\n", argv[0]);
    printf("example: %s 2302 4 - Read from an AM2302 connected to GPIO #4\n", argv[0]);
    return 2;
  }

  

  int type = 0;
  if (strcmp(argv[1], "11") == 0) type = DHT11;
  if (strcmp(argv[1], "22") == 0) type = DHT22;
  if (strcmp(argv[1], "2302") == 0) type = AM2302;
  if (type == 0) {
    printf("Select 11, 22, 2302 as type!\n");
    return 3;
  }
  
  int dhtpin = atoi(argv[2]);
  if (dhtpin <= 0) {
     printf("Please select a valid GPIO pin #\n");
     return 3;
  }

  printf("Using pin #%d\n", dhtpin);

  }

  return 0;

} // main
