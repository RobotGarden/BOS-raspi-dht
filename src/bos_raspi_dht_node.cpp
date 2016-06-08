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
  BosDhtNode(const std::string& topicStem, const int sampleIntervalSeconds=30, const char* id=NULL, const char* host="127.0.0.1", const int port=1883, const int keepAlive=120);
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
  
  BDNState state;
  const int interval;
  int msgCount;
  const std::string temperatureTopic;
  const std::string humidityTopic;
  float temperatureSample;
  float humiditySample;
  struct timeval lastSampleTime;
};

BosDhtNode::BosDhtNode(const std::string& topicStem, const int sampleIntervalSeconds, const char* id, const char* host, const int port, const int keepAlive) :
  mosquittopp(id),
  state(initalized),
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
        XXX Sample the sensor and publish the result
        temperatureSample = XXX
        humiditySample = XXX
        gettimeofday(&lastSampleTime, NULL);
        state = publishTmp;
        publishSample(temperatureTopic, temperatureSample);
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

int readDHT(int type, int pin, float* t, float* h);

int main(int argc, char **argv) {

  if (!bcm2835_init()) return 1;

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

  ros::init(argc, argv, "raspi_dht");

  ros::NodeHandle n;

  ros::Publisher temperature_pub = n.advertise<std_msgs::Float32>("temperature", 2);
  ros::Publisher humidity_pub    = n.advertise<std_msgs::Float32>("humidity", 2);

  ros::Rate loop_rate(0.016);
  ros::Duration retry_interval(5.0);
  ros::Duration backoff_interval(200);

  bool last_had_data = true;

  while (ros::ok()) {
    std_msgs::Float32 temperature_msg, humidity_msg;
    float t, h;

    if (readDHT(type, dhtpin, &t, &h)) {
      temperature_msg.data = t;
      humidity_msg.data = h;
#ifdef DEBUG
      printf("Publishing data: t=%f, h=%f\n", t, h);
      ROS_INFO("BOS_RASPI_DHT_NODE: %f, %f", t, h);
#endif
      temperature_pub.publish(temperature_msg);
      humidity_pub.publish(humidity_msg);
      ros::spinOnce();
      loop_rate.sleep();
      last_had_data = true;
    }
    else {
      if (last_had_data) {
	ROS_INFO("BOS_raspi_DHT_node: no data, retrying soon");
	retry_interval.sleep();
      }
      else {
	ROS_INFO("BOS_raspi_DHT_node: no data, waiting a while");
	backoff_interval.sleep();
      }
      last_had_data = false;
    }
  }

  return 0;

} // main


int readDHT(int type, int pin, float* t, float* h) {
  int bits[250], data[100];
  int bitidx = 0;
  int counter = 0;
  int laststate = HIGH;
  int j=0;

  // Set GPIO pin to output
  bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

  bcm2835_gpio_write(pin, HIGH);
  usleep(500000);  // 500 ms
  bcm2835_gpio_write(pin, LOW);
  usleep(20000);

  bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  // wait for pin to drop?
  while (bcm2835_gpio_lev(pin) == 1) {
    usleep(1);
  }

  // read data!
  for (int i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while ( bcm2835_gpio_lev(pin) == laststate) {
	counter++;
	//nanosleep(1);		// overclocking might change this?
        if (counter == 1000)
	  break;
    }
    laststate = bcm2835_gpio_lev(pin);
    if (counter == 1000) break;
    bits[bitidx++] = counter;

    if ((i>3) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      data[j/8] <<= 1;
      if (counter > 200)
        data[j/8] |= 1;
      j++;
    }
  }


#ifdef DEBUG
  for (int i=3; i<bitidx; i+=2) {
    printf("bit %d: %d\n", i-3, bits[i]);
    printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > 200);
  }
#endif

  if ((j >= 39) &&
      (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) ) {
    // yay!
    if (type == DHT11) {
      *t = (float)data[2];
      *h = (float)data[0];
#ifdef DEBUG
      printf("Temp = %d *C, Hum = %d \%\n", data[2], data[0]);
#endif
    }
     if (type == DHT22) {
	*h = data[0] * 256 + data[1];
	*h /= 10;

	*t = (data[2] & 0x7F)* 256 + data[3];
        *t /= 10.0;
        if (data[2] & 0x80) *t *= -1;
#ifdef DEBUG
	printf("Temp =  %.1f *C, Hum = %.1f \%\n", *t, *h);
#endif
    }
    return 1;
  }

  return 0;
}
