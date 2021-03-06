#include "cloud.h"
#include "espressif/esp_common.h"
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>
#include "client.h"
#include "parsers.h"

static volatile char *tbToken[21];

void mqttTask(void *pvParameters)
{
	struct mqtt_network network;
	mqtt_client_t client = mqtt_client_default;
	uint8_t mqttBuf[128];
	uint8_t mqttReadBuf[128];
	mqtt_packet_connect_data_t data = mqtt_packet_connect_data_initializer;

	char mqttId[20];
	client_s *clientCreds = (client_s *) pvParameters;
	convertPlcPhyAddressToString(mqttId, clientCreds->plcPhyAddr);

	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.clientID.cstring = mqttId;
	data.username.cstring = MQTT_HOST;
	data.password.cstring = MQTT_PASS;
	data.keepAliveInterval = 60;
	data.cleansession = 0;

	mqtt_message_t message;
	message.payload = MQTT_MSG;
	message.payloadlen = sizeof(MQTT_MSG) - 1;
	message.dup = 0;
	message.qos = MQTT_QOS1;
	message.retained = 0;

	mqtt_network_new(&network);

	int ret;
	// TODO: Add supervisor task which suspends any task which uses Wifi connecitivity when connection is closed.
	while (1)
	{
		printf("%s %s\n", data.username.cstring, data.clientID.cstring);
		printf("Establishing MQTT connection...\n\r");
		ret = mqtt_network_connect(&network, MQTT_HOST, MQTT_PORT);
		if (ret != MQTT_SUCCESS)
		{
			printf("Error connecting to MQTT server: %d\n\r", ret);
			vTaskDelay(pdMS_TO_TICKS(2000));
			continue;
		}
		else
			printf("Successfully connected to MQTT server\n\r");

		mqtt_client_new(&client, &network, 5000, mqttBuf, sizeof(mqttBuf), mqttReadBuf, sizeof(mqttReadBuf));
		ret = mqtt_connect(&client, &data);
		if (ret != MQTT_SUCCESS)
		{
			printf("Error sending MQTT CONNECT: %d\n\r", ret);
			mqtt_network_disconnect(&network);
			continue;
		}
		else
			printf("MQTT CONNECT sent successfull\n\r");

		for (;;)
		{
			ret = mqtt_publish(&client, "v1/devices/me/telemetry", &message);
			if (ret != MQTT_SUCCESS)
			{
				printf("Error while publishing message: %d\n\r", ret);
				mqtt_network_disconnect(&network);
				break;
			}
			else
				printf("MQTT Publishing successful\n\r");
			vTaskDelay(pdMS_TO_TICKS(10000));
		}
	}
}

void setTbToken(char *newTbToken)
{
	memcpy(tbToken, newTbToken, 20);
	tbToken[20] = '\0';
}

char *getTbToken()
{
	return (char *)tbToken;
}