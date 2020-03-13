/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2018 MAESTRO project
  ------------------------------------------------------------------------------
  -- File        : timecontrol.c
  -- Author      : Sebastian Loh Lindholm
  -- Description : MAESTRO
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
#include <signal.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

#include "timecontrol.h"
#include "logger.h"
#include "maestroTime.h"
#include "datadictionary.h"


#define TIME_CONTROL_HOSTNAME_BUFFER_SIZE 20
#define TIME_CONTROL_RECEIVE_BUFFER_SIZE 54
#define TIME_CONTROL_TASK_PERIOD_MS 1
#define TIME_INTERVAL_NUMBER_BYTES 4
#define REPLY_TIMEOUT_S 3

#define SLEEP_TIME_GPS_CONNECTED_S 0
#define SLEEP_TIME_GPS_CONNECTED_NS 5000000	//500000000
#define SLEEP_TIME_NO_GPS_CONNECTED_S 1
#define SLEEP_TIME_NO_GPS_CONNECTED_NS 0

#define LOG_PATH "./timelog/"
#define LOG_FILE "time.log"
#define LOG_BUFFER_LENGTH 128

#define FIX_QUALITY_NONE 0
#define FIX_QUALITY_BASIC 1
#define FIX_QUALITY_DIFFERENTIAL 2

// Time intervals for sleeping when no message bus message was received and for when one was received
#define TC_SLEEP_TIME_EMPTY_MQ_S 0
#define TC_SLEEP_TIME_EMPTY_MQ_NS 10000000
#define TC_SLEEP_TIME_NONEMPTY_MQ_S 0
#define TC_SLEEP_TIME_NONEMPTY_MQ_NS 0

/*------------------------------------------------------------
  -- Function declarations.
  ------------------------------------------------------------*/

static int TimeControlCreateTimeChannel(const char *name, const uint32_t port, int *sockfd,
										struct sockaddr_in *addr);
static int TimeControlSendUDPData(int *sockfd, struct sockaddr_in *addr, C8 * SendData, int Length,
								  char debug);
//static void TimeControlRecvTime(int* sockfd, char* buffer, int length, int* recievedNewData);
static void TimeControlRecvTime(int *sockfd, C8 * buffer, int length, int *recievedNewData);
U32 TimeControlIPStringToInt(C8 * IP);
U16 TimeControlGetMillisecond(TimeType * GPSTime);
static void TimeControlDecodeTimeBuffer(TimeType * GPSTime, C8 * TimeBuffer, C8 debug);
static void signalHandler(int signo);

/*------------------------------------------------------------
  -- Private variables.
  ------------------------------------------------------------*/
#define MODULE_NAME "TimeControl"
static volatile int iExit = 0;

