#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <httpd/httpd.h>
#include "http_server.h"
#include "jsmn.h"
#include "plc.h"

typedef enum {
    NONE,
    SET_CONFIG,
    PLC_FUNCTION
} WebsocketClbkUse_e;

volatile WebsocketClbkUse_e websocketClbkUse = NONE;

const char *plcFunctionNames[] =
    {
        "readPLCregister",
        "readPLCregisters",
        "writePLCregister",
        "writePLCregisters",
        "setPLCtxAddrType",
        "setPLCtxDA",
        "setPLCnodeLA",
        "setPLCnodeGA",
        "getPLCrxAddrType",
        "getPLCrxSA",
        "readPLCrxPacket",
        "readPLCintRegister",
        "initPLCdevice"};

static void JSONAddInt(char *buf, int *len, uint32_t val)
{
    sprintf(buf + *len, "\"%d\"", val);
    *len = strlen(buf + *len);
}

static void JSONAddString(char *buf, int *len, char *str)
{
    sprintf(buf + *len, "%s", str);
    *len = strlen(buf + *len);
}

static void JSONAddByteArray(char *buf, int *len, uint8_t *array, int num)
{
    int length = *len;
    buf[length++] = '[';
    for (int i = 0; i < num; i++)
    {
        sprintf(buf + length, "\"%d\",", (uint32_t)array[i] & 0x000000FF);
        length = strlen(buf + length);
    }
    // Last comma should be overwritten.
    buf[length - 1] = ']';
    *len = length;
}

static uint32_t getRegFromInput(char *start, char *end)
{
    uint32_t temp;
    char *e;
    temp = strtol(start, &e, 10);
    if (e != end)
    {
        printf("Invalid argument in getRegFromInput\n");
        return -1;
    }
    return temp;
}

