/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2016 CHRONOS project
  ------------------------------------------------------------------------------
  -- File        : logger.c
  -- Author      : Karl-Johan Ode, Sebastian Loh Lindholm
  -- Description : CHRONOS
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/
#include "logger.h"

#include <dirent.h>
#include <errno.h>
#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <time.h>

#include "util.h"

/*------------------------------------------------------------
  -- Defines
  ------------------------------------------------------------*/
#define LOG_PATH "./log/"
#define LOG_FILE "event.log"
#define LOG_CONTROL_MODE 0
#define LOG_REPLAY_MODE 1
#define TASK_PERIOD_MS 1
#define HEARTBEAT_TIME_MS 10
#define TIMESTAMP_BUFFER_LENGTH 20
#define SPECIFIC_CHAR_THRESHOLD_COUNT 10  

/*------------------------------------------------------------
  -- Function declarations.
  ------------------------------------------------------------*/
static void vCreateLogFolder(char logFolder[MAX_FILE_PATH]);
static int ReadLogLine(FILE *fd, char *Buffer);
static int CountFileRows(FILE *fd);

/*------------------------------------------------------------
-- Private variables
------------------------------------------------------------*/


/*------------------------------------------------------------
  -- Public functions
  ------------------------------------------------------------*/
void logger_task()
{
  char pcCommand[100];
  char pcLogFolder[MAX_FILE_PATH];
  char pcLogFile[MAX_FILE_PATH];
  char pcBuffer[MQ_MAX_MESSAGE_LENGTH+100];
  char pcRecvBuffer[MQ_MAX_MESSAGE_LENGTH];
  char pcSendBuffer[MQ_MAX_MESSAGE_LENGTH];
  char pcReadBuffer[MQ_MAX_MESSAGE_LENGTH];
  struct timeval tvTime;
  FILE *filefd;
  FILE *replayfd;
  struct timespec sleep_time, ref_time;


	(void)iCommInit(IPC_RECV_SEND,MQ_LG,0);
	//(void)iCommInit(IPC_SEND,MQ_LG_1,0);

  /* Create folder and event.log file */
  vCreateLogFolder(pcLogFolder);
  (void)strcpy(pcLogFile,pcLogFolder);
  (void)strcat(pcLogFile,LOG_FILE);

  //#ifdef DEBUG
    printf("INF: Open log file to use: <%s>\n",pcLogFile);
    fflush(stdout);
  //#endif
  filefd = fopen (pcLogFile, "w+");

  bzero(pcBuffer,MQ_MAX_MESSAGE_LENGTH+100);
  strcpy(pcBuffer, "Log started...\n");
  (void)fwrite(pcBuffer,1,strlen(pcBuffer),filefd);

  /* Copy drive files */
  (void)strcpy(pcCommand,"cp -R ");
  (void)strcat(pcCommand,TRAJECTORY_PATH);
  (void)strcat(pcCommand," ");
  (void)strcat(pcCommand,pcLogFolder);
  (void)system(pcCommand);

  /* Copy conf file */
  (void)strcpy(pcCommand,"cp ");
  (void)strcat(pcCommand,TEST_CONF_FILE);
  (void)strcat(pcCommand," ");
  (void)strcat(pcCommand,pcLogFolder);
  (void)system(pcCommand);

  /* Listen for commands */
  int iExit = 0;
  int iCommand;

	/* Execution mode*/
	int LoggerExecutionMode = LOG_CONTROL_MODE;

	int RowCount = 0;

	while(!iExit)
	{
		bzero(pcRecvBuffer,MQ_MAX_MESSAGE_LENGTH);
		(void)iCommRecv(&iCommand,pcRecvBuffer,MQ_MAX_MESSAGE_LENGTH);

		if(LoggerExecutionMode == LOG_CONTROL_MODE)
		{		
			/* Write time, command, buffer */
			gettimeofday(&tvTime, NULL);
			uint64_t uiTime = (uint64_t)tvTime.tv_sec*1000 + (uint64_t)tvTime.tv_usec/1000 - 
			  MS_FROM_1970_TO_2004_NO_LEAP_SECS + DIFF_LEAP_SECONDS_UTC_ETSI*1000;
			bzero(pcBuffer,MQ_MAX_MESSAGE_LENGTH+100);
			sprintf ( pcBuffer,"%" PRIu64 ": %d %s\n",uiTime,iCommand,pcRecvBuffer);
			//printf("INF: Data written to logfile <%s>", pcBuffer);			
			(void)fwrite(pcBuffer,1,strlen(pcBuffer),filefd);
		}

		if(iCommand == COMM_REPLAY)
		{
			LoggerExecutionMode = LOG_REPLAY_MODE;
			printf("Logger in REPLAY mode <%s>\n", pcRecvBuffer);
			//replayfd = fopen ("log/33/event.log", "r");
			replayfd = fopen (pcRecvBuffer, "r");
			RowCount = CountFileRows(replayfd);
			fclose(replayfd);
			//replayfd = fopen ("log/33/event.log", "r");
			replayfd = fopen (pcRecvBuffer, "r");
			printf("Rows %d\n", RowCount);	
			if(replayfd)
			{			
				ReadLogLine(replayfd, pcReadBuffer);//Just read first line
				int SpecChars = 0, j=0;
				char TimestampBuffer[TIMESTAMP_BUFFER_LENGTH];
				int FirstIteration = 1;
				char *src;			
				uint64_t NewTimestamp, OldTimestamp;
				do
				{			
					bzero(pcReadBuffer,MQ_MAX_MESSAGE_LENGTH);			
					SpecChars = ReadLogLine(replayfd, pcReadBuffer);
					
					j++;
					if(SpecChars == SPECIFIC_CHAR_THRESHOLD_COUNT)
					{				
						/* Read to second ';' in row = 418571059920: 3 1;0;418571059920;577776566;127813082;0;0;3600;0; */			
						src = strchr(pcReadBuffer, ';');
						src = strchr(src+1, ';');

						/* Get the current timestamp */					
						bzero(TimestampBuffer, TIMESTAMP_BUFFER_LENGTH);
						strncpy(TimestampBuffer, src+1, (uint64_t)strchr(src+1, ';') - (uint64_t)strchr(src, ';')  - 1);
						NewTimestamp = atol(TimestampBuffer);

						if(!FirstIteration)
						{	/* Wait a little bit */
							sleep_time.tv_sec = 0;
							sleep_time.tv_nsec = (NewTimestamp - OldTimestamp)*1000000;
							(void)nanosleep(&sleep_time,&ref_time);
						} else OldTimestamp = NewTimestamp;

						//printf("Wait time : %ld ms\n", NewTimestamp - OldTimestamp);
						//printf("Timestamp: %s\n", TimestampBuffer);
						/* Build the message */					
						/* Read to second ' ' in row = 418571059920: 3 1;0;418571059920;577776566;127813082;0;0;3600;0; */			
						src = strchr(pcReadBuffer, ' ');
						src = strchr(src+1, ' ');					
						bzero(pcSendBuffer,MQ_MAX_MESSAGE_LENGTH);			
						//strcpy(pcSendBuffer, "MONR;");					
						strcat(pcSendBuffer, src+1);
						(void)iCommSend(COMM_MONI, pcSendBuffer);
						FirstIteration = 0;
						OldTimestamp = NewTimestamp;
					};
					printf("%d:%d:%d<%s>\n", RowCount, j, SpecChars, pcSendBuffer);	

					/*
					(void)iCommRecv(&iCommand,pcRecvBuffer,MQ_MAX_MESSAGE_LENGTH);

					if(iCommand == COMM_STOP)
					{
						printf("Replay stopped by user.\n");
						(void)iCommSend(COMM_CONTROL, NULL);
					}*/
				
				} while(--RowCount >= 0);

			} 
			else
			{ 
				printf("Failed to open file:%s\n",  pcRecvBuffer);
			}

			printf("Replay done.\n");
			//(void)iCommInit(IPC_RECV_SEND,MQ_LG,0);
			(void)iCommSend(COMM_CONTROL, NULL);

		}
		else if(iCommand == COMM_CONTROL)
		{
			LoggerExecutionMode = LOG_CONTROL_MODE;
			printf("Logger in CONTROL mode\n");		  
		}
		else if(iCommand == COMM_EXIT)
		{
			#ifdef DEBUG
				printf("Logger exit\n");
			#endif		  
			iExit = 1;  
		}
		else
		{
		  #ifdef DEBUG
			printf("Unhandled command in logger\n");
		  #endif
		}
	}

  (void)iCommClose();

  bzero(pcBuffer,MQ_MAX_MESSAGE_LENGTH+100);
  strcpy(pcBuffer, "Log closed...\n");
  (void)fwrite(pcBuffer,1,strlen(pcBuffer),filefd);

  fclose(filefd);
}

