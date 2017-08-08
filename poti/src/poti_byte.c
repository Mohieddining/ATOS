/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2016 CHRONOS project
  --------------------------------------------------------------------------------
  -- File        : poti_byte.c
  -- Author      : Henrik Eriksson
  -- Description : Byte-based POTI service for uncontrolled object
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdint.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "nmea2etsi.h"

/*------------------------------------------------------------
  -- Defines.
  ------------------------------------------------------------*/

#define SAFETY_CHANNEL_PORT    53240       // Default port, safety channel
#define CONTROL_CHANNEL_PORT   53241       // Default port, control channel
#define ORIGIN_LATITUDE        57.7773602  // Default latitude,  origin
#define ORIGIN_LONGITUDE       012.7804715 // Default longitude, origin
#define ORIGIN_ALTITUDE        201.485     // Default altitude,  origin
#define DATALEN                10000
#define SAVED_TRAJECTORY_LINES 100
#define PI                     acos(-1)
#define DEBUG 0

#define BufferLength 80

#define SERVER "127.0.0.1"

#define SERVPORT 2948

#define RTK_RECV_BUFFER 4096
#define WORK_BUFFER 8192

/* 34 years between 1970 and 2004, 8 days for leap year between 1970 and 2004      */
/* Calculation: 34 * 365 * 24 * 3600 * 1000 + 8 * 24 * 3600 * 1000 = 1072915200000 */
#define MS_FROM_1970_TO_2004_NO_LEAP_SECS 1072915200000

/* Number of leap seconds since 1970 */
#define NBR_LEAP_SECONDS_FROM_1970 27

/* Difference of leap seconds between UTC and ETSI */
#define DIFF_LEAP_SECONDS_UTC_ETSI 5

//GPS data fields from NMEA
char nmea_msg[20]              = "N/A";
char utc[20]                   = "N/A";
char status[20]                = "N/A";
char latitude[20]              = "N/A";
char northsouth[20]            = "N/A";
char longitude[20]             = "N/A";
char eastwest[20]              = "N/A";
char gps_speed[20]             = "N/A";
char gps_heading[20]           = "N/A";
char date[20]                  = "N/A";
char gps_quality_indicator[20] = "N/A";
char satellites_used[20]       = "N/A";
char pdop[20]                  = "N/A";
char hdop[20]                  = "N/A";
char vdop[20]                  = "N/A";
char antenna_altitude[20]      = "N/A";
char satellites_in_view[20]    = "N/A";
char sentence[BufferLength];

void getField(char* buffer, int index)
{
  int sentencePos = 0;
  int fieldPos = 0;
  int commaCount = 0;
  while (sentencePos < BufferLength)
    {
      if (sentence[sentencePos] == ',')
	{
	  commaCount ++;
	  sentencePos ++;
	}
      if (commaCount == index)
	{
	  buffer[fieldPos] = sentence[sentencePos];
	  fieldPos ++;
	}
      sentencePos ++;
    }
  buffer[fieldPos] = '\0';
}