/*------------------------------------------------------------
-- The main function.
------------------------------------------------------------*/
void timecontrol_task(TimeType * GPSTime, GSDType * GSD, LOG_LEVEL logLevel) {

	C8 TextBufferC8[TIME_CONTROL_HOSTNAME_BUFFER_SIZE];
	C8 ServerIPC8[TIME_CONTROL_HOSTNAME_BUFFER_SIZE];
	U16 ServerPortU16;
	I32 SocketfdI32 = -1;
	struct sockaddr_in time_addr;
	const struct timespec mqEmptyPollPeriod = { TC_SLEEP_TIME_EMPTY_MQ_S, TC_SLEEP_TIME_EMPTY_MQ_NS };
	const struct timespec mqNonEmptyPollPeriod =
		{ TC_SLEEP_TIME_NONEMPTY_MQ_S, TC_SLEEP_TIME_NONEMPTY_MQ_NS };

	I32 result;
	C8 TimeBuffer[TIME_CONTROL_RECEIVE_BUFFER_SIZE];
	C8 LogBuffer[LOG_BUFFER_LENGTH];
	I32 ReceivedNewData, i;
	C8 SendData[TIME_INTERVAL_NUMBER_BYTES] = { 0, 0, 3, 0xe8 };
	//C8 SendData[4] = {0, 0, 0, 1};
	struct timespec sleep_time, ref_time;
	C8 MqRecvBuffer[MBUS_MAX_DATALEN];
	struct timeval tv, ExecTime;
	struct tm *tm;

	U32 IpU32;
	U8 PrevSecondU8;
	U16 CurrentMilliSecondU16, PrevMilliSecondU16;
	U8 CycleCount = 0;

	enum COMMAND command;
	char busReceiveBuffer[MBUS_MAX_DATALEN];

	// Create log
	LogInit(MODULE_NAME, logLevel);
	LogMessage(LOG_LEVEL_INFO, "Time control task running with PID: %i", getpid());

	// Set up signal handlers
	if (signal(SIGINT, signalHandler) == SIG_ERR)
		util_error("Unable to initialize signal handler");

	// Set up message bus connection
	if (iCommInit())
		util_error("Unable to initialize message bus connection");

	GPSTime->isGPSenabled = 0;

	gettimeofday(&ExecTime, NULL);
	CurrentMilliSecondU16 = (U16) (ExecTime.tv_usec / 1000);
	PrevMilliSecondU16 = CurrentMilliSecondU16;
	// Set time server IP
	DataDictionaryGetTimeServerIPC8(GSD, ServerIPC8, TIME_CONTROL_HOSTNAME_BUFFER_SIZE);
	DataDictionaryGetTimeServerIPU32(GSD, &IpU32);

	// Set time server port
	DataDictionaryGetTimeServerPortU16(GSD, &ServerPortU16);

	// If time server is specified, connect to it
	if (IpU32 != 0) {
		LogMessage(LOG_LEVEL_INFO, "Connecting to time server...");

		if (TimeControlCreateTimeChannel(ServerIPC8, ServerPortU16, &SocketfdI32, &time_addr)) {
			LogMessage(LOG_LEVEL_INFO, "Using time server reference");
			TimeControlSendUDPData(&SocketfdI32, &time_addr, SendData, TIME_INTERVAL_NUMBER_BYTES, 0);
			GPSTime->isGPSenabled = 1;
		}
		else {
			LogMessage(LOG_LEVEL_INFO, "Defaulting to system time");

			// Send warning over MQ
			LOG_SEND(LogBuffer, "Unable to connect to time server");
		}

	}

	if (!GPSTime->isGPSenabled) {
		LogMessage(LOG_LEVEL_INFO, "Initializing with system time");

		gettimeofday(&tv, NULL);

		GPSTime->MicroSecondU16 = 0;
		GPSTime->GPSMillisecondsU64 =
			tv.tv_sec * 1000 + tv.tv_usec / 1000 - MS_TIME_DIFF_UTC_GPS + MS_LEAP_SEC_DIFF_UTC_GPS;
		GPSTime->GPSWeekU16 = (U16) (GPSTime->GPSMillisecondsU64 / WEEK_TIME_MS);
		GPSTime->GPSSecondsOfWeekU32 =
			(U32) ((GPSTime->GPSMillisecondsU64 - (U64) (GPSTime->GPSWeekU16) * WEEK_TIME_MS) / 1000);
		GPSTime->GPSSecondsOfDayU32 = (GPSTime->GPSMillisecondsU64 % DAY_TIME_MS) / 1000;
		GPSTime->GPSMinutesU32 = (GPSTime->GPSMillisecondsU64 / 1000) / 60;
		GPSTime->isTimeInitializedU8 = 1;
	}

	while (!iExit) {

		// Ignore any commands received, just empty the bus
		do {
			iCommRecv(&command, busReceiveBuffer, sizeof (busReceiveBuffer), NULL);
		} while (command != COMM_INV);

		gettimeofday(&ExecTime, NULL);
		CurrentMilliSecondU16 = (U16) (ExecTime.tv_usec / 1000);
		if (CurrentMilliSecondU16 < PrevMilliSecondU16) {
			GSD->TimeControlExecTimeU16 = CurrentMilliSecondU16 + (1000 - PrevMilliSecondU16);
			//printf("%d\n", GSD->TimeControlExecTimeU16);
		}
		else {
			GSD->TimeControlExecTimeU16 = abs(PrevMilliSecondU16 - CurrentMilliSecondU16);
			//printf("%d\n", GSD->TimeControlExecTimeU16);
		}
		PrevMilliSecondU16 = CurrentMilliSecondU16;

		if (GPSTime->isGPSenabled) {
			bzero(TimeBuffer, TIME_CONTROL_RECEIVE_BUFFER_SIZE);
			TimeControlRecvTime(&SocketfdI32, TimeBuffer, TIME_CONTROL_RECEIVE_BUFFER_SIZE, &ReceivedNewData);

			if (ReceivedNewData) {
				TimeControlDecodeTimeBuffer(GPSTime, TimeBuffer, 0);

				if (GPSTime->GPSMillisecondsU64 < INT64_MAX) {
					struct timespec newSystemTime;

					TimeSetToGPSms(&tv, (int64_t) GPSTime->GPSMillisecondsU64);
					newSystemTime.tv_sec = tv.tv_sec;
					newSystemTime.tv_nsec = tv.tv_usec * 1000;
					if (clock_settime(CLOCK_REALTIME, &newSystemTime) == -1) {
						switch (errno) {
						case EPERM:
							LogMessage(LOG_LEVEL_ERROR,
									   "Unable to set system time - ensure this program has the correct capabilities");
							break;
						case EINVAL:
							LogMessage(LOG_LEVEL_ERROR, "Clock type not supported on this system");
							break;
						default:
							LogMessage(LOG_LEVEL_ERROR, "Error setting system time");
							break;
						}
					}
				}
				else {
					LogMessage(LOG_LEVEL_ERROR,
							   "Current GPS time exceeds limit and would be interpreted as negative");
				}
			}
		}
		else if (!GPSTime->isGPSenabled) {
			gettimeofday(&tv, NULL);

			tm = localtime(&tv.tv_sec);

			// Add 1900 to get the right year value
			GPSTime->YearU16 = (U16) tm->tm_year + 1900;
			// Months are 0 based in struct tm
			GPSTime->MonthU8 = (U8) tm->tm_mon + 1;
			GPSTime->DayU8 = (U8) tm->tm_mday;
			GPSTime->HourU8 = (U8) tm->tm_hour;
			GPSTime->MinuteU8 = (U8) tm->tm_min;
			GPSTime->SecondU8 = (U8) tm->tm_sec;
			GPSTime->MillisecondU16 = (U16) (tv.tv_usec / 1000);

			GPSTime->LocalMillisecondU16 = (U16) (tv.tv_usec / 1000);

			GPSTime->GPSMillisecondsU64 = GPSTime->GPSMillisecondsU64 + 1000;

			if (GPSTime->SecondU8 != PrevSecondU8) {
				PrevSecondU8 = GPSTime->SecondU8;
				GPSTime->SecondCounterU32++;
				if (GPSTime->GPSSecondsOfDayU32 >= 86400)
					GPSTime->GPSSecondsOfDayU32 = 0;
				else
					GPSTime->GPSSecondsOfDayU32++;

				if (GPSTime->GPSSecondsOfWeekU32 >= 604800) {
					GPSTime->GPSSecondsOfWeekU32 = 0;
					GPSTime->GPSWeekU16++;
				}
				else
					GPSTime->GPSSecondsOfWeekU32++;

				if (GPSTime->SecondU8 == 0)
					GPSTime->GPSMinutesU32++;
			}
		}

		if (GSD->ExitU8 == 1) {
			if (GPSTime->isGPSenabled) {
				SendData[0] = 0;
				SendData[1] = 0;
				SendData[2] = 0;
				SendData[3] = 0;
				TimeControlSendUDPData(&SocketfdI32, &time_addr, SendData, TIME_INTERVAL_NUMBER_BYTES, 0);
			}
			iExit = 1;
			(void)iCommClose();
		}


		sleep_time = command == COMM_INV ? mqEmptyPollPeriod : mqNonEmptyPollPeriod;
		nanosleep(&sleep_time, &ref_time);
	}

	LogMessage(LOG_LEVEL_INFO, "Time control exiting");
}

