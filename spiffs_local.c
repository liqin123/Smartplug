#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "esp8266.h"

#include <string.h>
#include "fcntl.h"
#include "unistd.h"

#include "spiffs.h"
#include "esp_spiffs.h"
#include "spiffs_local.h"
#include "http_server.h"
#include "system.h"
#include "plc.h"
#include "parsers.h"

const char clientStr[] = "CLIENT";
const char brokerStr[] = "BROKER";

/* TODO:    1. Use O_RDWR in place of O_WRONLY (problems occured when using o_RDWR)
            2. Delete rubbish at the end of file, 
                which are leftovers from past config data, which was longer than now. */

static void getConfigFileContent(char *buffer, int size);

int initFileSystem()
{
	esp_spiffs_init();

	if (esp_spiffs_mount() != SPIFFS_OK)
	{
		printf("Error mounting SPIFFS\n");
		return -1;
	}

	return 0;
}

void saveConfigDataToFile(PermConfData_s *configData)
{
	int fd = open("smartplug.conf", O_WRONLY, 0);
	if (fd < 0)
	{
		printf("Error opening configuration file\n");
		return;
	}

	lseek(fd, 0, SEEK_SET);

	if (configData->mode == BROKER_CONF)
		write(fd, brokerStr, sizeof(brokerStr) - 1);
	else
		write(fd, clientStr, sizeof(clientStr) - 1);

	write(fd, "\n", 1);
	write(fd, configData->ssid, configData->ssidLen);
	write(fd, "\n", 1);
	write(fd, configData->password, configData->passwordLen);
	write(fd, "\n", 1);
	write(fd, configData->tbToken, 20);
	write(fd, "\n", 1);
	write(fd, configData->plcPhyAddr, 16);
	write(fd, "\n", 1);
	write(fd, configData->deviceName, configData->deviceNameLen);
	write(fd, "\n", 1);

	close(fd);
}

int getDeviceModeFromFile(char *buf)
{
	// Read file which contains mode of operation.
	int fd = open("smartplug.conf", O_RDONLY, 0);
	if (fd < 0)
	{
		printf("Error opening configuration file.\n");
		*buf = '\0';
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	read(fd, buf, 6);
	close(fd);

	return 0;
}

static void getConfigFileContent(char *buffer, int size)
{
	// Read file which contains mode of operation.
	int fd = open("smartplug.conf", O_RDONLY, 0);
	if (fd < 0)
	{
		printf("Error opening configuration file.\n");
		*buffer = '\0';
		return;
	}

	lseek(fd, 0, SEEK_SET);
	read(fd, buffer, size);
	close(fd);
}

void getCredentialsFromFile(char *ssid, char *wifiPassword, char *tbToken, char *plcPhyAddr, char *deviceName)
{
	char buffer[172];
	getConfigFileContent(buffer, sizeof(buffer));

	char *p = strtok(buffer, "\n");
	p = strtok(NULL, "\n");
	if(ssid)
		copyString(ssid, p);
	p = strtok(NULL, "\n");
	if(wifiPassword)
		copyString(wifiPassword, p);
	p = strtok(NULL, "\n");
	if(tbToken)
		copyString(tbToken, p);
	p = strtok(NULL, "\n");
	if (plcPhyAddr)
		copyString(plcPhyAddr, p);
	p = strtok(NULL, "\n");
	if(deviceName)
		copyString(deviceName, p);
}

void saveClientDataToFile(client_s *newClient)
{
	char plcPhyAddrStr[17];
	convertPlcPhyAddressToString(plcPhyAddrStr, newClient->plcPhyAddr);

	int fd = open("client.list", O_WRONLY, 0);
	if (fd < 0)
	{
		printf("Error opening file\n");
		return;
	}

	lseek(fd, 0, SEEK_END);
	write(fd, plcPhyAddrStr, 16);
	write(fd, " ", 1);
	write(fd, newClient->deviceName, strlen(newClient->deviceName));
	write(fd, "\n", 1);
	close(fd);
}

void retrieveClientListFromFile()
{
	int fd = open("client.list", O_RDONLY, 0);
	if (fd < 0)
	{
		printf("Error opening file\n");
		return;
	}

	lseek(fd, 0, SEEK_SET);
	char buffer[(16 + 1 + 32 + 1)];
	off_t offset = 0;
	while(read(fd, buffer, sizeof(buffer)) >= 0)
	{
		*(strchr(buffer, '\n')) = '\0';
		int strLen = strlen(buffer + 17);
		addClient(createClientFromAscii(buffer, buffer + 17, strLen));
		offset = lseek(fd, offset + 16 + 1 + strLen + 1, SEEK_SET);
	}
	
	close(fd);
}

void printFileContent()
{
	char buffer[182];
	getConfigFileContent(buffer, sizeof(buffer));
	printf("%s\n", buffer);
}