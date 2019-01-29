/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2018 CHRONOS II project
  ------------------------------------------------------------------------------
  -- File        : citscontrol.c
  -- Author      : Sebastian Loh Lindholm
  -- Description : CHRONOS II
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>  

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

#include "util.h"
#include "logger.h"


#define CITS_CONTROL_CONF_FILE_PATH  "conf/test.conf"
#define CITS_CONTROL_BUFFER_SIZE_20 20
#define CITS_CONTROL_BUFFER_SIZE_52 52
#define CITS_CONTROL_TASK_PERIOD_MS 1

/*------------------------------------------------------------
  -- Function declarations.
  ------------------------------------------------------------*/



/*------------------------------------------------------------
-- The main function.
------------------------------------------------------------*/
int citscontrol_task(TimeType *GPSTime, GSDType *GSD)
{

  I32 iExit = 0, iCommand;
  C8 MqRecvBuffer[MQ_MAX_MESSAGE_LENGTH];
  (void)iCommInit(IPC_RECV_SEND,MQ_LG,0);


  printf("Starting cits control...\n");
  while(!iExit)
  {


    bzero(MqRecvBuffer,MQ_MAX_MESSAGE_LENGTH);
    (void)iCommRecv(&iCommand,MqRecvBuffer,MQ_MAX_MESSAGE_LENGTH);

    if(iCommand == COMM_EXIT)
    {
      iExit = 1;
      printf("citscontrol exiting.\n");
      (void)iCommClose();
    }
    
  }


}


