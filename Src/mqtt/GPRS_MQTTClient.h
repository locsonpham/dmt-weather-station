/**
  ******************************************************************************
  * @file    GPRS_MQTTClient.h 
  * @author  Hao Nguyen
  * @version
  * @date
  * @brief
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef	__GPRS_MQTTClient_H
#define __GPRS_MQTTClient_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "SIM900TCPClient.h"

/* Private define ------------------------------------------------------------*/
//#define min(a,b)				({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
//#define GPRS_REGISTRATION
#define MQTT_PROTOCOL_LEVEL		4

#define MQTT_CTRL_CONNECT		0x1
#define MQTT_CTRL_CONNECTACK	0x2
#define MQTT_CTRL_PUBLISH		0x3
#define MQTT_CTRL_PUBACK		0x4
#define MQTT_CTRL_PUBREC		0x5
#define MQTT_CTRL_PUBREL		0x6
#define MQTT_CTRL_PUBCOMP		0x7
#define MQTT_CTRL_SUBSCRIBE		0x8
#define MQTT_CTRL_SUBACK		0x9
#define MQTT_CTRL_UNSUBSCRIBE	0xA
#define MQTT_CTRL_UNSUBACK		0xB
#define MQTT_CTRL_PINGREQ		0xC
#define MQTT_CTRL_PINGRESP		0xD
#define MQTT_CTRL_DISCONNECT	0xE

#define MQTT_QOS_1				0x1
#define MQTT_QOS_0				0x0

/* Adjust as necessary, in seconds */
#define MQTT_CONN_KEEPALIVE		5

#define MQTT_CONN_USERNAMEFLAG	0x80
#define MQTT_CONN_PASSWORDFLAG	0x40
#define MQTT_CONN_WILLRETAIN	0x20
#define MQTT_CONN_WILLQOS_1		0x08
#define MQTT_CONN_WILLQOS_2		0x18
#define MQTT_CONN_WILLFLAG		0x04
#define MQTT_CONN_CLEANSESSION	0x02

#define DEFAULT_BUFFER_SIZE		200
#define DEFAULT_TIMEOUT			10000
#define DEFAULT_CRLF_COUNT		2

/* Select Demo */
#define SUBSRCIBE_DEMO								/* Define SUBSRCIBE demo */
#define PUBLISH_DEMO								/* Define PUBLISH demo */

#define AIO_SERVER			  "m14.cloudmqtt.com"//"broker.hivemq.com"//		/* Adafruit server */
#define AIO_SERVER_PORT		"10457"//"1883"//					/* Server port */
#define AIO_BASE_URL		//"/api/v2"				/* Base URL for api */
#define AIO_USERNAME		  "ddvyjdzl"//0//"Enter Username"		/* Enter username here */
#define AIO_KEY				    "mwdCX9s2gs20"//0//"Enter AIO key"			/* Enter AIO key here */
#define AIO_FEED			"test"		/* Enter feed key */

/********************************************
 * NETWORK	 * APN		    * USER * PASSWORD *
 ********************************************
 * mobifone  * m-wap      * mms  * mms      *
 ********************************************
 * vinafone  * 3m-world   * mms  * mms		  *
 ********************************************
 * viettel   * v-internet * -    * -        *
 ********************************************
 * VN mobile * wap		    * -    * -        *
 ********************************************/
#define APN					  "v-internet"
#define USERNAME			""
#define PASSWORD			""

/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
uint16_t StringToUint16(uint8_t* _String);
uint8_t* AddStringToBuf(uint8_t *_buf, const char *_string);
bool sendPacket(uint8_t *buffer, uint16_t len);
uint16_t readPacket(uint8_t *buffer, int16_t _timeout);
uint8_t MQTT_ConnectToServer(void);
uint16_t MQTT_connectpacket(uint8_t* packet);
uint16_t MQTT_publishPacket(uint8_t *packet, const char *topic, char *data, uint8_t qos);
uint16_t MQTT_subscribePacket(uint8_t *packet, const char *topic, uint8_t qos);
uint16_t min_len(uint16_t a, uint16_t b);
bool MQTT_SubscribedData(uint8_t* ReceivePacket, uint16_t ReceivePacketLength, uint8_t* ReceiveData, const char* LastKeyTopic);
#endif /*__GPRS_MQTTClient_H*/