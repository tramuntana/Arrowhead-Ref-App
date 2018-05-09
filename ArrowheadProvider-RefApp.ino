/*
   This is a skeleton and a reference application for a simple Arrowhead-compliant providing system.

   Read more about Arrowhead Framework: http://www.arrowhead.eu/
   Here you can download the Arrowhead G3.2 Framework (Milestone 3): https://github.com/hegeduscs/arrowhead
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "DHTesp.h"

/*
   We manage the time with the help of the NTPClient library.
   Read more and see examples: https://www.arduinolibraries.info/libraries/ntp-client
*/
#define NTP_OFFSET  1  * 60 * 60 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "0.pool.ntp.org"

/
#define SERVER_PORT 8454
/*
   These two variables are required to access Wifi.
   Obviously, the values need to be changed according to our own data.
*/
const char* ssid = "Arrowhead-RasPi-IoT";
const char* password = "arrowhead";

/*
   Parameters for MQTT connection to ThingSpeak

   Read more: https://thingspeak.com/
*/
/* The username could be whatever you want*/
char mqttUserName[] = "testUserName";
/*  Change this to your MQTT API Key (Account -> MyProfile)*/
char mqttPass[] = "YourMQTTAPIKey";
/* Change to your channel Write API Key (Channels -> MyChannels -> APIKeys) */
char writeAPIKey[] = "YourWriteAPIKey";
/* Change it to your own ChannelID (Channels -> MyChannels) */
long channelID = 123456; //

/* This is used for the random generation of the clientID */
static const char alphanum[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.


/*
   The initialization of the Wifi Client Library and the PubSubClient library.
   The PubSubClient library offers an Arduino client for MQTT.
   Finally, we define the ThingSpeak MQTT broker.

   Read more: https://www.arduino.cc/en/Reference/WiFiClient
              https://pubsubclient.knolleary.net/api.html#state
*/
WiFiClient client;
PubSubClient mqttClient(client);
const char* mqttServer = "mqtt.thingspeak.com";

/*
    Define other global variables to track the last connection time and to define the time interval to publish the data.
    In this example we will post data in every 60 seconds, but you can modify it according to your needs
*/
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 60L * 1000L;


/* The ServiceRegistryEntry strings which will used to register our services to Service Registry.
   You should modify this part according to your own services.
  
  Read more and see example ServiceRegistryEntry payloads in Arrowhead ServiceDiscovery M3 IDD REST-JSON-TLS.docx:
  https://github.com/hegeduscs/arrowhead/tree/M3/documentation/ServiceRegistry
 */
String SRentry1 = String("{\"providedService\":{\"serviceDefinition\": \"IndoorTemperature\",\"interfaces\": [\"json\"],\"serviceMetadata\": {\"unit\": \"celsius\"}},\"provider\": {\"systemName\": \"InsecureTemperatureSensor\",\"address\":\"");
String SRentry2 = String("\",\"port\": 8454},\"port\": 8454,\"serviceURI\": \"temperature\",\"version\": 1}");

String senml_1 = String("{\"bn\": \"TemperatureSensors_InsecureTemperatureSensor\",\"bt\":"); //"base timestamp
String senml_2 = String(",\"bu\": \"celsius\",\"ver\": 1,\"e\": [{\"n\": \"Temperature_IndoorTemperature\",\"v\":"); //+value
String senml_3 = String(",\"t\":0}]}");


AsyncWebServer server(SERVER_PORT);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

/* Initialize DHT sensor. This part will change in your code. */
DHTesp dht;

/* This function implements the registering to the Service Registry.
   The Service Registry is one of the base systems in the Arrowhead Framework.
   If the service creation is failed, then you should remove your service and after reregister it as it shown below.

   Read more: https://github.com/hegeduscs/arrowhead/tree/M3/documentation/ServiceRegistry

   These registered service can be searched from Table service_registry and here we also have the opportunity to modify them.
   The databes can be managed and accessed easily through the internet using the phpMyAdmin tool.

   Read more: https://www.phpmyadmin.net/
    
*/
void registerService(String SRentry) {
  HTTPClient http_sr;
  /*  You can modify the IP address and the associated port  according to your system's data, if needed.*/
  http_sr.begin("http://192.168.42.1:8442/serviceregistry/register");
  /*  Specifying the content-type header */
  http_sr.addHeader("Content-Type", "application/json"); 
  /* Sending the actual POST request with the payload what is specified above.The return value will be an HTTP code.*/
  int httpResponseCode_sr = http_sr.POST(String(SRentry)); 
  Serial.print("Registered to SR with status code:");
  Serial.println(httpResponseCode_sr);
  /* Free resources. */
  http_sr.end();

  if (httpResponseCode_sr != HTTP_CODE_CREATED) { 
    HTTPClient http_remove;
    /* Sending the actual PUT request. */
    http_remove.begin("http://192.168.42.1:8442/serviceregistry/remove"); //Specify destination for HTTP request
     /*  Specifying the content-type header */
    http_remove.addHeader("Content-Type", "application/json");
    /* Sending the actual PUT request with the payload what is specified above.The return value will be an HTTP code.*/
    int httpResponseCode_remove = http_remove.PUT(String(SRentry)); //Send the actual PUT request
    Serial.print("Removed previous entry with status code:");
    Serial.println(httpResponseCode_remove);
    /* Getting the response to the request and then printing it for verification purposes. The printing step may omitted.*/
    String response_remove = http_remove.getString();          
    Serial.println(response_remove);    
    /* Free resources. */
    http_remove.end();

    delay(1000);

    HTTPClient http_renew;
    
    http_renew.begin("http://192.168.42.1:8442/serviceregistry/register");
     /*  Specifying the content-type header */
    http_renew.addHeader("Content-Type", "application/json"); 
    /* Sending the actual POST request with the payload what is specified above.The return value will be an HTTP code.*/
    int httpResponseCode_renew = http_renew.POST(String(SRentry)); 
    Serial.print("Re-registered with status code:");
    Serial.println(httpResponseCode_renew);
    /* Getting the response to the request and then printing it for verification purposes. The printing step may omitted.*/
    String response_renew = http_renew.getString();                       
    Serial.println(response_renew);           
    /* Free resources. */
    http_renew.end();  //Free resources
  
}

/*
   Generally the setup() function initializes and sets the initial values
   The setup() function will only run once, after each powerup or reset of the Arduino board.
*/
void setup() {
  /* Initialize serial and wait for port to open */
  Serial.begin(115200);
  delay(1000);   //Delay needed before calling the WiFi.begin

/*
     Building up a wifi connection and accessing the network.
     This section can be used without modification.

     Read more: https://www.arduino.cc/en/Reference/WiFiStatus
  */
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("Connected, my IP is:");
  Serial.println(WiFi.localIP());

  /* Set the MQTT Broker details: host and port */
  mqttClient.setServer(mqttServer, 1883);

  /* Building ServiceRegistryEntry and register our service to Service Registry. */
  String SRentry = SRentry1 + WiFi.localIP().toString() + SRentry2;
  registerService(SRentry);
  
  /* Initialize NTP connection. */
  timeClient.begin();  
  }

  /* Starting the DHT sensor on PIN0. You should modify it accourding to your setup*/
  dht.setup(0);

  /* Starting a web server which will response to the REST calls. We may create more web server to the different services.  */
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Received HTTP request, responding with:");
    unsigned long currentTime = timeClient.getEpochTime();
    /* Reading some data from sensor. You should modify it according to you sensors. */
    float temperature = dht.getTemperature();
    String response = senml_1 + currentTime + senml_2 + temperature + senml_3;
    Serial.print(response);

    request->send(200, "application/json", response);
  });
  server.begin();

  /* Prepare LED pin. */
  pinMode(LED_BUILTIN, OUTPUT);
}


