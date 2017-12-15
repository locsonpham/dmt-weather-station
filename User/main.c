/**
  ******************************************************************************
  * @file    main.c 
  * @author  Robot Club BK HCM
  * @version 1.0
  * @date
  * @brief	Main program body
  ******************************************************************************
  */
  
/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "defines.h"
#include "gsm_handler.h"
#include "com.h"
#include "comms.h"

#include "sensor.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
// Rain SUm Area 55x115

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t sendPri = 0;

uint8_t main_started = 0;

Timeout_Type tGetInfo;

uint16_t mainWDG = 0;

uint16_t resetCnt = 0;

bool connect_flag = false;
uint16_t len;

float temp, humid, light, pressure, soilT, soilH;

// GSM region
Timeout_Type tMODEMInit;
Timeout_Type tMODEM_AvailableCheck;
Timeout_Type tGPRSInit;
Timeout_Type tSMSCheckTimeout;
Timeout_Type tReConnectServer[SOCKET_NUM];
Timeout_Type tTimeSync;

uint8_t modemAvailableCnt = 0;
uint8_t mainBuf[768];
uint32_t receiveLen;
uint8_t flagCalling = 0;

// GPRS
uint8_t gprsTryNum = 5;
uint16_t u16Temp;
uint32_t u32Temp;
uint32_t nackCnt = 0;
uint32_t serverNumTry[SOCKET_NUM] = {0};
uint8_t svUseIP;
uint8_t *priServerIp;
uint8_t svUseIP_ts;
uint8_t *priServerIp_ts;
uint16_t priServerPort_ts;
uint8_t *secServerIp;
uint16_t priServerPort;
uint16_t secServerPort;
uint8_t  curServer[SOCKET_NUM];
Timeout_Type tReConnectServer[SOCKET_NUM];

char *server_domain = "m14.cloudmqtt.com";//"device.demeter.vn";//
int server_port = 10457;//3000;//

Timeout_Type tCheckPeriodSendData[SOCKET_NUM];
Timeout_Type tTimeReCheckAck;

Timeout_Type tThingSpeak;
Timeout_Type tSendServer;

const uint16_t periodSendDataMaxTime[SOCKET_NUM];

// CELLULAR INFO
Timeout_Type tGetCellID;

// SOCKET
Timeout_Type tSocketOn;


/* Private function prototypes -----------------------------------------------*/
void init(void);
void GSM_Handler(void);
void GPRS_Handler(uint8_t task);
void CELL_Handler(void);

/* Private functions ---------------------------------------------------------*/
void init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    SysTick_Init();
//    /* WWDG configuration */
//    /* Enable WWDG clock */
//    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);

//    /* WWDG clock counter = (PCLK1 (42MHz)/4096)/8 = 1281 Hz (~780 us)  */
//    WWDG_SetPrescaler(WWDG_Prescaler_8);

//    /* Set Window value to 80; WWDG counter should be refreshed only when the counter
//    is below 80 (and greater than 64) otherwise a reset will be generated */
//    WWDG_SetWindowValue(80);

//    /* Enable WWDG and set counter value to 127, WWDG timeout = ~780 us * 64 = 49.92 ms 
//     In this case the refresh window is: 
//           ~780 * (127-80) = 36.6ms < refresh window < ~780 * 64 = 49.9ms
//    */
//    WWDG_Enable(127);

    delay_ms(500);
    
    // Config UART3 for debug
    UART_Config(USART3, 9600);
	/* Init GSM */
    InitTimeout(&tMODEMInit, 0);
    modemInitFlag = 1;	
    for(int i = 0;i < SOCKET_NUM;i++)
	{
        InitTimeout(&tReConnectServer[i], TIME_SEC(0));
        
        // If after 5min have no data sent then will close socket
		InitTimeout(&tCheckPeriodSendData[i], TIME_SEC(300));
	}
}

/**
  * @brief  GPRS Handler
  *         Connect socket, send/receive data
  * @param  None
  * @retval None
  */
