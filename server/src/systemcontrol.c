/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2016 CHRONOS project
  ------------------------------------------------------------------------------
  -- File        : systemcontrol.c
  -- Author      : Karl-Johan Ode, Sebastian Loh Lindholm
  -- Description : CHRONOS
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/
#include <mqueue.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "systemcontrol.h"



#include "util.h"


/*------------------------------------------------------------
  -- Defines
  ------------------------------------------------------------*/
typedef enum {
  SERVER_STATUS_INIT,
  SERVER_STATUS_OBJECT_CONNECTED,
  SERVER_STATUS_OBJECT_LOADED,
  SERVER_STATUS_ARMED,
  SERVER_STATUS_RUNNING,
  SERVER_STATUS_STOPPED,
  SERVER_STATUS_DONE
} state_t;

#define IPC_BUFFER_SIZE   256

#define SYSTEM_CONTROL_ARG_COUNT 			2
#define SYSTEM_CONTROL_COMMAND_MAX_LENGTH 	10

typedef enum {
   idle_0, status_0, arm_0, start_0, stop_0, replay_1, control_0, exit_0, cx_0, cc_0, pt_0, tsp_1, nocommand
} SystemControlCommand_t;
const char* SystemControlCommandsArr[] = { "idle_0", "status_0", "arm_0", "start_0", "stop_0", "replay_1", "control_0", "exit_0", "cx_0", "cc_0", "pt_0", "tsp_1"};
SystemControlCommand_t PreviousSystemControlCommand = nocommand;
char SystemControlCommandArgCnt[SYSTEM_CONTROL_ARG_COUNT];
char SystemControlStrippedCommand[SYSTEM_CONTROL_COMMAND_MAX_LENGTH];

/*------------------------------------------------------------
  -- Function declarations.
  ------------------------------------------------------------*/
SystemControlCommand_t SystemControlFindCommand(const char* CommandBuffer, SystemControlCommand_t *CurrentCommand, int *ArgCount);
/*------------------------------------------------------------
  -- Public functions
  ------------------------------------------------------------*/
