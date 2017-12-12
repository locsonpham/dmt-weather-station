/*
 * SIM900TCPClient.c
 * http://www.electronicwings.com
 */

#include "SIM900TCPClient.h"		/* Include TCP Client header file */

int8_t Response_Status, CRLF_COUNT = 0;
volatile int16_t Counter = 0, pointer = 0;
uint32_t TimeOut = 0;
char RESPONSE_BUFFER[DEFAULT_BUFFER_SIZE];

void Read_Response(void)
{
	char CRLF_BUF[2];
	char CRLF_FOUND;
	uint32_t TimeCount = 0, ResponseBufferLength;
	while(1)
	{
		if(TimeCount >= (DEFAULT_TIMEOUT+TimeOut))
		{
			CRLF_COUNT = 0; TimeOut = 0;
			Response_Status = SIM900_RESPONSE_TIMEOUT;
			return;
		}

		if(Response_Status == SIM900_RESPONSE_STARTING)
		{
			CRLF_FOUND = 0;
			memset(CRLF_BUF, 0, 2);
			Response_Status = SIM900_RESPONSE_WAITING;
		}
		ResponseBufferLength = strlen(RESPONSE_BUFFER);
		if (ResponseBufferLength)
		{
			delay_ms(1);
			TimeCount++;
			if (ResponseBufferLength==strlen(RESPONSE_BUFFER))
			{
				for (uint16_t i=0;i<ResponseBufferLength;i++)
				{
					memmove(CRLF_BUF, CRLF_BUF + 1, 1);
					CRLF_BUF[1] = RESPONSE_BUFFER[i];
					if(!strncmp(CRLF_BUF, "\r\n", 2))
					{
						if(++CRLF_FOUND == (DEFAULT_CRLF_COUNT+CRLF_COUNT))
						{
							CRLF_COUNT = 0; TimeOut = 0;
							Response_Status = SIM900_RESPONSE_FINISHED;
							return;
						}
					}
				}
				CRLF_FOUND = 0;
			}
		}
		delay_ms(1);
		TimeCount++;
	}
}

void TCPClient_Clear(void)
{
	memset(RESPONSE_BUFFER,0,DEFAULT_BUFFER_SIZE);
	Counter = 0;	pointer = 0;
}

void Start_Read_Response(void)
{
	Response_Status = SIM900_RESPONSE_STARTING;
	do {
		Read_Response();
	} while(Response_Status == SIM900_RESPONSE_WAITING);

}

void GetResponseBody(char* Response, uint16_t ResponseLength)
{

	uint16_t i = 12;
	char buffer[5];
	while(Response[i] != '\r')
	++i;

	strncpy(buffer, Response + 12, (i - 12));
	ResponseLength = atoi(buffer);

	i += 2;
	uint16_t tmp = strlen(Response) - i;
	memcpy(Response, Response + i, tmp);

	if(!strncmp(Response + tmp - 6, "\r\nOK\r\n", 6))
	memset(Response + tmp - 6, 0, i + 6);
}

bool WaitForExpectedResponse(char* ExpectedResponse)
{
	TCPClient_Clear();
	delay_ms(200);
	Start_Read_Response();						/* First read response */
	if((Response_Status != SIM900_RESPONSE_TIMEOUT) && (strstr(RESPONSE_BUFFER, ExpectedResponse) != NULL))
	return true;							/* Return true for success */
	return false;								/* Else return false */
}

bool SendATandExpectResponse(char* ATCommand, char* ExpectedResponse)
{
	COMM_Puts(ATCommand);				/* Send AT command to SIM900 */
	COMM_Putc('\r');
	return WaitForExpectedResponse(ExpectedResponse);
}

bool TCPClient_ApplicationMode(uint8_t Mode)
{
	char _buffer[20];
	sprintf(_buffer, "AT+CIPMODE=%d\r", Mode);
	_buffer[19] = 0;
	COMM_Puts(_buffer);
	return WaitForExpectedResponse("OK");
}

bool TCPClient_ConnectionMode(uint8_t Mode)
{
	char _buffer[20];
	sprintf(_buffer, "AT+CIPMUX=%d\r", Mode);
	_buffer[19] = 0;
	COMM_Puts(_buffer);
	return WaitForExpectedResponse("OK");
}

