/*------------------------------------------------------------------------------
  -- Copyright   : (C) 2016 CHRONOS project
  ------------------------------------------------------------------------------
  -- File        : supervision.c
  -- Author      : Karl-Johan Ode
  -- Description : CHRONOS
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/


#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "supervision.h"

/*------------------------------------------------------------
  -- Defines
  ------------------------------------------------------------*/

#define LOCALHOST "127.0.0.1"

#define LDM_SIZE            5
#define RECV_MESSAGE_BUFFER 1024
#define SAFETY_MARGIN       0.5
#define DEBUG 1



/*------------------------------------------------------------
  -- Function declarations & definitions.
  ------------------------------------------------------------*/

/* This code is a straight copy from objectcontrol.c */

static void vFindObjectsInfo ( char object_traj_file    [MAX_OBJECTS][MAX_FILE_PATH],
                               char object_address_name [MAX_OBJECTS][MAX_FILE_PATH],
                               int* nbr_objects )
{
  DIR*           traj_directory;
  struct dirent* directory_entry;
  int            iForceObjectToLocalhost;

  iForceObjectToLocalhost = 0;

  traj_directory = opendir(TRAJECTORY_PATH);
  if(traj_directory == NULL)
  {
    util_error("ERR: Failed to open trajectory directory");
  }

  (void)iUtilGetIntParaConfFile("ForceObjectToLocalhost",
                                &iForceObjectToLocalhost);

  while ((directory_entry = readdir(traj_directory)) &&
         ((*nbr_objects) < MAX_OBJECTS))
  {
    /* Check so it's not . or .. */
    if (strncmp(directory_entry->d_name,".",1))
    {
      bzero(object_address_name[(*nbr_objects)],MAX_FILE_PATH);

      bzero(object_traj_file[(*nbr_objects)],MAX_FILE_PATH);
      (void)strcat(object_traj_file[(*nbr_objects)],TRAJECTORY_PATH);
      (void)strcat(object_traj_file[(*nbr_objects)],directory_entry->d_name);

      if(0 == iForceObjectToLocalhost)
      {
        (void)strncat(object_address_name[(*nbr_objects)],
                      directory_entry->d_name,
                      strlen(directory_entry->d_name));
      }
      else
      {
        (void)strcat(object_address_name[(*nbr_objects)],LOCALHOST);
      }

#ifdef DEBUG
      printf ( "DBG: SV : otf = %s , oan = %s \n",
               object_traj_file    [(*nbr_objects)],
               object_address_name [(*nbr_objects)] );
        ;
      fflush(stdout);
#endif

      ++(*nbr_objects);
    }
  }
  (void)closedir(traj_directory);
}


/*------------------------------------------------------------
-- Private variables
------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Public functions
  ------------------------------------------------------------*/

