/**
  ******************************************************************************
  * @file    SIM900TCPClient.h
  * @author  Hao Nguyen
  * @version
  * @date
  * @brief	 Reference:	http://www.electronicwings.com
  ******************************************************************************
  */

#ifndef SIM900TCPCLIENT_H			/* Define library H file if not defined */
#define SIM900TCPCLIENT_H

#include <stdint.h>
#include <stdio.h>					/* Include standard library */
#include <stdlib.h>					/* Include standard library */
#include <string.h>					/* Include string library */
#include <stdbool.h>				/* Include standard boolean library */
#include "stm32f4xx.h"
#include "uart.h"		/* Include USART header file */
#include "sim900.h"

#define DEFAULT_BUFFER_SIZE		200
#define DEFAULT_TIMEOUT			10000
#define DEFAULT_CRLF_COUNT		2

enum SIM900_RESPONSE_STATUS {
	SIM900_RESPONSE_WAITING,
	SIM900_RESPONSE_FINISHED,
	SIM900_RESPONSE_TIMEOUT,
	SIM900_RESPONSE_BUFFER_FULL,
	SIM900_RESPONSE_STARTING,
	SIM900_RESPONSE_ERROR
};

extern char RESPONSE_BUFFER[DEFAULT_BUFFER_SIZE];

void Read_Response(void);
void TCPClient_Clear(void);
void Start_Read_Response(void);
void GetResponseBody(char* Response, uint16_t ResponseLength);
bool WaitForExpectedResponse(char* ExpectedResponse);
bool SendATandExpectResponse(char* ATCommand, char* ExpectedResponse);
bool TCPClient_ApplicationMode(uint8_t Mode);
bool TCPClient_ConnectionMode(uint8_t Mode);
bool AttachGPRS(void);
bool SIM900_Start(void);
bool TCPClient_Shut(void);
bool TCPClient_Close(void);
bool TCPClient_Connect(char* _APN, char* _USERNAME, char* _PASSWORD);
bool TCPClient_connected(void);
int16_t TCPClient_DataAvailable(void);
uint8_t TCPClient_DataRead(void);
uint8_t TCPClient_Start(uint8_t _ConnectionNumber, uint8_t* Domain, uint8_t* Port);
uint8_t TCPClient_Send(char* Data, uint16_t _length);
void Receive_ISR_routine(uint8_t data);
#endif