/*------------------------------------------------------------
  -- The main function.
  ------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  size_t len;
  int monitor_socket_fd;
  int command_server_socket_fd;
  int command_com_socket_fd;
  int n;
  socklen_t cli_length;
  struct sockaddr_in monitor_server_addr;
  struct sockaddr_in monitor_from_addr_temp;
  struct sockaddr_in monitor_from_addr;
  struct sockaddr_in command_server_addr;
  struct sockaddr_in cli_addr;
  struct hostent *hp;
  char buffer[RTK_RECV_BUFFER];
  char bMonitorBuffer[29];
  struct timespec sleep_time;
  struct timespec ref_time;
  double lat_start = ORIGIN_LATITUDE;
  double lon_start = ORIGIN_LONGITUDE;
  double alt_start = ORIGIN_ALTITUDE;
  useconds_t loop_delay = 10000;
  char *ret = NULL;
  int result = 0;
  float   time;
  unsigned int safety_port = SAFETY_CHANNEL_PORT;
  unsigned int control_port = CONTROL_CHANNEL_PORT;
  int optval = 1;

  struct timeval tv;
  struct timeval tvTime;
  struct timeval tvTime2;

  int sd, rc, length = sizeof(int);
  struct sockaddr_in serveraddr;
  char character; 
  char server[255];
  char temp;
  int totalcnt = 0;
  int i = 0;

  char etsi_time_string[20];
  TimestampIts etsi_time = 0;
  HeadingValue etsi_heading = 0;
  SpeedValue etsi_speed = 0;
  Latitude etsi_lat = 0;
  Longitude etsi_lon = 0;
  AltitudeValue etsi_alt = 0;

  struct hostent *hostp;


  
  if ( !( (argc == 1) || (argc == 4) || (argc == 6))){
    perror("Please use 0, 3, or 5 arguments: object <lat,lon,alt><safety port,control port>");
    exit(1);
  }
  
  if (argc > 1){
    lat_start = atof(argv[1]);
    lon_start = atof(argv[2]);
    alt_start = atof(argv[3]);
  }
  
#ifdef DEBUG
  printf("INF: Start point set to: lat:%.7lf lon:%.7lf alt:%f\n",lat_start,lon_start,alt_start);
#endif
  
  if (argc > 4){
    safety_port = atoi(argv[4]);
    control_port = atoi(argv[5]);
  }
#ifdef DEBUG
  printf("INF: Ports set to: safety:%d control:%d\n",safety_port,control_port);
#endif
  
#ifdef DEBUG
  printf("INF: Starting object\n");
  fflush(stdout);
#endif


  if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("Client-socket() error");
      exit(-1);
    }
  else
    printf("Client-socket() OK\n");
  /*If the server hostname is supplied*/
  if(argc > 1)
    {
      /*Use the supplied argument*/
      strcpy(server, argv[1]);
      printf("Connecting to %s, port %d ...\n", server, SERVPORT);
    }
  else
    {
      /*Use the default server name or IP*/
      strcpy(server, SERVER);
    }

  memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(SERVPORT);

  if((serveraddr.sin_addr.s_addr = inet_addr(server)) == (unsigned long)INADDR_NONE)
    {
      hostp = gethostbyname(server);

      if(hostp == (struct hostent *)NULL)
	{
	  printf("HOST NOT FOUND --> ");
	  printf("h_errno = %d\n",h_errno);
	  printf("---This is a client program---\n");
	  printf("Command usage: %s <server name or IP>\n", argv[0]);
	  close(sd);
	  exit(-1);
	}
      memcpy(&serveraddr.sin_addr, hostp->h_addr, sizeof(serveraddr.sin_addr));
    }

  if((rc = connect(sd, (struct sockaddr *)&serveraddr, sizeof(serveraddr))) < 0)
    {
      perror("Client-connect() error");
      close(sd);
      exit(-1);
    }
  else
    printf("Connection established...\n");

  /* set socket to non-blocking */
  (void)fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK);


  /* Init monitor socket */
#ifdef DEBUG
  printf("INF: Init monitor socket\n");
  fflush(stdout);
#endif

  monitor_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (monitor_socket_fd < 0)
    {
      perror("ERR: Failed to create monitor socket");
      exit(1);
    }
  bzero(&monitor_server_addr, sizeof(monitor_server_addr));
  monitor_server_addr.sin_family = AF_INET;
  monitor_server_addr.sin_addr.s_addr = INADDR_ANY;
  monitor_server_addr.sin_port = htons(safety_port);

  if (bind(monitor_socket_fd,(struct sockaddr *) &monitor_server_addr, sizeof(monitor_server_addr)) < 0)
    {
      perror("ERR: Failed to bind to monitor socket");
      exit(1);
    }

  socklen_t fromlen = sizeof(struct sockaddr_in);

  /* Init control socket */
#ifdef DEBUG
  printf("INF: Init control socket\n");
  fflush(stdout);