void systemcontrol_task()
{
	state_t server_state = SERVER_STATUS_INIT;
	SystemControlCommand_t SystemControlCommand = idle_0;

	int CommandArgCount=0, CurrentCommandArgCount=0;
	char pcBuffer[IPC_BUFFER_SIZE];
	char inchr;
	struct timeval tvTime;
	int iExit = 0;
	(void)iCommInit(IPC_SEND,MQ_SC,0);
	ObjectPosition OP;

	while(!iExit)
	{
		//printf("Buffer: %s\n", pcBuffer);
		bzero(pcBuffer,IPC_BUFFER_SIZE);

		scanf("%49s",pcBuffer);
		
		SystemControlFindCommand(pcBuffer, &SystemControlCommand, &CommandArgCount);
		
		switch(SystemControlCommand)
		{
			case idle_0:
 				CurrentCommandArgCount = 0;
			break;
			case status_0:
				printf("Server status: %d\n", server_state);
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case arm_0:
				(void)iCommSend(COMM_ARMD,NULL);
			    server_state = SERVER_STATUS_ARMED;
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case start_0:
				bzero(pcBuffer, IPC_BUFFER_SIZE);

				gettimeofday(&tvTime, NULL);

				uint64_t uiTime = (uint64_t)tvTime.tv_sec*1000 + (uint64_t)tvTime.tv_usec/1000 - 
				MS_FROM_1970_TO_2004_NO_LEAP_SECS + 
				DIFF_LEAP_SECONDS_UTC_ETSI*1000;

				/* Add 5 seconds to get room for all objects to get command */
				uiTime += 5000;

				sprintf ( pcBuffer,"%" PRIu8 ";%" PRIu64 ";",0,uiTime);

				//#ifdef DEBUG
				printf("INF: System control Sending TRIG on IPC <%s>\n",pcBuffer);
				fflush(stdout);
				//#endif

				(void)iCommSend(COMM_TRIG,pcBuffer);
				server_state = SERVER_STATUS_RUNNING;
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case stop_0:
				(void)iCommSend(COMM_STOP,NULL);
				server_state = SERVER_STATUS_STOPPED;
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case replay_1:
				if(CurrentCommandArgCount == CommandArgCount)
				{
					if(!strcmp(pcBuffer,"-help"))
					{
						printf("-----REPLAY-----\n");
						printf("Syntax: replay [arg]\n");
						printf("Ex: replay log/33/event.log\n");
					}
					else
					{
						(void)iCommSend(COMM_REPLAY, pcBuffer);
						printf("INF: System control sending REPLAY on IPC <%s>\n", pcBuffer);
						fflush(stdout);
					}
					SystemControlCommand = idle_0;
					CurrentCommandArgCount = 0;
				} else CurrentCommandArgCount ++;
			break;
			case control_0:
				(void)iCommSend(COMM_CONTROL, NULL);
				printf("INF: System control sending CONTROL on IPC <%s>\n", pcBuffer);
				fflush(stdout);
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case exit_0:
				(void)iCommSend(COMM_EXIT,NULL);
				iExit = 1;
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
			break;
			case cx_0:
				printf("Argument count: %d\n", CommandArgCount);
				printf("CurrentCommandArgCount: %d\n", CurrentCommandArgCount);
				SystemControlCommand = idle_0;
				CurrentCommandArgCount = 0;
				CommandArgCount = 0;
				printf("Current active command: %s\n", SystemControlCommandsArr[SystemControlCommand]);
			break;
			case cc_0:
				printf("Command waiting for input: %s\n", SystemControlCommandsArr[PreviousSystemControlCommand]);				
				//printf("Argument count: %d\n", SystemControlCommandsArgCnt[PreviousSystemControlCommand]);
				printf("CurrentCommandArgCount: %d\n", CurrentCommandArgCount-1);
				SystemControlCommand = PreviousSystemControlCommand;
			break;
			case pt_0:
				SystemControlCommand = idle_0;
//				UtilCalcPositionDelta(57.6626302333,12.1056869167,57.6626269972, 12.1057250694, &OP);

				//UtilCalcPositionDelta(57.466397884,12.469130065,57.466397865, 12.469129955, &OP);
				//UtilCalcPositionDelta(57.7773298066,12.7818834416,57.777329775, 12.7818832583, &OP);
				UtilCalcPositionDelta(57.77752072,12.7813972116667, 57.77785588,12.7811657866667, &OP);


//				UtilCalcPositionDelta(57.7773716086,12.7804629583,57.7773717086, 12.7804630583, &OP);
				printf("p1(57.777125795, 12.781569353) -> p2(57.777156948, 12.781550638) => Calc d = %4.3f m, iterations = %d\n", OP.OrigoDistance, OP.CalcIterations);
				printf("Latitude = %3.10f \n", OP.Latitude);
				printf("Longitude = %3.10f\n", OP.Longitude);
				printf("ForwardAzimuth1 = %3.10f \n", OP.ForwardAzimuth1);
				printf("ForwardAzimuth2 = %3.10f\n", OP.ForwardAzimuth2);
				printf("DeltaForwardAzimuth = %3.15f \n", OP.ForwardAzimuth1-OP.ForwardAzimuth2);
				printf("x = %4.15f\n", OP.x);
				printf("y = %4.15f\n", OP.y);

				//printf("d = 0.590 m, calc d = %4.3f m\n", UtilCalcPositionDelta(57.6626340000,12.1056979028,57.6626332417, 12.1057076556));
				//printf("d = 2.643 m, calc d = %4.3f m\n", UtilCalcPositionDelta(57.6626378750,12.1056752083,57.6626339472, 12.1057190472));
				//printf("d = 1.901 m, calc d = %4.3f m\n", UtilCalcPositionDelta(57.6626295889,12.1056851083,57.6626267528, 12.1057165278));
				//printf("d = 1.453 m, calc d = %4.3f m\n", UtilCalcPositionDelta(57.6626295444,12.1056859139,57.6626273222, 12.1057100556));
				

			break;
			case tsp_1:

				if(CurrentCommandArgCount == 1)
				{
					if(!strcmp(pcBuffer,"-help"))
					{
						printf("-----TEST SYNCHRONIZATION POINT FUNCTIONS-----\n");
						printf("The function returns the time until reaching a specific position in the trajectory.\n");
						printf("Arguments: 1\n");
						printf("Syntax: tsp something\n");
						//printf("Ex: ftp traj/traj.txt 45.568 -80.445 0\n");
						printf("Ex: tsp \n");
						SystemControlCommand = idle_0;
						CurrentCommandArgCount = 0;
					}
				}

				if(CurrentCommandArgCount == CommandArgCount)
				{
					FILE *Trajfd;
  					Trajfd = fopen ("traj/192.168.0.1", "r");
    				OP.TrajectoryPositionCount = UtilCountFileRows(Trajfd) - 1;
    				float SpaceArr[OP.TrajectoryPositionCount];
    				float TimeArr[OP.TrajectoryPositionCount];
    				SpaceTime SpaceTimeArr[OP.TrajectoryPositionCount];
    				fclose(Trajfd);
    				OP.SpaceArr = SpaceArr;
					OP.TimeArr = TimeArr;
					OP.SpaceTimeArr = SpaceTimeArr;
					//UtilCalcPositionDelta(57.7773716086,12.7804629583, 57.7773620, 12.7819188, &OP); //1
					UtilCalcPositionDelta(57.7773716086,12.7804629583,57.7776699,12.7808555, &OP); //2
					//UtilCalcPositionDelta(57.7773716086,12.7804629583,57.7776528,12.7812795, &OP); //3
					//printf("OrigoDistance = %4.3f\n", OP.OrigoDistance);
					if(OP.OrigoDistance > -1)
					{	
						UtilPopulateSpaceTimeArr(&OP, "traj/192.168.0.1");
						UtilSetSyncPoint(&OP, 60.039, -25.547, 0, 0);
						if (OP.SyncIndex > -1)
						{
							printf("Sync point found=%4.3f, Time=%4.3f, Index=%d\n", OP.SpaceArr[OP.SyncIndex], OP.TimeArr[OP.SyncIndex], OP.SyncIndex);
							double CurrentTime = 15.1;				
							//UtilFindCurrentTrajectoryPosition(&OP, 0, 30.9, 0.2);  //1
							UtilFindCurrentTrajectoryPosition(&OP, 0, CurrentTime, 0.2);	//2
							//UtilFindCurrentTrajectoryPosition(&OP, 0, 26.6, 0.2);	//3
							if(OP.BestFoundTrajectoryIndex > -1 && OP.SyncIndex > -1)
							{	
							    printf("Current origo distance=%4.3f m\n", OP.OrigoDistance);
							    printf("Matched origo distance=%4.3f m\n", OP.SpaceArr[OP.BestFoundTrajectoryIndex]);
							    printf("Distance error=%4.3f m\n", OP.OrigoDistance - OP.SpaceArr[OP.BestFoundTrajectoryIndex]);
							    printf("Current time=%4.3f s\n", CurrentTime);
							    printf("Expected time=%4.3f s (index=%d)\n", OP.TimeArr[OP.BestFoundTrajectoryIndex], OP.BestFoundTrajectoryIndex);
							    printf("Time error=%4.3f s\n", CurrentTime - OP.TimeArr[OP.BestFoundTrajectoryIndex]);
								printf("Time to sync point = %4.3f s\n", fabs(UtilCalculateTimeToSync(&OP) - (CurrentTime - OP.TimeArr[OP.BestFoundTrajectoryIndex]))); 
							} else printf("Failed to find current position in trajectory\n");
						
						} else printf("Failed to find sync point!\n");
					} else printf("Distance calculation to origo failed to converge.\n");
					SystemControlCommand = idle_0;
					CurrentCommandArgCount = 0;
				} else CurrentCommandArgCount ++;
			break;
			
			default:

			break;

		}

  }
  (void)iCommClose();
}

