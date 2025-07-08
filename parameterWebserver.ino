//All of the includes necessary for the program to run

#include <WiFiS3.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketServer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino_JSON.h>
#include <ArduinoJson.h>
#include "index.h"
#include <EEPROM.h>

using namespace net;

#define X_EEPROM_OFFSET 0
#define Y_EEPROM_OFFSET 32
#define B_EEPROM_OFFSET 64
#define S_EEPROM_OFFSET 66  

//#define CLEAR_EEPROM

bool updateX = false;
bool updateY = false;
bool updateB = false;
bool updateS = false;


int interval = 1000; // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0; //use the "millis()" command for time reference and this will output an unsigned long

WebSocketServer wss(81);
WiFiServer server(80);

const char ssid[] = "CampusWireless";  // change your network SSID
const char pass[] = "Progress1234";   // change your network password

int status = WL_IDLE_STATUS;

WiFiClient client;


struct userParameters 
{
  int varX;
  int varY;
  bool yOrN;
  String randomData;
};

String stringX; 
String stringY;
String stringB;
String stringS;

userParameters upObj;

int processJson(const char *  jsonChar, WebSocket &ws)
{
 Serial.println("Processing Json."); 
 char * jsonToProcess = strdup(jsonChar); // copies string into a manipulatable string
 if (strstr(jsonToProcess, "{\"X\":\"")!= NULL ) // searches string for X header
  {
    jsonToProcess += 6; // moves pointer past the header
    upObj.varX = atoi(jsonToProcess); // converts ascii value to an integer
    String varXString = String("updateX") + String(upObj.varX); // concatenates updateX string and the struct object varX which contains the value of X that is parsed from json
    client.print("The new X parameter is: ");
    ws.send(WebSocket::DataType::TEXT, varXString.c_str(), varXString.length()); // sends updated value back to the client
    Serial.println(upObj.varX); // prints value to serial
    writeStringToEEPROM(X_EEPROM_OFFSET, String(upObj.varX)); // stores new X value to EEPROM 
  }
 else if (strstr(jsonToProcess, "{\"Y\":\"")!= NULL )
 {
    jsonToProcess += 6;
    upObj.varY = atoi(jsonToProcess);
    String varYString = String("updateY") + String(upObj.varY);
    client.print("The new Y parameter is: ");
    ws.send(WebSocket::DataType::TEXT, varYString.c_str(), varYString.length());
    Serial.println(upObj.varY);
    writeStringToEEPROM(Y_EEPROM_OFFSET, String(upObj.varY));
 }
else if (strstr(jsonToProcess, "{\"B\":\"")!= NULL )
 { 
    if (strstr(jsonToProcess, "true"))
    {
      upObj.yOrN = true;
    }
    else if (strstr(jsonToProcess, "false"))
    {
      upObj.yOrN = false;
    }
    String yOrNString = String("updateB") + String(upObj.yOrN);
    Serial.print(yOrNString);
    client.print("The new bool parameter is: ");
    ws.send(WebSocket::DataType::TEXT, yOrNString.c_str(), yOrNString.length());
    Serial.println(upObj.yOrN);
    writeStringToEEPROM(B_EEPROM_OFFSET, String(upObj.yOrN));
 }
 else if (strstr(jsonToProcess, "{\"S\":\"")!= NULL )
 {
    jsonToProcess += 6; //increments past json header
    String randomString = String("updateS") + String (jsonToProcess);
    upObj.randomData = randomString.substring(0, randomString.length() - 3);
    ws.send(WebSocket::DataType::TEXT, upObj.randomData.c_str(), upObj.randomData.length());
    Serial.println(upObj.randomData);
    writeStringToEEPROM(S_EEPROM_OFFSET, (randomString.substring(7, randomString.length() - 3)));
 }

}

int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length(); // declares variable that stores the length of the string
  EEPROM.write(addrOffset, len); // writes string length to the address
  for (int i = 0; i < len; i++) // increments bytes for length of string
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]); // moves past the length indicator, and starts writing characters based on the increment of the loop
  }
  return addrOffset + 1 + len; // returns past the end of the string so it doesnt overwrite the string with another input 
}

int readStringFromEEPROM(int addrOffset, String *strToRead) 
{
  int newStrLen = EEPROM.read(addrOffset); // reads length of the string from offset
  char data[newStrLen + 1]; // declares a character array that is the same length as the string stored in eeprom  +1 for the null term
  for (int i = 0; i < newStrLen; i++) // read as many bytes as the string is long
  {
    data[i] = EEPROM.read(addrOffset + 1 + i); // reads eeprom from the start +1 to skip len indicator + the index of the letters
  }
  data[newStrLen] = '\0'; // set null term
  *strToRead = String(data); // derefrences strToRead and sets it equal to data as a string
  return addrOffset + 1 + newStrLen; // returns past the end of the string so it doesnt overwrite the string with another input
}

