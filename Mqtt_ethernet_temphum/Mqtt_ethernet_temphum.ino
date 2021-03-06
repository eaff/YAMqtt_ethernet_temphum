/***************************************************
Adafruit MQTT Library Ethernet Example

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Alec Moore
Derived from the code written by Limor Fried/Ladyada for Adafruit Industries.
MIT license, all text above must be included in any redistribution
****************************************************/

/***************************************************

Extended by Eduardo Farias <eaff.pt@gmail.com>
MIT license, all text above must be included in any redistribution

***************************************************/
#include <EthernetUdp.h>
#include <EthernetServer.h>
#include <SPI.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <Ethernet.h>
#include <EthernetClient.h>
#include <Dns.h>
#include <Dhcp.h>

/*DHT Sensor includes */
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include "LoginDetails.h"

/************************* Ethernet Client Setup *****************************/
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//Uncomment the following, and set to a valid ip if you don't have dhcp available.
IPAddress iotIP (192, 168, 1, 105);
//Uncomment the following, and set to your preference if you don't have automatic dns.
IPAddress dnsIP(8, 8, 8, 8);
//If you uncommented either of the above lines, make sure to change "Ethernet.begin(mac)" to "Ethernet.begin(mac, iotIP)" or "Ethernet.begin(mac, iotIP, dnsIP)"
IPAddress gwIP(192, 168, 1, 254);

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#ifndef __LOGIN_DETAILS
#define __LOGIN_DETAILS
#define AIO_USERNAME    "teste"
#define AIO_KEY         "testing_aio_key"
#endif // !AIO_LOGINDETAILS 




/************************* DHT Sensor  Setup *****************************/

#define DHTPIN            7         // Pin which is connected to the DHT sensor.

// Uncomment the type of sensor in use:
//#define DHTTYPE           DHT11     // DHT 11
#define DHTTYPE           DHT22     // DHT 22 (AM2302)
//#define DHTTYPE           DHT21     // DHT 21 (AM2301)


/************************* Room  Setup *****************************/
#define ROOM "homeoffice"

/************ Global State (you don't need to change this!) ******************/

//Set up the ethernet client
EthernetClient client;


// Store the MQTT server, client ID, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM = AIO_SERVER;
// Set a unique MQTT client ID using the AIO key + the date and time the sketch
// was compiled (so this should be unique across multiple devices for a user,
// alternatively you can manually set this to a GUID or other random value).
const char MQTT_CLIENTID[] PROGMEM = __TIME__ AIO_USERNAME;
const char MQTT_USERNAME[] PROGMEM = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM = AIO_KEY;

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, AIO_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

//Set up the dht sensor client

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;


/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char AMBIENTTEMPERATURE_FEED[] PROGMEM = AIO_USERNAME "/feeds/" ROOM "ambienttemperature";
Adafruit_MQTT_Publish temperature = Adafruit_MQTT_Publish(&mqtt, AMBIENTTEMPERATURE_FEED);

const char RELATIVEHUMIDITY_FEED[] PROGMEM = AIO_USERNAME "/feeds/" ROOM "relativehumidity";
Adafruit_MQTT_Publish humidity = Adafruit_MQTT_Publish(&mqtt, RELATIVEHUMIDITY_FEED);

// Setup a feed called 'onoff' for subscribing to changes.
const char ONOFF_FEED[] PROGMEM = AIO_USERNAME "/feeds/onoff";
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, ONOFF_FEED);

/*************************** Sketch Code ************************************/