void signalHandler(int signo) {
	if (signo == SIGINT) {
		LogMessage(LOG_LEVEL_WARNING, "Caught keyboard interrupt");
		iExit = 1;
	}
	else {
		LogMessage(LOG_LEVEL_ERROR, "Caught unhandled signal");
	}
}

U16 TimeControlGetMillisecond(TimeType * GPSTime) {
	struct timeval now;
	U16 MilliU16 = 0, NowU16 = 0;

	gettimeofday(&now, NULL);
	NowU16 = (U16) (now.tv_usec / 1000);
	//if(NowU16 >= GPSTime->LocalMillisecondU16) MilliU16 = NowU16 - GPSTime->LocalMillisecondU16;
	//else if(NowU16 < GPSTime->LocalMillisecondU16) MilliU16 = 1000 + ((I16)NowU16 - (I16)GPSTime->LocalMillisecondU16);

	if (NowU16 >= GPSTime->LocalMillisecondU16)
		MilliU16 = NowU16 - GPSTime->LocalMillisecondU16;
	else if (NowU16 < GPSTime->LocalMillisecondU16)
		MilliU16 = 1000 - GPSTime->LocalMillisecondU16 + NowU16;

	//printf("Result= %d, now= %d, local= %d \n", MilliU16, NowU16, GPSTime->LocalMillisecondU16);
	return MilliU16;
}