void supervision_task()
{
  char           object_traj_file    [ MAX_OBJECTS ][ MAX_FILE_PATH ];
  char           object_address_name [ MAX_OBJECTS ][ MAX_FILE_PATH ];
  int            nbr_objects = 0 ;

  FILE*          fp          [ MAX_OBJECTS ]              ;
  char           bFileLine   [ MAX_OBJECTS ] [3] [100]    ;
  char*          bFileLine_p [ MAX_OBJECTS ]              ;
  size_t         len                                      ;
  int            read                                     ;
  int            i                                        ;
  monitor_t      traj                                     ;
  monitor_t      trajs       [ MAX_OBJECTS ] [3]          ;
  ObjectPosition deviation   [ MAX_OBJECTS ] [3]          ;
  double         distance    [ MAX_OBJECTS ] [3]          ;
  int            bestFit     [ MAX_OBJECTS ] [3]          ;
  int            bestFitDone [ MAX_OBJECTS ]              ;

  float     time       ;
  double    x          ;
  double    y          ;
  double    z          ;
  float     hdg        ;
  float     vel        ;

  uint16_t  iIndex = 0     ;

  uint64_t  timestamp      ;
  int32_t   latitude       ;
  int32_t   longitude      ;
  int32_t   altitude       ;
  uint16_t  speed          ;
  uint16_t  heading        ;
  uint8_t   drivedirection ;
  monitor_t monitor        ;

  monitor_t ldm          [ MAX_OBJECTS ] [ LDM_SIZE ] ;
  int       ldm_act_step [ MAX_OBJECTS ]              ;
  char      cpBuffer     [ RECV_MESSAGE_BUFFER ]      ;


  struct timespec sleep_time ;
  struct timespec ref_time   ;

  ObjectPosition tObjectPos;
  double         dP1Lat  = 0;
  double         dP1Long = 0;
  double         dP2Lat  = 0;
  double         dP2Long = 0;

  char           pcTempBuffer [512];

  double         dRes = 0.0;


  printf ("--------------------------------------------------\n");
  printf ("INF: SV Supervision started.\n");
  printf ("--------------------------------------------------\n");
  fflush(stdout);


  /*----------------------------------------------------------------------
  -- Init
  ----------------------------------------------------------------------*/

  tObjectPos.x = 0;
  tObjectPos.y = 0;

  for ( iIndex = 0           ;
        iIndex < MAX_OBJECTS ;
        ++iIndex             )
    {
      ldm_act_step [ iIndex ] = 0;

      for ( i = 0;
            i < 3;
            i++ ) {

        deviation    [ iIndex ] [i] = tObjectPos ;
        distance     [ iIndex ] [i] = 0          ;
        bestFit      [ iIndex ] [i] = -1         ;
        bestFitDone  [ iIndex ]      = 0         ;
      }
    }


  /*----------------------------------------------------------------------
  -- Open a drive file
  ----------------------------------------------------------------------*/

  /* Get objects; name and drive file */
  vFindObjectsInfo ( object_traj_file,
                     object_address_name,
                     &nbr_objects);

  printf ( "DGB : SV : dir nr of files = %d\n", nbr_objects );

  for ( iIndex = 0           ;
        iIndex < nbr_objects ;
        ++iIndex )
    {

      printf ( "INF: SV : Open index = %d file %s \n",
               iIndex,
               object_traj_file [ iIndex ] );

      fp [ iIndex ]  = fopen ( object_traj_file [ iIndex ], "rb");
      if ( fp [ iIndex ] == NULL)
        {
          util_error("ERR: Failed to open trajectory file");
        }


      bFileLine_p [ iIndex ] = bFileLine [ iIndex ] [0];

      len = sizeof(bFileLine [ iIndex ] [0]);
      bzero ( &bFileLine [ iIndex ] [0], len );
      read = getline ( &bFileLine_p [ iIndex ], &len, fp [ iIndex ]);
      printf ( "INF: SV line n = %ld \n -- s = %s",
               (ssize_t)len,
               bFileLine [ iIndex ] [0] );

      for ( i = 0;
            i < 3;
            i++ ) {

        bFileLine_p [ iIndex ] = bFileLine [ iIndex ] [i];
        len = sizeof(bFileLine [ iIndex ] [i]);
        bzero ( &bFileLine [ iIndex ] [i], len);
        read = getline ( &bFileLine_p [ iIndex ] , &len, fp [ iIndex ] );
        printf ( "INF: SV i = %d , line n = %ld \n -- s = %s",
                 i,
                 (ssize_t) len,
                 bFileLine [ iIndex ] [i]);
      }
    }


  printf ( "INF: SV : Opened all trajectory files.\n" );


  for ( iIndex = 0           ;
        iIndex < nbr_objects ;
        ++iIndex ) {

    for ( i = 0;
          i < 3;
          i++ ) {

      sscanf ( &bFileLine [ iIndex ] [i] [5],
               "%f;%lf;%lf;%lf;%f;%f;",
               &time ,
               &x    ,
               &y    ,
               &z    ,
               &hdg  ,
               &vel  );

      traj2ldm ( time  ,
                 x     ,
                 y     ,
                 z     ,
                 hdg   ,
                 vel   ,
                 &traj );

      trajs [ iIndex ] [ i ].timestamp  = traj.timestamp ;
      trajs [ iIndex ] [ i ].latitude   = traj.latitude  ;
      trajs [ iIndex ] [ i ].longitude  = traj.longitude ;

      printf ( "DBG: SV %d %d : t %" PRIu64 ", lat %d, lon %d \n",
               iIndex ,
               i      ,
               trajs [ iIndex ] [ i ].timestamp ,
               trajs [ iIndex ] [ i ].latitude  ,
               trajs [ iIndex ] [ i ].longitude );
    }
  }


  /*----------------------------------------------------------------------
  -- Listen loop.
  ----------------------------------------------------------------------*/

  (void) iCommInit ( IPC_RECV ,
                     MQ_SV    ,
                     0        );

  /* Start sending and receiving HEAB, MONR and visualization */
  int iExit                  = 0 ;
  int iCommand                   ;
  int doInitialAlignmentLoop = 1 ;

  while(!iExit)
  {
    bzero(cpBuffer, RECV_MESSAGE_BUFFER);
    (void) iCommRecv ( &iCommand,
                       cpBuffer,
                       RECV_MESSAGE_BUFFER);

#ifdef DEBUG
    printf("INF: SV received a command: %s\n", cpBuffer);
    fflush(stdout);
#endif

    if ( iCommand == COMM_MONI )
    {
#ifdef DEBUG
      printf("INF: Recieved MONITOR message: %s\n", cpBuffer);
      fflush(stdout);
#endif

      /* Start parsing messages */
      sscanf (cpBuffer,
              "%" SCNu16 ";0;%" SCNu64 ";%" SCNd32 ";%" SCNd32 ";%" SCNd32 ";%" SCNu16 ";%" SCNu16 ";%" SCNu8 ";",
              &iIndex,
              &timestamp,
              &latitude,
              &longitude,
              &altitude,
              &speed,
              &heading,
              &drivedirection );

       monitor.timestamp      = timestamp      ;
       monitor.latitude       = latitude       ;
       monitor.longitude      = longitude      ;
       monitor.altitude       = altitude       ;
       monitor.speed          = speed          ;
       monitor.heading        = heading        ;
       monitor.drivedirection = drivedirection ;

       printf ( "DBG: SV i %d : t %" PRIu64 ", lat %d, lon %d, a %d, s %d, h %d, d %d \n",
                iIndex                 ,
                monitor.timestamp      ,
                monitor.latitude       ,
                monitor.longitude      ,
                monitor.altitude       ,
                monitor.speed          ,
                monitor.heading        ,
                monitor.drivedirection );
       fflush(stdout);

       if ( doInitialAlignmentLoop == 1) {

         if ( bestFitDone [ iIndex ] == 0 ) {

           for ( i = 0 ;
                 i < 3 ;
                 i++   ) {

             dP1Lat  = (double) trajs [ iIndex ] [ i ].latitude  / 10000000;
             dP1Long = (double) trajs [ iIndex ] [ i ].longitude / 10000000;

             dP2Lat  = (double) latitude  / 10000000;
             dP2Long = (double) longitude / 10000000;

             double         dLat  = dP1Lat  - dP2Lat;
             double         dLong = dP1Long - dP2Long;

             printf ( "DGB : SV : iIndex = %d, dP1Lat = %f, dP1Long = %f,  dP2Lat = %f, dP2Long = %f, dLat = %f, dLong = %f\n",
                      iIndex  ,
                      dP1Lat  ,
                      dP1Long ,
                      dP2Lat  ,
                      dP2Long ,
                      dLat    ,
                      dLong   );

             tObjectPos.x = 0;
             tObjectPos.y = 0;

             UtilCalcPositionDelta ( dP1Lat  ,
                                     dP1Long ,
                                     dP2Lat  ,
                                     dP2Long ,
                                     &tObjectPos );

             dRes = sqrt ( tObjectPos.x * tObjectPos.x +
                           tObjectPos.y * tObjectPos.y );

             deviation [ iIndex ] [ i ] = tObjectPos ;
             distance  [ iIndex ] [ i ] = dRes       ;

             printf ( "DGB : SV : iIndex = %d, i = %d, x = %f, y = %f, dRes %f \n",
                      iIndex       ,
                      i            ,
                      tObjectPos.x ,
                      tObjectPos.y ,
                      dRes
                      );
           }
           bestFitDone [ iIndex ] = 1;
         }
       } else {

         ldm [ iIndex ][ ldm_act_step [ iIndex ] ] = monitor;

#ifdef DEBUG
         printf ( "DBG: SV i %d, s %d: t %" PRIu64 ", lat %d, lon %d, a %d, s %d, h %d, d %d \n",
                  iIndex                                                    ,
                  ldm_act_step [ iIndex ]                                   ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].timestamp      ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].latitude       ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].longitude      ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].altitude       ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].speed          ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].heading        ,
                  ldm [ iIndex ] [ ldm_act_step [ iIndex ] ].drivedirection );
         fflush(stdout);