/*------------------------------------------------------------
  -- Private functions
  ------------------------------------------------------------*/

SystemControlCommand_t SystemControlFindCommand(const char* CommandBuffer, SystemControlCommand_t *CurrentCommand, int *CommandArgCount)
{

	SystemControlCommand_t command; 	
	
	for (command = idle_0; command != nocommand; command++)
    {
		bzero(SystemControlCommandArgCnt, SYSTEM_CONTROL_ARG_COUNT);    
        bzero(SystemControlStrippedCommand, SYSTEM_CONTROL_COMMAND_MAX_LENGTH);
		strncpy(SystemControlStrippedCommand, SystemControlCommandsArr[(int)command], (uint64_t)strchr(SystemControlCommandsArr[(int)command],'_') - (uint64_t)SystemControlCommandsArr[(int)command] );
		strncpy(SystemControlCommandArgCnt, strchr(SystemControlCommandsArr[(int)command],'_')+1, strlen(SystemControlCommandsArr[(int)command]) - ((uint64_t)strchr(SystemControlCommandsArr[(int)command],'_') - (uint64_t)SystemControlCommandsArr[(int)command] + 1));
		
		if (!strcmp(SystemControlStrippedCommand, CommandBuffer))
        {
			if(command != cc_0)
			{
				*CommandArgCount = atoi(SystemControlCommandArgCnt);				
				*CurrentCommand = command;            
				return command;
			} 
			else
			{
				PreviousSystemControlCommand = *CurrentCommand;
				*CurrentCommand = command;
				return command;
			}
        }
    }
    return nocommand;
}


