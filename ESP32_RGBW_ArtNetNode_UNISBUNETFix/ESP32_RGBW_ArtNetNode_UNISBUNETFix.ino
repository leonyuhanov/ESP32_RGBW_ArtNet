/*
 ESP32 RGBW ArtNet Node
	
 Boot Pin
	Pin 34
	
 SPI DATA for Pixels
	Pin 23
 */
#include "Arduino.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include "SPIFFS.h"
#include <string>
#include <AsyncUDP.h>
#include "artNetPacket.h"
#include "I2SClocklessLedDriver.h"

//Mode change/Boot Pin
const byte bootPin = 34;
byte systemMode=0;

//LED Config
const byte ledOutputPin = 23;
unsigned short int totalPixels=0;
I2SClocklessLedDriver ledStrip; 
uint8_t* leds;
int pins[2]={ledOutputPin,18};
unsigned short int numberOfPixels=0;

//Config Files in SPIFFS
const char* networkConfig="/networkConfig";
const char* wifiConfig="/WiFiConfig";
const char* artNetConfig="/artNetConfig";

//Timers
unsigned long timeData[3];

//Network Settings
unsigned int artNetPort = 6454;
const short int maxPacketBufferSize = 530;
char packetBuffer[maxPacketBufferSize];
short int packetSize=0;
AsyncUDP udp;
artNetPacket artNetData;
int networkMode=0;		//0=DHCP		1=SATIC	
char* hostName;
IPAddress my_ip;
IPAddress my_mask;
IPAddress my_gateway;
char* myWiFiSSID;
char* myWiFiKey;

//ArtNet Configuration
uint8_t numberOfDMXUniverses = 0;
unsigned short int** artNetFrames;
const uint8_t maxPixelsPerUniverse = 128;



//--------------------  WEB UI Stuff
unsigned short int configFileSize=0;
const char* indexPageFilePath="/index";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
const char *ssid = "WOW-AP";
const char *password = "wowaccesspoint";
unsigned short int indexFileSize=0;
IPAddress local_ip = IPAddress(10,10,10,1);
IPAddress gateway = IPAddress(10,10,10,1);
IPAddress subnet = IPAddress(255,255,255,0);
//--------------------  WEB UI Stuff

void setup() 
{
  unsigned short int uCount=0;
  uint8_t wifiFlag=0;
  
  
  //Disable Bluetooth
  btStop();
  //Init Serial Out
  Serial.begin(115200);
  Serial.printf("\r\n\r\n\r\nSystem Booting....\r\nRGBW NEOPIXEL ARTNET\r\n");
  //Set up input
  pinMode(bootPin, INPUT);
  systemMode = digitalRead(bootPin);
  //Init File System
  initFS();
  //Load Configuration if any
  loadConfig();
  
  if(systemMode==0)
  {
  	//--------- Pixel Set up  -----------------------
    //Count total number of pixels
    for(uCount=0; uCount<numberOfDMXUniverses; uCount++)
    {
      totalPixels+=artNetFrames[uCount][2];
    }
    //If more than 0 pixels are set up initiate the LED System
    if(totalPixels>0)
    {
      leds = new uint8_t[1*totalPixels*4];
      ledStrip.initled(leds,pins,1,totalPixels,ORDER_RGBW);
      //clear
      for(uCount=0; uCount<totalPixels; uCount++)
      {
        ledStrip.setPixel(uCount,0,0,0,0);
      }
      ledStrip.showPixels();
    }
    //------------------------------------------------
    
  	//Eable WIFI
  	WiFi.mode(WIFI_STA);
  	//Set DHCP or Static Mode based on networkMode value
  	if(networkMode==0)
  	{
  		//DHCP Mode
  	}
  	else
  	{
  		//Static IP Mode
  		WiFi.config(my_ip, my_mask, my_gateway);
  	}
  	WiFi.setHostname(hostName);
  	WiFi.begin(myWiFiSSID, myWiFiKey);
  	while (WiFi.status() != WL_CONNECTED)
  	{
  		delay(100);
  		Serial.print(".");
      //Flash Pixels RED untill wifi is connected
      if(wifiFlag==0)
      {
        for(uCount=0; uCount<totalPixels; uCount++)
        {
          ledStrip.setPixel(uCount,10,0,0,0);
        }
        ledStrip.showPixels();
        wifiFlag=1;
      }
      else
      {
        for(uCount=0; uCount<totalPixels; uCount++)
        {
          ledStrip.setPixel(uCount,0,0,0,0);
        }
        ledStrip.showPixels();
        wifiFlag=0;
      }
  	}
   //set to black just in case we missed it
   for(uCount=0; uCount<totalPixels; uCount++)
    {
      ledStrip.setPixel(uCount,0,0,0,0);
    }
    ledStrip.showPixels();
    ledStrip.showPixels();
    //ready
  	Serial.print("\r\nONLINE\t");
  	Serial.print(WiFi.localIP());
  
  	//Set up UDP
  	udp.listen(artNetPort);
  	udp.onPacket(pollDMX);
    
    
  }
  else if(systemMode==1)
  {
    //Begin Config mode
    Serial.printf("\r\n\tBooting into WEB UI configuration mode...");
    //configure Access Point
    WiFi.mode(WIFI_AP);
    WiFi.enableAP(true);
    delay(100);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ssid, password);
    delay(100);
    Serial.printf("\r\n\tWIFI_AP MAC\t");
    Serial.print(WiFi.softAPmacAddress());
    Serial.printf("\r\n\tAP IP Address\t");
    Serial.print(WiFi.softAPIP());
    Serial.printf("\r\nSSID[");
    Serial.print(ssid);
    Serial.printf("]\t\tKEY[");
    Serial.print(password);
    Serial.printf("]");
    //init Web server
    //Handle Root
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    {
      handleRoot(request);
    });
    server.onNotFound(notFound);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
  }  
}