static int TimeControlCreateTimeChannel(const char *name, const uint32_t port, int *sockfd,
										struct sockaddr_in *addr) {
	int result;
	struct hostent *object;
	C8 packetIntervalMs[TIME_INTERVAL_NUMBER_BYTES] = { 0, 0, 0, 100 };	// Make server send with this interval while waiting for first reply
	C8 timeBuffer[TIME_CONTROL_RECEIVE_BUFFER_SIZE];
	int receivedNewData = 0;
	struct timeval timeout = { REPLY_TIMEOUT_S, 0 };
	struct timeval tEnd, tCurr;
	TimeType tempGPSTime;

	LogMessage(LOG_LEVEL_INFO, "Specified time server address: %s:%d", name, port);
	/* Connect to object safety socket */

	*sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (*sockfd < 0) {
		util_error("Failed to connect to time socket");
	}

	/* Set address to object */
	object = gethostbyname(name);

	if (object == NULL) {
		util_error("Unknown host");
	}

	bcopy((char *)object->h_addr, (char *)&addr->sin_addr.s_addr, object->h_length);
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);


	if (setsockopt(*sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof (timeout)) < 0)
		util_error("Setsockopt failed");



	/* set socket to non-blocking */
	result = fcntl(*sockfd, F_SETFL, fcntl(*sockfd, F_GETFL, 0) | O_NONBLOCK);
	if (result < 0) {
		util_error("Error calling fcntl");
	}
	LogMessage(LOG_LEVEL_INFO, "Created socket and time address: %s:%d", name, port);

	// Check for existence of remote server
	LogMessage(LOG_LEVEL_INFO, "Awaiting reply from time server...");
	// Set send interval to be as short as possible to minimise wait for reply
	TimeControlSendUDPData(sockfd, addr, packetIntervalMs, TIME_INTERVAL_NUMBER_BYTES, 0);

	// Set time to stop waiting for reply
	gettimeofday(&tEnd, NULL);
	timeradd(&tEnd, &timeout, &tEnd);

	do {
		gettimeofday(&tCurr, NULL);
		TimeControlRecvTime(sockfd, timeBuffer, TIME_CONTROL_RECEIVE_BUFFER_SIZE, &receivedNewData);
		if (receivedNewData) {
			TimeControlDecodeTimeBuffer(&tempGPSTime, timeBuffer, 0);
			switch (tempGPSTime.FixQualityU8) {
			case FIX_QUALITY_NONE:
				LogMessage(LOG_LEVEL_WARNING, "Received reply from time server: no satellite fix");
				return 0;
			case FIX_QUALITY_BASIC:
				LogMessage(LOG_LEVEL_INFO,
						   "Received reply from time server: non-differential fix on %d satellite(s)",
						   tempGPSTime.NSatellitesU8);
				return 1;
			case FIX_QUALITY_DIFFERENTIAL:
				LogMessage(LOG_LEVEL_INFO,
						   "Received reply from time server: differential fix on %d satellite(s)",
						   tempGPSTime.NSatellitesU8);
				return 1;
			default:
				LogMessage(LOG_LEVEL_ERROR,
						   "Received reply from time server: unexpected fix quality parameter");
				return 0;
			}
		}
	} while (timercmp(&tCurr, &tEnd, <));

	LogMessage(LOG_LEVEL_WARNING, "Unable to connect to specified time server: %s:%d", name, port);
	return 0;
}