void GSM_Handler(void)
{
    // Init MODEM (if not initalized yet)
    if ( (modemInitFlag == 1) && (CheckTimeout(&tMODEMInit) == 0))
    {
        if (MODEM_Init() == 0)
        {
            MODEM_Info("MODEM Init OK\r\n");
            modemInitFlag = 0;  // Init done -> Clear Flag
            modemStatus = 1;    // MODEM Status OK
            modemAvailableCnt = 0;
            InitTimeout(&tMODEM_AvailableCheck, TIME_SEC(60));
            
            // START GPRS
            gprsStatus = 0;
            gprsInitFlag = 1;
            InitTimeout(&tGPRSInit, TIME_SEC(5));
            
            // START CELLID
            cellStatus = 0;
            cellInitFlag = 1;
            InitTimeout(&tGetCellID, TIME_SEC(10));
            
            // CLOSE ALL SOCKET
            for (int i = 0; i < SOCKET_NUM; i++)
            {
                socketMask[i] = 0xff;
                tcpSocketStatus[i] = SOCKET_CLOSE;
            }
        }
        else
        {
            modemStatus = 0;    // MODEM Init Fail
            // ReInit after while
            MODEM_Info("MODEM Init FAIL\r\n");
            InitTimeout(&tMODEMInit, TIME_SEC(10));
        }
    }
    
    // Check MODEM available (not halt)
    if (modemStatus == 1 && CheckTimeout(&tMODEM_AvailableCheck) == 0)
    {
        if(MODEM_SendCommand(modemOk, modemError, 5000, 3, "AT\r") == 0)
        {
            modemAvailableCnt = 0;  // MODEM Ready
        }
        else
        {
            modemAvailableCnt++;
        }
        
        InitTimeout(&tMODEM_AvailableCheck, TIME_SEC(60));
        
        if (modemAvailableCnt >= 3) // Already Stop => ReInit
        {
            SIM900_HardReset();
            modemStatus = 0;
            modemInitFlag = 1;
            
            InitTimeout(&tMODEMInit, TIME_SEC(5));
        }
    }
    
    /* SMS & CALL Manage --------------------------------------------------*/
    if (modemStatus)    // GSM READY -> SMS READY
    {
        /*---- SMS CHECK ----*/
        if(smsNewMessageFlag || (CheckTimeout(&tSMSCheckTimeout) == TIMEOUT))
        {
            SMS_Manage(mainBuf, sizeof(mainBuf));
            smsNewMessageFlag = 0;
            InitTimeout(&tSMSCheckTimeout, TIME_SEC(30));
        }
        /*---- END SMS CHECK ----*/
        
        /*---- CALL MANAGE ----*/
        if (ringFlag)  // Receive call
        {
            MODEM_Info("MAIN: CALLER->%s\r\n", callerPhoneNum);
            ringFlag = 0;
            CallingProcess(0);
            NVIC_SystemReset();
        }
        
        // MAKE CALL
        if(flagCalling == 1)   // Step1: Make call and wait
        {
            inCalling = 0;
         
//             if(VoiceSetup() == 0)
//             {
//                 sprintf((char *)mainBuf,"ATD%s;\r", "01655236473");
//                 COMM_Puts(mainBuf);
//                 
//                 if(MODEM_CheckResponse("OK",5000) == 0)
//                 {
//                     flagCalling = 2;
//                     inCalling = 1;
//                 }
//             }
        }
        else if(inCalling && (flagCalling == 0))        // Cancel call
        {
            if(CallingProcess(0) == 0)
            {
                inCalling = 0;
            }
        }
        else if((inCalling == 0) && (flagCalling == 2))  // Busy - no answer
        {
             flagCalling = 0;
        }
        /*---- END CALL MANAGE ----*/
    }
}

