/**
  ******************************************************************************
  * @file    GPRS_MQTTClient.c
  * @author  Hao Nguyen
  * @version
  * @date
  * @brief
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "GPRS_MQTTClient.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
char clientID[] ="";
char will_topic[] ="";
char will_payload[] ="";
uint8_t will_qos = 1;
uint8_t will_retain = 0;
int16_t packet_id_counter = 0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
uint16_t min_len(uint16_t a, uint16_t b)
{
	if(a > b)
	{
		return b;
	}
	else
	{
		return a;
	}
}

uint16_t StringToUint16(uint8_t* _String)
{
	uint16_t value = 0;
	while ('0' == *_String || *_String == ' ' || *_String == '"')
	{
		_String++;
	}
	while ('0' <= *_String && *_String <= '9')
	{
		value *= 10;
		value += *_String - '0';
		_String++;
	}
	return value;
}

uint8_t* AddStringToBuf(uint8_t *_buf, const char *_string)
{
  uint16_t _length = strlen(_string);
  _buf[0] = _length >> 8;
  _buf[1] = _length & 0xFF;
  _buf+=2;
  strncpy((char *)_buf, _string, _length);
  return _buf + _length;
}

bool sendPacket(uint8_t *buffer, uint16_t len)
{
  if (TCPClient_connected())
	{
		uint16_t sendlen; 
		sendlen = min_len(len, 250);
		TCPClient_Send((char*)buffer, sendlen);
		return true;
  } 
	else
      return false;
}

uint16_t readPacket(uint8_t *buffer, int16_t _timeout) 
{
	uint16_t len = 0;
	while (TCPClient_DataAvailable() > 0 && _timeout >=0) 
	{
		buffer[len++] = TCPClient_DataRead();
		_timeout-= 10;	
		delay_ms(10);
	}
	buffer[len] = 0;
	return len;
}

uint8_t MQTT_ConnectToServer(void)
{
	return TCPClient_Start(1, AIO_SERVER, AIO_SERVER_PORT);
}

uint16_t MQTT_connectpacket(uint8_t* packet)
{
	uint8_t*_packet = packet;
	uint16_t _length;

	_packet[0] = (MQTT_CTRL_CONNECT << 4);
	_packet+=2;
	_packet = AddStringToBuf(_packet, "MQTT");
	_packet[0] = MQTT_PROTOCOL_LEVEL;
	_packet++;
	
	_packet[0] = MQTT_CONN_CLEANSESSION;
	if (will_topic && strlen(will_topic) != 0) {
		_packet[0] |= MQTT_CONN_WILLFLAG;
		if(will_qos == 1)
		_packet[0] |= MQTT_CONN_WILLQOS_1;
		else if(will_qos == 2)
		_packet[0] |= MQTT_CONN_WILLQOS_2;
		if(will_retain == 1)
		_packet[0] |= MQTT_CONN_WILLRETAIN;
	}
	if (strlen(AIO_USERNAME) != 0)
	_packet[0] |= MQTT_CONN_USERNAMEFLAG;
	if (strlen(AIO_KEY) != 0)
	_packet[0] |= MQTT_CONN_PASSWORDFLAG;
	_packet++;

	_packet[0] = MQTT_CONN_KEEPALIVE >> 8;
	_packet++;
	_packet[0] = MQTT_CONN_KEEPALIVE & 0xFF;
	_packet++;
	if (strlen(clientID) != 0) {
		_packet = AddStringToBuf(_packet, clientID);
		} else {
		_packet[0] = 0x0;
		_packet++;
		_packet[0] = 0x0;
		_packet++;
	}
	if (will_topic && strlen(will_topic) != 0) {
		_packet = AddStringToBuf(_packet, will_topic);
		_packet = AddStringToBuf(_packet, will_payload);
	}

	if (strlen(AIO_USERNAME) != 0) {
		_packet = AddStringToBuf(_packet, AIO_USERNAME);
	}
	if (strlen(AIO_KEY) != 0) {
		_packet = AddStringToBuf(_packet, AIO_KEY);
	}
	_length = _packet - packet;
	packet[1] = _length-2;

	return _length;
}

uint16_t MQTT_publishPacket(uint8_t *packet, const char *topic, char *data, uint8_t qos)
{
	uint8_t *_packet = packet;
	uint16_t _length = 0;
	uint16_t Datalen=strlen(data);

	_length += 2;
	_length += strlen(topic);
	if(qos > 0) {
		_length += 2;
	}
	_length += Datalen;
	_packet[0] = MQTT_CTRL_PUBLISH << 4 | qos << 1;
	_packet++;
	do {
		uint8_t encodedByte = _length % 128;
		_length /= 128;
		if ( _length > 0 ) {
			encodedByte |= 0x80;
		}
		_packet[0] = encodedByte;
		_packet++;
	} while ( _length > 0 );
	_packet = AddStringToBuf(_packet, topic);
	if(qos > 0) {
		_packet[0] = (packet_id_counter >> 8) & 0xFF;
		_packet[1] = packet_id_counter & 0xFF;
		_packet+=2;
		packet_id_counter++;
	}
	memmove(_packet, data, Datalen);
	_packet+= Datalen;
	_length = _packet - packet;

	return _length;
}

uint16_t MQTT_subscribePacket(uint8_t *packet, const char *topic, uint8_t qos) 
{
	uint8_t *_packet = packet;
	uint16_t _length;

	_packet[0] = MQTT_CTRL_SUBSCRIBE << 4 | MQTT_QOS_1 << 1;
	_packet+=2;

	_packet[0] = (packet_id_counter >> 8) & 0xFF;
	_packet[1] = packet_id_counter & 0xFF;
	_packet+=2;
	packet_id_counter++;

	_packet = AddStringToBuf(_packet, topic);

	_packet[0] = qos;
	_packet++;

	_length = _packet - packet;
	packet[1] = _length-2;

	return _length;
}

bool MQTT_SubscribedData(uint8_t* ReceivePacket, uint16_t ReceivePacketLength, uint8_t* ReceiveData, const char* LastKeyTopic)
{
	uint16_t j = 0;
	uint8_t k = 0;
	uint16_t i = 0;
	char _buff[100];
	for ( i=0; i<ReceivePacketLength ;i++)
	{
		for (k = 0; k < strlen(LastKeyTopic); k++)/* here length is aio_feed char length e.g. aio_feed "test" has length of 4 char */
		_buff[k] = ReceivePacket[i + k];
		if (strstr(_buff, LastKeyTopic) != 0)
		{
			for(i=i+k; i < ReceivePacketLength; i++)
			{
				ReceiveData[j]=ReceivePacket[i];
				j++;
			}
			return true;
		}
		else{}//do not thing
	}
	return false;
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics - 2017 DEMETER *****END OF FILE****/