void setup() {
	Serial.begin(115200);

	Serial.println(F("Adafruit MQTT demo"));

	// Initialize dht device.
	dht.begin();

	Serial.println("DHTxx Unified Sensor Example");
	// Print temperature sensor details.
	sensor_t sensor;
	dht.temperature().getSensor(&sensor);
	Serial.println("------------------------------------");
	Serial.println("Temperature");
	Serial.print("Sensor:       "); Serial.println(sensor.name);
	Serial.print("Driver Ver:   "); Serial.println(sensor.version);
	Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
	Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
	Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
	Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");
	Serial.println("------------------------------------");
	// Print humidity sensor details.
	dht.humidity().getSensor(&sensor);
	Serial.println("------------------------------------");
	Serial.println("Humidity");
	Serial.print("Sensor:       "); Serial.println(sensor.name);
	Serial.print("Driver Ver:   "); Serial.println(sensor.version);
	Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
	Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
	Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
	Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");
	Serial.println("------------------------------------");
	// Set delay between sensor readings based on sensor details.
	delayMS = sensor.min_delay / 1000;

	// Initialise the Client
	Serial.print(F("\nInit the Client..."));
	Ethernet.begin(mac, iotIP, dnsIP, gwIP);
	/*if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
		// no point in carrying on, so do nothing forevermore:
		for (;;)
			;
	}*/
	delay(1000); //give the ethernet a second to initialize
	printIPAddress();

	mqtt.subscribe(&onoffbutton);
}



void loop() {
	// Ensure the connection to the MQTT server is alive (this will make the first
	// connection and automatically reconnect when disconnected).  See the MQTT_connect
	// function definition further below.
	MQTT_connect();

	// this is our 'wait for incoming subscription packets' busy subloop
	Adafruit_MQTT_Subscribe *subscription;
	while ((subscription = mqtt.readSubscription(1000))) {
		if (subscription == &onoffbutton) {
			Serial.print(F("Got: "));
			Serial.println((char *)onoffbutton.lastread);
		}
	}

	// Delay between measurements.
	// delay(delayMS);
	delay(1000);

	// Get temperature event and print its value and send its value
	sensors_event_t event;
	dht.temperature().getEvent(&event);
	if (isnan(event.temperature)) {
		Serial.println("Error reading temperature!");
	}
	else {
		// Serial.print("Temperature: ");
		// Serial.print(event.temperature);
		// Serial.println(" *C");
		// Now we can publish to the io
		// Serial.print(F("\nSending ambient temperature val "));
		//  Serial.print(event.temperature);
		//  Serial.print("...");
		if (!temperature.publish(event.temperature)) {
			Serial.println(F("Failed"));
		}
		else {
			//  Serial.println(F("OK!"));
		}
	}


	// Get humidity event  and print its value and send its value
	dht.humidity().getEvent(&event);
	if (isnan(event.relative_humidity)) {
		Serial.println("Error reading humidity!");
	}
	else {
		//Serial.print("humidity: ");
		//Serial.print(event.relative_humidity);
		//Serial.println("%");
		// Now we can publish to the io
		//Serial.print(F("\nSending relative_humidity val "));
		//Serial.print(event.relative_humidity);
		//Serial.print("...");
		if (!humidity.publish(event.relative_humidity)) {
			//Serial.println(F("Failed"));
		}
		else {
			//Serial.println(F("OK!"));
		}
	}
/*	// Maintain DHCP Address
	if ((Ethernet.maintain()))
	{
		Serial.println("Failed to renew IP using DHCP");
		// no point in carrying on, so do nothing forevermore:
		for (;;)
			;
	}
	*/
	// ping the server to keep the mqtt connection alive
	if (!mqtt.ping()) {
		Serial.print("Ping to MQTT Failed Disconnecting... ");
		mqtt.disconnect();
	}

}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
	int8_t ret;

	// Stop if already connected.
	if (mqtt.connected()) {
		return;
	}

	Serial.print("Connecting to MQTT... ");

	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
		Serial.println(mqtt.connectErrorString(ret));
		printIPAddress();
		Serial.println("Retrying MQTT connection in 5 seconds...");
		mqtt.disconnect();
		delay(5000);  // wait 5 seconds
	}
	Serial.println("MQTT Connected!");
}

void printIPAddress()
{
	Serial.print("My IP address: ");
	for (byte thisByte = 0; thisByte < 4; thisByte++) {
		// print the value of each byte of the IP address:
		Serial.print(Ethernet.localIP()[thisByte], DEC);
		Serial.print(".");
	}
	Serial.println("");
}