#endif
         ldm_act_step[iIndex] = ++ldm_act_step[iIndex] % LDM_SIZE;

         /* Check if passing line */

         bzero(pcTempBuffer,512);
         (void) iUtilGetParaConfFile("OrigoLatidude=", pcTempBuffer);
         sscanf(pcTempBuffer, "%lf", &dP1Lat);

         bzero(pcTempBuffer,512);
         (void) iUtilGetParaConfFile("OrigoLongitude=", pcTempBuffer);
         sscanf(pcTempBuffer, "%lf", &dP1Long);

         dP2Lat  = (double) latitude  / 10000000;
         dP2Long = (double) longitude / 10000000;

         UtilCalcPositionDelta ( dP1Lat  ,
                                 dP1Long ,
                                 dP2Lat  ,
                                 dP2Long ,
                              &tObjectPos );

         //double r = 6378137.0;
         //double latmid=(dP1Lat+dP2Lat)/2;
         //tObjectPos.x =(dP2Long-dP1Long)*(M_PI/180)*r*cos(latmid*M_PI/180);
         //tObjectPos.y =(dP2Lat-dP1Lat)*(M_PI/180)*r;

#ifdef DEBUG
         printf("INF: Object position latitude: %lf longitude: %lf \n",dP2Lat, dP2Long);
         printf("INF: Origo position latitude: %lf longitude: %lf \n",dP1Lat, dP1Long);
         printf("INF: Calculated value x: %lf y: %lf \n",tObjectPos.x, tObjectPos.y);
         fflush(stdout);