//---------------   beggin web ui functionality  ----------------------
void initFS()
{
  Serial.printf("\r\n\tSetting up SPIFS...");
  SPIFFS.begin(1);
  Serial.printf("\tSPIFS READY!\r\n");
}

void loadConfig()
{
	File fileObject;
	char* tempHostName = "NODE_0000";
  hostName = new char[10];
  char* blankText = "NULL";
  char* tempData;
  char* localSSID;
  char* localKEY;
  unsigned short int ssidLength=0, keyLength=0, fileSize=0;
  unsigned short int fIndex=0;
  uint8_t tempIP[4] = {0,0,0,0};
  uint8_t tempMask[4] = {0,0,0,0};
  uint8_t tempGateway[4] = {0,0,0,0};
  unsigned short int dIndex=0, uIndex=0;
	
	//Try to load the network Configugartion file
	fileObject = SPIFFS.open(networkConfig, "r");
	if(!fileObject || fileObject.size()==0)
	{
		//  IF NO Network CONFIG FILE EXISTS IN SPIFS set network system to DHCP and set HostName to HOST+last 4 digits of MAC Address
		Serial.printf("\r\nNo Network configuration exists.");
		fileObject.close();
		networkMode=0;
		memcpy(hostName, tempHostName, strlen(tempHostName));
		hostName[9]=0;
	}
  else
  {
     //Network config found
    fileSize = fileObject.size();
    Serial.printf("\r\nNetwork configuration present [%d]Bytes", fileSize);
    tempData = new char[fileSize];
    fileObject.readBytes(tempData, fileSize);
    fileObject.close();
    for(fIndex=0; fIndex<fileSize; fIndex++)
    {
      Serial.printf("\r\n%d\t[%d][%c]", fIndex, tempData[fIndex], tempData[fIndex]);
    }
    //Set DHCP mode 1=ON 2=STATIC
    if(tempData[0]==1)
    {
      networkMode = 0;
    }
    else
    {
      networkMode = 1;
    }
    if(networkMode==0)
    {
      hostName = new char[fileSize];
      memcpy(hostName, tempData+1, fileSize-1);
      hostName[fileSize-1]=0;
      Serial.printf("\r\nDHCP ON\r\n\tHostname[%s][%d]", hostName, fileSize);
    }
    else
    {
      fIndex = findNeedleCount(tempData, fileSize, 0, 1)-1;
      hostName = new char[fIndex+1];
      memcpy(hostName, tempData+1, fIndex);
      hostName[fIndex]=0;
      Serial.printf("\r\nDHCP OFF\r\n\tHostname[%s][%d]", hostName, fileSize);
      fIndex = findNeedleCount(tempData, fileSize, 0, 1)+1;
      //IP address
      for(dIndex=0; dIndex<4; dIndex++)
      {
        tempIP[dIndex] = tempData[fIndex];
        fIndex++;
      }
      //IP mask
      for(dIndex=0; dIndex<4; dIndex++)
      {
        tempMask[dIndex] = tempData[fIndex];
        fIndex++;
      }
      //gateway
      for(dIndex=0; dIndex<4; dIndex++)
      {
        tempGateway[dIndex] = tempData[fIndex];
        fIndex++;
      }
      //set GLobal IP details
      my_ip = IPAddress(tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
      my_mask = IPAddress(tempMask[0], tempMask[1], tempMask[2], tempMask[3]);
      my_gateway = IPAddress(tempGateway[0], tempGateway[1], tempGateway[2], tempGateway[3]);
      
      Serial.printf("\r\nIP[%d.%d.%d.%d]", tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
      Serial.printf("\r\nMASK[%d.%d.%d.%d]", tempMask[0], tempMask[1], tempMask[2], tempMask[3]);
      Serial.printf("\r\nGATEWAY[%d.%d.%d.%d]", tempGateway[0], tempGateway[1], tempGateway[2], tempGateway[3]);
    }
  }
	
	//Try to load the ArtNet Configugartion file
	fileObject = SPIFFS.open(artNetConfig, "r");
	if(!fileObject || fileObject.size()==0)
	{
		if(systemMode==0)
    {
  		//IF NO artnet config file exists reboot the device
  		Serial.printf("\r\nNo ArtNet Configugartion exists.");
    }
    else
    {
      //1st time set up do nothing
    }
	}
  else
  {
    //load the artnet config
    fileSize = fileObject.size();
    tempData = new char[fileSize];
    fileObject.readBytes(tempData, fileSize);
    fileObject.close();
    numberOfDMXUniverses = fileSize/3;
    //Set up the artNetFrames array. Each universe has a 3 byte array, [Universe,Subnet,PixelCount]
    artNetFrames = new unsigned short int*[numberOfDMXUniverses];
    dIndex=0;
    for(uIndex=0; uIndex<numberOfDMXUniverses; uIndex++)
    {
      artNetFrames[uIndex] = new unsigned short int[3];
      artNetFrames[uIndex][0] = tempData[dIndex];
      artNetFrames[uIndex][1] = tempData[dIndex+1];
      artNetFrames[uIndex][2] = tempData[dIndex+2];
      Serial.printf("\r\nU[%d]\tS[%d]\tP[%d]", artNetFrames[uIndex][0], artNetFrames[uIndex][1], artNetFrames[uIndex][2]);
      dIndex+=3;
    }
   
  }
 

  //Try to load the Wifi Configugartion file
  fileObject = SPIFFS.open(wifiConfig, "r");
  if(!fileObject || fileObject.size()==0)
  {
    if(systemMode==0)
    {
      //IF NO WIFI config file exists reboot the device
      Serial.printf("\r\nNo WIFI Configugartion exists.");
      delay(2000);
      ESP.restart();
    }
    else
    {
      //1st time set up
      //Set wifi credentials to 0
      myWiFiSSID = new char[5];
      myWiFiKey = new char[5];
      memcpy(myWiFiSSID, blankText, strlen(blankText));
      myWiFiSSID[4]=0;
      memcpy(myWiFiKey, blankText, strlen(blankText));
      myWiFiKey[4]=0;
    }
  }
  else
  {
    //wifi config file exists
    //Read into temp buffer
    fileSize = fileObject.size();
    tempData = new char[fileSize];
    fileObject.readBytes(tempData, fileSize);
    fileObject.close();
    //Find ssid length
    ssidLength = findNeedleCount(tempData, fileSize, 0, 1)+1;
    //Find Key Length
    keyLength = fileSize - findNeedleCount(tempData, fileSize, 0, 1);
    localSSID = new char[ssidLength];
    localKEY = new char[keyLength];
    Serial.printf("\r\nSSIDLength[%d]\tKeyLentgh[%d]",ssidLength, keyLength);
    memcpy(localSSID, tempData, ssidLength);
    localSSID[ssidLength-1]=0;
    memcpy(localKEY, tempData+ssidLength, keyLength);
    localKEY[keyLength-1]=0;
    Serial.printf("\r\nSSID[%s]\tKey[%s]",localSSID, localKEY);
    //load into memory
    myWiFiSSID = new char[ssidLength];
    myWiFiKey = new char[keyLength];
    memcpy(myWiFiSSID, localSSID, ssidLength);
    memcpy(myWiFiKey, localKEY, keyLength);
  }
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Nothing Here :)");
}

void blankFunction()
{
  while(true)
  {
    yield();
  }
}

void handleRoot(AsyncWebServerRequest *request)
{
	File tempFile;

	tempFile = SPIFFS.open(indexPageFilePath, "r");
	indexFileSize = tempFile.size();
	tempFile.close();
	if(indexFileSize>0)
	{
		request->send(SPIFFS, indexPageFilePath, "text/html");
		Serial.printf("\r\nServed UI\tSent\t%d Bytes", indexFileSize);
	}
	else
	{
		request->send(200, "text/html", "UI not found.");
		Serial.printf("\r\nServed UI\tSent\t%d Bytes", indexFileSize);
	}
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  
  char* tempCommandString;
  
  //Hadnle socket COnnect event
  if(type == WS_EVT_CONNECT)
  {
    Serial.println("\r\nClient connected!");  
  }
  else if(type == WS_EVT_DISCONNECT)
  {
    Serial.println("\r\nClient Disconnected!");
  }
  //Handle Data reception
  else if(type == WS_EVT_DATA)
  {
	  //Store received command
	  tempCommandString = new char[len+1];
    memcpy(tempCommandString, data, len);
    tempCommandString[len]=0;   //Terminate string
    if(strcmp(tempCommandString, "getNetwork")==0)
    {
      Serial.printf("\r\nSending Network Config...");
      sendNetworkConfig(client);
    }
	  else if(strcmp(tempCommandString, "getArtnet")==0)
    {
      Serial.printf("\r\nSending ArtNet Config...");
      sendArtNetConfig(client);
    }
    else if(strcmp(tempCommandString, "getMacaddress")==0)
    {
      Serial.printf("\r\nSending MAC Address...");
      sendMacAddress(client);
    }
    else if(strcmp(tempCommandString, "getWifi")==0)
    {
      Serial.printf("\r\nSending Wifi Details...");
      sendWifiDetails(client);
    }
    else if(tempCommandString[0]=='W')
    {
      //Save WIFI Config
      saveWifiConfig(tempCommandString, len);
    }
    else if(tempCommandString[0]=='N')
    {
      //Save Network Config
      saveNetworkConfig(tempCommandString, len);
    }
    else if(tempCommandString[0]=='A')
    {
      //Save ArtNet Config
      saveArtNetConfig(tempCommandString, len);
    }
    else if(tempCommandString[0]=='R')
    {
      //Restart
      ESP.restart();
    }
  }
}

void saveArtNetConfig(char* configData, unsigned short int dataLength)
{
  File fileObject;
  unsigned short int bytesWritten=0;
  unsigned short int universeCount = (dataLength-1)/3;
  unsigned short int uCount=0;
  for(uCount=0; uCount<dataLength; uCount++)
  {
    Serial.printf("\r\n%d-[%d]", uCount, configData[uCount]);
  }
  Serial.printf("\r\nRecived\t[%d]\tUniverse configurations", universeCount);
  //write to artnet config file
  fileObject = SPIFFS.open(artNetConfig, "w");
  bytesWritten += fileObject.write((uint8_t*)configData+1, dataLength-1);
  fileObject.close();
  Serial.printf("\r\nSaved ArtNet Config with %d Bytes.", bytesWritten);
}

void saveNetworkConfig(char* configData, unsigned short int dataLength)
{
  File fileObject;
  unsigned short int bytesWritten=0;
  char* localHostName;
  char byteData[2] = {1,2};
  unsigned short int hostNameLength=0;
  unsigned short int dCounter=0;
  uint8_t tempIP[4] = {0,0,0,0};
  uint8_t tempMask[4] = {0,0,0,0};
  uint8_t tempGateway[4] = {0,0,0,0};
  //  networkMode   0=DHCP    1=STATIC

  for(dCounter=0; dCounter<dataLength; dCounter++)
  {
    Serial.printf("\r\n%d[%d][%c]", dCounter, configData[dCounter], configData[dCounter]);
  }
  networkMode = configData[1];
  if(networkMode==0)
  {
    //DHCP ON set hostname and nothing else
    hostNameLength = dataLength-2;
    localHostName = new char[hostNameLength+1];
    memcpy(localHostName, configData+2, hostNameLength);
    localHostName[hostNameLength]=0;
    Serial.printf("DHCP ON\tHOSTNAME[%s]", localHostName);
    //write to wifi config file
    fileObject = SPIFFS.open(networkConfig, "w");
    bytesWritten += fileObject.write((uint8_t*)byteData, 1);
    bytesWritten += fileObject.write((uint8_t*)localHostName, hostNameLength);
    fileObject.close();
    Serial.printf("\r\nSaved Network Config with %d Bytes.", bytesWritten);
  }
  else
  {
    //DHCP OF set hostname & ALL IP Details, NO DNS required
    hostNameLength = dataLength-2-12;
    localHostName = new char[hostNameLength+1];
    memcpy(localHostName, configData+2, hostNameLength);
    localHostName[hostNameLength]=0;
    Serial.printf("\r\nSTATIC IP\tHOSTNAME[%s]", localHostName);
    //store IP Address
    for(dCounter=0; dCounter<4; dCounter++)
    {
      tempIP[dCounter] = configData[hostNameLength+2+dCounter];
    }
    //store mask Address
    for(dCounter=0; dCounter<4; dCounter++)
    {
      tempMask[dCounter] = configData[hostNameLength+2+dCounter+4];
    }
    //store gatewat Address
    for(dCounter=0; dCounter<4; dCounter++)
    {
      tempGateway[dCounter] = configData[hostNameLength+2+dCounter+8];
    }
    Serial.printf("\r\nIP[%d.%d.%d.%d]", tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
    Serial.printf("\r\nMASK[%d.%d.%d.%d]", tempMask[0], tempMask[1], tempMask[2], tempMask[3]);
    Serial.printf("\r\nGATEWAY[%d.%d.%d.%d]", tempGateway[0], tempGateway[1], tempGateway[2], tempGateway[3]);
    //write to wifi config file
    fileObject = SPIFFS.open(networkConfig, "w");
    //DHCP Mode
    byteData[0] = 2;
    bytesWritten += fileObject.write((uint8_t*)byteData, 1);
    //HostName
    bytesWritten += fileObject.write((uint8_t*)localHostName, hostNameLength);
    //NullSpace
    byteData[0]=0;
    bytesWritten += fileObject.write((uint8_t*)byteData, 1);
    //IP Address
    bytesWritten += fileObject.write((uint8_t*)tempIP, 4);
    //Mask
    bytesWritten += fileObject.write((uint8_t*)tempMask, 4);
    //Gateway
    bytesWritten += fileObject.write((uint8_t*)tempGateway, 4);
    
    fileObject.close();
    Serial.printf("\r\nSaved Network Config with %d Bytes.", bytesWritten);
  }
  
}

void saveWifiConfig(char* configData, unsigned short int dataLength)
{
  File fileObject;
  unsigned short int bytesWritten=0;
  char* localSSID;
  char* localKEY;
  unsigned short int ssidLength = findNeedleCount(configData, dataLength, 0, 1)-1+1;
  unsigned short int keyLength = dataLength-findNeedleCount(configData, dataLength, 0, 1)-1+1;
  localSSID = new char[ssidLength];
  localKEY = new char[keyLength];
  unsigned short int dCounter=0;
  char byteData[2] = {0,1};
  
  Serial.printf("\r\nSSID[%d]\tKEY[%d]", ssidLength, keyLength);
  for(dCounter=0; dCounter<dataLength; dCounter++)
  {
    Serial.printf("\r\n%d[%d][%c]", dCounter, configData[dCounter], configData[dCounter]);
  }
  memcpy(localSSID, configData+1, ssidLength);
  localSSID[ssidLength-1]=0;
  memcpy(localKEY, configData+1+ssidLength, keyLength);
  localKEY[keyLength-1]=0;
  Serial.printf("\r\nSSID[%s]\tKEY[%s]",localSSID, localKEY);
  
  //write to wifi config file
  fileObject = SPIFFS.open(wifiConfig, "w");
  bytesWritten += fileObject.write((uint8_t*)localSSID, ssidLength);
  bytesWritten += fileObject.write((uint8_t*)byteData[0], 1);
  bytesWritten += fileObject.write((uint8_t*)localKEY, keyLength);
  fileObject.close();
  Serial.printf("\r\nSaved WIFI Config with %d Bytes.", bytesWritten);
  
}

void sendNetworkConfig(AsyncWebSocketClient * client)
{
  /*
	DHCP Mode	1 Byte
	HostName	STRLEN(hostName)
	IPAddress	4 Bytes
	Subnet		4 Bytes
	GateWay		4 Bytes
  */
  unsigned short int dataLength = 1+strlen(hostName)+4+4+4+1;
  char *dataToSend = new char[dataLength];
  uint8_t index=0;
  
  //set DHCP Mode
  if(networkMode==0)
  {
	  dataToSend[0] = 0;
  }
  else
  {
	  dataToSend[0] = 1;
  }
  index++;
  //HostName
  memcpy(dataToSend+index, hostName, strlen(hostName));
  index+=strlen(hostName);
  //IP Address
  dataToSend[index] = my_ip[0];
  index++;
  dataToSend[index] = my_ip[1];
  index++;
  dataToSend[index] = my_ip[2];
  index++;
  dataToSend[index] = my_ip[3];
  index++;
  //Subnet
  dataToSend[index] = my_mask[0];
  index++;
  dataToSend[index] = my_mask[1];
  index++;
  dataToSend[index] = my_mask[2];
  index++;
  dataToSend[index] = my_mask[3];
  index++;
  //gateway
  dataToSend[index] = my_gateway[0];
  index++;
  dataToSend[index] = my_gateway[1];
  index++;
  dataToSend[index] = my_gateway[2];
  index++;
  dataToSend[index] = my_gateway[3];
  index++;
  dataToSend[index] = 0;
  
  for(index=0; index<dataLength; index++)
  {
    Serial.printf("\r\n[%d]=[%d][%c]", index, dataToSend[index], dataToSend[index]);
  }
  //TX to browser
  ws.binary(client->id(), dataToSend, dataLength);
}

void sendArtNetConfig(AsyncWebSocketClient * client)
{
  uint8_t* dataToSend;
  unsigned short int dataLength = 0;
  unsigned short int uCount = 0, dIndex=0;
  
  if(numberOfDMXUniverses==0)
  {
    dataLength = 1;
    dataToSend = new uint8_t[dataLength];
    dataToSend[0] = 0;
    ws.binary(client->id(), dataToSend, dataLength);
  }
  else
  {
    dataLength = numberOfDMXUniverses*3;
    dataToSend = new uint8_t[dataLength];
    for(uCount=0; uCount<numberOfDMXUniverses; uCount++)
    {
      dataToSend[dIndex] = artNetFrames[uCount][0];
      dataToSend[dIndex+1] = artNetFrames[uCount][1];
      dataToSend[dIndex+2] = artNetFrames[uCount][2];
      dIndex+=3;
    }
    ws.binary(client->id(), dataToSend, dataLength);
  }
}

void sendMacAddress(AsyncWebSocketClient * client)
{
  uint8_t wifiMacAddress[6] = {0,0,0,0,0,0};
  
  esp_wifi_get_mac(WIFI_IF_STA, wifiMacAddress);
  //TX to browser
  ws.binary(client->id(), wifiMacAddress, 6);
}

void sendWifiDetails(AsyncWebSocketClient * client)
{
  unsigned short int dataLength = strlen(myWiFiSSID)+1+strlen(myWiFiKey);
  char* dataToSend = new char[dataLength];
  unsigned short int dIndex=0;
  
  memcpy(dataToSend, myWiFiSSID, strlen(myWiFiSSID));
  dIndex=strlen(myWiFiSSID);
  dataToSend[dIndex]=0;
  dIndex++;
  memcpy(dataToSend+dIndex, myWiFiKey, strlen(myWiFiKey));
  dIndex+=strlen(myWiFiKey);
  dataToSend[dIndex]=0;
  ws.binary(client->id(), dataToSend, dataLength);
  Serial.printf("\r\nssid[%d]\tkey[%d]\rtotal[%d]", strlen(myWiFiSSID),strlen(myWiFiKey), dataLength);
  for(dIndex=0; dIndex<dataLength; dIndex++)
  {
    Serial.printf("\r\n%d->[%d][%c]", dIndex, dataToSend[dIndex], dataToSend[dIndex]);
  }
}

short int findNeedleCount(char* haystack, unsigned short int hayStackLength, char needle, unsigned short needleCount)
{
  unsigned short int hayCount=0, nCount=0;
  for(hayCount; hayCount<hayStackLength; hayCount++)
  {
    if(haystack[hayCount]==needle)
    {
      nCount++;
      if(nCount==needleCount)
      {
        return hayCount;
      }
    }
  }
  return -1;
}
short int countNeedles(char* haystack, unsigned short int hayStackLength, char needle)
{
  unsigned short int found;
  unsigned short int hayCount=0;
  
  for(hayCount; hayCount<hayStackLength; hayCount++)
  {
    if(haystack[hayCount]==needle)
    {
      found++;
    }
  }
  return found;
}

//---------------   end web ui functionality  ----------------------

byte hasTimedOut()
{
  timeData[1] = millis();
  if(timeData[2] < timeData[1]-timeData[0])
  {
    return 1;
  }
  return 0;
}
void startTimer(unsigned long durationInMillis)
{
  timeData[0] = millis(); 
  timeData[2] = durationInMillis;
}

void loop()
{   
  if(systemMode==1)
  {
    //Config Mode
	  File fileObject; 
    indexFileSize = 0;
    fileObject = SPIFFS.open(indexPageFilePath, "r");
    if(!fileObject)
    {
        while(true)
        {
          Serial.println("\r\n\tFAILED to open UI file for reading please UPLOAD index file from DATA directory via SKETCH DATA UPLOAD TOOL");
          delay(1000);
          yield();
        }
    }
    else
    {
      Serial.print("\r\n\tUI File present");
      indexFileSize = fileObject.size();
      fileObject.close();
      Serial.printf("\t%d\tBytes\r\n", indexFileSize); 
      blankFunction();
    }
  }
  else if(systemMode==0)
  {
    //ArtNet Mode
    if(totalPixels>0)
    {
      //ledStrip.showPixels();
      //delay(2);
      yield();
    }
  }
}

//ARTNET STUFF
void pollDMX(AsyncUDPPacket &packet)
{
     unsigned short int uCount=0, testCounter=0;;
     uint8_t subnetPart=0, universePart=0;
      
     packetSize = packet.length();
     if(packetSize==maxPacketBufferSize)
     {
        memcpy(packetBuffer, packet.data(), maxPacketBufferSize);
        for(uCount=0; uCount<numberOfDMXUniverses; uCount++)
        {
          //packetBuffer[14] is the UNIVERSE byte
          //packetBuffer[15] is the SUBNET byte
          subnetPart = packetBuffer[14]>>4;
          universePart = packetBuffer[14]-((packetBuffer[14]>>4)<<4);
          //if(artNetFrames[uCount][0]==packetBuffer[14] && artNetFrames[uCount][1]==packetBuffer[15])
          if(artNetFrames[uCount][0]==universePart && artNetFrames[uCount][1]==subnetPart)
          {
            //Serial.printf("\r\nGot Data for U[%d]S[%d]",packetBuffer[14],packetBuffer[15] );
            artNetData.parseArtNetPacket(packetBuffer);
            writeToPixels(uCount);
            ledStrip.showPixels();
            return;
          }
        }
     }
}

void writeToPixels(uint8_t universeId)
{
  unsigned short int uCount=0, pIndex=0, pStart=0, dmxIndex=0;

  //work our the statting pixel based on its order in teh universe config
  for(uCount=0; uCount<universeId; uCount++)
  {
    pStart+=artNetFrames[uCount][2];
  }
  //render this universes pixel count to the pixels
  for(pIndex=0; pIndex<artNetFrames[universeId][2]; pIndex++)
  {
    ledStrip.setPixel(pStart+pIndex, artNetData.data[dmxIndex], artNetData.data[dmxIndex+1], artNetData.data[dmxIndex+2], artNetData.data[dmxIndex+3]);
    dmxIndex+=4;
  }

  //Serial.printf("\r\n\tRendeing [%d] pixels", artNetFrames[universeId][2] );
}