void setup() {

  Serial.begin(115200);  //Initialize serial and wait for port to open:

#ifdef CLEAR_EEPROM  //clears eprom if def is uncommented
  for (int i = 0; i < 500; i++)
  {
    EEPROM.write(i, 0);
  }

  Serial.println("Cleared 500 bytes of eeprom.");
#endif

  String fv = WiFi.firmwareVersion();  // gets firmware version, and reminds user to update if not latest version
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
    Serial.println("Please upgrade the firmware");

  while (status != WL_CONNECTED) { // while not connected attempts to connect using ssid and pass every 4 seconds
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);

    delay(4000);
  }

  if (EEPROM.read(X_EEPROM_OFFSET) || EEPROM.read(Y_EEPROM_OFFSET) || EEPROM.read(B_EEPROM_OFFSET) || EEPROM.read(S_EEPROM_OFFSET)) // if any of these reads return any number other than 0
  {

    if (EEPROM.read(X_EEPROM_OFFSET)) // checks to see if there is a value at X address
    {
      readStringFromEEPROM(X_EEPROM_OFFSET, &stringX); // reads the eeprom string, and stores it into stringX
      stringX = "updateX" + stringX; // prepends updateX, in coordination with the JS, so it can be parsed on the JS side for the value
      updateX = true; // set this flag to be true 
    }

    if (EEPROM.read(Y_EEPROM_OFFSET)) // checks to see if there is a value at Y address
    {
      readStringFromEEPROM(Y_EEPROM_OFFSET, &stringY);
      stringY = "updateY" + stringY;
      updateY = true;
    }
    if (EEPROM.read(B_EEPROM_OFFSET)) // checks to see if there is a value at B address
    {
      readStringFromEEPROM(B_EEPROM_OFFSET, &stringB);
      stringB = "updateB" + stringB;
      updateB = true;
    }
    if (EEPROM.read(S_EEPROM_OFFSET)) // checks to see if there is a value at S address
    {
      readStringFromEEPROM(S_EEPROM_OFFSET, &stringS);
      stringS = "updateS" + stringS;
      updateS = true;
    }

  }

  // for debugging purposes
  /*Serial.println(stringX);
  Serial.println(stringY);
  Serial.println(stringB);
  Serial.println(stringS);*/

  Serial.print("IP Address: "); // print your board's IP address:
  Serial.println(WiFi.localIP());

  server.begin(); // starts webserver

  wss.onConnection([](WebSocket &ws) { // on connection to websock
    const auto protocol = ws.getProtocol(); 
    if (protocol) {
      Serial.print(F("Client protocol: "));
      Serial.println(protocol);
    }

    if (updateX) // update value read from eeprom then
    {
      ws.send(WebSocket::DataType::TEXT, stringX.c_str(), stringX.length()); // sends value from updateX string 
    }

    if (updateY) // and so on for the rest
    {
      ws.send(WebSocket::DataType::TEXT, stringY.c_str(), stringY.length());
    }

    if (updateB)
    {
      ws.send(WebSocket::DataType::TEXT, stringB.c_str(), stringB.length());
    }

    if (updateS)
    {
      ws.send(WebSocket::DataType::TEXT, stringS.c_str(), stringS.length());
    }
    
    //on message from UI 
    ws.onMessage([](WebSocket &ws, const WebSocket::DataType dataType, const char *message, uint16_t length) 
    {
      switch (dataType) //based on recieved data type
      {
        case WebSocket::DataType::TEXT:  
          Serial.print(F("Received: ")); Serial.println(message);          
          if (strstr(message, "{\"") != NULL) //if its a string
          {
                        
            processJson(message, ws);

          }
          
          break;
        case WebSocket::DataType::BINARY: // if its binary
          Serial.println(F("Received binary data")); 
          break;
      }

      String reply = "Received: " + String((char *)message); // displays on UI that it has received sent message(s)
      ws.send(dataType, reply.c_str(), reply.length());
    });


    
    ws.onClose([](WebSocket &, const WebSocket::CloseCode, const char *, // when client disconnects from WebSocket
                  uint16_t) {
      Serial.println(F("Disconnected")); 
    });

    Serial.print(F("New WebSocket Connnection from client: "));
    Serial.println(ws.getRemoteIP());

    const char message[]{ "Welcome to my Demo Project!" };
    ws.send(WebSocket::DataType::TEXT, message, strlen(message));
  });

  wss.begin(); //begins WebSocket server
}

void loop() 
{
  wss.listen(); // listens for input from the Websockets

  // listens for incoming clients
  client = server.available();
  if (client)
   {
    // reads the HTTP request header line by line
    while (false && client.connected()) 
    {
      if (client.available()) 
      {
        String HTTP_header = client.readStringUntil('\n'); // reads the header line of HTTP request

        if (HTTP_header.equals("\r"))  // the end of HTTP request
          break;

        Serial.print("<< ");
        Serial.println(HTTP_header); // print HTTP request to Serial Monitor
      }

    }

    // send the HTTP response header
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");  // the connection will be closed after completion of the response
    client.println();                     // the separator between HTTP header and body
    
    String html = String(HTML_CONTENT);
    
    client.println(html);

    client.flush();

    // give the web browser time to receive the data
    delay(50);

    // close the connection:
    client.stop();
  }
}