bool AttachGPRS(void)
{
	COMM_Puts("AT+CGATT=1\r");
	return WaitForExpectedResponse("OK");
}

bool SIM900_Start(void)
{
	for (uint8_t i=0;i<5;i++)
	{
		if(SendATandExpectResponse("ATE0","OK")||SendATandExpectResponse("AT","OK"))
		return true;
	}
	return false;
}

bool TCPClient_Shut(void)
{
	COMM_Puts("AT+CIPSHUT\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_Close(void)
{
	COMM_Puts("AT+CIPCLOSE=1\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_Connect(char* _APN, char* _USERNAME, char* _PASSWORD)
{

	COMM_Puts("AT+CREG?\r");
	if(!WaitForExpectedResponse("+CREG: 0,1"))
	return false;

	COMM_Puts("AT+CGATT?\r");
	if(!WaitForExpectedResponse("+CGATT: 1"))
	return false;
	
#ifdef GPRS_REGISTRATION
	COMM_Puts("AT+CSTT=\"");
	COMM_Puts(_APN);
	COMM_Puts("\",\"");
	COMM_Puts(_USERNAME);
	COMM_Puts("\",\"");
	COMM_Puts(_PASSWORD);
	COMM_Puts("\"\r");
	if(!WaitForExpectedResponse("OK"))
	return false;

	COMM_Puts("AT+CIICR\r");
	if(!WaitForExpectedResponse("OK"))
	return false;
#endif
	
	COMM_Puts("AT+CIFSR\r");
	if(!WaitForExpectedResponse("."))
	return false;

	COMM_Puts("AT+CIPSPRT=1\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_connected(void) {
	COMM_Puts("AT+CIPSTATUS\r");
	CRLF_COUNT = 2;
	return WaitForExpectedResponse("CONNECT OK");
}

uint8_t TCPClient_Start(uint8_t _ConnectionNumber, uint8_t* Domain, uint8_t* Port)
{
	char _buffer[25];
	COMM_Puts("AT+CIPMUX?\r");
	if(WaitForExpectedResponse("+CIPMUX: 0"))
	COMM_Puts("AT+CIPSTART=\"TCP\",\"");
	else
	{
		sprintf(_buffer, "AT+CIPSTART=\"%d\",\"TCP\",\"", _ConnectionNumber);
		COMM_Puts(_buffer);
	}
	
	COMM_Puts(Domain);
	COMM_Puts("\",\"");
	COMM_Puts(Port);
	COMM_Puts("\"\r");

	CRLF_COUNT = 2;
	if(!WaitForExpectedResponse("CONNECT OK"))
	{
		if(Response_Status == SIM900_RESPONSE_TIMEOUT)
		return SIM900_RESPONSE_TIMEOUT;
		return SIM900_RESPONSE_ERROR;
	}
	return SIM900_RESPONSE_FINISHED;
}

uint8_t TCPClient_Send(char* Data, uint16_t _length)
{
	COMM_Puts("AT+CIPSEND\r");
	CRLF_COUNT = -1;
	WaitForExpectedResponse(">");
	
	for (uint16_t i = 0; i < _length; i++)
	COMM_Putc(Data[i]);
	COMM_Putc(0x1A);

	if(!WaitForExpectedResponse("SEND OK"))
	{
		if(Response_Status == SIM900_RESPONSE_TIMEOUT)
		return SIM900_RESPONSE_TIMEOUT;
		return SIM900_RESPONSE_ERROR;
	}
	return SIM900_RESPONSE_FINISHED;
}

int16_t TCPClient_DataAvailable(void)
{
	return (Counter - pointer);
}

uint8_t TCPClient_DataRead(void)
{
	if(pointer<Counter)
	return RESPONSE_BUFFER[pointer++];
	else{
		TCPClient_Clear();
		return 0;
	}
}

void Receive_ISR_routine(uint8_t data)							/* Receive ISR routine */
{
	RESPONSE_BUFFER[Counter] = data;							/* Copy data to buffer & increment counter */
	Counter++;
	if(Counter == DEFAULT_BUFFER_SIZE)							/* check if any overrun occur due to continuous reception */
	{							
		Counter = 0; 
		pointer = 0;
	}															
}