#endif

      //double dRes = (tObjectPos.x-41.2)*(26.1-55.2)-(tObjectPos.y-55.2)*(65.2-41.2);


         dRes = sqrt((x-tObjectPos.x)*(x-tObjectPos.x)+(y-tObjectPos.y)*(y-tObjectPos.y));

         dRes = 0.0;
         if(dRes > SAFETY_MARGIN)
           {
             printf("INF: Sending ABORT from supervisor\n");
             fflush(stdout);
          //              (void)iCommSend(COMM_ABORT,NULL);
           }
       } // if ( doInitialAlignmentLoop == 1) .. else
    }
    else if (iCommand == COMM_REPLAY)
      {
        printf("INF: Supervision received REPLAY message: %s\n",cpBuffer);
      }
    else if( iCommand == COMM_EXIT)
      {
        iExit = 1;
      }
    else
      {
#ifdef DEBUG
        printf("INF: Unhandled command in supervision\n");
        fflush(stdout);
#endif
      }
  } // while(!iExit)


  /*--------------------------------------------------
    --  Clean upp.
    --------------------------------------------------*/

  (void) iCommClose();

  for ( iIndex = 0           ;
        iIndex < nbr_objects ;
        ++iIndex )
    {
      fclose ( fp [ iIndex ] );
    }

}