void createReportedPackage(char *msg, int *len) {

	char *buff = msg;
	char tmpBuf[64];
	
	buff[0] = 0;
	
	sprintf(tmpBuf, "{");
	strcat(buff, tmpBuf);
	
	sprintf(tmpBuf, "\"deviceID\":%s,", modemId);
	strcat(buff, tmpBuf);
	
	sprintf(tmpBuf, "\"payload\": {");
	strcat(buff, tmpBuf);

	// reported
	sprintf(tmpBuf, "\"reported\":{\"field1\": {\"value\":\"%.1f\"},", temperature);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "\"field2\": {\"value\":\"%.1f\"},", humidity);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "\"field3\": {\"value\":\"%d\"},", lux);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "\"field4\": {\"value\":\"%d\"},", rainSum);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "\"field5\": {\"value\":\"%d\"},", windSpeed_data);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "\"field6\": {\"value\":\"%d\"}}", windDir);
	strcat(buff, tmpBuf);

	sprintf(tmpBuf, "}}");
	strcat(buff, tmpBuf);

	*len = strlen(buff);
}


/**
  * @brief  GPRS Handler
  *         Connect socket, send/receive data
  * @param  None
  * @retval None
  */

void GPRS_Handler(uint8_t task)
{
    if ( (modemStatus == 1) && (gprsInitFlag == 1) && (CheckTimeout(&tGPRSInit) == 0))  // Init GPRS
    {
        if (MODEM_EnableGPRS() == 0)    // Init OK
        {
            MODEM_Info("GPRS OK\r\n");
            gprsStatus = 1;
            gprsInitFlag = 0;
            gprsTryNum = 5;
        }
        else
        {
            // Reinit GPRS if fails
            InitTimeout(&tGPRSInit, TIME_INITGPRS);
            gprsTryNum--;
        }
    }
    
    if (gprsTryNum == 0)    // Can't init GPRS, reinit GSM
    {
        modemStatus = 0;
        gprsInitFlag = 0;
        modemInitFlag = 1;
        InitTimeout(&tMODEMInit, TIME_SEC(5));
    }
    
    int i = task % SOCKET_NUM;
    
    if (gprsStatus && inCalling == 0) // GPRS Ready
    {
         switch(i)   // Update IP and PORT
         {
             case 0: // Primary Server
             {
                 svUseIP = 0;
                 if (svUseIP)
                 {
                     priServerIp = (uint8_t *)sysCfg.priDserverIp;
                 }
                 else
                 {
                     priServerIp = (uint8_t *)server_domain;  
                 }
                 priServerPort = server_port;
                     
             } break;
             
             case 1: // Secondary Server
             {
             } break;
             
             case 2: // Firmware Server
             {
             } break;
             
             default: break;
         }
        
        /* Connect SOCKET ------------------------------------------------------------*/
        if ((socketMask[0] != 0) || (tcpSocketStatus[0] == SOCKET_CLOSE))
        {
             static uint8_t errsocket_cnt = 0;
             if (CheckTimeout(&tReConnectServer[0]) == TIMEOUT)
             {
                 INFO("Socket[%d] CLOSED -> Connect now \n", i);
                 if (MODEM_ConnectSocket(0, priServerIp, priServerPort, svUseIP) == 0)
                 {
					INFO("Connect server success !!!\n");
					InitTimeout(&tTimeReCheckAck, TIME_SEC(1));
					InitTimeout(&tCheckPeriodSendData[0], TIME_SEC(120));
					InitTimeout(&tTimeSync, TIME_SEC(1));
					InitTimeout(&tSendServer, TIME_SEC(1));

					INFO("Send connect packet to MQTT server\n");
					len = MQTT_connectpacket(mainBuf);
					MODEM_GprsSendData(0,mainBuf, len);
					delay_ms(200);
				 
					INFO("Send subscribe packet to MQTT server\n");
					len = MQTT_subscribePacket(mainBuf,"HaoNguyen/feeds/test", 1);
					MODEM_GprsSendData(0,mainBuf, len);
					delay_ms(200);
                 }
                 else
                 {
					 INFO("Connect server fail, close socket !!!\n");
                     if (MODEM_CloseSocket(0)) errsocket_cnt++;
                     
                     if (errsocket_cnt >= 5)
                     {
                         errsocket_cnt = 0;
                         
                         modemStatus = 0;
                         gprsInitFlag = 0;
                         modemInitFlag = 1;
                         InitTimeout(&tMODEMInit, TIME_SEC(5));
                     }
                     InitTimeout(&tReConnectServer[0], TIME_RECONSERVER);
                 }
             }
        }
        /* End Connect SOCKET ---------------------------------------------------------*/
        /* Receive DATA ---------------------------------------------------------------*/
        if (socketRecvFlag[0])
        {
             uint16_t length;
             uint8_t c;
             uint8_t i = 0;
             uint8_t buf[10];
             
             gprsRecvFlag = 0;
             length = gprsRecvDataLen;
             uint8_t mode = 0;
			
			INFO("MSG: Receive Data ...\n");
             
//             while (RINGBUF_Get(&gprsRingBuff[0], &c) == 0)
//             {
//			}
        }
        /* End Receive DATA -----------------------------------------------------------*/
        
        /* SEND DATA ---------------------------------------------------------*/
        // OPEN Socket
        // Send data to primary server 
        if(tcpSocketStatus[0] == SOCKET_OPEN)
        {   
             if(CheckTimeout(&tSendServer) == TIMEOUT)
             {
                 if(CheckTimeout(&tTimeReCheckAck) == TIMEOUT)
                 {
                     if((MODEM_CheckGPRSDataOut(0) == 0)) // Data sent
                     {
                        int i = 0;
						char _buf[10];
						memset(_buf, 0, 10);

						//insert sensor value to MQTT packet
						sprintf(_buf,"{%.1lf}",temperature);

						//Create MQTT packet and Get length of this packet
						i = MQTT_publishPacket(mainBuf, "demeter/field2/test", _buf, 1);

                        //createReportedPackage((char *)mainBuf, &i);
                         if((i > 0) && (i < sizeof(mainBuf)))
                         {
                             MODEM_Info("Prepare to Send\n");
                             
                             MODEM_GprsSendData(0, mainBuf, i);
                             mainBuf[i] = 0;
                             MODEM_Info("\r\nGPRS->SEND:%s\n\n", mainBuf);
							 
							 INFO("Reinit Socket Timeout\n");
							 InitTimeout(&tCheckPeriodSendData[0], TIME_SEC(60));
                         }		
                     }
					 InitTimeout(&tSendServer, TIME_SEC(10));
                     InitTimeout(&tTimeReCheckAck, TIME_SEC(1));
                 }
             }
        }
        
        for(i = 0; i < SOCKET_NUM; i++)
		{
 			if(tcpSocketStatus[i] == SOCKET_OPEN || tcpSocketStatus[i] == SOCKET_ERROR)
             {
 				if(CheckTimeout(&tCheckPeriodSendData[i]) == TIMEOUT)
 				{
					INFO("Socket TIMEOUT => CLOSE\n");
					static uint8_t err_cnt = 0;
					if (MODEM_CloseSocket(i)) err_cnt++;

					if (err_cnt >= 5)   // Reset GSM
					{
						modemStatus = 0;
						gprsInitFlag = 0;
						modemInitFlag = 1;
					}
					InitTimeout(&tReConnectServer[i], TIME_SEC(5));
 				}
             }
		}
        /* End SEND DATA ---------------------------------------------------------*/
        
//        if (CheckTimeout(&tThingSpeak) == TIMEOUT)
//        {
//            static uint8_t firstRun = 1;
//            static uint8_t channel = 0;
//            
//            if (temperature == 0) return;
//            
//            if (MODEM_ConnectSocket(1, "api.thingspeak.com", 80, 0) == 0) 
//            {
//                
//                // Channel Weather
//                if((MODEM_CheckGPRSDataOut(1) == 0) && channel == 0) //data sent
//                {
//                    char *buff = (char*)mainBuf;

//                    sprintf(buff, "GET /update?key=DDA6TUXEPJOHZOEU&field1=%.1f\r\n",
//                                temperature);

//                    strcat(buff, "\r\n");
//                    int len = strlen(buff);

//                    if((len > 0) && (len <  sizeof(mainBuf)))
//                    {
//                        INFO("Ready to SEND\n");
//                        MODEM_GprsSendData(1, buff, len);
//                        mainBuf[len] = 0;
//                        MODEM_Info("\r\nGPRS->SEND:%s", mainBuf);
//                    }		
//                }
//                
//                tcpSocketStatus[1] = SOCKET_CLOSE;
//                MODEM_CloseSocket(1);
//                
//                InitTimeout(&tThingSpeak, TIME_SEC(600)); 
//            }
//            else
//            {
//                static uint8_t err_cnt = 0;
//                tcpSocketStatus[1] = SOCKET_CLOSE;
//                if (MODEM_CloseSocket(1)) 
//                {
//                    err_cnt++;
//                }
//                
//                if (err_cnt >=5)
//                {
//                    err_cnt = 0;
//                    modemStatus = 0;
//                    gprsInitFlag = 0;
//                    modemInitFlag = 1;
//                    InitTimeout(&tMODEMInit, TIME_SEC(5));
//                }
//                InitTimeout(&tThingSpeak, TIME_SEC(30));
//            }
//        }
    }
}