U32 TimeControlIPStringToInt(C8 * IP) {
	C8 *p, *ps;
	C8 Buffer[3];
	U32 IpU32 = 0;

	ps = IP;
	p = strchr(IP, '.');
	if (p != NULL) {
		bzero(Buffer, 3);
		strncpy(Buffer, ps, (U64) p - (U64) ps);
		IpU32 = (IpU32 | (U32) atoi(Buffer)) << 8;

		ps = p + 1;
		p = strchr(ps, '.');
		bzero(Buffer, 3);
		strncpy(Buffer, ps, (U64) p - (U64) ps);

		IpU32 = (IpU32 | (U32) atoi(Buffer)) << 8;

		ps = p + 1;
		p = strchr(ps, '.');
		bzero(Buffer, 3);
		strncpy(Buffer, ps, (U64) p - (U64) ps);

		IpU32 = (IpU32 | (U32) atoi(Buffer)) << 8;

		ps = p + 1;
		p = strchr(ps, 0);
		bzero(Buffer, 3);
		strncpy(Buffer, ps, (U64) p - (U64) ps);

		IpU32 = (IpU32 | (U32) atoi(Buffer));

		//printf("IpU32 = %x\n", IpU32);
	}

	return IpU32;
}


static int TimeControlSendUDPData(int *sockfd, struct sockaddr_in *addr, C8 * SendData, int Length,
								  char debug) {
	int result, i;

	result = sendto(*sockfd, SendData, Length, 0, (const struct sockaddr *)addr, sizeof (struct sockaddr_in));


	if (debug) {
		// TODO: Change to log write when bytes thingy has been implemented
		for (i = 0; i < Length; i++)
			printf("[%d]=%x ", i, (C8) * (SendData + i));
		printf("\n");
	}

	if (result < 0) {
		util_error("Failed to send on time socket");
	}

	return 0;
}


static void TimeControlRecvTime(int *sockfd, C8 * buffer, int length, int *receivedNewData) {
	int result;

	*receivedNewData = 0;
	do {
		result = recv(*sockfd, buffer, length, 0);

		if (result < 0) {
			// If we received a _real_ error, report it. Otherwise, nothing was received
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				util_error("Failed to receive from time socket");
			else
				return;
		}
		else if (result == 0) {
			// EOF received
			LogMessage(LOG_LEVEL_ERROR, "Time server disconnected");
			*receivedNewData = 0;
			return;
		}
		else {
			// If message size is equal to what is expected according to the format, keep reading until the newest has been read
			if (result == length) {
				*receivedNewData = 1;
				LogMessage(LOG_LEVEL_DEBUG, "Received data: <%s>, result=%d", buffer, result);
			}
			else {
				*receivedNewData = 0;
				LogMessage(LOG_LEVEL_ERROR, "Received badly formatted message from time server");
			}

		}
	} while (result > 0);
	return;
}