/*------------------------------------------------------------
  -- Private functions
  ------------------------------------------------------------*/

int CountFileRows(FILE *fd)
{
	int c = 0;
	int rows = 0;

	while(c != EOF)
	{
		c = fgetc(fd);
		if(c == '\n') rows++;
	}

	return rows;
}


int ReadLogLine(FILE *fd, char *Buffer)
{
	int c = 0;
	int SpecChars = 0;

	while( (c != EOF) && (c != '\n') )
	{
		c = fgetc(fd);
		if(c != '\n')
		{
			*Buffer = (char)c;
		 	Buffer++;
		 	if(c == ';' || c == ':') SpecChars++;
		} 
	}

	return SpecChars;
}




void vCreateLogFolder(char logFolder[MAX_FILE_PATH])
{
  int iResult;
  DIR* directory;
  struct dirent *directory_entry;
  int iMaxFolder = 0;

  directory = opendir(LOG_PATH);
  if(directory == NULL)
  {
    iResult = mkdir(LOG_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (iResult < 0)
    {
      util_error("ERR: Failed to create LOG dir");
    }
  }

  while (directory_entry = readdir(directory))
  {
    
    /* Check so it's not . or .. */
    if (strncmp(directory_entry->d_name,".",1))
    {
      
      int iTemp = atoi(directory_entry->d_name);

      if( iTemp > iMaxFolder)
      {
        iMaxFolder = iTemp;
      }
    }
  }
  (void)closedir(directory);

  /* step up one to create a new folder */
  ++iMaxFolder;
  bzero(logFolder,MAX_FILE_PATH);
  sprintf(logFolder,"%s%d/",LOG_PATH,iMaxFolder);

  iResult = mkdir(logFolder, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (iResult < 0)
  {
    util_error("ERR: Failed to create dir");
  }
}
