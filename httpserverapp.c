//*****************************************************************************
// httpserver_app.c
//
// camera application macro & APIs
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************
//*****************************************************************************
//
//! \addtogroup Httpserverapp
//! @{
//
//*****************************************************************************

#include <string.h>
#include <stdlib.h>

// Driverlib Includes
#include "rom_map.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "prcm.h"
#include "utils.h"
#include "timer.h"

// SimpleLink include
#include "simplelink.h"

// Free-RTOS/TI-RTOS include
#include "osi.h"
#include <ti/sysbios/knl/Semaphore.h>

// HTTP lib includes
#include "HttpCore.h"
#include "HttpRequest.h"
#include "WebSockHandler.h"

// Common-interface includes
#include "network_if.h"
#include "uart_if.h"
#include "common.h"
#include "timer_if.h"
#include "gpio_if.h"
#include "httpserverapp.h"

typedef struct
{
    UINT16 connection;
    char * buffer;
}event_msg;

// Network App specific status/error codes which are used only in this file
typedef enum{
     // Choosing this number to avoid overlap w/ host-driver's error codes
    DEVICE_NOT_IN_STATION_MODE = -0x7F0,
    DEVICE_NOT_IN_AP_MODE = DEVICE_NOT_IN_STATION_MODE - 1,
    DEVICE_NOT_IN_P2P_MODE = DEVICE_NOT_IN_AP_MODE - 1,

    STATUS_CODE_MAX = -0xBB8
}e_NetAppStatusCodes;

/****************************************************************************
                              Global variables
****************************************************************************/
char *startcounter = "start";
char *stopcounter = "stop";
UINT8 g_success = 0;
int g_close = 0;
UINT16 g_uConnection;
static volatile unsigned long g_ulBase;
static OsiSyncObj_t g_CounterSyncObj;
OsiMsgQ_t g_recvQueue;
extern volatile unsigned long  g_ulStatus;   /* SimpleLink Status */
extern Semaphore_Handle httpServerInitCompleteSemaphore;

void InitializeAppVariables();

void WebSocketCloseSessionHandler(void)
{
	g_close = 1;
}


/*!
 *  \brief                  This websocket Event is called when WebSocket Server receives data
 *                          from client. Declared in WebSockHandler.h (webserver library), but must be
 *                          implemented by application.
 *
 *
 *  \param[in]  uConnection Websocket Client Id
 *  \param[in] *ReadBuffer      Pointer to the buffer that holds the payload.
 *
 *  \return                 none.
 *
 */
void WebSocketRecvEventHandler(UINT16 uConnection, char *ReadBuffer)
{
    g_close = 0;
    event_msg msg;

    msg.connection = uConnection;
    msg.buffer = ReadBuffer;

    // Notify that HTTP server initialization is complete
    Semaphore_post(httpServerInitCompleteSemaphore);
    g_uConnection = msg.connection;

#if 0
    if (!strcmp(msg.buffer,startcounter))
    {
        g_uConnection = msg.connection;
        // Signal to Counter task to start counter
        osi_SyncObjSignal(&g_CounterSyncObj);
    }
    else if (!strcmp(msg.buffer,stopcounter))
    {
        // Stop counter timer
        Timer_IF_Stop(g_ulBase, TIMER_A);
    }
#endif

}


/*!
 * 	\brief 						This websocket Event indicates successful handshake with client
 * 								Once this is called the server can start sending data packets over websocket using
 * 								the sl_WebSocketSend API.
 *
 *
 * 	\param[in] uConnection			Websocket Client Id
 *
 * 	\return						none
 */
void WebSocketHandshakeEventHandler(UINT16 uConnection)
{
	g_success = 1;
}

//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  None
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect()
{
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = (signed char*)SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    lRetVal = sl_WlanConnect((signed char*)SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Wait for WLAN Event
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))
    {
        // Toggle LEDs to Indicate Connection Progress
        GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
        GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
    }

    return SUCCESS;

}

#define ROLE_INVALID            (-5)
signed int g_uiIpAddress = 0;
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
volatile unsigned short g_usMCNetworkUstate = 0;
int g_uiSimplelinkRole = ROLE_INVALID;

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
long ConfigureSimpleLinkToDefaultState2()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = modifiedSocketsStartUp();
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);



    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();

    return lRetVal; // Success
}

//****************************************************************************
//
//!    \brief Connects to the Network in AP or STA Mode - If ForceAP Jumper is
//!                                             Placed, Force it to AP mode
//!
//! \return                        0 on success else error code
//
//****************************************************************************

long ConnectToNetwork()
{
    long lRetVal = -1;

    // starting simplelink
    g_uiSimplelinkRole =  sl_Start(NULL,NULL,NULL);

    // Ensure device is in STA mode
    if(g_uiSimplelinkRole != ROLE_STA)
    {
        // Switch to STA Mode
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(SL_STOP_TIMEOUT);

        g_usMCNetworkUstate = 0;
        g_uiSimplelinkRole =  sl_Start(NULL,NULL,NULL);
    }

    // Device should now be in STA mode, proceed to connect
    lRetVal = WlanConnect();
    ASSERT_ON_ERROR(lRetVal);

    // Device is connected to the desired Wi-Fi network in STA mode

    // Stop Internal HTTP Server
    lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR( lRetVal);

    // Start Internal HTTP Server
    lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR( lRetVal);

    UART_PRINT("\n\rDevice is in STA Mode, Connected to AP[%s] and type"
          " IP address [%d.%d.%d.%d] in the browser \n\r", g_ucConnectionSSID,
          SL_IPV4_BYTE(g_uiIpAddress,3), SL_IPV4_BYTE(g_uiIpAddress,2),
          SL_IPV4_BYTE(g_uiIpAddress,1), SL_IPV4_BYTE(g_uiIpAddress,0));

    return SUCCESS;
}

//****************************************************************************
//
//! HttpServerAppTask
//!
//! \param Initialize the network processor, stop internal HTTP server, start webserver
//!
//! \return none
//!
//****************************************************************************

void HttpServerAppTask(void * param)
{
	long lRetVal = -1;
	//InitializeAppVariables();

    lRetVal = ConfigureSimpleLinkToDefaultState2();
    if(lRetVal < 0)
    {
        //if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
        UART_PRINT("Failed to configure the device in its default state\n\r");

        LOOP_FOREVER();
    }

    UART_PRINT("Device is configured in default state \n\r");

    //memset(g_ucSSID,'\0',AP_SSID_LEN_MAX);

    //Read Device Mode Configuration
    //ReadDeviceConfiguration();

    //Connect to Network
    lRetVal = ConnectToNetwork();

	//Stop Internal HTTP Server
	lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }	

    UART_PRINT("Start Websocket Server \n\r");

    //
	// Run application library HTTP Server
    // Note this function does not return
    //
	HttpServerInitAndRun(NULL);

	LOOP_FOREVER();

}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