#endif

  command_server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (command_server_socket_fd < 0)
    {
      perror("ERR: Failed to create control socket");
      exit(1);
    }
  bzero((char *) &command_server_addr, sizeof(command_server_addr));

  command_server_addr.sin_family = AF_INET;
  command_server_addr.sin_addr.s_addr = INADDR_ANY; 
  command_server_addr.sin_port = htons(control_port);

  optval = 1;
  result = setsockopt(command_server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

  if (result < 0)
    {
      perror("ERR: Failed to call setsockopt");
      exit(1);
    }

  if (bind(command_server_socket_fd, (struct sockaddr *) &command_server_addr, sizeof(command_server_addr)) < 0) 
    {
      perror("ERR: Failed to bind to control socket");
      exit(1);
    }

  /* Monitor and control sockets up. Wait for central to connect to control socket to get server address*/
#ifdef DEBUG
  printf("INF: Listen on control socket\n");
  fflush(stdout);
#endif

  listen(command_server_socket_fd, 5);
  cli_length = sizeof(cli_addr);

#ifdef DEBUG
  printf("INF: Accept on control socket: %i \n", htons(command_server_addr.sin_port));
  fflush(stdout);
#endif

  command_com_socket_fd = accept(command_server_socket_fd, (struct sockaddr *) &cli_addr, &cli_length);
  if (command_com_socket_fd < 0) 
    {
      perror("ERR: Failed to accept from central");
      exit(1);
    }

  /* set socket to non-blocking */
  (void)fcntl(command_com_socket_fd, F_SETFL, fcntl(command_com_socket_fd, F_GETFL, 0) | O_NONBLOCK);


  /* Recieve address from server */
  bzero(buffer,256);
  rc = recvfrom(monitor_socket_fd, buffer, 255, 0, (struct sockaddr *) &monitor_from_addr, &fromlen);
  if (rc < 0)
    {
      perror("ERR: Failed to receive from monitor socket");
      exit(1);
    }

  /* set socket to non-blocking */
  (void)fcntl(monitor_socket_fd, F_SETFL, fcntl(monitor_socket_fd, F_GETFL, 0) | O_NONBLOCK);

#ifdef DEBUG
  printf("INF: Received: <%s>\n", buffer);
#endif

  char workingBuffer[WORK_BUFFER];
  char tempBuffer[WORK_BUFFER];
  bzero(workingBuffer,WORK_BUFFER);

  int nbrOfBytesLeft = 0;
  int iWorkbufferSize = 0;
  int receivedNewData = 0;
  while(1)
    {
      gettimeofday(&tvTime2, NULL);
      uint64_t uiTime2 = (uint64_t)tvTime2.tv_sec*1000 + (uint64_t)tvTime2.tv_usec/1000 - 
	MS_FROM_1970_TO_2004_NO_LEAP_SECS + 
	DIFF_LEAP_SECONDS_UTC_ETSI*1000;

      /* Receive heartbeat from server */
      bzero(buffer,256);
      rc = recvfrom(monitor_socket_fd, buffer, 255, 0, (struct sockaddr *) &monitor_from_addr_temp, &fromlen);
      if (rc < 0)
	{
	  if(errno != EAGAIN && errno != EWOULDBLOCK)
	    {
	      perror("ERR: Failed to receive from monitor socket");
	      exit(1);
	    }
	  else
	    {
	      printf("INF: No data received\n");
	      fflush(stdout);
	    }
	}
      else
	{
	  /* recieved data, copy port */
	  monitor_from_addr.sin_family = monitor_from_addr_temp.sin_family;
	  monitor_from_addr.sin_addr.s_addr = monitor_from_addr_temp.sin_addr.s_addr;
	  monitor_from_addr.sin_port = monitor_from_addr_temp.sin_port;

	}

      /* Recieve from RTK socket */
      bzero(buffer,RTK_RECV_BUFFER);
      receivedNewData = 0;
      rc = recv(sd, buffer, RTK_RECV_BUFFER-1, 0);
      if (rc < 0)
	{
	  if(errno != EAGAIN && errno != EWOULDBLOCK)
	    {
	      perror("ERR: Failed to receive from command socket");
	      exit(1);
	    }
	  else
	    {
	      printf("INF: No data received\n");
	      fflush(stdout);
	    }
	}
      else
	{
	  receivedNewData = 1;
	}

      if(receivedNewData)
	{

	  (void)strncat(&workingBuffer[nbrOfBytesLeft],buffer,RTK_RECV_BUFFER);
	  iWorkbufferSize = strlen(workingBuffer);

	  /* loop until message has been parsed */
	  int i = 0;
	  while(i < iWorkbufferSize)
	    {
	      int k = 0;
	      /* get next message */
	      while((workingBuffer[i] != '\n') && (i < iWorkbufferSize))
		{
		  sentence[k] = workingBuffer[i];
		  k++;
		  i++;
		}

	      /* Move past newline sign */
	      i++;

	      /* Check if we stopped due to new message */
	      if(workingBuffer[i-1] == '\n')
		{
		  /* Calc how many bytes left */
		  nbrOfBytesLeft = iWorkbufferSize-i;

		  sentence[k] = '\0';

		  getField(nmea_msg, 0);
		  if (strcmp(nmea_msg, "$GPRMC") == 0) 
		    {
		      getField(utc, 1);
		      getField(status, 2);
		      getField(latitude, 3);
		      getField(northsouth, 4);
		      getField(longitude, 5);
		      getField(eastwest, 6);
		      getField(gps_speed, 7);
		      getField(gps_heading, 8);
		      getField(date, 9);
		    }
		  else if (strcmp(nmea_msg, "$GPGGA") == 0) 
		    {
		      getField(utc, 1);
		      getField(latitude, 2);
		      getField(northsouth, 3);
		      getField(longitude, 4);
		      getField(eastwest, 5);
		      getField(gps_quality_indicator, 6);
		      getField(satellites_used, 7);
		      getField(antenna_altitude, 9);
		      printf("%s %s%s %s%s %s %s %s\n",utc,latitude,northsouth,longitude,eastwest,antenna_altitude,gps_quality_indicator,satellites_used);
		      fflush(stdout);
		    }
		  else if (strcmp(nmea_msg, "$GPGSV") == 0) 
		    {
		      getField(satellites_in_view, 3);
		    }
		  else if (strcmp(nmea_msg, "$GPGSA") == 0) 
		    {
		      getField(pdop, 4);
		      getField(hdop, 5);
		      getField(vdop, 6);
		    }
		  /* Convert from NMEA to ETSI format */
		  etsi_lat          = ConvertLatitudeNMEAtoETSICDD(latitude, northsouth, status);
		  etsi_lon          = ConvertLongitudeNMEAtoETSICDD(longitude, eastwest, status);
		  etsi_speed        = ConvertSpeedValueNMEAtoETSICDD(gps_speed, status);
		  etsi_heading      = ConvertHeadingValueNMEAtoETSICDD(gps_heading, status);
		  etsi_alt          = ConvertAltitudeValueNMEAtoETSICDD(antenna_altitude, status);
		  etsi_time         = ConvertTimestapItsNMEAtoETSICDD(utc, date, status);
		  sprintf(etsi_time_string, "%" PRIu64 "", etsi_time);
		}
	    }
    

	  /* Copy bytes to beginning */
	  if(nbrOfBytesLeft != 0)
	    {
	      bzero(tempBuffer,WORK_BUFFER);
	      strncpy(tempBuffer,&workingBuffer[iWorkbufferSize-nbrOfBytesLeft],nbrOfBytesLeft);
	      bzero(workingBuffer,WORK_BUFFER);
	      strncpy(workingBuffer,tempBuffer,nbrOfBytesLeft);
	    }
	  else
	    {
	      bzero(workingBuffer,WORK_BUFFER);
	      iWorkbufferSize = 0;
	    }
	}
      /* Convert to byte-based format */
      bzero(bMonitorBuffer, 256);
      bMonitorBuffer[0] = (uint8_t) ((0x06 >> 0) & 0xFF);
      bMonitorBuffer[1] = (uint8_t) ((0x00 >> 24) & 0xFF);
      bMonitorBuffer[2] = (uint8_t) ((0x00 >> 16) & 0xFF);
      bMonitorBuffer[3] = (uint8_t) ((0x00 >> 8) & 0xFF);
      bMonitorBuffer[4] = (uint8_t) ((0x19 >> 0) & 0xFF);
            
      bMonitorBuffer[5] = (uint8_t) ((uiTime2 >> 40) & 0xFF);
      bMonitorBuffer[6] = (uint8_t) ((uiTime2 >> 32) & 0xFF);
      bMonitorBuffer[7] = (uint8_t) ((uiTime2 >> 24) & 0xFF);
      bMonitorBuffer[8] = (uint8_t) ((uiTime2 >> 16) & 0xFF);
      bMonitorBuffer[9] = (uint8_t) ((uiTime2 >> 8) & 0xFF);
      bMonitorBuffer[10] = (uint8_t) ((uiTime2 >> 0) & 0xFF);

      bMonitorBuffer[11] = (uint8_t) ((etsi_lat >> 24) & 0xFF);
      bMonitorBuffer[12] = (uint8_t) ((etsi_lat >> 16) & 0xFF);
      bMonitorBuffer[13] = (uint8_t) ((etsi_lat >> 8) & 0xFF);
      bMonitorBuffer[14] = (uint8_t) ((etsi_lat >> 0) & 0xFF);

      bMonitorBuffer[15] = (uint8_t) ((etsi_lon >> 24) & 0xFF);
      bMonitorBuffer[16] = (uint8_t) ((etsi_lon >> 16) & 0xFF);
      bMonitorBuffer[17] = (uint8_t) ((etsi_lon >> 8) & 0xFF);
      bMonitorBuffer[18] = (uint8_t) ((etsi_lon >> 0) & 0xFF);

      bMonitorBuffer[19] = (uint8_t) ((etsi_alt >> 24) & 0xFF);
      bMonitorBuffer[20] = (uint8_t) ((etsi_alt >> 16) & 0xFF);
      bMonitorBuffer[21] = (uint8_t) ((etsi_alt >> 8) & 0xFF);
      bMonitorBuffer[22] = (uint8_t) ((etsi_alt >> 0) & 0xFF);

      bMonitorBuffer[23] = (uint8_t) ((etsi_speed >> 8) & 0xFF);
      bMonitorBuffer[24] = (uint8_t) ((etsi_speed >> 0) & 0xFF);

      bMonitorBuffer[25] = (uint8_t) ((etsi_heading >> 8) & 0xFF);
      bMonitorBuffer[26] = (uint8_t) ((etsi_heading >> 0) & 0xFF);
      
      bMonitorBuffer[27] = (uint8_t) ((1 >> 0) & 0xFF);

      bMonitorBuffer[28] = (uint8_t) ((atoi(gps_quality_indicator) >> 0) & 0xFF);
      
      n = sendto(monitor_socket_fd, bMonitorBuffer, sizeof(bMonitorBuffer), 0, (struct sockaddr *) &monitor_from_addr, fromlen);

#ifdef DEBUG
      printf("INF: Sending: <%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x>\n", (uint8_t) bMonitorBuffer[0], (uint8_t) bMonitorBuffer[1], (uint8_t) bMonitorBuffer[2], (uint8_t) bMonitorBuffer[3], (uint8_t) bMonitorBuffer[4], (uint8_t) bMonitorBuffer[5], (uint8_t) bMonitorBuffer[6], (uint8_t) bMonitorBuffer[7],(uint8_t) bMonitorBuffer[8], (uint8_t) bMonitorBuffer[9], (uint8_t) bMonitorBuffer[10], (uint8_t) bMonitorBuffer[11], (uint8_t) bMonitorBuffer[12], (uint8_t) bMonitorBuffer[13], (uint8_t) bMonitorBuffer[14], (uint8_t) bMonitorBuffer[15], (uint8_t) bMonitorBuffer[16], (uint8_t) bMonitorBuffer[17], (uint8_t) bMonitorBuffer[18], (uint8_t) bMonitorBuffer[19], (uint8_t) bMonitorBuffer[20], (uint8_t) bMonitorBuffer[21], (uint8_t) bMonitorBuffer[22], (uint8_t) bMonitorBuffer[23], (uint8_t) bMonitorBuffer[24], (uint8_t) bMonitorBuffer[25], (uint8_t) bMonitorBuffer[26], (uint8_t) bMonitorBuffer[27], (uint8_t) bMonitorBuffer[28]);
#endif

      if (n < 0)
	{
	  perror("ERR: Failed to send monitor message");
	  exit(1);
	}

      usleep(loop_delay);
    }

  close(monitor_socket_fd);
  close(command_com_socket_fd);
  close(sd);

  return 0;
}
