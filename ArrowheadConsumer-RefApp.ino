/*
   This is a skeleton and a reference application for a simple Arrowhead-compliant consuming system.

   Read more about Arrowhead Framework: http://www.arrowhead.eu/
   Here you can download the Arrowhead G3.2 Framework (Milestone 3): https://github.com/hegeduscs/arrowhead
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

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


/*
   Creating orchRequest message.
   The backslash (\) escape character turns special characters into string characters;

   You could read more about the orchestration request in Arrowhead Orchestration M3 IDD-REST-JSON-TLS document:
   https://github.com/hegeduscs/arrowhead/tree/M3/documentation/Orchestrator
*/
String reqSys = String("\"requesterSystem\": {\"systemName\": \"client1\",\"address\": \"localhost\",\"port\": 0},");
String reqSrv = String("\"requestedService\": {\"serviceDefinition\": \"IndoorTemperature\",\"interfaces\": [\"json\"],\"serviceMetadata\": {\"unit\": \"celsius\"}},");
String orchFlags = String("\"orchestrationFlags\": {\"overrideStore\": true, \"metadataSearch\": true, \"enableInterCloud\": true}");
String orchRequest = String("{") + reqSys + reqSrv + orchFlags + String("}");

/* The orchestrated service endpoint */
String endpoint = String("");

/* We define a DHT sensor. You must modify this section according to your sensors! */
#define DHT_PIN 0
DHT dht(DHT_PIN, DHT11);

/*
   ArduinoJson library uses a fixed memory allocation, allowing to work on devices with very little RAM.
   In this example we choose to store data in the stack. You have the opportunity to use heap instead of the stack.

   ArduinoJson is a header-only library, meaning that all the code is the headers.
   This greatly simplifies the compilation as you don’t have to worry about compiling and linking the library.

   Read more and see examples: https://arduinojson.org/
*/
StaticJsonBuffer<1000> JSONBuffer;
StaticJsonBuffer<500> JSONBuffer_srv;


/*
     In this function we will request an orchestration from the Orchestrator System.
     The POST request will be sent with the help of the HTTPClient library

     Read more: https://github.com/hegeduscs/arrowhead/tree/M3/documentation/Orchestrator
                https://www.arduino.cc/en/Tutorial/HttpClient
*/
void sendOrchReq(String &endpoint , String request_name , String unit ) {
  Serial.println("Sending orchestration request with the following payload:");
  Serial.println(orchRequest);
  HTTPClient http_orch;
  /*You can modify the IP address and the associated port  according to your system's data, if needed.  */
  http_orch.begin("http://192.168.42.1:8440/orchestrator/orchestration");
  /*  Specifying the content-type header */
  http_orch.addHeader("Content-Type", "application/json");
  /* Sending the actual POST request with the payload what is specified above.The return value will be an HTTP code.*/
  int httpResponseCode_orch = http_orch.POST(String(orchRequest));
  /* Getting the response to the request and then printing it for verification purposes. The printing step may omitted.*/
  String orch_response = http_orch.getString();
  Serial.println("Orchestration Response:");
  Serial.println(orch_response);
  http_orch.end();

  /*
    Parsing the orchestration response with the help of the ArduinoJSON library.

    The response is an error message, if the orchestration process failed (e.g. there is no proper system that can serve our request).
  */
  JsonObject& root = JSONBuffer.parseObject(orch_response);

  if (!root.containsKey("errorMessage")) {
    /* The address, port and uri variables belong to the provider system which provides the requested service for us.
      The endpoint is a concatenated URL which leading exactly to the requested service.
    */
    const char* address = root["response"][0]["provider"]["address"];
    const char* port = root["response"][0]["provider"]["port"];
    const char* uri = root["response"][0]["serviceURI"];
    endpoint = "http://" + String(address) + ":" + String(port) + "/" + String(uri);

    Serial.println("Received endpoint, connecting to: " + endpoint);
  } else {
    Serial.println("Orchestration failed!");
  }
}



/*
   Generally the setup() function initializes and sets the initial values
   The setup() function will only run once, after each powerup or reset of the Arduino board.
*/
void setup() {
  /* Initialize serial and wait for port to open */
  Serial.begin(115200);
  delay(1000);

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
  /* Send the orchestration request. */
  sendOrchReq(endpointTemp, "Temperature", "celsius");
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
   Finally in this section we will use the orchestrated service. The service can be invoked with a simple REST call.

   Generally the loop() function does precisely what its name suggests, and loops consecutively, allowing your program to change and respond.
   Use it to actively control the Arduino board.
*/
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

  if (endpoint != "") {
    HTTPClient http_srv;
    http_srv.begin(endpoint);
    /*  Specifying the content-type header */
    http_srv.addHeader("Content-Type", "application/json");
    /* Sending the actual GET request with the payload what is specified above.The return value will be an HTTP code.*/
    int httpResponseCode_srv = http_srv.GET();
    /* Getting the response to the request and then printing it for verification purposes. The printing step may omitted.*/
    String srv_response = http_srv.getString();
    Serial.println("Service Response:");
    Serial.println(srv_response);
    http_srv.end();

    /*
      Parsing the service response with the help of the ArduinoJSON library.

      In additon we will use the SenML (Sensor Markup Language) that aims to simplify gathering data from different devices across the network
      The "e" imply an event. The event object inside the events array contain a "v" field, which is a numeric value. In this example this is the actual temperature.

      Read more more about SenML: https://wiki.tools.ietf.org/id/draft-jennings-core-senml-04.html:
    */
    JsonObject& srv_root = JSONBuffer_srv.parseObject(srv_response);
    const char* temp_raw = srv_root["e"][0]["v"];
    String temp_str = String(temp_raw);
    Serial.println("");
    Serial.println("The temperature is " + temp_str + "°C.");
    JSONBuffer_srv.clear();
    delay(5000);
  } else {
    Serial.println("No endpoint to connect to!");
  }
}