/*
   This function implements the reconnection to MQTT Broker
   It contains a loop what continuously trying to connect until we succeed.
*/
void reconnect()
{
  char clientID[10];

  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    /* Generate a random ClientID based on alphanum global variable.*/
    for (int i = 0; i < 8; i++) {
      clientID[i] = alphanum[random(51)];
    }
    /* This character is very important, beacuse clientID strings are terminated with a null character (ASCII code 0). */
    clientID[8] = '\0';
    /* Connect to the MQTT Broker */
    if (mqttClient.connect(clientID, mqttUserName, mqttPass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      /* Printing the cause of the failure.
         For the failure code explanation visit this site: http://pubsubclient.knolleary.net/api.html#state
      */
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/* This function implements the publish to the channel (ThingSpeak). */
void mqttpublish() {
  /* Reading some data from sensor. You should modify it according to you sensors. */
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  /* Create data string to send to ThingSpeak. You could concatenate as many fields as you want (or as you have) */
  String data = String("field1=" + String(temp, DEC) + "&field2=" + String(hum, DEC));
  int length = data.length();
  char msgBuffer[length];
  data.toCharArray(msgBuffer, length + 1);
  Serial.println(msgBuffer);

  /* Create a topic string and publish data to ThingSpeak channel feed. */
  String topicString = "channels/" + String( channelID ) + "/publish/" + String(writeAPIKey);
  length = topicString.length();
  char topicBuffer[length];
  topicString.toCharArray(topicBuffer, length + 1);

  mqttClient.publish( topicBuffer, msgBuffer );

  lastConnectionTime = millis();
}

/*
   Finally in this section we will blink 1 kHz if everything is set up and connect to MQTT Broker.

   Generally the loop() function does precisely what its name suggests, and loops consecutively, allowing your program to change and respond.
   Use it to actively control the Arduino board.
*/
int ledStatus = 0;
void loop() {
  /* Reconnect if MQTT client is not connected. */
  if (!mqttClient.connected()) {
    reconnect();
  }

  /* We should call the loop continuously to establish connection to the server.*/
  mqttClient.loop();

  /* If interval time has passed since the last connection, publish data to ThingSpeak */
  if (millis() - lastConnectionTime > postingInterval) {
    mqttpublish();
  }

  timeClient.update();

  if (ledStatus) {
    digitalWrite(LED_BUILTIN, HIGH);
    ledStatus = 0;
  }
  else {
    digitalWrite(LED_BUILTIN, LOW);
    ledStatus = 1;
  }
  delay(1000);
}