static void TimeControlDecodeTimeBuffer(TimeType * GPSTime, C8 * TimeBuffer, C8 debug) {
	struct timeval tv;

	gettimeofday(&tv, NULL);

	GPSTime->ProtocolVersionU8 = TimeBuffer[0];
	GPSTime->YearU16 = ((U16) TimeBuffer[1]) << 8 | TimeBuffer[2];
	GPSTime->MonthU8 = TimeBuffer[3];
	GPSTime->DayU8 = TimeBuffer[4];
	GPSTime->HourU8 = TimeBuffer[5];
	GPSTime->MinuteU8 = TimeBuffer[6];
	GPSTime->SecondU8 = TimeBuffer[7];
	GPSTime->MillisecondU16 = ((U16) TimeBuffer[8]) << 8 | TimeBuffer[9];
	GPSTime->MicroSecondU16 = 0;
	GPSTime->SecondCounterU32 =
		((U32) TimeBuffer[10]) << 24 | ((U32) TimeBuffer[11]) << 16 | ((U32) TimeBuffer[12]) << 8 |
		TimeBuffer[13];
	GPSTime->GPSMillisecondsU64 =
		((U64) TimeBuffer[14]) << 56 | ((U64) TimeBuffer[15]) << 48 | ((U64) TimeBuffer[16]) << 40 | ((U64)
																									  TimeBuffer
																									  [17]) <<
		32 | ((U64) TimeBuffer[18]) << 24 | ((U64) TimeBuffer[19]) << 16 | ((U64) TimeBuffer[20]) << 8 |
		TimeBuffer[21];
	GPSTime->GPSMinutesU32 =
		((U32) TimeBuffer[22]) << 24 | ((U32) TimeBuffer[23]) << 16 | ((U32) TimeBuffer[24]) << 8 |
		TimeBuffer[25];
	GPSTime->GPSWeekU16 = ((U16) TimeBuffer[26]) << 8 | TimeBuffer[27];
	GPSTime->GPSSecondsOfWeekU32 =
		((U32) TimeBuffer[28]) << 24 | ((U32) TimeBuffer[29]) << 16 | ((U32) TimeBuffer[30]) << 8 |
		TimeBuffer[31] + MS_LEAP_SEC_DIFF_UTC_GPS / 1000;
	GPSTime->GPSSecondsOfDayU32 =
		((U32) TimeBuffer[32]) << 24 | ((U32) TimeBuffer[33]) << 16 | ((U32) TimeBuffer[34]) << 8 |
		TimeBuffer[35];
	GPSTime->ETSIMillisecondsU64 =
		((U64) TimeBuffer[36]) << 56 | ((U64) TimeBuffer[37]) << 48 | ((U64) TimeBuffer[38]) << 40 | ((U64)
																									  TimeBuffer
																									  [39]) <<
		32 | ((U64) TimeBuffer[40]) << 24 | ((U64) TimeBuffer[41]) << 16 | ((U64) TimeBuffer[42]) << 8 |
		TimeBuffer[43];
	GPSTime->LatitudeU32 =
		((U32) TimeBuffer[44]) << 24 | ((U32) TimeBuffer[45]) << 16 | ((U32) TimeBuffer[46]) << 8 |
		TimeBuffer[47];
	GPSTime->LongitudeU32 =
		((U32) TimeBuffer[48]) << 24 | ((U32) TimeBuffer[49]) << 16 | ((U32) TimeBuffer[50]) << 8 |
		TimeBuffer[51];
	GPSTime->FixQualityU8 = TimeBuffer[52];
	GPSTime->NSatellitesU8 = TimeBuffer[53];

	gettimeofday(&tv, NULL);

	GPSTime->LocalMillisecondU16 = (U16) (tv.tv_usec / 1000);

	GPSTime->isTimeInitializedU8 = 1;

	if (debug) {
		//TimeControlGetMillisecond(GPSTime);
		//LogPrintBytes(TimeBuffer,0,TIME_CONTROL_RECEIVE_BUFFER_SIZE);
		//LogPrint("ProtocolVersionU8: %d", GPSTime->ProtocolVersionU8);
		LogPrint("YearU16: %d", GPSTime->YearU16);
		LogPrint("MonthU8: %d - %d", GPSTime->MonthU8, TimeBuffer[3]);
		LogPrint("DayU8: %d", GPSTime->DayU8);
		LogPrint("Time: %d:%d:%d", GPSTime->HourU8, GPSTime->MinuteU8, GPSTime->SecondU8);
		//LogPrint("MinuteU8: %d", GPSTime->MinuteU8);
		//LogPrint("SecondU8: %d", GPSTime->SecondU8);
		//LogPrint("MillisecondU16: %d", GPSTime->MillisecondU16);
		//LogPrint("SecondCounterU32: %d", GPSTime->SecondCounterU32);
		//LogPrint("GPSMillisecondsU64: %ld", GPSTime->GPSMillisecondsU64);
		//LogPrint("GPSMinutesU32: %d", GPSTime->GPSMinutesU32);
		//LogPrint("GPSWeekU16: %d", GPSTime->GPSWeekU16);
		//LogPrint("GPSSecondsOfWeekU32: %d", GPSTime->GPSSecondsOfWeekU32);
		//LogPrint("GPSSecondsOfDayU32: %d", GPSTime->GPSSecondsOfDayU32);
		//LogPrint("ETSIMillisecondsU64: %ld", GPSTime->ETSIMillisecondsU64);
		//LogPrint("LatitudeU32: %d", GPSTime->LatitudeU32);
		//LogPrint("LongitudeU32: %d", GPSTime->LongitudeU32);
		//LogPrint("LocalMillisecondU16: %d", GPSTime->LocalMillisecondU16);
		//LogPrint("FixQualityU8: %d", GPSTime->FixQualityU8);
		//LogPrint("NSatellitesU8: %d", GPSTime->NSatellitesU8);
	}
}