void plcFunction(char *data, u16_t len, struct tcp_pcb *pcb)
{
    char txBuffer[64];
    int txBufferLen = 0;
    jsmn_parser jsmnParser;
    jsmntok_t t[16];
    jsmn_init(&jsmnParser);
    int r = jsmn_parse(&jsmnParser, data, len, t, sizeof(t) / sizeof(t[0]));

    if (r < 0)
    {
        printf("JSON Parsing failed.\n");
        return;
    }

    JSONAddString(txBuffer, &txBufferLen, "{\"data\":");

    char *functionName = data + t[2].start;
    char functionNameLen = t[2].end - t[2].start;
    if (!strncmp("readPLCregister", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        uint32_t res = (uint32_t)readPLCregister((uint8_t)reg);
        JSONAddInt(txBuffer, &txBufferLen, res);
    }
    else if (!strncmp("readPLCregisters", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        uint32_t num = getRegFromInput(data + t[6].start, data + t[6].end);
        if (num == -1)
            return;
        uint8_t buffer[32];
        readPLCregisters(reg, buffer, num);
        JSONAddByteArray(txBuffer, &txBufferLen, buffer, num);
    }
    else if (!strncmp("writePLCregister", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        uint32_t val = getRegFromInput(data + t[6].start, data + t[6].end);
        if (val == -1)
            return;
        writePLCregister((uint8_t)reg, (uint8_t)val);
        return;
    }
    else if (!strncmp("writePLCregisters", functionName, functionNameLen))
    {
        uint8_t buffer[32];
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        int j = 0;
        for (int i = 6; t[i].type == JSMN_STRING; i++)
        {
            uint32_t temp = getRegFromInput(data + t[i].start, data + t[i].end);
            if (temp == -1)
                return;
            buffer[j++] = (uint8_t)temp;
        }
        writePLCregisters((uint8_t)reg, buffer, (uint8_t)j);
        return;
    }
    else if (!strncmp("setPLCtxAddrType", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        uint32_t val = getRegFromInput(data + t[6].start, data + t[6].end);
        if (val == -1)
            return;
        setPLCtxAddrType((uint8_t)reg, (uint8_t)val);
        return;
    }
    else if (!strncmp("setPLCtxDA", functionName, functionNameLen))
    {
        uint8_t buffer[32];
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        int j = 0;
        for (int i = 6; t[i].type == JSMN_STRING; i++)
        {
            uint32_t temp = getRegFromInput(data + t[i].start, data + t[i].end);
            if (temp == -1)
                return;
            buffer[j++] = (uint8_t)temp;
        }
        setPLCtxDA((uint8_t)reg, buffer);
        return;
    }
    else if (!strncmp("setPLCnodeLA", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        setPLCnodeLA((uint8_t)reg);
        return;
    }
    else if (!strncmp("setPLCnodeGA", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        setPLCnodeGA((uint8_t)reg);
        return;
    }
    else if (!strncmp("getPLCrxAddrType", functionName, functionNameLen))
    {
        uint8_t buffer[2];
        getPLCrxAddrType(&buffer[0], &buffer[1]);
        JSONAddByteArray(txBuffer, &txBufferLen, buffer, 2);
    }
    else if (!strncmp("getPLCrxSA", functionName, functionNameLen))
    {
        uint8_t buffer[8];
        getPLCrxSA(buffer);
        JSONAddByteArray(txBuffer, &txBufferLen, buffer, 8);
    }
    else if (!strncmp("readPLCrxPacket", functionName, functionNameLen))
    {
        uint8_t buffer[36];
        readPLCrxPacket(&buffer[0], &buffer[2], &buffer[1]);
        JSONAddByteArray(txBuffer, &txBufferLen, buffer, buffer[1]);
    }
    else if (!strncmp("readPLCintRegister", functionName, functionNameLen))
    {
        uint32_t intReg = (uint32_t)readPLCintRegister();
        JSONAddInt(txBuffer, &txBufferLen, intReg);
    }
    else if (!strncmp("initPLCdevice", functionName, functionNameLen))
    {
        uint32_t reg = getRegFromInput(data + t[5].start, data + t[5].end);
        if (reg == -1)
            return;
        initPLCdevice((uint8_t) reg);
        return;
    }
    else
    {
        printf("Wrong function name\n");
    }

    JSONAddString(txBuffer, &txBufferLen, "}");
    websocket_write(pcb, (uint8_t*) txBuffer, txBufferLen, WS_BIN_MODE);
}

char *index_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/index.html";
}

char *plc_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/plc.html";
}
/**
 * This function is called when websocket frame is received.
 *
 * Note: this function is executed on TCP thread and should return as soon
 * as possible.
 */
void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode)
{
    printf("[websocket_callback]:\n%.*s\n", (int)data_len, (char *)data);

    if (websocketClbkUse == SET_CONFIG)
    {
    }
    else if (websocketClbkUse == PLC_FUNCTION)
    {
        plcFunction((char *)data, data_len, pcb);
    }
    /*
    uint8_t response[2];
    uint16_t val;

    websocket_write(pcb, response, 2, WS_BIN_MODE); */
}

/**
 * This function is called when new websocket is open.
 */
void websocket_open_cb(struct tcp_pcb *pcb, const char *uri)
{
    printf("WS URI: %s\n", uri);
    if (!strcmp("/set-config", uri))
    {
        websocketClbkUse = SET_CONFIG;
        printf("Set config\n");
    }
    else if (!strcmp("/plc-function", uri))
    {
        websocketClbkUse = PLC_FUNCTION;
        printf("PLC function\n");
    }
    else
    {
        websocketClbkUse = NONE;
        printf("NONE\n");
    }
}

void httpd_task(void *pvParameters)
{
    tCGI pCGIs[] = {
        {"/index", (tCGIHandler)index_cgi_handler},
        {"/plc", (tCGIHandler)plc_cgi_handler}
    };

    /* register handlers and start the server */
    http_set_cgi_handlers(pCGIs, sizeof(pCGIs) / sizeof(pCGIs[0]));
    websocket_register_callbacks((tWsOpenHandler)websocket_open_cb, (tWsHandler)websocket_cb);
    httpd_init();

    for (;;)
        ;
}