/*******************************************************************************
Function name: Main program
Decription: None
Input: None
Output: None
*******************************************************************************/
int main(void)
{
    // Init
    init();
    
    // Init Sensor
    Sensor_Init();
    
//    // Track Init
//    Tracking_Init();
    
    // INFO
    InitTimeout(&tGetInfo, 0);
    INFO("\n\nSystem Init Done!\n");
    main_started = 1; // Start
    InitTimeout(&tSendServer, TIME_SEC(1));
    
    while (1)
    {
        static uint8_t main_task = 0;
        
        
        mainWDG = 0; // Counter for WDG
		
		if (CheckTimeout(&tGetInfo) == TIMEOUT) {
			// Update Sensor
			Sensor_Update();
			
			INFO("*C: %.1f\n", temperature);
			INFO("H: %.1f\n", humidity);
			INFO("lux: %d\n", lux);
			INFO("rainSum: %d\n", rainSum);
			INFO("windSpeed: %d  %.1f %d\n", windSpeed_data, fwindSpeed, windspeed_cnt);
			INFO("windDir: %d\n", windDir);
			INFO("volt: %f\n", voltage_power);
			INFO("rst: %d\n", resetCnt);
			
						INFO("ADC: %d\n", ADC_Val[2]);
			
			// Update socket status
			for (uint8_t i = 0; i<SOCKET_NUM; i++)
			{
				if (tcpSocketStatus[i] == SOCKET_OPEN)
					INFO("Socket[%d]: OPENED\n", i);
				else INFO("Socket[%d]: CLOSED\n", i);
			}
			
			INFO("\n");
			
			InitTimeout(&tGetInfo, TIME_SEC(2));
		}
		/* Receive DATA ---------------------------------------------------------------*/
		if (socketRecvFlag[0])
		{
				 uint16_t length = gprsRecvDataLen[0];
				 uint8_t __buff[20];
				 uint8_t i = 0;
				 gprsRecvFlag = 0;
				 uint8_t mode = 0;
				 socketRecvFlag[0]=0;	
				MQTT_SubscribedData(gprsRxBuff[0],length,__buff,AIO_FEED);
				INFO("MSG: Received Data: %s \n",gprsRxBuff[0]);//\" %s \" \n",UART1_RxRingBuff.pt);
		}
		
		/* GSM Region ************************/
        GSM_Handler();
        GPRS_Handler(main_task);
		/* END GSM Region *******************/
        // Increase main_task
        main_task++;
    }
}



/*******************************************************************************
Function name: USE_FULL_ASSERT
Decription: None
Input: None
Output: None
*******************************************************************************/
#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}

#endif

/******************* (C) COPYRIGHT 2011 STMicroelectronics - 2013 Robot Club BKHCM *****END OF FILE****/
