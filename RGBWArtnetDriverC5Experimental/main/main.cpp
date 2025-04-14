/* 	Experimental Build
   
	idf.py --preview set-target esp32c5
	idf.py menuconfig
	idf.py build
	idf.py -p com4 flash
	idf.py -p com4 flash monitor

*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <netdb.h>
#include "esp_eth.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include "esp_mac.h"
//SPI
#include "spi_flash_mmap.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
//SPIFFS
#include "esp_spiffs.h"
//HTTP Server
#include "esp_tls_crypto.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include <esp_http_server.h>
#include "esp_netif.h"
#include "esp_tls.h"
#include <sys/param.h>
//GPIO
#include "driver/gpio.h"
#define BOOT_PIN	GPIO_NUM_6
//SOFT AP
#include "lwip/inet.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif

extern "C" {
		void app_main(void);
}

//custom
#include "NeoViaSPI.h"
#include "artNetPacket.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (512)
#define EXAMPLE_ESP_MAXIMUM_RETRY 10
#define UDP_RX_PORT 6454

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK

//SPI config
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  24
#define maxSPIFrequency 3333333
#define maxSPIFrameInBytes 8000
//SPI Vars
spi_device_handle_t spi;
spi_transaction_t spiTransObject;
esp_err_t ret;
spi_bus_config_t buscfg;
spi_device_interface_config_t devcfg;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WIFI UDP RX Node";
static int s_retry_num = 0;

//Pixels
NeoViaSPI* leds;

//Contsnats for local SPIFFS file names
char* webUI = "/spiffs/index.html";
char* configFile = "/spiffs/configFile";

//structure for websockets
struct async_resp_arg 
{
    httpd_handle_t hd;
    int fd;
};
//structure for storing the config file during set up
struct configFileStore 
{
    uint8_t loaded = 0;
	unsigned char deviceMAC[6] = {0,0,0,0,0,0};
	char* deviceSSID;
    char* deviceKEY;
	uint8_t deviceDHCPMODE=0;
	uint8_t deviceIP[4] = {0,0,0,0};
	uint8_t deviceSUBNET[4] = {0,0,0,0};
	uint8_t deviceGATEWAY[4] = {0,0,0,0};
	char* deviceHOSTNAME;
	uint8_t deviceUNIVERSE_COUNT=0;
	uint8_t* deviceUNIVERSE_LIST;
	uint8_t* deviceSUBNET_LIST;
	uint8_t* devicePIXEL_LIST;
	char* fileSaveDataBuffer;
	unsigned short int fileDataBufferLength=0;
};
uint8_t action=0;
struct configFileStore deviceConfig;
//structure for storing self hosted wifi soft access point
struct saConfig
{
	uint8_t deviceIP[4] = {10,10,10,1};
	uint8_t deviceSUBNET[4] = {255,255,255,0};
	uint8_t deviceGATEWAY[4] = {10,10,10,1};
	char* softAPSSID = "WOW-AP";
	char* softAPKEY = "wowaccesspoint";
	uint8_t deviceWIFIAPChanel = 1;
};
struct saConfig softAPConfig;
int bootMode = 0;

//artNet Stuff
artNetPacket artNetData;
const short int maxPacketBufferSize = 530;
char packetBuffer[maxPacketBufferSize];


//utility search fucntion that returns the INDEX of a CHAR value counted by needleCount
short int findNeedleCount(char* haystack, unsigned short int hayStackLength, char needle, unsigned short needleCount)
{
  unsigned short int hayCount=0, nCount=0;
  for(hayCount=0; hayCount<hayStackLength; hayCount++)
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
  unsigned short int found=0;
  unsigned short int hayCount=0;
  
  for(hayCount=0; hayCount<hayStackLength; hayCount++)
  {
    if(haystack[hayCount]==needle)
    {
      found++;
    }
  }
  return found;
}
//-----------------

void testBootPin()
{
	//zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1<<6;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
	
	gpio_set_direction(BOOT_PIN, GPIO_MODE_INPUT);
	bootMode = gpio_get_level(BOOT_PIN);
	printf("\r\n\t\tBoot Pin is\t[%d]\r\n", bootMode);
}

void clearCharBlock(char* data, unsigned short int length)
{
	unsigned short int dIndex=0;
	for(dIndex=0; dIndex<length; dIndex++)
	{
		data[dIndex]=0;
	}
}
//WIFI SOFT AP

//--------------------------------------------------------------------------------------------------------------------------

unsigned short int getFileSize(char* fileName)
{
	FILE* fileObject;
	unsigned short int fileSize=0;
	fileObject = fopen(fileName, "r");
	if(fileObject == NULL)
	{
		return 0;
	}
	else
	{
		fseek(fileObject, 0L, SEEK_END);
		fileSize = ftell(fileObject);
		rewind(fileObject);
		fclose(fileObject);
		return fileSize;
	}
}
void returnFileContents(char* fileName, char* buffer)
{
	FILE* fileObject;
	unsigned short int fileSize = getFileSize(fileName);
	unsigned short int fIndex=0;
	
	fileObject = fopen(fileName, "r");
	if(fileObject == NULL)
	{
		return;
	}
	else
	{
		//printf("\r\n\tFile[%s]\tsize[%d]", fileName, fileSize);
		while(fIndex<fileSize)
		{
			buffer[fIndex] = fgetc(fileObject);
			fIndex++;
		}
		fclose(fileObject);
		return;
	}
}

uint8_t writeConfigFile(char* dataToWrite, unsigned short int dataLength, char* fileName)
{
	FILE* fileObject;
	unsigned short int bytesWritten=0;
	fileObject = fopen(fileName, "w");
	bytesWritten = fwrite(dataToWrite, dataLength, 1, fileObject)*dataLength;
	fclose(fileObject);
	return bytesWritten;
}
void readConfigFile(char* fileName)
{
	unsigned short int bufferIndex=0;
	short int startIndex=0, endIndex=0;
	char fieldSeperator = 9;
	deviceConfig.fileDataBufferLength = getFileSize(fileName);
	if(deviceConfig.fileDataBufferLength>0)
	{
		printf("\r\n\tLoading config from SPIFFS...");
		deviceConfig.fileSaveDataBuffer = new char[deviceConfig.fileDataBufferLength];
		clearCharBlock(deviceConfig.fileSaveDataBuffer, deviceConfig.fileDataBufferLength);
		returnFileContents(fileName, deviceConfig.fileSaveDataBuffer);
		//MAC Address
		memcpy(deviceConfig.deviceMAC, deviceConfig.fileSaveDataBuffer, 6);
		bufferIndex+=6;
		//SSID
		endIndex = findNeedleCount(deviceConfig.fileSaveDataBuffer, deviceConfig.fileDataBufferLength, fieldSeperator, 1)-bufferIndex;
		deviceConfig.deviceSSID = new char[endIndex+1];
		clearCharBlock(deviceConfig.deviceSSID, endIndex+1);
		memcpy(deviceConfig.deviceSSID, deviceConfig.fileSaveDataBuffer+bufferIndex, endIndex);
		bufferIndex+=endIndex+1;
		//KEY
		endIndex = findNeedleCount(deviceConfig.fileSaveDataBuffer, deviceConfig.fileDataBufferLength, fieldSeperator, 2)-bufferIndex;
		deviceConfig.deviceKEY = new char[endIndex+1];
		clearCharBlock(deviceConfig.deviceKEY, endIndex+1);
		memcpy(deviceConfig.deviceKEY, deviceConfig.fileSaveDataBuffer+bufferIndex, endIndex);
		bufferIndex+=endIndex+1;
		//DHCP Mode 0=on 1=off
		deviceConfig.deviceDHCPMODE = deviceConfig.fileSaveDataBuffer[bufferIndex];
		bufferIndex++;
		//IP ADDRESS
		memcpy(deviceConfig.deviceIP, deviceConfig.fileSaveDataBuffer+bufferIndex, 4);
		bufferIndex+=4;
		//SUBNET MASK
		memcpy(deviceConfig.deviceSUBNET, deviceConfig.fileSaveDataBuffer+bufferIndex, 4);
		bufferIndex+=4;
		//GATEWAY
		memcpy(deviceConfig.deviceGATEWAY, deviceConfig.fileSaveDataBuffer+bufferIndex, 4);
		bufferIndex+=4;
		//HOSTNAME
		endIndex = findNeedleCount(deviceConfig.fileSaveDataBuffer, deviceConfig.fileDataBufferLength, fieldSeperator, 3)-bufferIndex;
		deviceConfig.deviceHOSTNAME = new char[endIndex+1];
		clearCharBlock(deviceConfig.deviceHOSTNAME, endIndex+1);
		memcpy(deviceConfig.deviceHOSTNAME, deviceConfig.fileSaveDataBuffer+bufferIndex, endIndex);
		bufferIndex+=endIndex+1;
		//Output Count
		deviceConfig.deviceUNIVERSE_COUNT = deviceConfig.fileSaveDataBuffer[bufferIndex];
		bufferIndex++;
		//Universe List
		deviceConfig.deviceUNIVERSE_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		memcpy(deviceConfig.deviceUNIVERSE_LIST, deviceConfig.fileSaveDataBuffer+bufferIndex, deviceConfig.deviceUNIVERSE_COUNT);
		bufferIndex+=deviceConfig.deviceUNIVERSE_COUNT;
		//SUBNET List
		deviceConfig.deviceSUBNET_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		memcpy(deviceConfig.deviceSUBNET_LIST, deviceConfig.fileSaveDataBuffer+bufferIndex, deviceConfig.deviceUNIVERSE_COUNT);
		bufferIndex+=deviceConfig.deviceUNIVERSE_COUNT;
		//PX List
		deviceConfig.devicePIXEL_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		memcpy(deviceConfig.devicePIXEL_LIST, deviceConfig.fileSaveDataBuffer+bufferIndex, deviceConfig.deviceUNIVERSE_COUNT);
		bufferIndex+=deviceConfig.deviceUNIVERSE_COUNT;
		//debug
		printf("\r\n\tDevice MAC\t\t[%02X:%02X:%02X:%02X:%02X:%02X]", deviceConfig.deviceMAC[0], deviceConfig.deviceMAC[1], deviceConfig.deviceMAC[2], deviceConfig.deviceMAC[3], deviceConfig.deviceMAC[4], deviceConfig.deviceMAC[5]);
		printf("\r\n\tSSID\t\t[%s]", deviceConfig.deviceSSID);
		printf("\r\n\tKEY\t\t[%s]", deviceConfig.deviceKEY);
		printf("\r\n\tDHCP MODE\t\t[%d]", deviceConfig.deviceDHCPMODE);
		printf("\r\n\tIP\t\t[%d.%d.%d.%d]", deviceConfig.deviceIP[0], deviceConfig.deviceIP[1], deviceConfig.deviceIP[2], deviceConfig.deviceIP[3]);
		printf("\r\n\tMASK\t\t[%d.%d.%d.%d]", deviceConfig.deviceSUBNET[0], deviceConfig.deviceSUBNET[1], deviceConfig.deviceSUBNET[2], deviceConfig.deviceSUBNET[3]);
		printf("\r\n\tGATEWAY\t\t[%d.%d.%d.%d]", deviceConfig.deviceGATEWAY[0], deviceConfig.deviceGATEWAY[1], deviceConfig.deviceGATEWAY[2], deviceConfig.deviceGATEWAY[3]);
		printf("\r\n\tHOSTNAME\t\t[%s]", deviceConfig.deviceHOSTNAME);
		printf("\r\n\tOutputs\t\t[%d]", deviceConfig.deviceUNIVERSE_COUNT);		
		for(bufferIndex=0; bufferIndex<deviceConfig.deviceUNIVERSE_COUNT; bufferIndex++)
		{
			printf("\r\n\tOutput\t[%d]\tU[%d]\tS[%d]\tPxCount[%d]", bufferIndex, deviceConfig.deviceUNIVERSE_LIST[bufferIndex], deviceConfig.deviceSUBNET_LIST[bufferIndex], deviceConfig.devicePIXEL_LIST[bufferIndex]);
		}
		printf("\r\n");
		deviceConfig.loaded = 1;
		return;
	}
	else
	{
		printf("\r\n\tNo Config file to load.");
		esp_read_mac(deviceConfig.deviceMAC, ESP_MAC_WIFI_STA);
		deviceConfig.loaded = 0;
		return;
	}	
}

void initSPIFFS()
{
	int returnData=0, fTrack=0;
	int fileSize=0;
	char* fileData;
	
	esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
	returnData = esp_vfs_spiffs_register(&conf);
	printf("\r\nInitializing SPIFFS\t[%d]", returnData);
}
int setupSPI()
{
	int result;
	//Set up the Bus Config struct	
	buscfg.miso_io_num=-1;
	buscfg.mosi_io_num=PIN_NUM_MOSI;
	buscfg.sclk_io_num=PIN_NUM_CLK;
	buscfg.quadwp_io_num=-1;
	buscfg.quadhd_io_num=-1;
	buscfg.max_transfer_sz=maxSPIFrameInBytes;
	
	//Set up the SPI Device Configuration Struct
	devcfg.clock_speed_hz=maxSPIFrequency;
	devcfg.mode=0;                        
	devcfg.spics_io_num=-1;  //CS PIN           
	devcfg.queue_size=1;

	//Initialize the SPI driver
	result=spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    printf("\r\n\tINIT SPI DRIVER RESULT\t[%d]", result);
	//Add SPI port to bus
	result=spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
	printf("\r\n\tADDING SPI BUS RESULT\t[%d]", result);
	return result;
	
	//Set up SPI tx/rx storage Object
	memset(&spiTransObject, 0, sizeof(spiTransObject));
	spiTransObject.length = leds->_NeoBitsframeLength*8;
	spiTransObject.tx_buffer = leds->neoBits;
	printf("SPI Object Initilized...\r\n");
}

static esp_err_t example_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE))
	{
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static void example_set_static_ip(esp_netif_t *netif)
{
    
	if (esp_netif_dhcpc_stop(netif) != ESP_OK) 
	{
        printf("\r\n[%s]\tFailed to stop dhcp client", TAG);
        return;
    }
	if(deviceConfig.loaded==1)
	{
		//Config file exists and has been preloaded
		if(deviceConfig.deviceDHCPMODE==1)
		{
			esp_netif_ip_info_t ip;
			memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
			//ip.ip.addr = ipaddr_addr(EXAMPLE_STATIC_IP_ADDR);
			ip.ip.addr = ESP_IP4TOADDR(deviceConfig.deviceIP[0], deviceConfig.deviceIP[1], deviceConfig.deviceIP[2], deviceConfig.deviceIP[3]) ;
			//ip.netmask.addr = ipaddr_addr(EXAMPLE_STATIC_NETMASK_ADDR);
			ip.netmask.addr = ESP_IP4TOADDR(deviceConfig.deviceSUBNET[0], deviceConfig.deviceSUBNET[1], deviceConfig.deviceSUBNET[2], deviceConfig.deviceSUBNET[3]);
			//ip.gw.addr = ipaddr_addr(EXAMPLE_STATIC_GW_ADDR);
			ip.gw.addr = ESP_IP4TOADDR(deviceConfig.deviceGATEWAY[0], deviceConfig.deviceGATEWAY[1], deviceConfig.deviceGATEWAY[2], deviceConfig.deviceGATEWAY[3]);
			if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) 
			{
				printf("\r\n[%s]\tFailed to set static ip info from config file..", TAG);
			}
			else
			{
				printf("\r\n\tSuccess to set static IP");//\t[%s]\tmask\t[%s]Gateway\t[%s]\r\n", TAG, EXAMPLE_STATIC_IP_ADDR, EXAMPLE_STATIC_NETMASK_ADDR, EXAMPLE_STATIC_GW_ADDR);    
				printf("[%d.%d.%d.%d]\tMask[%d.%d.%d.%d]\tGateway[%d.%d.%d.%d]\r\n", deviceConfig.deviceIP[0],deviceConfig.deviceIP[1],deviceConfig.deviceIP[2],deviceConfig.deviceIP[3],deviceConfig.deviceSUBNET[0], deviceConfig.deviceSUBNET[1], deviceConfig.deviceSUBNET[2], deviceConfig.deviceSUBNET[3],deviceConfig.deviceGATEWAY[0], deviceConfig.deviceGATEWAY[1], deviceConfig.deviceGATEWAY[2], deviceConfig.deviceGATEWAY[3]);
				//ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr(EXAMPLE_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
				//ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr(EXAMPLE_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
			}
		}
		else
		{
			//DHCP is ON return and do nothing
		}
	}
	else
	{
		//save the devices MAC address
		esp_read_mac(deviceConfig.deviceMAC, ESP_MAC_WIFI_STA);
	}
	printf("\r\nDevice MAC Address\t[%02X:%02X:%02X:%02X:%02X:%02X]", deviceConfig.deviceMAC[0], deviceConfig.deviceMAC[1], deviceConfig.deviceMAC[2], deviceConfig.deviceMAC[3],deviceConfig.deviceMAC[4], deviceConfig.deviceMAC[5]);	

}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
	{
		esp_wifi_connect();
    }
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
		{
            esp_wifi_connect();
            s_retry_num++;
            printf("\r\n[%s] Retry Connect to the AP", TAG);
        }
		else
		{
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
		printf("\r\n[%s] connect to the AP fail", TAG);
    }
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		esp_ip4_addr_t* deviceIP = &event->ip_info.ip;
        printf("\r\n\t[%s]\tGot IP:[%d.%d.%d.%d]",TAG, esp_ip4_addr_get_byte(deviceIP, 0),esp_ip4_addr_get_byte(deviceIP, 1),esp_ip4_addr_get_byte(deviceIP, 2),esp_ip4_addr_get_byte(deviceIP, 3) );
		s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(deviceConfig.loaded==1)
	{
		if(deviceConfig.deviceDHCPMODE==1)
		{
			esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
			assert(sta_netif);
			//set static IP
			example_set_static_ip(sta_netif);
		}
		else
		{
			esp_netif_create_default_wifi_sta();
		}
	}
	else
	{
		esp_netif_create_default_wifi_sta();
	}
	
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,&instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler, NULL,&instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = {EXAMPLE_ESP_WIFI_SSID},
            //.password = {EXAMPLE_ESP_WIFI_PASS},
			.threshold = {.rssi = {}, .authmode = WIFI_AUTH_WPA_PSK},
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = CONFIG_ESP_WIFI_ENABLED
        },
    };	
	memcpy(wifi_config.sta.ssid, deviceConfig.deviceSSID, strlen(deviceConfig.deviceSSID));
	memcpy(wifi_config.sta.password, deviceConfig.deviceKEY, strlen(deviceConfig.deviceKEY));
	
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

	printf("\r\n[%s]\twifi_init_sta finished.", TAG);
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE, pdFALSE,portMAX_DELAY);
    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
	{
        printf("\r\n[%s]\tconnected to ap SSID:[%s]\tKey:[%s]", TAG, deviceConfig.deviceSSID, deviceConfig.deviceKEY);
    }
	else if (bits & WIFI_FAIL_BIT)
	{
        printf("\r\n[%s]\tFAILED to connected to ap SSID:[%s]\tKey:[%s]", TAG, deviceConfig.deviceSSID, deviceConfig.deviceKEY);
    }
	else 
	{
        printf("\r\n[%s]\tUNEXPECTED EVENT", TAG);
    }
}

static void udp_client_task(void *pvParameters)
{
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
	struct sockaddr_in  self_ap_addr;
	//artnet stuff
	unsigned short int uCount=0, testCounter=0;;
    uint8_t subnetPart=0, universePart=0;
	unsigned short int uCCount=0, pIndex=0, pStart=0, dmxIndex=0;
	uint8_t tempColour[4] = {0,0,0,0};
	 
	self_ap_addr.sin_family = addr_family;
    self_ap_addr.sin_port = htons(UDP_RX_PORT);
	
    while (1)
	{
        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
		{
            printf("\r\n[%s]\tUnable to create socket: errno [%d]", TAG, errno);
            break;
        }
		if(bind(sock, (const struct sockaddr *)&self_ap_addr,  sizeof(struct sockaddr_in)) < 0 )
		{
		  printf("\r\n[%s]\tAP UDP socket not binded", TAG);
		  shutdown(sock, 0);
		  close(sock);
		  break;
		}

  
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
		//printf("\r\n[%s]\tSocket created", TAG);
        
		while(1)
		{
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, packetBuffer, maxPacketBufferSize, 0, (struct sockaddr *)&source_addr, &socklen);
			// Error occurred during receiving
            if (len < 0)
			{
				//printf("\r\n[%s]\trecvfrom failed: errno %d", TAG, errno);               
                break;
            }
            // Data received
            else
			{
                if(len==maxPacketBufferSize)
				{
					for(uCount=0; uCount<deviceConfig.deviceUNIVERSE_COUNT; uCount++)
					{
					  subnetPart = packetBuffer[14]>>4;
					  universePart = packetBuffer[14]-((packetBuffer[14]>>4)<<4);
					  if(deviceConfig.deviceUNIVERSE_LIST[uCount]==universePart && deviceConfig.deviceSUBNET_LIST[uCount]==subnetPart)
					  {
						//printf("\r\nGot packet\rUniverse\t[%d]\tSubnet\t[%d]", universePart, subnetPart);
						artNetData.parseArtNetPacket(packetBuffer);
						pStart = 0;
						dmxIndex = 0;
						//work our the statting pixel based on its order in teh universe config
						for(uCCount=0; uCCount<uCount; uCCount++)
						{
							pStart += deviceConfig.devicePIXEL_LIST[uCCount];
						}
						//render this universes pixel count to the pixels
						for(pIndex=0; pIndex<deviceConfig.devicePIXEL_LIST[uCCount]; pIndex++)
						{
							tempColour[0] =  artNetData.data[dmxIndex];
							tempColour[1] =  artNetData.data[dmxIndex+1];
							tempColour[2] =  artNetData.data[dmxIndex+2];
							tempColour[3] =  artNetData.data[dmxIndex+3];
							leds->setPixel(pStart+pIndex, tempColour);
							dmxIndex+=4;
						}
					  }
					}
				}
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
		{
            //printf("\r\n[%s]\tShutting down socket and restarting...", TAG);
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
//		http server stuff
static esp_err_t root_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
	unsigned short int fileSize=0;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    printf("\r\n\t\tbuf_len1 => [%d]", buf_len);
	if (buf_len > 1) 
	{
		buf = new char[buf_len];
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) 
		{
            printf("\r\n\t\tFound header => Host: %s", buf);
        }
    }
	
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
	fileSize = getFileSize(webUI);
    printf("\r\n\tFilesize is [%d]", fileSize);
	char* fileDataBuffer = new char[fileSize];
	returnFileContents(webUI, fileDataBuffer);
	const char* resp_str = (const char*) fileDataBuffer;//req->user_ctx;
    httpd_resp_send(req, resp_str, fileSize);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) 
	{
        printf("\r\n\t\tRequest headers lost");
    }
    return ESP_OK;
}
//WEB Socket Stuff
static void ws_async_send(void *arg)
{
    char* output;
	unsigned short int dataLength=2;
	struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
	httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));	
	if(action==0)
	{
		dataLength=2;
		output = new char[dataLength];
		clearCharBlock(output, dataLength);
		output[0] = '0';
	}
	else if(action==1)
	{
		//Save wifi Credentials
		dataLength=2;
		output = new char[dataLength];
		clearCharBlock(output, dataLength);
		output[0] = '1';
	}
	else if(action==2)
	{
		//Save network details
		dataLength=2;
		output = new char[dataLength];
		clearCharBlock(output, dataLength);
		output[0] = '2';
	}
	else if(action==3)
	{
		//Save artnet config
		dataLength=2;
		output = new char[dataLength];
		clearCharBlock(output, dataLength);
		output[0] = '3';
	}
	else if(action==4)
	{
		//Save config and restart
		dataLength=2;
		output = new char[dataLength];
		clearCharBlock(output, dataLength);
		output[0] = '4';
	}
	else if(action==5)
	{
		//load config
		//check if config file exists
		dataLength = getFileSize(configFile);
		if(dataLength>0)
		{
			//config file present sent mac address
			output = new char[dataLength+1];
			clearCharBlock(output, dataLength+1);
			returnFileContents(configFile, output);
		}
		else
		{
			//no config file present just send MAC address
			dataLength = 6;
			output = new char[dataLength];
			clearCharBlock(output, dataLength);
			memcpy(output, deviceConfig.deviceMAC, 6);
		}
	}
	else
	{
		dataLength = getFileSize(configFile);
		output = new char[dataLength+1];
		clearCharBlock(output, dataLength+1);
		returnFileContents(configFile, output);
	}
	ws_pkt.payload = (uint8_t*)output;
	ws_pkt.len = dataLength;
    //ws_pkt.type = HTTPD_WS_TYPE_TEXT;HTTPD_WS_TYPE_BINARY
	ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    
	httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
	if(action==4)
	{
		esp_restart();
	}
}
static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req, uint8_t actionToTake)
{
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) 
	{
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
	action = actionToTake;
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) 
	{
        free(resp_arg);
    }
    return ret;
}
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
	{
        printf("\r\n\t\t[WS]\tHandshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
	unsigned short int packetLength=0, packetIndex=0;
	unsigned short int blockLength=0;
	short int blockStart=0, blockEnd=0;
	uint8_t fieldSeperator = 9;
	
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
	{
        printf("\r\n\t\t[WS]\thttpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    printf("\r\n\t\t[WS]\tframe len is %d", ws_pkt.len);
    if (ws_pkt.len)
	{
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = new uint8_t[ws_pkt.len + 1];//calloc(1, ws_pkt.len + 1);
        clearCharBlock((char*)buf, ws_pkt.len + 1);
		if (buf == NULL)
		{
            printf("\r\n\t\t[WS]\tFailed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
		{
            printf("\r\n\t\t[WS]\thttpd_ws_recv_frame failed with %d", ret);
            //free(buf);
            return ret;
        }
        printf("\r\n\t\t[WS]\tGot packet with message: %s", ws_pkt.payload);
    }
    printf("\r\n\t\t[WS]\tPacket type: %d", ws_pkt.type);
	for(packetIndex=0; packetIndex<ws_pkt.len; packetIndex++)
	{
		printf("\r\nIndex\t[%d]\t[%c][%d]", packetIndex, ws_pkt.payload[packetIndex], ws_pkt.payload[packetIndex]);
	}
	printf("\r\n");
    //Choose what to do based on message received
	if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp((char*)ws_pkt.payload,"Hello.") == 0)
	{
        //free(buf);
        return trigger_async_send(req->handle, req, 0);
    }
	//wifi credentials
	else if(ws_pkt.payload[0]=='W')
	{
		printf("\r\n\t\tReceived WIFI Credentials\r\n");
		blockStart = 1;
		blockEnd = findNeedleCount((char*)ws_pkt.payload, ws_pkt.len, 0, 1);
		deviceConfig.deviceSSID = new char[blockEnd+1];
		clearCharBlock(deviceConfig.deviceSSID, blockEnd+1);
		memcpy(deviceConfig.deviceSSID, ws_pkt.payload+blockStart, blockEnd-1);
		printf("\r\n\tEnd block\t[%d][%s]\r\n", blockEnd, deviceConfig.deviceSSID);
		blockStart = blockEnd+1;
		blockEnd = ws_pkt.len-blockStart;
		deviceConfig.deviceKEY = new char[blockEnd+1];
		clearCharBlock(deviceConfig.deviceKEY, blockEnd+1);
		memcpy(deviceConfig.deviceKEY, ws_pkt.payload+blockStart, blockEnd);
		printf("\r\n\tEnd block\t[%d][%s]\r\n", blockEnd, deviceConfig.deviceKEY);
		return trigger_async_send(req->handle, req, 1);
	}
	else if(ws_pkt.payload[0]=='N')
	{
		printf("\r\n\t\tReceived Network Details\r\n");
		//DHCP Mode
		deviceConfig.deviceDHCPMODE = ws_pkt.payload[1];
		blockStart = 2;
		//HostName
		blockEnd = findNeedleCount((char*)ws_pkt.payload, ws_pkt.len, 0, 2-deviceConfig.deviceDHCPMODE);
		deviceConfig.deviceHOSTNAME = new char[blockEnd-1];
		clearCharBlock(deviceConfig.deviceHOSTNAME, blockEnd-1);
		memcpy(deviceConfig.deviceHOSTNAME, ws_pkt.payload+blockStart, blockEnd-2);
		printf("\r\n\tEnd block\t[%d][%s]\r\n", blockEnd, deviceConfig.deviceHOSTNAME);
		blockStart = blockEnd+1;
		//IP Address
		blockEnd = blockStart+4;
		memcpy(deviceConfig.deviceIP, ws_pkt.payload+blockStart, 4);
		printf("\r\n\tEnd block\t[%d][%d.%d.%d.%d]\r\n", blockEnd, deviceConfig.deviceIP[0],deviceConfig.deviceIP[1],deviceConfig.deviceIP[2],deviceConfig.deviceIP[3]);
		blockStart = blockEnd;
		//SubnetMask
		blockEnd = blockStart+4;
		memcpy(deviceConfig.deviceSUBNET, ws_pkt.payload+blockStart, 4);
		printf("\r\n\tEnd block\t[%d][%d.%d.%d.%d]\r\n", blockEnd, deviceConfig.deviceSUBNET[0],deviceConfig.deviceSUBNET[1],deviceConfig.deviceSUBNET[2],deviceConfig.deviceSUBNET[3]);
		blockStart = blockEnd;
		//gateway
		blockEnd = blockStart+4;
		memcpy(deviceConfig.deviceGATEWAY, ws_pkt.payload+blockStart, 4);
		printf("\r\n\tEnd block\t[%d][%d.%d.%d.%d]\r\n", blockEnd, deviceConfig.deviceGATEWAY[0],deviceConfig.deviceGATEWAY[1],deviceConfig.deviceGATEWAY[2],deviceConfig.deviceGATEWAY[3]);
		return trigger_async_send(req->handle, req, 2);
	}
	else if(ws_pkt.payload[0]=='A')
	{
		printf("\r\n\t\tReceived ARTNET Config\r\n");
		//universe/subnet/px list count
		blockEnd = 1;
		deviceConfig.deviceUNIVERSE_COUNT = ws_pkt.payload[blockEnd];
		blockEnd++;
		deviceConfig.deviceUNIVERSE_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		deviceConfig.deviceSUBNET_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		deviceConfig.devicePIXEL_LIST = new uint8_t[deviceConfig.deviceUNIVERSE_COUNT];
		for(blockStart=0; blockStart<deviceConfig.deviceUNIVERSE_COUNT; blockStart++)
		{
			deviceConfig.deviceUNIVERSE_LIST[blockStart] = ws_pkt.payload[blockEnd];
			blockEnd++;
		}
		for(blockStart=0; blockStart<deviceConfig.deviceUNIVERSE_COUNT; blockStart++)
		{
			deviceConfig.deviceSUBNET_LIST[blockStart] = ws_pkt.payload[blockEnd];
			blockEnd++;
		}
		for(blockStart=0; blockStart<deviceConfig.deviceUNIVERSE_COUNT; blockStart++)
		{
			deviceConfig.devicePIXEL_LIST[blockStart] = ws_pkt.payload[blockEnd];
			blockEnd++;
		}
		printf("\r\nOutput Count\t[%d]\r\n", deviceConfig.deviceUNIVERSE_COUNT);
		for(blockStart=0; blockStart<deviceConfig.deviceUNIVERSE_COUNT; blockStart++)
		{
			printf("\r\nU\t[%d]\tS\t[%d]\tPX\t[%d]\r\n", deviceConfig.deviceUNIVERSE_LIST[blockStart], deviceConfig.deviceSUBNET_LIST[blockStart], deviceConfig.devicePIXEL_LIST[blockStart]);
		}
		return trigger_async_send(req->handle, req, 3);
	}
	else if(ws_pkt.payload[0]=='S')
	{
		//Save config to file
		//work out the total length of all the data to save to SPIFFS
		deviceConfig.fileDataBufferLength = 6+strlen(deviceConfig.deviceSSID)+1+strlen(deviceConfig.deviceKEY)+1+1+4+4+4+strlen(deviceConfig.deviceHOSTNAME)+1+1+(deviceConfig.deviceUNIVERSE_COUNT*3);
		printf("\r\nTotal data Length\t[%d]\r\n", deviceConfig.fileDataBufferLength);
		deviceConfig.fileSaveDataBuffer = new char[deviceConfig.fileDataBufferLength];
		blockStart=0;
		//MAC Address
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceMAC, 6);
		blockStart+=6;
		//SSID
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceSSID, strlen(deviceConfig.deviceSSID));
		blockStart+=strlen(deviceConfig.deviceSSID);
		deviceConfig.fileSaveDataBuffer[blockStart]=fieldSeperator;
		blockStart++;
		//KEY
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceKEY, strlen(deviceConfig.deviceKEY));
		blockStart+=strlen(deviceConfig.deviceKEY);
		deviceConfig.fileSaveDataBuffer[blockStart]=fieldSeperator;
		blockStart++;
		//DHCP Mode
		printf("\r\n\t\t\tblockStart->[%d][%d]\r\n", blockStart, deviceConfig.deviceDHCPMODE);
		//memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceDHCPMODE, 1);
		deviceConfig.fileSaveDataBuffer[blockStart] = deviceConfig.deviceDHCPMODE;
		blockStart++;
		//IP address
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceIP, 4);
		blockStart+=4;
		//SUBNET MASK
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceSUBNET, 4);
		blockStart+=4;
		//Gateway
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceGATEWAY, 4);
		blockStart+=4;
		//HOSTNAME
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceHOSTNAME, strlen(deviceConfig.deviceHOSTNAME));
		blockStart+=strlen(deviceConfig.deviceHOSTNAME);
		deviceConfig.fileSaveDataBuffer[blockStart]=fieldSeperator;
		blockStart++;
		//OutputList Count
		//memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceUNIVERSE_COUNT, 1);
		deviceConfig.fileSaveDataBuffer[blockStart] = deviceConfig.deviceUNIVERSE_COUNT;
		blockStart++;
		//Universe List
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceUNIVERSE_LIST, deviceConfig.deviceUNIVERSE_COUNT);
		blockStart+=deviceConfig.deviceUNIVERSE_COUNT;
		//SUBNET List
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.deviceSUBNET_LIST, deviceConfig.deviceUNIVERSE_COUNT);
		blockStart+=deviceConfig.deviceUNIVERSE_COUNT;
		//PIXEL Count List
		memcpy(deviceConfig.fileSaveDataBuffer+blockStart, deviceConfig.devicePIXEL_LIST, deviceConfig.deviceUNIVERSE_COUNT);
		blockStart+=deviceConfig.deviceUNIVERSE_COUNT;
		for(blockStart=0; blockStart<deviceConfig.fileDataBufferLength; blockStart++)
		{
			printf("\r\n\t[%d][%d][%c]", blockStart, deviceConfig.fileSaveDataBuffer[blockStart], deviceConfig.fileSaveDataBuffer[blockStart]);
		}
		printf("\r\n");
		writeConfigFile(deviceConfig.fileSaveDataBuffer, deviceConfig.fileDataBufferLength, configFile);
		return trigger_async_send(req->handle, req, 4);
	}
	else if(ws_pkt.payload[0]=='L')
	{
		//load config data from SPIFS into the WEBUI
		printf("\r\nLoading config from SPIFFS into WEBUI...");
		return trigger_async_send(req->handle, req, 5);
	}
    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK)
	{
        printf("\r\n\t\t[WS]\thttpd_ws_send_frame failed with %d", ret);
    }
    //free(buf);
    return ret;
}
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};
static const httpd_uri_t ws_uri = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
	int URIResult=0;

    // Start the httpd server
    printf("\r\n\t\tStarting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) 
	{
        // Set URI handlers
        printf("\r\n\t\tRegistering URI handlers..");
		URIResult = httpd_register_uri_handler(server, &ws_uri);			//Web Socket Handler
		printf("\r\n\t\t\tWS[%d]", URIResult);
		URIResult = httpd_register_uri_handler(server, &root);			//defaul root handler to server the web ui file
		printf("\r\n\t\t\tROOT[%d]", URIResult);
		printf("\r\n");
        return server;
    }
    printf("\r\n\t\tError starting server!");
    return NULL;
}
static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) 
	{
        printf("\r\n\t\tStarting webserver...");
        *server = start_webserver();
    }
}
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}
static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server)
	{
        printf("\r\n\t\tSTOPING webserver...");
        if (stop_webserver(*server) == ESP_OK)
		{
            *server = NULL;
        }
		else
		{
            printf("\r\n\t\tfaild to stop web server...");
        }
    }
}

/* Initialize soft AP */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    printf("\r\n\t\tWIFI AP->\t\tinside\t[wifi_event_handler]\r\n");
	if (event_id == WIFI_EVENT_AP_STACONNECTED)
	{
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
       // ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
	   printf("\r\n\t\tWIFI AP->\t\tStation joined[%d]\r\n", event->aid);
    }
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        printf("\r\n\t\tWIFI AP->\t\tStation left[%d][%d]\r\n", event->aid, event->reason);
		//ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    uint8_t ssidLength = strlen(softAPConfig.softAPSSID);
	int returnInfo = 0;
	
	printf("\r\n\t\tWIFI AP->\t\tAbout to create defaul settins\r\n");
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_create_default_wifi_ap();

	/*
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	returnInfo = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_SUBNET_MASK, &softAPConfig.deviceSUBNET, 4);
	printf("\r\nDHCPServver\tMASK[%02X]", returnInfo);          
	returnInfo = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &softAPConfig.deviceIP, 4);
	printf("\r\nDHCPServver\tIP[%02X]", returnInfo);  
	returnInfo = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &softAPConfig.deviceGATEWAY, 4);
	printf("\r\nDHCPServver\tGateway[%02X]", returnInfo);
	*/
	//--------------------STATIC IP for AP--------------------
	
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);
	if (esp_netif_dhcpc_stop(ap_netif) != ESP_OK) 
	{
        printf("\r\n\tFailed to stop dhcp client");
        return;
	}
	if (esp_netif_dhcps_stop(ap_netif) != ESP_OK) 
	{
        printf("\r\n\tFailed to stop dhcp server");
        return;
	}
	esp_netif_ip_info_t ip;
	memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
	ip.ip.addr = ESP_IP4TOADDR(softAPConfig.deviceIP[0], softAPConfig.deviceIP[1], softAPConfig.deviceIP[2], softAPConfig.deviceIP[3]) ;
	ip.netmask.addr = ESP_IP4TOADDR(softAPConfig.deviceSUBNET[0], softAPConfig.deviceSUBNET[1], softAPConfig.deviceSUBNET[2], softAPConfig.deviceSUBNET[3]);
	ip.gw.addr = ESP_IP4TOADDR(softAPConfig.deviceGATEWAY[0], softAPConfig.deviceGATEWAY[1], softAPConfig.deviceGATEWAY[2], softAPConfig.deviceGATEWAY[3]);
	if ((returnInfo = esp_netif_set_ip_info(ap_netif, &ip)) != ESP_OK) 
	{
		printf("\r\n\t\tFailed to set static ip info from config file..[%02X]", returnInfo);
	}
	else
	{
		printf("\r\n\tSuccess to set static IP");  
		printf("[%d.%d.%d.%d]\tMask[%d.%d.%d.%d]\tGateway[%d.%d.%d.%d]\r\n", softAPConfig.deviceIP[0],softAPConfig.deviceIP[1],softAPConfig.deviceIP[2],softAPConfig.deviceIP[3],softAPConfig.deviceSUBNET[0], softAPConfig.deviceSUBNET[1], softAPConfig.deviceSUBNET[2], softAPConfig.deviceSUBNET[3],softAPConfig.deviceGATEWAY[0], softAPConfig.deviceGATEWAY[1], softAPConfig.deviceGATEWAY[2], softAPConfig.deviceGATEWAY[3]);
		esp_netif_dhcps_start(ap_netif);
		//ESP_ERROR_CHECK(example_set_dns_server(ap_netif, ipaddr_addr(EXAMPLE_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
		//ESP_ERROR_CHECK(example_set_dns_server(ap_netif, ipaddr_addr(EXAMPLE_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
	}
	//--------------------------------------------------------------------------------

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler,NULL,NULL));

	 wifi_config_t wifi_config = {
        .ap = {
            //.ssid = {0},
			//.password = {0},
            .ssid_len = ssidLength,
            .channel = softAPConfig.deviceWIFIAPChanel,
            //.authmode = WIFI_AUTH_WPA2_PSK,
            .authmode = WIFI_AUTH_WPA2_WPA3_PSK,
			.max_connection = 10,
            .pmf_cfg = {
                .required = false,
            },
			.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
	printf("\r\n\t\tWIFI AP->\t\tset up ap structure\r\n");
	memcpy(wifi_config.ap.ssid, softAPConfig.softAPSSID, strlen(softAPConfig.softAPSSID));
	memcpy(wifi_config.ap.password, softAPConfig.softAPKEY, strlen(softAPConfig.softAPKEY));
	printf("\r\n\t\tWIFI AP->\t\tmemcopied data\r\n");
	
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("\r\n\t\tWIFI AP->\twifi_init_softap finished!");

}

void app_main(void)
{	
	static httpd_handle_t server = NULL;
	unsigned short int totalPixelCount=0, universeIndex=0;
	uint8_t tempColour[4] = {0,0,0,0};
	
	//Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    printf("\r\n\r\n\t\t\tNVS[%d]", ret);
	//SPIFFS
	initSPIFFS();
	//readconfig if any from SPIFS
	readConfigFile(configFile);
	//bootpin test
	testBootPin();
	if(bootMode==1)
	{
		//Config mode when boot button is pressed upon power up/reset
		printf("\r\n\t\tWIFI AP!\r\n");
		wifi_init_softap();
		//http server
		esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server);
		esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server);
		server = start_webserver();
		while (server)
		{
			sleep(5);
		}
	}
	else if(bootMode==0)
	{
		if(deviceConfig.loaded==0)
		{
			//device has not been configured
			printf("\r\n\t\tDevice has no config please reboot into config mode!\r\n");
			while(1)
			{
				sleep(10);
			}
		}
		else
		{
			//Work out the total Pixel count
			for(universeIndex=0; universeIndex<deviceConfig.deviceUNIVERSE_COUNT; universeIndex++)
			{
				totalPixelCount += deviceConfig.devicePIXEL_LIST[universeIndex];
			}
			//set up the LED output driver
			leds = new NeoViaSPI(totalPixelCount);
			//clear out pixel array
			for(universeIndex=0; universeIndex<totalPixelCount; universeIndex++)
			{
				leds->setPixel(universeIndex, tempColour);
			}
			//init wifi and connect to the AP configured in config file
			wifi_init_sta();
			//Set up UDP reception 
			xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
			//SPI for pixel driving
			printf("\r\nINIT SPI [%d]", setupSPI());
			printf("\r\n\tDevice ready to receive artnet!\r\n");
			while(1)
			{
				//render pixels continuously
				leds->encode();
				spiTransObject.length = leds->_NeoBitsframeLength*8;
				spiTransObject.tx_buffer = leds->neoBits;
				spi_device_transmit(spi, &spiTransObject);
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
		}
	}
}
