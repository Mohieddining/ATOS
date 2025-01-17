/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
/*------------------------------------------------------------------------------
  -- File        : util.c
  -- Author      : Sebastian Loh Lindholm
  -- Description : CHRONOS
  -- Purpose     :
  -- Reference   :
  ------------------------------------------------------------------------------*/

/*------------------------------------------------------------
  -- Include files.
  ------------------------------------------------------------*/

#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include <float.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>


#include "util.h"
#include "atosTime.h"


/*------------------------------------------------------------
  -- Defines
  ------------------------------------------------------------*/

#define EARTH_EQUATOR_RADIUS_M	6378137.0	// earth semimajor axis (WGS84) (m)
#define INVERSE_FLATTENING	298.257223563	//in WGS84, f = 1/298.257223563
#define EARTH_FLATTENING ( 1.0 / INVERSE_FLATTENING )
#define EARTH_POLE_RADIUS_M	6356752.3142451794975639665996337	//b = (1-f)*a
#define VINCENTY_MIN_STEP_TOLERANCE	1e-12

#define SMALL_BUFFER_SIZE_128 128
#define SMALL_BUFFER_SIZE_64 64

// File paths
#define JOURNAL_DIR_NAME "journal"
#define ATOS_TEST_DIR_NAME ".astazero/ATOS"
#define CONFIGURATION_DIR_NAME "conf"
#define TRAJECTORY_DIR_NAME "traj"
#define GEOFENCE_DIR_NAME "geofence"
#define OBJECT_DIR_NAME "objects"
#define OPENDRIVE_DIR_NAME "odr"
#define OPENSCENARIO_DIR_NAME "osc"
#define POINTCLOUD_DIR_NAME "pointclouds"

/* Message priorities on message queue */
// Abort message
#define PRIO_COMM_ABORT 31
// Object control state report
#define PRIO_COMM_OBC_STATE 26
// Internal configuration
#define PRIO_COMM_DATA_DICT 24
// Configuration affecting other configurations
#define PRIO_COMM_OSEM 22
// Configuration messages
#define PRIO_COMM_ACCM 20
#define PRIO_COMM_TRCM 20
// Messages affecting state change messages
#define PRIO_OBJECTS_CONNECTED 18
#define PRIO_COMM_FAILURE 18
// State change messages
#define PRIO_COMM_STRT 16
#define PRIO_COMM_ARM 16
#define PRIO_COMM_DISARM 16
#define PRIO_COMM_STOP 16
#define PRIO_COMM_REPLAY 16
#define PRIO_COMM_INIT 16
#define PRIO_COMM_CONNECT 16
#define PRIO_COMM_DISCONNECT 16
#define PRIO_COMM_REMOTECTRL_ENABLE 16
#define PRIO_COMM_REMOTECTRL_DISABLE 16
#define PRIO_COMM_ENABLE_OBJECT 16
#define PRIO_COMM_ABORTING_DONE 16
// Single-shot messages relevant during test run
#define PRIO_COMM_EXAC 14
#define PRIO_COMM_TREO 14
// Frequent messages relevant during test run
#define PRIO_COMM_TRAJ_TOSUP 12
#define PRIO_COMM_TRAJ_FROMSUP 12
#define PRIO_COMM_GETSTATUS 10
#define PRIO_COMM_REMOTECTRL_MANOEUVRE 12
#define PRIO_COMM_BACKTOSTART 12
#define PRIO_COMM_GETSTATUS_OK 255

// Unused messages TODO: double check the priority of unused messages
#define PRIO_COMM_VIOP 5
#define PRIO_COMM_TRAJ 5
#define PRIO_COMM_ASP 5
// Server exit message
#define PRIO_COMM_EXIT 3

// Configuration parameter strings
static const char ParameterNameScenarioName[] = "ScenarioName";
static const char ParameterNameOriginLongitude[] = "OriginLongitude";
static const char ParameterNameOriginLatitude[] = "OriginLatitude";
static const char ParameterNameOriginAltitude[] = "OriginAltitude";
static const char ParameterNameVisualizationServerName[] = "VisualizationServerName";
static const char ParameterNameASPMaxTimeDiff[] = "ASPMaxTimeDiff";
static const char ParameterNameASPMaxTrajDiff[] = "ASPMaxTrajDiff";
static const char ParameterNameASPStepBackCount[] = "ASPStepBackCount";
static const char ParameterNameASPFilterLevel[] = "ASPFilterLevel";
static const char ParameterNameASPMaxDeltaTime[] = "ASPMaxDeltaTime";
static const char ParameterNameTimeServerIP[] = "TimeServerIP";
static const char ParameterNameTimeServerPort[] = "TimeServerPort";
static const char ParameterNameSimulatorIP[] = "SimulatorIP";
static const char ParameterNameSimulatorPortTCP[] = "SimulatorTCPPort";
static const char ParameterNameSimulatorPortUDP[] = "SimulatorUDPPort";
static const char ParameterNameSimulatorMode[] = "SimulatorMode";
static const char ParameterNameVOILReceivers[] = "VOILReceivers";
static const char ParameterNameDTMReceivers[] = "DTMReceivers";
static const char ParameterNameExternalSupervisorIP[] = "SupervisorIP";
static const char ParameterNameExternalSupervisorPortTCP[] = "SupervisorTCPPort";
static const char ParameterNameRVSSConfig[] = "RVSSConfig";
static const char ParameterNameRVSSRate[] = "RVSSRate";
static const char ParameterNameMaxPacketsLost[] = "MaxPacketsLost";
static const char ParameterNameTransmitterID[] = "TransmitterID";
static const char ParameterNameMiscData[] = "MiscData";

static const char ObjectSettingNameID[] = "ID";
static const char ObjectSettingNameIP[] = "IP";
static const char ObjectSettingNameTraj[] = "traj";
static const char ObjectSettingNameOpendrive[] = "odr";
static const char ObjectSettingNameOpenscenario[] = "osc";
static const char ObjectSettingNameIsAnchor[] = "isAnchor";
static const char ObjectSettingNameInjectorIDs[] = "injectorIDs";
static const char ObjectSettingNameLatitude[] = "originLatitude";
static const char ObjectSettingNameLongitude[] = "originLongitude";
static const char ObjectSettingNameAltitude[] = "originAltitude";
static const char ObjectSettingNameIsOsiCompatible[] = "isOsiCompatible";
static const char ObjectSettingNameTurningDiameter[] = "turningDiameter";
static const char ObjectSettingNameMaxSpeed[] = "maxSpeed";


/*------------------------------------------------------------
-- Local type definitions
------------------------------------------------------------*/
enum procFields {startTime = 21, vSize = 22};

/*------------------------------------------------------------
-- Private variables
------------------------------------------------------------*/

/*---------------------------------------------s---------------
  -- Private functions
  ------------------------------------------------------------*/
static void CopyHTTPHeaderField(char *request, char *targetContainer, size_t targetContainerSize,
								const char *fieldName);
static char rayFromPointIntersectsLine(double pointX, double pointY, double polyPointAX, double polyPointAY,
									   double polyPointBX, double polyPointBY);
static int deleteDirectoryContents(char *path, size_t pathLen);
static int deleteFile(char *path, size_t pathLen);

void CopyHTTPHeaderField(char *request, char *targetContainer, size_t targetContainerSize,
						 const char *fieldName) {
	char *firstPos, *lastPos;
	char *matchPos = strstr(request, fieldName);
	unsigned long fieldLength = 0;

	if (matchPos == NULL) {
		targetContainer[0] = '\0';
		return;
	}

	matchPos += strlen(fieldName);
	if (matchPos[0] == ':' || matchPos[0] == ';')
		matchPos++;
	else {
		targetContainer[0] = '\0';
		return;
	}

	while (1) {
		if (matchPos[0] == ' ' || matchPos[0] == '\t') {
			// Skip white space
			matchPos++;
		}
		else if (matchPos[0] == '\r' || matchPos[0] == '\n' || matchPos[0] == '\0') {
			// Empty field? Return empty string
			targetContainer[0] = '\0';
			return;
		}
		else {
			break;
		}
	}

	firstPos = matchPos;

	// Find end of field
	while (1) {
		if (matchPos[0] == '\r' || matchPos[0] == '\n' || matchPos[0] == '\0') {
			lastPos = matchPos;
			break;
		}
		else {
			matchPos++;
		}
	}

	// Check length
	fieldLength = lastPos - firstPos;
	if (fieldLength >= targetContainerSize) {
		fprintf(stderr, "Received too long HTTP header field: %s\n", fieldName);
		targetContainer[0] = '\0';
		return;
	}
	else {
		// Strings in the request may not be null terminated: make them so after copying them
		strncpy(targetContainer, firstPos, fieldLength);
		targetContainer[fieldLength] = '\0';
	}

}

/*! \brief Makes a directory and all parent directories if they do not exist.
 * \param dir The path to the directory to be created.
 * \param mode The permissions to be set on the directory.
 * \return 0 if it could successfully create the directory, non-zero if it could not.
*/
static int recursiveMkdir(const char *dir, int mode) {
	char tmp[256];
	char *p = NULL;
	size_t len;
	int res = 0;

	snprintf(tmp, sizeof(tmp),"%s",dir);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			res = mkdir(tmp, mode);
			*p = '/';
		}
	res = mkdir(tmp, mode);
	}

/*!
 * \brief deleteFile Deletes the file given in the parameter ::path
 * \param path The path to the file on the machine.
 * \param pathLen The length of ::the path string.
 * \return  0 if it could successfully delete file, non-zero if it could not.
 */
int deleteFile(char *path, size_t pathLen) {
	if (path == NULL) {
		fprintf(stderr, "Path is null-pointer\n");
		errno = EINVAL;
		return -1;
	}
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too large to handle\n");
		errno = EINVAL;
		return -1;
	}

	FILE *fd = fopen(path, "a");

	if (fd == NULL) {
		fprintf(stderr, "Path <%s> could not be opened\n", path);
		return -1;
	}
	fclose(fd);

	if (remove(path) != 0) {
		fprintf(stderr, "Path <%s> could not be deleted\n", path);
		return -1;
	}
	return 0;
}

/*!
 * \brief deleteDirectoryContents Deletes the directory given in the parameter ::path
 * \param path The path to the directory on the machine.
 * \param pathLen The length of ::the path string.
 * \return  0 if it could successfully delete file, non-zero if it could not.
 */
int deleteDirectoryContents(char *path, size_t pathLen) {
	if (path == NULL) {
		fprintf(stderr, "Path is null-pointer.\n");
		errno = EINVAL;
		return -1;
	}
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too large to handle.\n");
		errno = EINVAL;
		return -1;
	}
	// These are data types defined in the "dirent" header
	DIR *theFolder = opendir(path);

	if (theFolder == NULL) {
		fprintf(stderr, "Path: %s could not be opened.\n", path);
		errno = ENOENT;
		return -1;
	}
	struct dirent *next_file;
	char filepath[MAX_FILE_PATH];

	while ((next_file = readdir(theFolder)) != NULL) {
		// build the path for each file in the folder
		sprintf(filepath, "%s/%s", path, next_file->d_name);
		remove(filepath);
	}
	closedir(theFolder);
	return 0;
}

/*---------------------------------------------s---------------
  -- Public functions
  ------------------------------------------------------------*/

static const HTTPHeaderContent headerFieldNames = {
	"Accept-Charset", "Accept-Encoding", "Accept-Language",
	"Authorization", "Expect", "From", "Host", "If-Match", "If-Modified-Since",
	"If-None-Match", "If-Range", "If-Unmodified-Since", "Max-Forwards",
	"Proxy-Authorization", "Range", "Referer", "TE", "User-Agent"
};

// HTTP header decoder
void UtilDecodeHTTPRequestHeader(char *request, HTTPHeaderContent * header) {
	CopyHTTPHeaderField(request, header->AcceptCharset, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.AcceptCharset);
	CopyHTTPHeaderField(request, header->AcceptEncoding, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.AcceptEncoding);
	CopyHTTPHeaderField(request, header->AcceptLanguage, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.AcceptLanguage);
	CopyHTTPHeaderField(request, header->Authorization, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.Authorization);
	CopyHTTPHeaderField(request, header->Expect, HTTP_HEADER_MAX_LENGTH, headerFieldNames.Expect);
	CopyHTTPHeaderField(request, header->From, HTTP_HEADER_MAX_LENGTH, headerFieldNames.From);
	CopyHTTPHeaderField(request, header->Host, HTTP_HEADER_MAX_LENGTH, headerFieldNames.Host);
	CopyHTTPHeaderField(request, header->IfMatch, HTTP_HEADER_MAX_LENGTH, headerFieldNames.IfMatch);
	CopyHTTPHeaderField(request, header->IfModifiedSince, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.IfModifiedSince);
	CopyHTTPHeaderField(request, header->IfRange, HTTP_HEADER_MAX_LENGTH, headerFieldNames.IfRange);
	CopyHTTPHeaderField(request, header->IfNoneMatch, HTTP_HEADER_MAX_LENGTH, headerFieldNames.IfNoneMatch);
	CopyHTTPHeaderField(request, header->IfUnmodifiedSince, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.IfUnmodifiedSince);
	CopyHTTPHeaderField(request, header->MaxForwards, HTTP_HEADER_MAX_LENGTH, headerFieldNames.MaxForwards);
	CopyHTTPHeaderField(request, header->ProxyAuthorization, HTTP_HEADER_MAX_LENGTH,
						headerFieldNames.ProxyAuthorization);
	CopyHTTPHeaderField(request, header->Range, HTTP_HEADER_MAX_LENGTH, headerFieldNames.Range);
	CopyHTTPHeaderField(request, header->Referer, HTTP_HEADER_MAX_LENGTH, headerFieldNames.Referer);
	CopyHTTPHeaderField(request, header->TE, HTTP_HEADER_MAX_LENGTH, headerFieldNames.TE);
	CopyHTTPHeaderField(request, header->UserAgent, HTTP_HEADER_MAX_LENGTH, headerFieldNames.UserAgent);
}

// GPS TIME FUNCTIONS
uint64_t UtilgetGPSmsFromUTCms(uint64_t UTCms) {
	return UTCms - MS_TIME_DIFF_UTC_GPS + MS_LEAP_SEC_DIFF_UTC_GPS;
}
uint64_t UtilgetUTCmsFromGPSms(uint64_t GPSms) {
	return GPSms + MS_TIME_DIFF_UTC_GPS - MS_LEAP_SEC_DIFF_UTC_GPS;
}

uint64_t UtilgetMSfromGPStime(uint16_t GPSweek, uint32_t GPSquarterMSofWeek) {
	return (uint64_t) GPSweek *WEEK_TIME_MS + (uint64_t) (GPSquarterMSofWeek >> 2);
}

void UtilgetGPStimeFromMS(uint64_t GPSms, uint16_t * GPSweek, uint32_t * GPSquarterMSofWeek) {
	uint16_t tempGPSweek = (uint16_t) (GPSms / WEEK_TIME_MS);

	if (GPSweek)
		*GPSweek = tempGPSweek;
	uint64_t remainder = GPSms - (uint64_t) tempGPSweek * WEEK_TIME_MS;

	if (GPSquarterMSofWeek)
		*GPSquarterMSofWeek = (uint32_t) (remainder << 2);
/*
    uint16_t GPSday = remainder / DAY_TIME_MS;
    remainder -= (uint64_t)GPSday * DAY_TIME_MS;
    uint16_t GPShour = remainder / HOUR_TIME_MS;
    remainder -= (uint64_t)GPShour * HOUR_TIME_MS;
    uint16_t GPSminute = remainder / MINUTE_TIME_MS;
    remainder -= (uint64_t)GPSminute * HOUR_TIME_MS;

    qDebug() << "GPSWEEK:" << GPSweek <<
                "\nGPSSec" << GPSquarterMSofWeek;
    qDebug() << "GPSTIME: " << GPSweek <<
                ":" << GPSday <<
                ":" << GPShour <<
                ":" << GPSminute;

    qDebug() << "GPS WEEK: " << GPSweek <<
                "\nGPS DAY: " << GPSday <<
                "\nGPS HOUR:" << GPShour <<
                "\nGPS MINUTE:" << GPSminute;*/
}

void UtilgetGPStimeFromUTCms(uint64_t UTCms, uint16_t * GPSweek, uint32_t * GPSquarterMSofWeek) {
	UtilgetGPStimeFromMS(UtilgetGPSmsFromUTCms(UTCms), GPSweek, GPSquarterMSofWeek);
}
uint64_t UtilgetUTCmsFromGPStime(uint16_t GPSweek, uint32_t GPSquarterMSofWeek) {
	return UtilgetUTCmsFromGPSms(UtilgetMSfromGPStime(GPSweek, GPSquarterMSofWeek));
}


void UtilgetCurrentGPStime(uint16_t * GPSweek, uint32_t * GPSquarterMSofWeek) {
	UtilgetGPStimeFromUTCms(UtilgetCurrentUTCtimeMS(), GPSweek, GPSquarterMSofWeek);
}

uint64_t UtilgetCurrentUTCtimeMS() {
	struct timeval CurrentTimeStruct;

	gettimeofday(&CurrentTimeStruct, 0);
	return (uint64_t) CurrentTimeStruct.tv_sec * 1000 + (uint64_t) CurrentTimeStruct.tv_usec / 1000;
}

uint32_t UtilgetIntDateFromMS(uint64_t ms) {
	struct tm date_time;
	time_t seconds = (time_t) (ms / 1000);

	localtime_r(&seconds, &date_time);
	return (uint32_t) ((date_time.tm_year + 1900) * 10000 + (date_time.tm_mon + 1) * 100 + date_time.tm_mday);
}
uint64_t UtilgetETSIfromUTCMS(uint64_t utc_sec, uint64_t utc_usec) {
	return utc_sec * 1000 + utc_usec / 1000 - MS_FROM_1970_TO_2004_NO_LEAP_SECS +
		DIFF_LEAP_SECONDS_UTC_ETSI * 1000;

}

void UtilgetDateTimeFromUTCtime(int64_t utc_ms, char *buffer, int size_t) {
	time_t time_seconds = utc_ms / 1000;

	if (size_t < 26)
		return;
	strcpy(buffer, ctime(&time_seconds));
}
void UtilgetDateTimefromUTCCSVformat(int64_t utc_ms, char *buffer, int size_t) {
	struct tm date_time;
	char tmp_buffer_ms[10];
	int64_t ms;
	double tmp_ms;
	time_t time_seconds = (time_t) (utc_ms / 1000);

	localtime_r(&time_seconds, &date_time);
	tmp_ms = (double)(utc_ms) / 1000;
	tmp_ms = tmp_ms - utc_ms / 1000;

	ms = round(tmp_ms * 1000);
	strftime(buffer, size_t, "%Y;%m;%d;%H;%M;%S;", &date_time);

	sprintf(tmp_buffer_ms, "%" PRIi64, ms);
	strcat(buffer, tmp_buffer_ms);
}
void UtilgetDateTimeFromUTCForMapNameCreation(int64_t utc_ms, char *buffer, int size_t) {
	struct tm date_time;
	char tmp_buffer_ms[10];
	int64_t ms;
	double tmp_ms;
	time_t time_seconds = (time_t) (utc_ms / 1000);

	localtime_r(&time_seconds, &date_time);
	tmp_ms = (double)(utc_ms) / 1000;
	tmp_ms = tmp_ms - utc_ms / 1000;

	ms = round(tmp_ms * 1000);
	strftime(buffer, size_t, "%Y-%m-%d_%H:%M:%S:", &date_time);

	sprintf(tmp_buffer_ms, "%" PRIi64, ms);
	strcat(buffer, tmp_buffer_ms);
}
void util_error(const char *message) {
	fprintf(stderr, "%s\n", message);
	exit(EXIT_FAILURE);
}

void xyzToLlh(double x, double y, double z, double *lat, double *lon, double *height) {
	double e2 = EARTH_FLATTENING * (2.0 - EARTH_FLATTENING);
	double r2 = x * x + y * y;
	double za = z;
	double zk = 0.0;
	double sinp = 0.0;
	double v = EARTH_EQUATOR_RADIUS_M;

	while (fabs(za - zk) >= 1E-4) {
		zk = za;
		sinp = za / sqrt(r2 + za * za);
		v = EARTH_EQUATOR_RADIUS_M / sqrt(1.0 - e2 * sinp * sinp);
		za = z + v * e2 * sinp;
	}

	*lat = (r2 > 1E-12 ? atan(za / sqrt(r2)) : (z > 0.0 ? M_PI / 2.0 : -M_PI / 2.0)) * 180.0 / M_PI;
	*lon = (r2 > 1E-12 ? atan2(y, x) : 0.0) * 180.0 / M_PI;
	*height = sqrt(r2 + za * za) - v;
}

void llhToXyz(double lat, double lon, double height, double *x, double *y, double *z) {
	double sinp = sin(lat * M_PI / 180.0);
	double cosp = cos(lat * M_PI / 180.0);
	double sinl = sin(lon * M_PI / 180.0);
	double cosl = cos(lon * M_PI / 180.0);
	double e2 = EARTH_FLATTENING * (2.0 - EARTH_FLATTENING);
	double v = EARTH_EQUATOR_RADIUS_M / sqrt(1.0 - e2 * sinp * sinp);

	*x = (v + height) * cosp * cosl;
	*y = (v + height) * cosp * sinl;
	*z = (v * (1.0 - e2) + height) * sinp;
}

void llhToEnu(const double *iLlh, const double *llh, double *xyz) {
	double ix, iy, iz;

	llhToXyz(iLlh[0], iLlh[1], iLlh[2], &ix, &iy, &iz);

	double x, y, z;

	llhToXyz(llh[0], llh[1], llh[2], &x, &y, &z);

	double enuMat[9];

	createEnuMatrix(iLlh[0], iLlh[1], enuMat);

	double dx = x - ix;
	double dy = y - iy;
	double dz = z - iz;

	xyz[0] = enuMat[0] * dx + enuMat[1] * dy + enuMat[2] * dz;
	xyz[1] = enuMat[3] * dx + enuMat[4] * dy + enuMat[5] * dz;
	xyz[2] = enuMat[6] * dx + enuMat[7] * dy + enuMat[8] * dz;
}


void enuToLlh(const double *iLlh, const double *xyz, double *llh) {
	double ix, iy, iz;

	llhToXyz(iLlh[0], iLlh[1], iLlh[2], &ix, &iy, &iz);

	double enuMat[9];

	createEnuMatrix(iLlh[0], iLlh[1], enuMat);

	double x = enuMat[0] * xyz[0] + enuMat[3] * xyz[1] + enuMat[6] * xyz[2] + ix;
	double y = enuMat[1] * xyz[0] + enuMat[4] * xyz[1] + enuMat[7] * xyz[2] + iy;
	double z = enuMat[2] * xyz[0] + enuMat[5] * xyz[1] + enuMat[8] * xyz[2] + iz;

	xyzToLlh(x, y, z, &llh[0], &llh[1], &llh[2]);
}

void createEnuMatrix(double lat, double lon, double *enuMat) {
	double so = sin(lon * M_PI / 180.0);
	double co = cos(lon * M_PI / 180.0);
	double sa = sin(lat * M_PI / 180.0);
	double ca = cos(lat * M_PI / 180.0);

	// ENU
	enuMat[0] = -so;
	enuMat[1] = co;
	enuMat[2] = 0.0;

	enuMat[3] = -sa * co;
	enuMat[4] = -sa * so;
	enuMat[5] = ca;

	enuMat[6] = ca * co;
	enuMat[7] = ca * so;
	enuMat[8] = sa;
}



int UtilAddEightBytesMessageData(unsigned char *MessageBuffer, int StartIndex, unsigned long Data) {

	int i;

	for (i = 0; i < 8; i++) {
		*(MessageBuffer + StartIndex + i) = (char)(Data >> (7 - i) * 8);
		//printf("[%d]=%x\n", (StartIndex+i), *(MessageBuffer+StartIndex+i));
	}

	return (StartIndex + i);
}

int UtilAddSixBytesMessageData(unsigned char *MessageBuffer, int StartIndex, unsigned long Data) {

	int i;

	for (i = 0; i < 6; i++) {
		*(MessageBuffer + StartIndex + i) = (char)(Data >> (5 - i) * 8);
		//printf("[%d]=%x\n", (StartIndex+i), *(MessageBuffer+StartIndex+i));
	}

	return (StartIndex + i);
}


int UtilAddFourBytesMessageData(unsigned char *MessageBuffer, int StartIndex, unsigned int Data) {
	int i;

	for (i = 0; i < 4; i++) {
		*(MessageBuffer + StartIndex + i) = (unsigned char)(Data >> (3 - i) * 8);
		//printf("[%d]=%x\n", (StartIndex+i), *(MessageBuffer+StartIndex+i));
	}

	return StartIndex + i;
}

int UtilAddTwoBytesMessageData(unsigned char *MessageBuffer, int StartIndex, unsigned short Data) {
	int i;

	for (i = 0; i < 2; i++) {
		*(MessageBuffer + StartIndex + i) = (unsigned char)(Data >> (1 - i) * 8);
		//printf("[%d]=%x\n", (StartIndex+i), *(MessageBuffer+StartIndex+i));
	}

	return StartIndex + i;
}

int UtilAddOneByteMessageData(unsigned char *MessageBuffer, int StartIndex, unsigned char Data) {

	*(MessageBuffer + StartIndex) = Data;
	//printf("[%d]=%x\n", (StartIndex), *(MessageBuffer+StartIndex));
	return StartIndex + 1;
}


int UtilAddNBytesMessageData(unsigned char *MessageBuffer, int StartIndex, int Length, unsigned char *Data) {
	int i;

	for (i = 0; i < Length; i++) {
		*(MessageBuffer + StartIndex + i) = *(Data + i);
		//printf("[%d]=%x\n", (StartIndex+i), *(MessageBuffer+StartIndex+i));
	}

	return StartIndex + i;
}




int UtilSetAdaptiveSyncPoint(AdaptiveSyncPoint * ASP, FILE * filefd, char debug) {

	char DataBuffer[MAX_FILE_PATH];
	char RowBuffer[MAX_FILE_PATH];
	char *ptr, *ptr1;

	bzero(DataBuffer, MAX_FILE_PATH);
	bzero(RowBuffer, MAX_FILE_PATH);

	bzero(RowBuffer, MAX_FILE_PATH);
	UtilReadLineCntSpecChars(filefd, RowBuffer);
	ptr = strchr(RowBuffer, ';');
	strncpy(DataBuffer, RowBuffer, (uint64_t) ptr - (uint64_t) RowBuffer);
	strncpy(ASP->MasterIP, DataBuffer, strlen(DataBuffer));
	//if(USE_TEST_HOST == 1) strncpy(ASP->MasterIP, TESTHOST_IP, sizeof(TESTHOST_IP));

	bzero(DataBuffer, strlen(DataBuffer));
	ptr1 = ptr + 1;
	ptr = strchr(ptr + 2, ';');
	strncpy(DataBuffer, ptr1, (uint64_t) ptr - (uint64_t) ptr1);
	strncpy(ASP->SlaveIP, DataBuffer, strlen(DataBuffer));
	//if(USE_TEST_HOST == 1) strncpy(ASP->SlaveIP, TESTHOST_IP, sizeof(TESTHOST_IP));

	bzero(DataBuffer, strlen(DataBuffer));
	ptr1 = ptr + 1;
	ptr = strchr(ptr + 2, ';');
	strncpy(DataBuffer, ptr1, (uint64_t) ptr - (uint64_t) ptr1);
	ASP->MasterTrajSyncTime = atof(DataBuffer);

	bzero(DataBuffer, strlen(DataBuffer));
	ptr1 = ptr + 1;
	ptr = strchr(ptr + 2, ';');
	strncpy(DataBuffer, ptr1, (uint64_t) ptr - (uint64_t) ptr1);
	ASP->SlaveTrajSyncTime = atof(DataBuffer);

	bzero(DataBuffer, strlen(DataBuffer));
	ptr1 = ptr + 1;
	ptr = strchr(ptr + 2, ';');
	strncpy(DataBuffer, ptr1, (uint64_t) ptr - (uint64_t) ptr1);
	ASP->SlaveSyncStopTime = atof(DataBuffer);

	ASP->TestPort = 0;

	if (debug) {
		fprintf(stderr, "MasterIP: %s\n", ASP->MasterIP);
		fprintf(stderr, "SlaveIP: %s\n", ASP->SlaveIP);
		fprintf(stderr, "MasterTrajSyncTime %3.2f\n", ASP->MasterTrajSyncTime);
		fprintf(stderr, "SlaveTrajSyncTime %3.2f\n", ASP->SlaveTrajSyncTime);
		fprintf(stderr, "SlaveSyncStopTime %3.2f\n", ASP->SlaveSyncStopTime);
	}

	return 0;
}


void UtilSetObjectPositionIP(ObjectPosition * OP, char *IP) {
	strncpy(OP->IP, IP, strlen(IP));
}

int UtilSetMasterObject(ObjectPosition * OP, char *Filename, char debug) {

	FILE *filefd;
	char FilenameBuffer[MAX_FILE_PATH];
	char DataBuffer[20];
	double Time;

	bzero(FilenameBuffer, MAX_FILE_PATH);
	bzero(DataBuffer, 20);
	strcat(FilenameBuffer, Filename);
	strcat(FilenameBuffer, MASTER_FILE_EXTENSION);
	filefd = fopen(FilenameBuffer, "r");

	if (filefd) {
		UtilReadLineCntSpecChars(filefd, DataBuffer);
		Time = atof(DataBuffer);
		OP->Type = 'm';
		OP->SyncTime = Time;
		OP->SyncStopTime = 0;
		fclose(filefd);

		if (debug)
			fprintf(stderr, "Master object set: %s, SyncTime: %3.4f\n", FilenameBuffer, Time);

	}
	else {
		OP->Type = 'u';
		OP->SyncTime = 0;
	}


	return 0;
}


int UtilSetSlaveObject(ObjectPosition * OP, char *Filename, char debug) {
	FILE *filefd;
	char FilenameBuffer[MAX_FILE_PATH];
	char DataBuffer[20];
	double Time;

	bzero(FilenameBuffer, MAX_FILE_PATH);
	bzero(DataBuffer, 20);
	strcat(FilenameBuffer, Filename);
	strcat(FilenameBuffer, SLAVE_FILE_EXTENSION);
	filefd = fopen(FilenameBuffer, "r");

	if (filefd) {
		UtilReadLineCntSpecChars(filefd, DataBuffer);
		Time = atof(DataBuffer);
		OP->Type = 's';
		OP->SyncTime = Time;
		UtilReadLineCntSpecChars(filefd, DataBuffer);
		Time = atof(DataBuffer);
		OP->SyncStopTime = Time;
		fclose(filefd);
		if (debug)
			fprintf(stderr, "Slave object set: %s, SyncTime: %3.4f\n", FilenameBuffer, Time);
	}


	return 0;
}


/*!
 * \brief UtilMonitorDataToString Converts the data from a message queue monitor data struct into ASCII format
 * \param monrData Struct containing relevant monitor data
 * \param monrString String in which converted data is to be placed
 * \param stringLength Length of string in which converted data is to be placed
 * \return 0 upon success, -1 otherwise
 */
int UtilObjectDataToString(const ObjectDataType monitorData, char *monitorDataString, size_t stringLength) {
	memset(monitorDataString, 0, stringLength);
	inet_ntop(AF_INET, &monitorData.ClientIP, monitorDataString,
			  (stringLength > UINT_MAX) ? UINT_MAX : (socklen_t) stringLength);
	strcat(monitorDataString, ";0;");

	if (objectMonitorDataToASCII
		(&monitorData.MonrData, monitorDataString + strlen(monitorDataString),
		 stringLength - strlen(monitorDataString)) < 0) {
		memset(monitorDataString, 0, stringLength);
		return -1;
	}
	return 0;
}

/*!
 * \brief UtilStringToMonitorData Converts the data from an ASCII string into a message queue monitor data struct
 * \param monrString String in which converted data is to be placed
 * \param stringLength Length of string in which converted data is to be placed
 * \param monrData Struct containing relevant monitor data
 * \return 0 upon success, -1 otherwise
 */
int UtilStringToMonitorData(const char *monitorString, size_t stringLength, ObjectDataType * monitorData) {
	const char *token;
	const char delim[] = ";";
	struct in_addr addr;
	char *copy = strdup(monitorString);

	token = strtok(copy, delim);

	// IP address
	inet_pton(AF_INET, token, &addr);
	monitorData->ClientIP = addr.s_addr;

	// Skip the 0
	token = strtok(NULL, delim);

	// MONR data
	token = strtok(NULL, delim);
	if (ASCIIToObjectMonitorData(token, &monitorData->MonrData) == 0)
		return 0;
	else
		return -1;
}

/*!
 * \brief UtilIsPositionNearTarget Checks if position lies within or on a sphere with radius equal to tolerance_m
 * and centre at target.
 * \param position Position to verify
 * \param target Target position
 * \param tolerance_m Radius around target position defining "near"
 * \return true if position is within tolerance_m of target, false otherwise
 */
uint8_t UtilIsPositionNearTarget(CartesianPosition position, CartesianPosition target, double tolerance_m) {
	double distance = 0.0;

	if (!position.isPositionValid || !target.isPositionValid)
		return 0;
	distance = sqrt(pow(position.xCoord_m - target.xCoord_m, 2)
					+ pow(position.yCoord_m - target.yCoord_m, 2)
					+ pow(position.zCoord_m - target.zCoord_m, 2));
	return distance <= tolerance_m;
}

/*!
 * \brief UtilIsAngleNearTarget Checks if angle is within tolerence of target angle
 * \param position Position with angle to verify
 * \param target Target position with angle
 * \param tolerance Angle tolerance defining "near"
 * \return true if position is within tolerance of target, false otherwise
 */
uint8_t UtilIsAngleNearTarget(CartesianPosition position, CartesianPosition target, double tolerance) {

	const double oneRotation = 2.0 * M_PI;
	double posHeading = position.heading_rad, tarHeading = target.heading_rad;

	if (!position.isHeadingValid || !target.isHeadingValid)
		return 0;

	while (posHeading < 0) {
		posHeading += oneRotation;
	}
	while (tarHeading < 0) {
		tarHeading += oneRotation;
	}

	return fabs(posHeading - tarHeading) <= tolerance;
}

double UtilCalcPositionDelta(double P1Lat, double P1Long, double P2Lat, double P2Long, ObjectPosition * OP) {

	double f, d, P1LatRad, P1LongRad, P2LatRad, P2LongRad, U1, U2, L, lambda, sins, coss, sigma, sinalpha,
		cosalpha2, cossm, C, lambdaprim, u2, A, B, dsigma, s, alpha1, alpha2, cosU2, sinlambda, cosU1, sinU1,
		sinU2, coslambda;

	double iLlh[3] = { P1Lat, P1Long, 202.934115075 };
	double xyz[3] = { 0, 0, 0 };
	double Llh[3] = { P2Lat, P2Long, 202.934115075 };

	OP->Latitude = P2Lat;
	OP->Longitude = P2Long;
	P1LatRad = UtilDegToRad(P1Lat);
	P1LongRad = UtilDegToRad(P1Long);
	P2LatRad = UtilDegToRad(P2Lat);
	P2LongRad = UtilDegToRad(P2Long);

	f = 1 / INVERSE_FLATTENING;
	U1 = atan((1 - f) * tan(P1LatRad));
	U2 = atan((1 - f) * tan(P2LatRad));
	L = P2LongRad - P1LongRad;
	lambda = L;
	lambdaprim = lambda;
	//printf("Lambdadiff: %1.15f\n", fabs(lambda-lambdaprim));

	int i = ORIGO_DISTANCE_CALC_ITERATIONS;

	OP->CalcIterations = 0;
	/*
	   do
	   {
	   sins = sqrt( pow((cos(U2)*sin(lambda)),2) + pow((cos(U1)*sin(U2) - sin(U1)*cos(U2)*cos(lambda)),2) );
	   if (sins==0) return 0; //co-incident points
	   coss = sin(U1)*sin(U2) + cos(U1)*cos(U2)*cos(lambda);
	   sigma = atan(sins/coss);
	   sinalpha = (cos(U1)*cos(U2)*sin(lambda))/sins;
	   cosalpha2 = 1 - pow(sinalpha,2);
	   cossm = coss - (2*sin(U1)*sin(U2) / cosalpha2);
	   C = (f/16) * cosalpha2 * ( 4 + f*(4 - 3 * cosalpha2) );
	   lambdaprim = lambda;
	   lambda = L + (1-C)*f*sinalpha*(sigma+C*sins*(cossm + C*coss*(-1+ 2*pow(cossm,2))));
	   OP->CalcIterations ++;
	   //printf("Lambdadiff: %1.15f\n", fabs(lambda-lambdaprim));

	   } while(fabs(lambda-lambdaprim) > l  && --i > 0);

	   if (i == 0)
	   {
	   //printf("Failed to converge!\n");
	   OP->OrigoDistance = -1;
	   }
	   else
	   { */
	//u2 = cosalpha2*((pow(a,2) - pow(b,2))/pow(b,2));
	//A = 1 +(u2/16384)*(4096+u2*(-768+u2*(320-175*u2)));
	//B = (u2/1024)*(256 + u2*(-128*u2*(74-47*u2)));
	//dsigma = B*sins*(cossm+0.25*B*(coss*(-1+2*pow(cossm,2)) - (1/6)*B*cossm*(-3+4*pow(sins,2))*(-3+4*pow(cossm,2))));
	//s = b*A*(sigma-dsigma);
	s = sqrt(pow(OP->x, 2) + pow(OP->y, 2));
	OP->DeltaOrigoDistance = s - OP->OrigoDistance;
	OP->OrigoDistance = s;


	/*
	   cosU2 = cos(U2);
	   sinU2 = sin(U2);
	   cosU1 = cos(U1);
	   sinU1 = sin(U1);
	   sinlambda = sin(lambda);
	   coslambda = cos(lambda);
	 */

	//OP->ForwardAzimuth1 = atan2(cosU2*sinlambda,(cosU1*sinU2-sinU1*cosU2*coslambda));
	//OP->ForwardAzimuth2 = atan2(cosU1*sinlambda,(sinU1*cosU2*-1+cosU1*sinU2*coslambda));

	//llhToEnu(iLlh, Llh, xyz);


	//OP->x = xyz[0];
	//OP->y = xyz[1];


	//}
	return s;
}

int UtilVincentyDirect(double refLat, double refLon, double a1, double distance, double *resLat,
					   double *resLon, double *a2) {
	/* Algorithm based on 07032018 website https://en.wikipedia.org/wiki/Vincenty%27s_formulae */


	// Variables only calculated once
	double U1, f = 1 / INVERSE_FLATTENING, sigma1, sina, pow2cosa, pow2u, A, B, C, L, lambda;

	// Iterative variables
	double sigma, deltaSigma, sigma2m;

	// Temporary iterative variables
	double prev_sigma;

	U1 = atan((1 - f) * tan(UtilDegToRad(refLat)));
	sigma1 = atan2(tan(U1), cos(a1));

	sina = cos(U1) * sin(a1);

	pow2cosa = 1 - pow(sina, 2);
	pow2u =
		pow2cosa * (pow(EARTH_EQUATOR_RADIUS_M, 2) - pow(EARTH_POLE_RADIUS_M, 2)) / pow(EARTH_POLE_RADIUS_M,
																						2);

	A = 1 + pow2u / 16384.0 * (4096.0 + pow2u * (-768.0 + pow2u * (320.0 - 175.0 * pow2u)));
	B = pow2u / 1024.0 * (256.0 + pow2u * (-128.0 + pow2u * (74.0 - 47.0 * pow2u)));


	int iterations = 0;
	double init_sigma = distance / (EARTH_POLE_RADIUS_M * A);

	sigma = init_sigma;
	do {
		if (++iterations > 100) {
			return -1;
		}
		prev_sigma = sigma;

		sigma2m = 2 * sigma1 + sigma;
		deltaSigma =
			B * sin(sigma) * (cos(sigma2m) +
							  0.25 * B * (cos(sigma) * (-1.0 + 2.0 * pow(cos(sigma2m), 2)) -
										  B / 6 * cos(sigma2m) * (-3.0 + 4.0 * pow(sin(sigma), 2.0))
										  * (-3.0 + 4.0 * pow(cos(sigma2m), 2))
							  )
			);
		sigma = init_sigma + deltaSigma;
	} while (fabs(sigma - prev_sigma) > VINCENTY_MIN_STEP_TOLERANCE);


	*resLat = UtilRadToDeg(atan2(sin(U1) * cos(sigma) + cos(U1) * sin(sigma) * cos(a1),
								 (1 - f) * sqrt(pow(sina, 2) +
												pow(sin(U1) * sin(sigma) - cos(U1) * cos(sigma) * cos(a1), 2))
						   ));

	lambda = atan2(sin(sigma) * sin(a1), cos(U1) * cos(sigma) - sin(U1) * sin(sigma) * cos(a1));

	C = f / 16 * pow2cosa * (4 + f * (4 - 3 * pow2cosa));

	L = lambda - (1 - C) * f * sina * (sigma +
									   C * sin(sigma) * (cos(sigma2m) +
														 C * cos(sigma) * (-1 + 2 * pow(cos(sigma2m), 2))
									   )
		);

	*resLon = UtilRadToDeg(L) + refLon;

	*a2 = atan2(sina, -sin(U1) * sin(sigma) + cos(U1) * cos(sigma) * cos(a1)
		);

	return 0;
}

double UtilDegToRad(double Deg) {
	return (M_PI * Deg / 180);
}
double UtilRadToDeg(double Rad) {
	return (180 * Rad / M_PI);
}


int UtilPopulateSpaceTimeArr(ObjectPosition * OP, char *TrajFile) {

	FILE *Trajfd;
	int Rows, j = 0;
	char TrajRow[TRAJECTORY_LINE_LENGTH];

	Trajfd = fopen(TrajFile, "r");
	if (Trajfd) {
		Rows = OP->TrajectoryPositionCount;
		double x, y, z;
		float t;
		char ValueStr[NUMBER_CHAR_LENGTH];
		char *src1;
		char *src2;

		do {
			memset(TrajRow, 0, sizeof (TrajRow));
			if (UtilReadLineCntSpecChars(Trajfd, TrajRow) >= 10) {
				bzero(ValueStr, NUMBER_CHAR_LENGTH);
				src1 = strchr(TrajRow, ';');
				src2 = strchr(src1 + 1, ';');
				strncpy(ValueStr, src1 + 1, (uint64_t) src2 - (uint64_t) src1);
				t = atof(ValueStr);
				//printf("%d :t = %3.3f\n", j, t);
				bzero(ValueStr, NUMBER_CHAR_LENGTH);
				src1 = strchr(src2, ';');
				src2 = strchr(src1 + 1, ';');
				strncpy(ValueStr, src1 + 1, (uint64_t) src2 - (uint64_t) src1);
				x = atof(ValueStr);

				bzero(ValueStr, NUMBER_CHAR_LENGTH);
				src1 = strchr(src2, ';');
				src2 = strchr(src1 + 1, ';');
				strncpy(ValueStr, src1 + 1, (uint64_t) src2 - (uint64_t) src1);
				y = atof(ValueStr);

				/*
				   bzero(ValueStr, NUMBER_CHAR_LENGTH);
				   src1 = strchr(src2, ';');
				   src2 = strchr(src1+1, ';');
				   strncpy(ValueStr, src1+1, (uint64_t)src2 - (uint64_t)src1);
				   z = atof(ValueStr);
				 */

				OP->SpaceArr[j] = (float)sqrt(pow(x, 2) + pow(y, 2));
				OP->TimeArr[j] = (float)t;

				OP->SpaceTimeArr[j].Index = j;
				OP->SpaceTimeArr[j].Time = (float)t;
				OP->SpaceTimeArr[j].OrigoDistance = (float)sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
				OP->SpaceTimeArr[j].Bearing = tan(y / x);
				OP->SpaceTimeArr[j].x = x;
				OP->SpaceTimeArr[j].y = y;
				//printf("t = %5.3f\n", OP->TimeArr[j]);
				//printf("t = %5.3f\n", OP->TimeArr[j]);
				j++;
			}


		} while (--Rows >= 0 /*j < 10 */ );

		//UtilSortSpaceTimeAscending(OP);

		//for(int g=0; g < OP->TrajectoryPositionCount; g ++)
		//{
		//  printf("OrigoDistance=%4.3f, Time=%4.3f, Index=%d\n", OP->SpaceTimeArr[g].OrigoDistance, OP->SpaceTimeArr[g].Time, OP->SpaceTimeArr[g].Index);
		//}

		fclose(Trajfd);
	}
	else {
		fprintf(stderr, "Failed to open file:%s\n", TrajFile);
	}

	return 0;
}

int UtilSetSyncPoint(ObjectPosition * OP, double x, double y, double z, double Time) {

	int i = 0;
	int Gate1Reached = 0, Gate2Reached = 0;

	if (Time == 0) {
		float R = (float)sqrt(pow(x, 2) + pow(y, 2));

		while (i < (OP->TrajectoryPositionCount - 1) && Gate1Reached == 0) {
			if (OP->SpaceArr[i] == R) {
				Gate1Reached = 1;
				OP->SyncIndex = i;
				//printf("Sync point found=%4.3f, Time=%4.3f, Index=%d\n", OP->SpaceArr[i], OP->TimeArr[i], i);
			}
			else {
				OP->SyncIndex = -1;
			}
			i++;
		}
	}
	else {

		while (i < (OP->TrajectoryPositionCount - 1) && Gate1Reached == 0) {
			//printf("%4.3f, %4.3f\n", OP->TimeArr[i], OP->TimeArr[i+1]);

			if (Time >= OP->TimeArr[i] && Time <= OP->TimeArr[i + 1]) {
				Gate1Reached = 1;
				OP->SyncIndex = i;
				// printf("Sync point found=%4.3f, Time=%4.3f, Index=%d\n", OP->SpaceArr[i], OP->TimeArr[i], i);
			}
			else {
				OP->SyncIndex = -1;
			}
			i++;
		}

	}

}

float UtilCalculateTimeToSync(ObjectPosition * OP) {

	float t = OP->SpaceTimeArr[OP->SyncIndex].Time - OP->SpaceTimeArr[OP->BestFoundTrajectoryIndex].Time;

	OP->TimeToSyncPoint = t;
	//printf("%3.3f %d %d\n", t , OP->SyncIndex, OP->BestFoundTrajectoryIndex);
	return t;
}




/*!
 * \brief UtilIsPointInPolygon Checks if a point lies within a polygon specified by a number of vertices using
 * the ray casting algorithm (see http://rosettacode.org/wiki/Ray-casting_algorithm).
 * \param point Coordinate of the point to check
 * \param vertices Coordinates of the vertex points defining the polygon to check
 * \param nPtsInPolygon length of the vertex array
 * \return true if the point lies within the polygon, false otherwise
 */
char UtilIsPointInPolygon(const CartesianPosition point, const CartesianPosition * vertices,
						  unsigned int nPtsInPolygon) {
	int nIntersects = 0;

	if (nPtsInPolygon == 0) {
		return -1;
	}

	// Count the number of intersections with the polygon
	for (unsigned int i = 0; i < nPtsInPolygon - 1; ++i) {
		if (rayFromPointIntersectsLine
			(point.xCoord_m, point.yCoord_m, vertices[i].xCoord_m, vertices[i].yCoord_m,
			 vertices[i + 1].xCoord_m, vertices[i + 1].yCoord_m)) {
			nIntersects++;
		}
	}
	// If the first and last points are different, the polygon segment between them must also be included
	if (fabs(vertices[0].xCoord_m - vertices[nPtsInPolygon - 1].xCoord_m) > (double)(2 * FLT_EPSILON)
		|| fabs(vertices[0].yCoord_m - vertices[nPtsInPolygon - 1].yCoord_m) > (double)(2 * FLT_EPSILON)) {
		if (rayFromPointIntersectsLine(point.xCoord_m, point.yCoord_m,
									   vertices[0].xCoord_m, vertices[0].yCoord_m,
									   vertices[nPtsInPolygon - 1].xCoord_m,
									   vertices[nPtsInPolygon - 1].yCoord_m)) {
			nIntersects++;
		}
	}

	// If the number of ray intersections with the polygon is even, the point must be outside the polygon
	if (nIntersects % 2 == 0)
		return 0;
	else
		return 1;
}

/*!
 * \brief rayFromPointIntersectsLine Checks whether a ray in positive x direction, parallel to the x axis,
 * from a point intersects a line AB between two points A and B
 * \param pointX X coordinate of the point from which ray originates
 * \param pointY Y coordinate of the point from which ray originates
 * \param linePointAX X coordinate of the point A
 * \param linePointAY Y coordinate of the point A
 * \param linePointBX X coordinate of the point B
 * \param linePointBY Y coordinate of the point B
 * \return true if ray intersects line AB, false otherwise
 */
static char rayFromPointIntersectsLine(double pointX, double pointY,
									   double linePointAX, double linePointAY, double linePointBX,
									   double linePointBY) {
	double tmp, maxVal, minVal, slopeAB, slopeAP;

	// Need 2x eps because machine epsilon is too small
	// Need to use float epsilon in case the double points are just promoted floats
	const double algorithmEpsilon = (double)(2 * FLT_EPSILON);

	// The algorithm assumes the Y coordinate of A is smaller than that of B
	// -> if this is not the case, swap the points
	if (linePointBY < linePointAY) {
		tmp = linePointAX;
		linePointAX = linePointBX;
		linePointBX = tmp;

		tmp = linePointAY;
		linePointAY = linePointBY;
		linePointBY = tmp;
	}

	// If the ray casting would cause intersection with a vertex the result is undefined,
	// so we perform a heuristic to avoid this situation:
	if (pointY == linePointAY || pointY == linePointBY) {
		pointY += algorithmEpsilon;
	}

	// A ray in X axis direction will not intersect if Y coordinates are outside of the range
	// spanned by the line AB
	if (pointY < linePointAY || pointY > linePointBY)
		return 0;

	// A ray in X axis direction will not intersect if it starts at a larger X coordinate than
	// any in the range of the line AB
	maxVal = linePointAX > linePointBX ? linePointAX : linePointBX;
	if (pointX >= maxVal)
		return 0;

	// A ray in X axis direction will intersect if it starts at a smaller X coordinate than
	// any in the range of the line AB
	minVal = linePointAX < linePointBX ? linePointAX : linePointBX;
	if (pointX < minVal)
		return 1;

	// Having filtered away all the situations where the point lies outside the rectangle
	// defined by the two points A and B, it is necessary to do some more costly processing

	// Compare the slopes of the lines AB and AP - if the slope of AB is larger than that of AP
	// then a ray in X axis direction will not intersect AB
	if (linePointAX != linePointBX)
		slopeAB = (linePointBY - linePointAY) / (linePointBX - linePointAX);
	else
		slopeAB = (double)INFINITY;
	if (linePointAX != pointX)
		slopeAP = (pointY - linePointAY) / (pointX - linePointAX);
	else
		slopeAP = (double)INFINITY;

	if (slopeAP >= slopeAB)
		return 1;
	else
		return 0;
}


int UtilSortSpaceTimeAscending(ObjectPosition * OP) {
	int i, j, index;
	float r, t;

	for (i = 0; i < OP->TrajectoryPositionCount; ++i) {
		for (j = i + 1; j < OP->TrajectoryPositionCount; ++j) {
			if (OP->SpaceTimeArr[i].OrigoDistance > OP->SpaceTimeArr[j].OrigoDistance) {
				index = OP->SpaceTimeArr[i].Index;
				r = OP->SpaceTimeArr[i].OrigoDistance;
				t = OP->SpaceTimeArr[i].Time;
				OP->SpaceTimeArr[i].Index = OP->SpaceTimeArr[j].Index;
				OP->SpaceTimeArr[i].OrigoDistance = OP->SpaceTimeArr[j].OrigoDistance;
				OP->SpaceTimeArr[i].Time = OP->SpaceTimeArr[j].Time;
				OP->SpaceTimeArr[j].Index = index;
				OP->SpaceTimeArr[j].OrigoDistance = r;
				OP->SpaceTimeArr[j].Time = t;
			}
		}
	}
}


int UtilFindCurrentTrajectoryPosition(ObjectPosition * OP, int StartIndex, double CurrentTime,
									  double MaxTrajDiff, double MaxTimeDiff, char debug) {

	int i = StartIndex, j = 0, ErrorDecreasing = 1, PositionFound = -1, Init, Q1, Q2;
	double Angle1, Angle2, R1, R2, RDiff, AngleDiff, PrevAngleDiff;

	if (i <= -1)
		i = 0;
	//OP->BestFoundTrajectoryIndex = 0;
	if (debug)
		fprintf(stderr, "UtilFindCurrentTrajectoryPosition: StartIndex=%d, CurrentTime=%4.3f, MaxTrajDiff=%4.3f, MaxTimeDiff=%4.3f\n",
				OP->OrigoDistance, OP->x, OP->y, OP->SyncIndex);

	Init = 1;
	while (i < (OP->TrajectoryPositionCount - 1) && i <= OP->SyncIndex) {

		Angle1 = M_PI / 2 - atan(fabs(OP->SpaceTimeArr[i].y) / fabs(OP->SpaceTimeArr[i].x));
		Q1 = 0;
		if (OP->SpaceTimeArr[i].y >= 0 && OP->SpaceTimeArr[i].x >= 0)
			Q1 = 1;
		else if (OP->SpaceTimeArr[i].y > 0 && OP->SpaceTimeArr[i].x < 0)
			Q1 = 2;
		else if (OP->SpaceTimeArr[i].y < 0 && OP->SpaceTimeArr[i].x < 0)
			Q1 = 3;
		else if (OP->SpaceTimeArr[i].y < 0 && OP->SpaceTimeArr[i].x > 0)
			Q1 = 4;

		Angle2 = M_PI / 2 - atan(fabs(OP->y) / fabs(OP->x));
		Q2 = 0;
		if (OP->y >= 0 && OP->x >= 0)
			Q2 = 1;
		else if (OP->y > 0 && OP->x < 0)
			Q2 = 2;
		else if (OP->y < 0 && OP->x < 0)
			Q2 = 3;
		else if (OP->y < 0 && OP->x > 0)
			Q2 = 4;

		if (debug == 2) {
			R1 = sqrt(pow(OP->SpaceTimeArr[i].x, 2) + pow(OP->SpaceTimeArr[i].y, 2));
			R2 = sqrt(pow(OP->x, 2) + pow(OP->y, 2));
			fprintf(stderr, "%d, %3.5f, %3.5f, %3.5f, %d, %d, %3.6f\n", i, fabs(R1 - R2), fabs(R1 - OP->OrigoDistance),
					fabs(Angle1 - Angle2), Q1, Q2, fabs(Angle1 - OP->ForwardAzimuth1));
		}


		if (Q1 == Q2) {
			R1 = sqrt(pow(OP->SpaceTimeArr[i].x, 2) + pow(OP->SpaceTimeArr[i].y, 2));
			R2 = sqrt(pow(OP->x, 2) + pow(OP->y, 2));
			//RDiff = fabs(R1-R2);
			RDiff = fabs(R1 - OP->OrigoDistance);
			AngleDiff = fabs(Angle1 - Angle2);
			if (Init == 0) {
				if ((AngleDiff < PrevAngleDiff) && (i > OP->BestFoundTrajectoryIndex) && RDiff <= MaxTrajDiff) {
					PositionFound = i;
					if (debug == 2)
						fprintf(stderr, "Minimum: %d, %3.6f, %3.6f\n", i, AngleDiff, RDiff);
					PrevAngleDiff = AngleDiff;
				}
			}
			else {
				PrevAngleDiff = AngleDiff;
			}
			Init = 0;
		}
		i++;
	}

	if (debug)
		fprintf(stderr, "Selected time: %3.3f\n", OP->SpaceTimeArr[PositionFound].Time);

	if (PositionFound == -1)
		OP->BestFoundTrajectoryIndex = TRAJ_POSITION_NOT_FOUND;
	else if (PositionFound > TRAJ_POSITION_NOT_FOUND) {
		OP->BestFoundTrajectoryIndex = PositionFound;
		OP->SpaceTimeFoundIndex = PositionFound;
	}

	if (debug == 2) {
		fprintf(stderr, "BestFoundTrajectoryIndex=%d\n", OP->BestFoundTrajectoryIndex);
		fprintf(stderr, "Current origo distance=%4.3f m\n", OP->OrigoDistance);
		fprintf(stderr, "Current time=%4.3f s\n", CurrentTime);
		fprintf(stderr, "Matched origo distance=%4.3f m\n", OP->SpaceTimeArr[PositionFound].OrigoDistance);
		fprintf(stderr, "Distance error=%4.3f m\n", OP->OrigoDistance - OP->SpaceTimeArr[PositionFound].OrigoDistance);
		fprintf(stderr, "Expected time=%4.3f s (index=%d)\n", OP->SpaceTimeArr[PositionFound].Time,
				OP->SpaceTimeArr[PositionFound].Index);
		fprintf(stderr, "Time error=%4.3f s\n", CurrentTime - OP->SpaceTimeArr[PositionFound].Time);
	}

	return PositionFound;
}


int UtilFindCurrentTrajectoryPositionNew(ObjectPosition * OP, int StartIndex, double CurrentTime,
										 double MaxTrajDiff, double MaxTimeDiff, char debug) {

	int i = StartIndex, j = 0, ErrorDecreasing = 1, PositionFound = -1, Init, Q1, Q2;
	int Gate1Reached = 0, Gate2Reached = 0, SampledSpaceIndex[SYNC_POINT_BUFFER];
	double Diff, PrevDiff, FutDiff, MinDiff =
		-1, BearingDiff, MinBearingDiff, Angle1, Angle2, R1, R2, RDiff, AngleDiff, PrevRDiff, PrevAngleDiff;

	if (i <= -1)
		i = 0;
	OP->BestFoundTrajectoryIndex = 0;
	fprintf("UtilFindCurrentTrajectoryPositionNew: TrajectoryPositionCount=%d, SyncIndex=%d, OrigoDistance=%4.3f, x=%4.3f, y=%4.3f, SyncIndex=%d\n",
			OP->TrajectoryPositionCount, OP->SyncIndex, OP->OrigoDistance, OP->x, OP->y, OP->SyncIndex);

	Init = 1;
	while (i < (OP->TrajectoryPositionCount - 1) && i <= OP->SyncIndex) {

		PrevDiff = (fabs(OP->SpaceTimeArr[i - 2].OrigoDistance - OP->OrigoDistance));
		Diff = (fabs(OP->SpaceTimeArr[i].OrigoDistance - OP->OrigoDistance));
		FutDiff = (fabs(OP->SpaceTimeArr[i + 2].OrigoDistance - OP->OrigoDistance));
		BearingDiff = fabs(OP->SpaceTimeArr[i].Bearing - OP->ForwardAzimuth2);

		Angle1 = M_PI / 2 - atan(fabs(OP->SpaceTimeArr[i].y) / fabs(OP->SpaceTimeArr[i].x));
		Q1 = 0;
		if (OP->SpaceTimeArr[i].y >= 0 && OP->SpaceTimeArr[i].x >= 0)
			Q1 = 1;
		else if (OP->SpaceTimeArr[i].y > 0 && OP->SpaceTimeArr[i].x < 0)
			Q1 = 2;
		else if (OP->SpaceTimeArr[i].y < 0 && OP->SpaceTimeArr[i].x < 0)
			Q1 = 3;
		else if (OP->SpaceTimeArr[i].y < 0 && OP->SpaceTimeArr[i].x > 0)
			Q1 = 4;

		Angle2 = M_PI / 2 - atan(fabs(OP->y) / fabs(OP->x));
		Q2 = 0;
		if (OP->y >= 0 && OP->x >= 0)
			Q2 = 1;
		else if (OP->y > 0 && OP->x < 0)
			Q2 = 2;
		else if (OP->y < 0 && OP->x < 0)
			Q2 = 3;
		else if (OP->y < 0 && OP->x > 0)
			Q2 = 4;

		R1 = sqrt(pow(OP->SpaceTimeArr[i].x, 2) + pow(OP->SpaceTimeArr[i].y, 2));
		R2 = sqrt(pow(OP->x, 2) + pow(OP->y, 2));
		if (debug == 2)
			fprintf(stderr, "%d, %3.5f, %3.5f, %3.5f, %d, %d, %3.6f", i, fabs(R1 - R2), fabs(R1 - OP->OrigoDistance),
					 fabs(Angle1 - Angle2), Q1, Q2, fabs(Angle1 - OP->ForwardAzimuth1));

		if (Q1 == Q2) {
			R1 = sqrt(pow(OP->SpaceTimeArr[i].x, 2) + pow(OP->SpaceTimeArr[i].y, 2));
			R2 = sqrt(pow(OP->x, 2) + pow(OP->y, 2));
			RDiff = fabs(R1 - R2);
			AngleDiff = fabs(Angle1 - Angle2);
			if (Init == 0) {
				if ((AngleDiff < PrevAngleDiff) && (i > OP->BestFoundTrajectoryIndex)) {
					PositionFound = i;
					//SampledSpaceIndex[j] = i;
					//j++ ;
					if (debug == 2)
						fprintf("Minimum: %d, %3.6f, %3.6f", i, AngleDiff, RDiff);
					PrevAngleDiff = AngleDiff;
				}

			}
			else {
				PrevAngleDiff = AngleDiff;
			}
			Init = 0;
		}
		i++;
	}

	if (debug)
		fprintf(stderr, "Selected time: %3.3f\n", OP->SpaceTimeArr[PositionFound].Time);

	if (PositionFound == -1)
		OP->BestFoundTrajectoryIndex = TRAJ_POSITION_NOT_FOUND;
	else if (PositionFound > TRAJ_POSITION_NOT_FOUND) {
		OP->BestFoundTrajectoryIndex = PositionFound;
		OP->SpaceTimeFoundIndex = PositionFound;

		/*
		   printf("BestFoundTrajectoryIndex=%d\n", OP->BestFoundTrajectoryIndex);
		   printf("Current origo distance=%4.3f m\n", OP->OrigoDistance);
		   printf("Current time=%4.3f s\n", CurrentTime);
		   printf("Matched origo distance=%4.3f m\n", OP->SpaceTimeArr[PositionFound].OrigoDistance);
		   printf("Distance error=%4.3f m\n", OP->OrigoDistance - OP->SpaceTimeArr[PositionFound].OrigoDistance);
		   printf("Expected time=%4.3f s (index=%d)\n", OP->SpaceTimeArr[PositionFound].Time, OP->SpaceTimeArr[PositionFound].Index);
		   printf("Time error=%4.3f s\n", CurrentTime - OP->SpaceTimeArr[PositionFound].Time);
		 */
	}

	return PositionFound;
}



int UtilFindCurrentTrajectoryPositionPrev(ObjectPosition * OP, int StartIndex, double CurrentTime,
										  double MaxTrajDiff, double MaxTimeDiff, char debug) {

	int i = StartIndex, j = 0, ErrorDecreasing = 1, PositionFound = -1, Init;
	int Gate1Reached = 0, Gate2Reached = 0, SampledSpaceIndex[SYNC_POINT_BUFFER];
	double Diff, PrevDiff, FutDiff, MinDiff = -1, BearingDiff, MinBearingDiff;

	if (i <= -1)
		i = 2;
	OP->BestFoundTrajectoryIndex = 0;
	fprintf(stderr, "UtilFindCurrentTrajectoryPosition: StartIndex=%d, CurrentTime=%4.3f, MaxTrajDiff=%4.3f, MaxTimeDiff=%4.3f, SyncIndex=%d\n",
			StartIndex, CurrentTime, MaxTrajDiff, MaxTimeDiff, OP->SyncIndex);

	Init = 1;
	while (i < (OP->TrajectoryPositionCount - 1) && i <= OP->SyncIndex) {

		PrevDiff = (fabs(OP->SpaceTimeArr[i - 2].OrigoDistance - OP->OrigoDistance));
		Diff = (fabs(OP->SpaceTimeArr[i].OrigoDistance - OP->OrigoDistance));
		FutDiff = (fabs(OP->SpaceTimeArr[i + 2].OrigoDistance - OP->OrigoDistance));
		BearingDiff = fabs(OP->SpaceTimeArr[i].Bearing - OP->ForwardAzimuth2);

		if (debug == 2)
			fprintf(stderr, "%d, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.6f\n", i, PrevDiff, Diff, FutDiff,
					 OP->SpaceTimeArr[i].x, OP->x, OP->SpaceTimeArr[i].y, OP->y,
					 fabs(OP->SpaceTimeArr[i].Bearing - tan(OP->y / OP->x)));

		if (Init == 0) {
			if ((Diff <= PrevDiff) && (Diff <= FutDiff) && (i > OP->BestFoundTrajectoryIndex)) {
				MinDiff = Diff;
				MinBearingDiff = BearingDiff;
				PositionFound = i;
				SampledSpaceIndex[j] = i;
				j++;
				if (debug == 1)
					fprintf(stderr, "Minimum: %d, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.3f, %3.6f\n", i,
							 PrevDiff, Diff, FutDiff, OP->SpaceTimeArr[i].x, OP->x, OP->SpaceTimeArr[i].y,
							 OP->y, fabs(OP->SpaceTimeArr[i].Bearing - tan(OP->y / OP->x)));
			}
		}

		Init = 0;
		i++;
	}

	Init = 1;
	PositionFound = -1;
	for (i = 0; i < j; i++) {
		Diff = fabs(CurrentTime - OP->SpaceTimeArr[SampledSpaceIndex[i]].Time);
		if (Init == 0) {
			if (Diff < MinDiff) {
				PositionFound = SampledSpaceIndex[i];
			}
		}
		Init = 0;
		MinDiff = Diff;
	}

	if (debug)
		fprintf(stderr, "Selected time: %3.3f\n", OP->SpaceTimeArr[PositionFound].Time);

	if (PositionFound == -1)
		OP->BestFoundTrajectoryIndex = TRAJ_POSITION_NOT_FOUND;
	else if (PositionFound > TRAJ_POSITION_NOT_FOUND) {
		OP->BestFoundTrajectoryIndex = PositionFound;
		OP->SpaceTimeFoundIndex = PositionFound;

		/*
		   printf("BestFoundTrajectoryIndex=%d\n", OP->BestFoundTrajectoryIndex);
		   printf("Current origo distance=%4.3f m\n", OP->OrigoDistance);
		   printf("Current time=%4.3f s\n", CurrentTime);
		   printf("Matched origo distance=%4.3f m\n", OP->SpaceTimeArr[PositionFound].OrigoDistance);
		   printf("Distance error=%4.3f m\n", OP->OrigoDistance - OP->SpaceTimeArr[PositionFound].OrigoDistance);
		   printf("Expected time=%4.3f s (index=%d)\n", OP->SpaceTimeArr[PositionFound].Time, OP->SpaceTimeArr[PositionFound].Index);
		   printf("Time error=%4.3f s\n", CurrentTime - OP->SpaceTimeArr[PositionFound].Time);
		 */
	}

	return PositionFound;
}



//TODO THIS IS UNUSED - DELETE?
int UtilFindCurrentTrajectoryPositionOld(ObjectPosition * OP, int StartIndex, double CurrentTime,
										 double MaxTrajDiff, double MaxTimeDiff, char debug) {

	int i = StartIndex, j = 0;
	int Gate1Reached = 0, Gate2Reached = 0, SampledSpaceIndex[SYNC_POINT_BUFFER];
	double Diff;

	if (i <= -1)
		i = 0;
	OP->BestFoundTrajectoryIndex = 0;
	if (debug)
		printf("OPOrigoDistance=%4.3f, x=%4.3f, y=%4.3f\n", OP->OrigoDistance, OP->x, OP->y);

	while (i < (OP->TrajectoryPositionCount - 1) && Gate2Reached == 0) {

		Diff = fabs(OP->SpaceTimeArr[i].OrigoDistance - OP->OrigoDistance);
		//printf("%4.3f, %4.3f, %4.3f\n ", Diff,OP->SpaceTimeArr[i].OrigoDistance,OP->OrigoDistance);

		if (Diff < MaxTrajDiff && Gate1Reached == 0) {
			Gate1Reached = 1;
		}

		if (Diff > MaxTrajDiff && Gate1Reached == 1) {
			Gate2Reached = 1;
		}

		if (Gate1Reached == 1) {
			if (j < SYNC_POINT_BUFFER - 1 && OP->SpaceTimeArr[i].Index <= OP->SyncIndex
				&& OP->SpaceTimeArr[i].Index > OP->BestFoundTrajectoryIndex) {
				SampledSpaceIndex[j] = i;
				j++;
				if (debug)
					printf
						("i=%d, j=%d ,ArrOrigoDistance=%4.3f, Diff=%4.3f, ArrTime=%4.3f, Index=%d, CurrentTime=%4.3f \n",
						 i, j, OP->SpaceTimeArr[i].OrigoDistance, Diff, OP->SpaceTimeArr[i].Time,
						 OP->SpaceTimeArr[i].Index, CurrentTime);
			}
			else if (j >= SYNC_POINT_BUFFER) {
				printf("Sync point buffer overrun j=%d\n", j);
			}
		}

		i++;
	}

	if (j == 0)
		OP->BestFoundTrajectoryIndex = TRAJ_POSITION_NOT_FOUND;	//No trajectory position found

	int PositionFound = -1, kc = 0;

	if (OP->BestFoundTrajectoryIndex > TRAJ_POSITION_NOT_FOUND) {
		i = 0;
		int SampledTimeIndex[SYNC_POINT_BUFFER];

		if (MaxTimeDiff > 0) {
			while (i < j) {
				Diff = fabs(OP->SpaceTimeArr[SampledSpaceIndex[i]].Time - CurrentTime);
				if (debug)
					printf("%4.3f, ", Diff);
				if (Diff < MaxTimeDiff)
					//if(OP->SpaceTimeArr[SampledSpaceIndex[i]].Time > OP->SpaceTimeArr[OP->BestFoundTrajectoryIndex].Time)
				{
					SampledTimeIndex[kc] = SampledSpaceIndex[i];
					kc++;
				}
				i++;
			}
		}
		else
			for (i = 0; i < j; i++)
				SampledTimeIndex[i] = SampledSpaceIndex[i];

		if (debug)
			printf("\n");

		i = 0;
		int Init = 1;
		double PrevDiff = 0;

		while (i < kc) {
			if (Init == 1)
				PositionFound = SampledTimeIndex[i];
			Diff = fabs(OP->SpaceTimeArr[SampledTimeIndex[i]].OrigoDistance - OP->OrigoDistance);	//+ fabs(OP->SpaceTimeArr[SampledSpaceIndex[i]].Time - CurrentTime);
			if (debug)
				printf("%4.3f, ", Diff);
			if (Diff < PrevDiff && Init == 0) {
				PositionFound = SampledTimeIndex[i];
			}
			Init = 0;
			PrevDiff = Diff;
			i++;
		}
		if (debug)
			printf("\n");

		if (PositionFound > TRAJ_POSITION_NOT_FOUND) {
			OP->BestFoundTrajectoryIndex = OP->SpaceTimeArr[PositionFound].Index;
			OP->SpaceTimeFoundIndex = PositionFound;

			/*
			   printf("BestFoundTrajectoryIndex=%d\n", OP->BestFoundTrajectoryIndex);
			   printf("Current origo distance=%4.3f m\n", OP->OrigoDistance);
			   printf("Current time=%4.3f s\n", CurrentTime);
			   printf("Matched origo distance=%4.3f m\n", OP->SpaceTimeArr[PositionFound].OrigoDistance);
			   printf("Distance error=%4.3f m\n", OP->OrigoDistance - OP->SpaceTimeArr[PositionFound].OrigoDistance);
			   printf("Expected time=%4.3f s (index=%d)\n", OP->SpaceTimeArr[PositionFound].Time, OP->SpaceTimeArr[PositionFound].Index);
			   printf("Time error=%4.3f s\n", CurrentTime - OP->SpaceTimeArr[PositionFound].Time);
			 */
		}
		else {
			//OP->BestFoundTrajectoryIndex = TRAJ_MASTER_LATE;
			//printf("Not in time\n");
		}
	}
	return PositionFound;
}

/*!
 * \brief UtilCountFileRows Count number of rows terminated with \n (0x0A) 
 * \param fd fileDescriptor
 * \return number of rows
 */
int UtilCountFileRows(FILE * fd) {
	int c = 0;
	int rows = 0;

	while (c != EOF) {
		c = fgetc(fd);
		//printf("%x-", c);
		if (c == '\n')
			rows++;
	}

	return rows;
}

/*!
 * \brief UtilCountFileRowsInPath Count number of rows terminated with \n (0x0A) 
 * \                              in a file specificed by path
 * \param path Path to file
 * \param cpData Command data
 * \param dataLength Length of command data array
 * \return number of rows on success, -1 on error
 */
int UtilCountFileRowsInPath(const char *path, const size_t pathlen) {
	int c = 0;
	int rows = 0;
	if(pathlen <= 0)
		return -1;
	FILE *fd;
	fd = fopen(path, "r");
	if (fd != NULL)	rows = UtilCountFileRows(fd);
	else rows = -1;
	fclose(fd);
	return rows;
}


int UtilReadLineCntSpecChars(FILE * fd, char *Buffer) {
	int c = 0;
	int d = 0;
	int SpecChars = 0;
	int comment = 0;

	while ((c != EOF) && (c != '\n')) {
		c = fgetc(fd);
		//printf("%x-", c);
		if (c != '\n') {
			if (c == '/') {
				comment++;
				d = c;
			}

			if (comment == 0) {
				*Buffer = (char)c;
				Buffer++;
				if (c == ';' || c == ':')
					SpecChars++;
			}
			else if (comment == 1 && c != '/') {
				*Buffer = (char)d;
				Buffer++;
				if (d == ';' || d == ':')
					SpecChars++;
				*Buffer = (char)c;
				Buffer++;
				if (c == ';' || c == ':')
					SpecChars++;
				comment = 0;
			}
			else if (comment == 2 && c == '\n') {
				c = 1;
				comment = 0;
			}
			else if (comment == 2 && c != '\n') {
				//just continue
			}

		}
	}
	return SpecChars;
}


int UtilReadLine(FILE * fd, char *Buffer) {
	int c = 0;
	int d = 0;
	int SpecChars = 0;
	int comment = 0;

	while ((c != EOF) && (c != '\n')) {
		c = fgetc(fd);
		//printf("%x-", c);
		if (c != '\n') {
			*Buffer = (char)c;
			Buffer++;
			d++;
		}
	}
	return d;
}



/*!
 * \brief UtilGetRowInFile Gets a specific row in file specified by rowIndex
 * \param path Path to file
 * \param pathLength Length of path
 * \param rowIndex Selected row
 * \param rowBuffer Pointer to where to store the read row
 * \param bufferLength Length of buffer
 * \return 1 on success, -1 on error
 */
int UtilGetRowInFile(const char *path, const size_t pathLength,
					I32 rowIndex, char *rowBuffer, const size_t bufferLength){
	int c = 0;
	int rows = 0;
	int length = 0;
	FILE *fd;
	
	if(pathLength < 0)
		return -1;
	
	fd = fopen(path, "r");
	if (fd != NULL){
		rows = UtilCountFileRows(fd);
		fclose(fd);
		fd = fopen(path, "r");
		for(int i = 0; i < rows; i ++){
			length = UtilReadLine(fd, rowBuffer);
			if(length > bufferLength){ 
				fprintf(stderr, "Buffer to small for read row in file\n");
				return -1;
			}
			if(rowIndex == i){
				*(rowBuffer+length) = NULL;
				return i;
			} 
		}
	}
	fclose(fd);
	return -1;
}

C8 *UtilSearchTextFile(const C8 * Filename, C8 * Text1, C8 * Text2, C8 * Result) {

	FILE *fd;

	char RowBuffer[MAX_ROW_SIZE];
	char DataBuffer[MAX_ROW_SIZE];
	char *PtrText1;
	char *PtrText2;
	int Length;
	U8 Found = 0;
	int RowCount = 0;

	fd = fopen( (const char*)Filename, "r");

	if (fd == NULL) {
		sprintf(RowBuffer, "Unable to open file <%s>", (const char*) Filename);
		util_error(RowBuffer);
	}

	RowCount = UtilCountFileRows(fd);
	fclose(fd);

	fd = fopen((const char*) Filename, "r");
	if (fd != NULL) {
		do {
			bzero(RowBuffer, MAX_ROW_SIZE);
			UtilReadLineCntSpecChars(fd, RowBuffer);
			bzero(DataBuffer, MAX_ROW_SIZE);
			PtrText1 = strstr(RowBuffer, (const char *)Text1);
			if (PtrText1 != NULL) {
				if (strlen(Text2) > 0) {
					PtrText2 = strstr((const char *)(PtrText1 + 1), (const char *)Text2);
					if (PtrText2 != NULL) {
						strncpy(Result, PtrText1 + strlen(Text1),
								strlen(RowBuffer) - strlen(Text1) - strlen(Text2));
					}
				}
				else {
					strncpy(Result, PtrText1 + strlen(Text1), strlen(RowBuffer) - strlen(Text1));
				}
				Found = 1;
			}
			RowCount--;

		} while (Found == 0 && RowCount >= 0);

		fclose(fd);
	}
	else {
		sprintf(RowBuffer, "Unable to open file <%s>", (const char*) Filename);
		util_error(RowBuffer);
	}

	//printf("String found: %s\n", Result);
	return Result;

}


/*!
 * \brief getTestDirectoryPath Gets the absolute path to the directory where subdirectories for
 * trajectories, geofences, configuration etc. are stored, ending with a forward slash. The
 * function defaults to a subdirectory of the user's home directory if the environment variable
 * ATOS_TEST_DIR is not set.
 * \param path Char array to hold path name
 * \param pathLen Length of char array
 */
void UtilGetTestDirectoryPath(char *path, size_t pathLen) {
	char *envVar;

	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}

	strcpy(path, getenv("HOME"));
	strcat(path, "/");
	strcat(path, ATOS_TEST_DIR_NAME);
	strcat(path, "/");

}

/*!
 * \brief UtilGetJournalDirectoryPath Fetches the absolute path to where test logs are stored,
 * ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetJournalDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, JOURNAL_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilGetConfDirectoryPath Fetches the absolute path to where configuration files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetConfDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, CONFIGURATION_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilGetTrajDirectoryPath Fetches the absolute path to where trajectory files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetTrajDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, TRAJECTORY_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilGetOdrDirectoryPath Fetches the absolute path to where OpenDRIVE files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetOdrDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, OPENDRIVE_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilGetOscDirectoryPath Fetches the absolute path to where OpenSCENARIO files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetOscDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, OPENSCENARIO_DIR_NAME);
	strcat(path, "/");
}


/*!
 * \brief UtilGetGeofenceDirectoryPath Fetches the absolute path to where geofence files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetGeofenceDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, GEOFENCE_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilGetObjectDirectoryPath Fetches the absolute path to where object files
 * are stored, ending with a forward slash.
 * \param path Char array to hold the path
 * \param pathLen Length of char array
 */
void UtilGetObjectDirectoryPath(char *path, size_t pathLen) {
	if (pathLen > MAX_FILE_PATH) {
		fprintf(stderr, "Path variable too small to hold path data\n");
		path[0] = '\0';
		return;
	}
	UtilGetTestDirectoryPath(path, pathLen);
	strcat(path, OBJECT_DIR_NAME);
	strcat(path, "/");
}

/*!
 * \brief UtilDeleteTrajectoryFile deletes the specified trajectory
 * \param name
 * \param nameLen
 * \return returns 0 if the trajectory is now deleted. Non-zero values otherwise.
 */
int UtilDeleteTrajectoryFile(const char *name, const size_t nameLen) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetTrajDirectoryPath(filePath, sizeof (filePath));

	if (name == NULL) {
		errno = EINVAL;
		fprintf(stderr, "Attempt to call delete on null trajectory file\n");
		return -1;
	}
	if (strstr(name, "..") != NULL || strstr(name, "/") != NULL) {
		errno = EPERM;
		fprintf(stderr, "Attempt to call delete on trajectory file and navigate out of directory\n");
		return -1;
	}
	if (strlen(filePath) + nameLen > MAX_FILE_PATH) {
		errno = ENOBUFS;
		fprintf(stderr, "Trajectory file name too long\n");
		return -1;
	}

	if (filePath[0] == '\0')
		return -1;

	strcat(filePath, name);
	return deleteFile(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteGeofenceFile deletes the specified geofence
 * \param name
 * \param nameLen
 * \return returns 0 if the geofence is now deleted. Non-zero values otherwise.
 */
int UtilDeleteGeofenceFile(const char *name, const size_t nameLen) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetGeofenceDirectoryPath(filePath, sizeof (filePath));

	if (name == NULL) {
		errno = EINVAL;
		fprintf(stderr, "Attempt to call delete on null geofence file\n");
		return -1;
	}
	if (strstr(name, "..") != NULL || strstr(name, "/") != NULL) {
		errno = EPERM;
		fprintf(stderr, "Attempt to call delete on geofence file and navigate out of directory\n");
		return -1;
	}
	if (strlen(filePath) + nameLen > MAX_FILE_PATH) {
		errno = ENOBUFS;
		fprintf(stderr, "Geofence file name too long\n");
		return -1;
	}

	if (filePath[0] == '\0')
		return -1;

	strcat(filePath, name);
	return deleteFile(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteObjectFile deletes the specified object file
 * \param name
 * \param nameLen
 * \return returns 0 if the object is now deleted. Non-zero values otherwise.
 */
int UtilDeleteObjectFile(const char *name, const size_t nameLen) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetObjectDirectoryPath(filePath, sizeof (filePath));

	if (name == NULL) {
		errno = EINVAL;
		fprintf(stderr, "Attempt to call delete on null object file\n");
		return -1;
	}
	if (strstr(name, "..") != NULL || strstr(name, "/") != NULL) {
		errno = EPERM;
		fprintf(stderr, "Attempt to call delete on object file and navigate out of directory\n");
		return -1;
	}
	if (strlen(filePath) + nameLen > MAX_FILE_PATH) {
		errno = ENOBUFS;
		fprintf(stderr, "Object file name too long\n");
		return -1;
	}

	if (filePath[0] == '\0')
		return -1;

	strcat(filePath, name);
	return deleteFile(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteGeofenceFile deletes the specified file and deletes it
 * \param pathRelativeToWorkspace
 * \return returns 0 if the geofence is now deleted. Non-zero values otherwise.
 */
int UtilDeleteGenericFile(const char *pathRelativeToWorkspace, const size_t nameLen) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetTestDirectoryPath(filePath, sizeof (filePath));

	if (pathRelativeToWorkspace == NULL) {
		errno = EINVAL;
		fprintf(stderr, "Attempt to call delete on null generic file\n");
		return -1;
	}

	if (strstr(pathRelativeToWorkspace, "..") != NULL) {
		errno = EPERM;
		fprintf(stderr, "Attempt to call delete on generic file and navigate out of directory\n");
		return -1;
	}

	if (strlen(filePath) + nameLen > MAX_FILE_PATH) {
		errno = ENOBUFS;
		fprintf(stderr, "Generic path name too long\n");
		return -1;
	}

	if (filePath[0] == '\0')
		return -1;

	strcat(filePath, pathRelativeToWorkspace);
	return deleteFile(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteTrajectoryFiles finds the trajectory folder and deletes its contents
 * \return returns 0 if succesfull if the trajectory folder now is empty. Non-zero values otherwise.
 */
int UtilDeleteTrajectoryFiles(void) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetTrajDirectoryPath(filePath, sizeof (filePath));
	if (filePath[0] == '\0')
		return -1;
	return deleteDirectoryContents(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteGeofenceFiles finds the geofence folder and deletes its contents
 * \return returns 0 if succesful if the geofence folder now is empty. Non-zero values otherwise.
 */
int UtilDeleteGeofenceFiles(void) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetGeofenceDirectoryPath(filePath, sizeof (filePath));
	if (filePath[0] == '\0')
		return -1;
	return deleteDirectoryContents(filePath, sizeof (filePath));
}

/*!
 * \brief UtilDeleteObjectFiles finds the object folder and deletes its contents
 * \return returns 0 if succesful if the object folder now is empty. Non-zero values otherwise.
 */
int UtilDeleteObjectFiles(void) {
	char filePath[MAX_FILE_PATH] = { '\0' };
	UtilGetObjectDirectoryPath(filePath, sizeof (filePath));
	if (filePath[0] == '\0')
		return -1;
	return deleteDirectoryContents(filePath, sizeof (filePath));
}

/*!
 * \brief UtilParseTrajectoryFileHeader Attempts to parse a line into a trajectory header
 * \param line Line to be parsed
 * \param header Pointer to struct to fill
 * \return -1 if parsing failed, 0 otherwise
 */
int UtilParseTrajectoryFileHeader(char *line, TrajectoryFileHeader * header) {
	char *token;
	char *dotToken;
	const char delimiter[3] = ";\n";
	unsigned int column = 0;
	int noOfLines = 0;
	int retval = 0;

	header->ID = 0;
	memset(header->name, '\0', sizeof (header->name));
	header->majorVersion = 0;
	header->minorVersion = 0;
	header->numberOfLines = 0;

	token = strtok(line, delimiter);
	if (!strcmp(token, "TRAJECTORY")) {
		while (retval != -1 && (token = strtok(NULL, delimiter)) != NULL) {
			column++;
			switch (column) {
			case 1:
				header->ID = (unsigned int)atoi(token);
				break;
			case 2:
				if (strlen(token) > sizeof (header->name)) {
					fprintf(stderr, "Name field \"%s\" in trajectory too long\n", token);
					retval = -1;
				}
				else {
					strcpy(header->name, token);
				}
				break;
			case 3:
				header->majorVersion = (unsigned short)atoi(token);
				if ((dotToken = strchr(token, '.')) != NULL && *(dotToken + 1) != '\0') {
					header->minorVersion = (unsigned short)atoi(dotToken + 1);
				}
				else {
					header->minorVersion = 0;
				}
				break;
			case 4:
				noOfLines = atoi(token);
				if (noOfLines >= 0)
					header->numberOfLines = (unsigned int)noOfLines;
				else {
					fprintf(stderr, "Found negative number of lines in trajectory\n");
					retval = -1;
				}
				break;
			default:
				fprintf(stderr, "Found unexpected \"%s\" in header\n", token);
				retval = -1;
			}
		}
	}
	else {
		fprintf(stderr, "Cannot parse line \"%s\" as trajectory header\n", line);
		retval = -1;
	}

	if (retval == -1) {
		header->ID = 0;
		memset(header->name, '\0', sizeof (header->name));
		header->majorVersion = 0;
		header->minorVersion = 0;
		header->numberOfLines = 0;
	}
	return retval;
}

/*!
 * \brief UtilParseTrajectoryFileLine Attempts to parse a line into a trajectory line
 * \param line Line to be parsed
 * \param header Pointer to struct to fill
 * \return -1 if parsing failed, 0 otherwise
 */
int UtilParseTrajectoryFileLine(char *line, TrajectoryFileLine * fileLine) {

	char *tokenIndex = line;
	char *nextTokenIndex;
	const char delimiter = ';';
	char token[SMALL_BUFFER_SIZE_64];
	int retval = 0;
	unsigned short column = 0;

	if (fileLine->zCoord)
		free(fileLine->zCoord);
	if (fileLine->lateralVelocity)
		free(fileLine->lateralVelocity);
	if (fileLine->lateralAcceleration)
		free(fileLine->lateralAcceleration);
	if (fileLine->longitudinalVelocity)
		free(fileLine->longitudinalVelocity);
	if (fileLine->lateralAcceleration)
		free(fileLine->longitudinalAcceleration);

	memset(fileLine, 0, sizeof (*fileLine));

	// strtok() does not handle double delimiters well, more complicated parsing necessary
	while ((nextTokenIndex = index(tokenIndex, delimiter)) != NULL && retval != -1) {
		column++;
		memset(token, '\0', sizeof (token));
		memcpy(token, tokenIndex, (unsigned long)(nextTokenIndex - tokenIndex));
		switch (column) {
		case 1:
			if (strcmp(token, "LINE")) {
				fprintf(stderr, "Line start badly formatted\n");
				retval = -1;
			}
			break;
		case 2:
			fileLine->time = atof(token);
			break;
		case 3:
			fileLine->xCoord = atof(token);
			break;
		case 4:
			fileLine->yCoord = atof(token);
			break;
		case 5:
			if (strlen(token) != 0) {
				fileLine->zCoord = malloc(sizeof (fileLine->zCoord));
				*fileLine->zCoord = atof(token);
			}
			break;
		case 6:
			fileLine->heading = atof(token);
			break;
		case 7:
			if (strlen(token) != 0) {
				fileLine->longitudinalVelocity = malloc(sizeof (fileLine->longitudinalVelocity));
				*fileLine->longitudinalVelocity = atof(token);
			}
			break;
		case 8:
			if (strlen(token) != 0) {
				fileLine->lateralVelocity = malloc(sizeof (fileLine->lateralVelocity));
				*fileLine->lateralVelocity = atof(token);
			}
			break;
		case 9:
			if (strlen(token) != 0) {
				fileLine->longitudinalAcceleration = malloc(sizeof (fileLine->longitudinalAcceleration));
				*fileLine->longitudinalAcceleration = atof(token);
			}
			break;
		case 10:
			if (strlen(token) != 0) {
				fileLine->lateralAcceleration = malloc(sizeof (fileLine->lateralAcceleration));
				*fileLine->lateralAcceleration = atof(token);
			}
			break;
		case 11:
			fileLine->curvature = atof(token);
			break;
		case 12:
			fileLine->mode = (uint8_t) atoi(token);
			break;
		case 13:
			if (strcmp(token, "ENDLINE")) {
				fprintf(stderr, "Line end badly formatted\n");
				retval = -1;
			}
			break;
		default:
			fprintf(stderr, "Superfluous delimiter in line\n");
			retval = -1;
			break;
		}
		tokenIndex = nextTokenIndex + 1;
	}
	if (column != 13 && retval == 0) {
		fprintf(stderr, "Wrong number of fields (%u) in trajectory line\n", column);
		retval = -1;
	}

	return retval;
}

/*!
 * \brief UtilParseTrajectoryFileFooter Attempts to parse a line as a trajectory footer
 * \param line Line to be parsed
 * \return -1 if parsing failed, 0 otherwise
 */
int UtilParseTrajectoryFileFooter(char *line) {
	char *token;
	const char delimiter[3] = ";\n";
	int retval = 0;

	token = strtok(line, delimiter);
	if (!strcmp(token, "ENDTRAJECTORY")) {
		while ((token = strtok(NULL, delimiter)) != NULL) {
			fprintf(stderr, "Footer contained unexpected \"%s\"\n", token);
			retval = -1;
		}
	}
	else {
		retval = -1;
	}

	return retval;
}

/*!
 * \brief UtilCheckTrajectoryFileFormat Verifies that the file follows ISO format
 * \param path Path to the file to be checked
 * \param pathLen Length of the path variable
 * \return -1 if the file does not follow the correct format, 0 otherwise
 */
int UtilCheckTrajectoryFileFormat(const char *path, size_t pathLen) {
	int retval = 0;
	FILE *fp = fopen(path, "r");

	char *line;
	size_t len = 0;
	ssize_t read;

	unsigned int row = 0;

	TrajectoryFileHeader header;
	TrajectoryFileLine fileLine;

	memset(&fileLine, 0, sizeof (fileLine));

	if (fp == NULL) {
		fprintf(stderr, "Could not open file <%s>\n", path);
		return -1;
	}

	// Read line by line
	while ((read = getline(&line, &len, fp)) != -1) {
		row++;
		if (row == 1) {			// Header parsing
			// If header parsing failed, parsing the rest of the file could be risky
			if ((retval = UtilParseTrajectoryFileHeader(line, &header)) == -1) {
				fprintf(stderr, "Failed to parse header of file <%s>\n", path);
				break;
			}
		}
		else if (row == header.numberOfLines + 2) {	// Footer parsing
			if ((retval = UtilParseTrajectoryFileFooter(line)) != 0) {
				if (UtilParseTrajectoryFileLine(line, &fileLine) == 0) {
					fprintf(stderr, "File <%s> contains %u rows than were specified\n", path);
					break;
				}
				fprintf(stderr, "Failed to parse footer of file <%s>\n", path);
			}
		}
		else if (row > header.numberOfLines + 2) {
			fprintf(stderr, "File <%s> contains more rows than specified\n", path);
			retval = -1;
			break;
		}
		else {					// Line parsing
			if (UtilParseTrajectoryFileLine(line, &fileLine) != 0) {
				if (UtilParseTrajectoryFileFooter(line) == 0)
					fprintf(stderr, "File <%s> contains %u rows but %u were specified\n", path,
							   row - 2, header.numberOfLines);
				else
					fprintf(stderr, "Failed to parse line %u of file <%s>\n", row, path);
				retval = -1;
			}
		}
	}

	fclose(fp);

	if (line)
		free(line);
	if (fileLine.zCoord)
		free(fileLine.zCoord);
	if (fileLine.lateralVelocity)
		free(fileLine.lateralVelocity);
	if (fileLine.lateralAcceleration)
		free(fileLine.lateralAcceleration);
	if (fileLine.longitudinalVelocity)
		free(fileLine.longitudinalVelocity);
	if (fileLine.lateralAcceleration)
		free(fileLine.longitudinalAcceleration);

	return retval;
}



static void init_crc16_tab(void);

static bool crc_tab16_init = false;
static uint16_t crc_tab16[256];

#define   CRC_POLY_16   0xA001

static void init_crc16_tab(void) {

	uint16_t i;
	uint16_t j;
	uint16_t crc;
	uint16_t c;

	for (i = 0; i < 256; i++) {

		crc = 0;
		c = i;

		for (j = 0; j < 8; j++) {

			if ((crc ^ c) & 0x0001)
				crc = (crc >> 1) ^ CRC_POLY_16;
			else
				crc = crc >> 1;

			c = c >> 1;
		}

		crc_tab16[i] = crc;
	}

	crc_tab16_init = true;

}


#define   CRC_START_16    0x0000

uint16_t crc_16(const unsigned char *input_str, uint16_t num_bytes) {

	uint16_t crc;
	uint16_t tmp;
	uint16_t short_c;
	const unsigned char *ptr;
	uint16_t i = 0;

	if (!crc_tab16_init)
		init_crc16_tab();

	crc = CRC_START_16;
	ptr = input_str;

	if (ptr != NULL)
		for (i = 0; i < num_bytes; i++) {

			short_c = 0x00ff & (uint16_t) * ptr;
			tmp = crc ^ short_c;
			crc = (crc >> 8) ^ crc_tab16[tmp & 0xff];
			ptr++;
		}

	return crc;

}								/* crc_16 */




U16 SwapU16(U16 val) {
	return (val << 8) | (val >> 8);
}

I16 SwapI16(I16 val) {
	return (val << 8) | ((val >> 8) & 0xFF);
}

U32 SwapU32(U32 val) {
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
	return (val << 16) | (val >> 16);
}

I32 SwapI32(I32 val) {
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
	return (val << 16) | ((val >> 16) & 0xFFFF);
}

I64 SwapI64(I64 val) {
	val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
	val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
	return (val << 32) | ((val >> 32) & 0xFFFFFFFFULL);
}

U64 SwapU64(U64 val) {
	val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
	val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
	return (val << 32) | (val >> 32);
}


I32 UtilConnectTCPChannel(const C8 * Module, I32 * Sockfd, const C8 * IP, const U32 Port) {
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[256];
	int iResult;

	*Sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (*Sockfd < 0) {
		fprintf(stderr, "Failed to open control socket\n");
	}

	server = gethostbyname(IP);
	if (server == NULL) {
		fprintf(stderr, "Unknown host\n");
	}

	bzero((char *)&serv_addr, sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;

	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(Port);

	fprintf(stderr, "Attempting to connect to control socket: %s:%i\n", IP, Port);

	do {
		iResult = connect(*Sockfd, (struct sockaddr *)&serv_addr, sizeof (serv_addr));

		if (iResult < 0) {
			if (errno == ECONNREFUSED) {
				fprintf(stderr, "Unable to connect to %s port %d, retrying in 3 sec...\n", IP, Port);
				fflush(stdout);
				(void)sleep(3);
			}
			else {
				fprintf(stderr, "Failed to connect to control socket\n");
			}
		}
	} while (iResult < 0);


	iResult = fcntl(*Sockfd, F_SETFL, fcntl(*Sockfd, F_GETFL, 0) | O_NONBLOCK);
	fprintf(stderr, "ATOS connected to %s:%i\n", IP, Port);
	return iResult;

}


void UtilSendTCPData(const C8 * Module, const C8 * Data, I32 Length, const int *Sockfd, U8 Debug) {
	I32 i, n, error = 0;

	socklen_t len = sizeof (error);
	I32 retval;

	// TODO: Change this when bytes thingy has been implemented in logging
	if (Debug == 1) {
		printf("[%s] Bytes sent: ", Module);
		i = 0;
		for (i = 0; i < Length; i++)
			printf("%x-", (C8) * (Data + i));
		printf("\n");
	}

	n = write(*Sockfd, Data, Length);

	retval = getsockopt(*Sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

	if (retval != 0) {
		fprintf(stderr, "Failed to get socket error code = %s\n", strerror(retval));
	}

	if (error != 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(error));
	}

	if (n < 0) {
		fprintf(stderr, "Failed to send on control socket, length = %d\n", Length);
	}
}


I32 UtilReceiveTCPData(const C8 * Module, I32 * Sockfd, C8 * Data, I32 Length, U8 Debug) {
	I32 i, Result;

	if (Length <= 0)
		Result = recv(*Sockfd, Data, TCP_RX_BUFFER, MSG_DONTWAIT);
	else
		Result = recv(*Sockfd, Data, Length, MSG_DONTWAIT);

	if (Result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		Result = 0;
	}
	// TODO: Change this when bytes thingy has been implemented in logging
	if (Debug == 1 && Result < 0) {
		printf("[%s] Received TCP data: ", Module);
		i = 0;
		for (i = 0; i < Result; i++)
			printf("%x-", (C8) * (Data + i));
		printf("\n");
	}

	return Result;
}




void UtilCreateUDPChannel(const C8 * Module, I32 * Sockfd, const C8 * IP, const U32 Port,
						  struct sockaddr_in *Addr) {
	int result;
	struct hostent *object;


	*Sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (*Sockfd < 0) {
		fprintf(stderr, "Failed to create CPC socket\n");
	}

	/* Set address to object */
	object = gethostbyname(IP);

	if (object == 0) {
		fprintf(stderr, "Unknown host\n");
	}

	bcopy((char *)object->h_addr, (char *)&Addr->sin_addr.s_addr, object->h_length);
	Addr->sin_family = AF_INET;
	Addr->sin_port = htons(Port);

	/* set socket to non-blocking */
	result = fcntl(*Sockfd, F_SETFL, fcntl(*Sockfd, F_GETFL, 0) | O_NONBLOCK);
	if (result < 0) {
		fprintf(stderr, "Error calling fcntl\n");
	}
	fprintf(stderr, "Created UDP channel to address: %s:%d\n", IP, Port);


}


void UtilSendUDPData(const C8 * Module, I32 * Sockfd, struct sockaddr_in *Addr, C8 * Data, size_t Length,
					 U8 Debug) {
	ssize_t result;

	result = sendto(*Sockfd, Data, Length, 0, (struct sockaddr *)Addr, sizeof (struct sockaddr_in));

	// TODO: Change this when bytes thingy has been implemented in logging
	if (Debug) {
		printf("[%s] Bytes sent: ", Module);
		for (size_t i = 0; i < Length; i++)
			printf("%x-", (unsigned char)*(Data + i));
		printf("\n");
	}

	if (result < 0) {
		fprintf(stderr, "Failed to send on process control socket\n");
	}

}


void UtilReceiveUDPData(const C8 * Module, I32 * Sockfd, C8 * Data, I32 Length, I32 * ReceivedNewData,
						U8 Debug) {
	I32 Result, i;

	*ReceivedNewData = 0;
	do {
		Result = recv(*Sockfd, Data, Length, 0);

		if (Result < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				fprintf(stderr, "Failed to receive from monitor socket\n");
			}
			else {
				fprintf(stderr, "No data received\n");
			}
		}
		else {
			*ReceivedNewData = 1;
			fprintf(stderr, "Received UDP data: %s\n", Data);
			// TODO: Change this when bytes thingy has been implemented in logging
			if (Debug == 1) {
				fprintf(stderr,"[%s] Received UDP data: ", Module);
				i = 0;
				for (i = 0; i < Result; i++)
					fprintf(stderr,"%x-", (C8) * (Data + i));
				fprintf(stderr,"\n");
			}

		}

	} while (Result > 0);
}


U32 UtilIPStringToInt(C8 * IP) {
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


U32 UtilHexTextToBinary(U32 DataLength, C8 * Text, C8 * Binary, U8 Debug) {
	U32 i, j = 0;
	C8 Bin;
	C8 Hex;

	for (i = 0; i < DataLength;) {

		Hex = *(Text + i++);
		if (Hex >= 0x30 && Hex <= 0x39)
			Hex = Hex - 0x30;
		else if (Hex >= 0x41 && Hex <= 0x46)
			Hex = Hex - 0x37;
		Bin = Hex << 4;

		Hex = *(Text + i++);
		if (Hex >= 0x30 && Hex <= 0x39)
			Hex = Hex - 0x30;
		else if (Hex >= 0x41 && Hex <= 0x46)
			Hex = Hex - 0x37;
		Bin = Bin | Hex;

		*(Binary + j++) = Bin;

	}

	if (Debug) {
		// TODO: Change this when bytes thingy has been implemented in logging
		printf("[Util:UtilHexTextToBinary] Length = %d: ", DataLength / 2);
		for (i = 0; i < DataLength / 2; i++)
			printf("%x ", *(Binary + i));
		printf("\n");
	}


	return j;
}


U32 UtilBinaryToHexText(U32 DataLength, C8 * Binary, C8 * Text, U8 Debug) {
	U32 i, j = 0;
	C8 Hex;


	for (i = 0; i < DataLength; i++) {
		Hex = *(Binary + i) >> 4;
		//Hex = Hex >> 4;
		//printf("%x\n", Hex);
		if (Hex >= 0 && Hex <= 9)
			Hex = Hex + 0x30;
		else if (Hex >= 10 && Hex <= 15)
			Hex = Hex + 0x37;
		//printf("%x\n", Hex);
		*(Text + j++) = Hex;

		Hex = *(Binary + i) & 0x0F;
		//printf("%x\n", Hex);
		//Hex = Hex & 0x0F;
		//printf("%x\n", Hex);
		if (Hex >= 0 && Hex <= 9)
			Hex = Hex + 0x30;
		else if (Hex >= 10 && Hex <= 15)
			Hex = Hex + 0x37;
		//printf("%x", Hex);
		*(Text + j++) = Hex;
	}


	if (Debug) {
		// TODO: Change this when bytes thingy has been implemented in logging
		printf("[Util:UtilBinaryToHexText] Length = %d: ", j);
		for (i = 0; i < j; i++)
			printf("%x ", *(Text + i));
		printf("\n");
	}



	return j;
}


#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define BLUE  "\x1B[34m"

// "F-<filename>\n" and "D-<filename>\n" is 4 bytes longer than only the filename
#define FILE_INFO_LENGTH (MAX_PATH_LENGTH+4)

U32 UtilCreateDirContent(C8 * DirPath, C8 * TempPath) {

	FILE *fd;
	C8 Filename[FILE_INFO_LENGTH];
	C8 CompletePath[MAX_PATH_LENGTH];

	bzero(CompletePath, MAX_PATH_LENGTH);
	UtilGetTestDirectoryPath(CompletePath, sizeof (CompletePath));
	strcat(CompletePath, DirPath);	//Concatenate dir path

	DIR *d = opendir(CompletePath);	// open the path

	if (d == NULL)
		return 1;				// if was not able return
	struct dirent *dir;			// for the directory entries

	bzero(CompletePath, MAX_PATH_LENGTH);
	UtilGetTestDirectoryPath(CompletePath, sizeof (CompletePath));
	strcat(CompletePath, TempPath);	//Concatenate temp file path

	fd = fopen(CompletePath, "r");
	if (fd != NULL) {
		fclose(fd);
		remove(CompletePath);	//Remove file if exist
	}

	fd = fopen(CompletePath, "w+");	//Create the file
	if (fd == NULL)				//return if failing to create file
	{
		return 2;
	}

	while ((dir = readdir(d)) != NULL)	// if we were able to read somehting from the directory
	{
		bzero(Filename, FILE_INFO_LENGTH);

		if (dir->d_type != DT_DIR)
			//printf("%s%s\n",BLUE, dir->d_name); // if the type is not directory just print it with blue
			sprintf(Filename, "F-%s\n", dir->d_name);
		else if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)	// if it is a directory
		{
			sprintf(Filename, "D-%s\n", dir->d_name);
			//printf("%s%s\n", GREEN, dir->d_name); // print its name in green
			//printf("D-%s\n", dir->d_name); // print its name in green
			//char d_path[255]; // here I am using sprintf which is safer than strcat
			//sprintf(d_path, "%s/%s", Path, dir->d_name);
			//UtilCreateDirContent(d_path, TempPath); // recall with the new path
		}

		if (strlen(Filename) > 0) {
			//printf("%s", Filename);
			fwrite(Filename, 1, strlen(Filename), fd);	//write dir content to file
			fflush(fd);
		}
	}
	//printf("\n");
	//printf("%s\n", CompletePath);

	closedir(d);				// close the directory

	fclose(fd);					//close the file

	return 0;
}


U16 UtilGetMillisecond(TimeType * GPSTime) {
	struct timeval now;
	U16 MilliU16 = 0, NowU16 = 0;

	gettimeofday(&now, NULL);
	NowU16 = (U16) (now.tv_usec / 1000);

	if (NowU16 >= GPSTime->LocalMillisecondU16)
		MilliU16 = NowU16 - GPSTime->LocalMillisecondU16;
	else if (NowU16 < GPSTime->LocalMillisecondU16)
		MilliU16 = 1000 - GPSTime->LocalMillisecondU16 + NowU16;

	//printf("Result= %d, now= %d, local= %d \n", MilliU16, NowU16, GPSTime->LocalMillisecondU16);
	return MilliU16;
}

/*!
 * \brief UtilGetObjectFileSetting Gets the specified setting from an object file
 * \param setting Setting to get
 * \param objectFilePath File containing the setting
 * \param filePathLength Length of the file path variable
 * \param objectSetting Output variable
 * \param objectSettingSize Size of output variable
 * \return 0 if successful, -1 otherwise
 */
int UtilGetObjectFileSetting(const enum ObjectFileParameter setting, const char *objectFilePath,
							 const size_t filePathLength, char *objectSetting,
							 const size_t objectSettingSize) {
	char textBuffer[SMALL_BUFFER_SIZE_128];

	UtilGetObjectParameterAsString(setting, textBuffer, sizeof (textBuffer));
	strcat(textBuffer, "=");

	memset(objectSetting, 0, objectSettingSize);
	UtilSearchTextFile(objectFilePath, textBuffer, "", objectSetting);

	fprintf(stderr, "Read object parameter: %s%s\n", textBuffer, objectSetting);

	return objectSetting[0] == '\0' ? -1 : 0;
}

/*!
 * \brief UtilGetObjectParameterAsString Converts an enumerated value to its string representation,
 *			or an empty string if an invalid parameter was requested.
 * \param parameter
 * \param returnValue
 * \param bufferLength
 * \return Pointer to the string
 */
char *UtilGetObjectParameterAsString(const enum ObjectFileParameter parameter,
									 char *returnValue, const size_t bufferLength) {
	const char *outputString = NULL;

	switch (parameter) {
	case OBJECT_SETTING_ID:
		outputString = ObjectSettingNameID;
		break;
	case OBJECT_SETTING_IP:
		outputString = ObjectSettingNameIP;
		break;
	case OBJECT_SETTING_TRAJ:
		outputString = ObjectSettingNameTraj;
		break;
	case OBJECT_SETTING_OPENDRIVE:
		outputString = ObjectSettingNameOpendrive;
		break;
	case OBJECT_SETTING_OPENSCENARIO:
		outputString = ObjectSettingNameOpenscenario;
		break;	
	case OBJECT_SETTING_IS_ANCHOR:
		outputString = ObjectSettingNameIsAnchor;
		break;
	case OBJECT_SETTING_INJECTOR_IDS:
		outputString = ObjectSettingNameInjectorIDs;
		break;
	case OBJECT_SETTING_ORIGIN_LATITUDE:
		outputString = ObjectSettingNameLatitude;
		break;
	case OBJECT_SETTING_ORIGIN_LONGITUDE:
		outputString = ObjectSettingNameLongitude;
		break;
	case OBJECT_SETTING_ORIGIN_ALTITUDE:
		outputString = ObjectSettingNameAltitude;
		break;
	case OBJECT_SETTING_IS_OSI_COMPATIBLE:
		outputString = ObjectSettingNameIsOsiCompatible;
		break;
	case OBJECT_SETTING_TURNING_DIAMETER:
		outputString = ObjectSettingNameTurningDiameter;
		break;
	case OBJECT_SETTING_MAX_SPEED:
		outputString = ObjectSettingNameMaxSpeed;
		break;
	default:
		fprintf(stderr, "No matching configuration parameter for enumerated input\n");
		outputString = "";
	}

	if (strlen(outputString) + 1 > bufferLength) {
		fprintf(stderr, "Buffer too small to hold object setting parameter name\n");
		returnValue = "";
	}
	else {
		strncpy(returnValue, outputString, bufferLength);
	}
	return returnValue;
}

int UtilReadOriginConfiguration(GeoPositionType* origin) {
	GeoPositionType retval;
	char setting[20];
	char* endptr;
	// if (UtilReadConfigurationParameter(CONFIGURATION_PARAMETER_ORIGIN_LATITUDE,
	// 								   setting, sizeof (setting))) {
	// 	retval.Latitude = strtod(setting, &endptr);
	// 	if (endptr == setting) {
	// 		return -1;
	// 	}
	// }
	// if (UtilReadConfigurationParameter(CONFIGURATION_PARAMETER_ORIGIN_LONGITUDE,
	// 								   setting, sizeof (setting))) {
	// 	retval.Longitude = strtod(setting, &endptr);
	// 	if (endptr == setting) {
	// 		return -1;
	// 	}
	// }
	// if (UtilReadConfigurationParameter(CONFIGURATION_PARAMETER_ORIGIN_ALTITUDE,
	// 								   setting, sizeof (setting))) {
	// 	retval.Altitude = strtod(setting, &endptr);
	// 	if (endptr == setting) {
	// 		return -1;
	// 	}
	// }
	retval.Heading = 0; // TODO
	*origin = retval;
	return 0;
}

/*!
 * \brief UtilPopulateMonitorDataStruct Takes an array of raw monitor data and fills a monitor data struct with the content
 * \param rawData Array of raw monitor data
 * \param rawDataSize Size of raw monitor data array
 * \param MONR Struct where monitor data should be placed
 * \return -1 on failure, 0 on success
 */
int UtilPopulateMonitorDataStruct(const char *rawData, const size_t rawDataSize, ObjectDataType * monitorData) {

	if (rawDataSize != sizeof (ObjectDataType)) {
		errno = EMSGSIZE;
		fprintf(stderr, "Raw monitor data array wrong size, %d != %d\n",
				rawDataSize, sizeof (ObjectDataType));
		return -1;
	}

	memcpy(monitorData, rawData, rawDataSize);

	return 0;
}

/*!
 * \brief UtilPopulateTREODataStructFromMQ Fills TREO data struct with COMM_TREO MQ message contents
 * \param rawTREO MQ data
 * \param rawTREOsize size of MQ data
 * \param treoData Data struct to be filled
 */
int UtilPopulateTREODataStructFromMQ(char * rawTREO, size_t rawTREOsize, TREOData * treoData) {
	char *rdPtr = rawTREO;

	if (rawTREOsize < sizeof (TREOData)) {
		fprintf(stderr, "Raw TREO array too small to hold all necessary TREO data\n");
		return -1;
	}

	memcpy(&treoData->triggerID, rdPtr, sizeof (treoData->triggerID));
	rdPtr += sizeof (treoData->triggerID);

	memcpy(&treoData->timestamp_qmsow, rdPtr, sizeof (treoData->timestamp_qmsow));
	rdPtr += sizeof (treoData->timestamp_qmsow);

	memcpy(&treoData->ip, rdPtr, sizeof (treoData->ip));
	rdPtr += sizeof (treoData->ip);

	return 0;
}

/*!
 * \brief UtilPopulateEXACDataStructFromMQ Fills EXAC data struct with COMM_EXAC MQ message contents
 * \param rawEXAC MQ data
 * \param rawEXACsize size of MQ data
 * \param exacData Data struct to be filled
 */
int UtilPopulateEXACDataStructFromMQ(char * rawEXAC, size_t rawEXACsize, EXACData * exacData) {
	char *rdPtr = rawEXAC;

	if (rawEXACsize < sizeof (EXACData)) {
		fprintf(stderr, "Raw EXAC array too small to hold all necessary EXAC data\n");
		return -1;
	}

	memcpy(&exacData->actionID, rdPtr, sizeof (exacData->actionID));
	rdPtr += sizeof (exacData->actionID);

	memcpy(&exacData->executionTime_qmsoW, rdPtr, sizeof (exacData->executionTime_qmsoW));
	rdPtr += sizeof (exacData->executionTime_qmsoW);

	memcpy(&exacData->ip, rdPtr, sizeof (exacData->ip));
	rdPtr += sizeof (exacData->ip);

	return 0;
}

/*!
 * \brief UtilPopulateTRCMDataStructFromMQ Fills TRCM data struct with COMM_TRCM MQ message contents
 * \param rawTRCM MQ data
 * \param rawTRCMsize size of MQ data
 * \param trcmData Data struct to be filled
 */
int UtilPopulateTRCMDataStructFromMQ(char * rawTRCM, size_t rawTRCMsize, TRCMData * trcmData) {
	char *rdPtr = rawTRCM;

	if (rawTRCMsize < sizeof (TRCMData)) {
		fprintf(stderr, "Raw TRCM array too small to hold all necessary TRCM data\n");
		return -1;
	}

	memcpy(&trcmData->triggerID, rdPtr, sizeof (trcmData->triggerID));
	rdPtr += sizeof (trcmData->triggerID);

	memcpy(&trcmData->triggerType, rdPtr, sizeof (trcmData->triggerType));
	rdPtr += sizeof (trcmData->triggerType);

	memcpy(&trcmData->triggerTypeParameter1, rdPtr, sizeof (trcmData->triggerTypeParameter1));
	rdPtr += sizeof (trcmData->triggerTypeParameter1);

	memcpy(&trcmData->triggerTypeParameter2, rdPtr, sizeof (trcmData->triggerTypeParameter2));
	rdPtr += sizeof (trcmData->triggerTypeParameter2);

	memcpy(&trcmData->triggerTypeParameter3, rdPtr, sizeof (trcmData->triggerTypeParameter3));
	rdPtr += sizeof (trcmData->triggerTypeParameter3);

	memcpy(&trcmData->ip, rdPtr, sizeof (trcmData->ip));
	rdPtr += sizeof (trcmData->ip);

	return 0;
}

/*!
 * \brief UtilPopulateACCMDataStructFromMQ Fills ACCM data struct with COMM_ACCM MQ message contents
 * \param rawACCM MQ data
 * \param rawACCMsize size of MQ data
 * \param accmData Data struct to be filled
 */
int UtilPopulateACCMDataStructFromMQ(char * rawACCM, size_t rawACCMsize, ACCMData * accmData) {
	char *rdPtr = rawACCM;

	if (rawACCMsize < sizeof (ACCMData)) {
		fprintf(stderr, "Raw ACCM array too small to hold all necessary ACCM data\n");
		return -1;
	}

	memcpy(&accmData->actionID, rdPtr, sizeof (accmData->actionID));
	rdPtr += sizeof (accmData->actionID);

	memcpy(&accmData->actionType, rdPtr, sizeof (accmData->actionType));
	rdPtr += sizeof (accmData->actionType);

	memcpy(&accmData->actionTypeParameter1, rdPtr, sizeof (accmData->actionTypeParameter1));
	rdPtr += sizeof (accmData->actionTypeParameter1);

	memcpy(&accmData->actionTypeParameter2, rdPtr, sizeof (accmData->actionTypeParameter2));
	rdPtr += sizeof (accmData->actionTypeParameter2);

	memcpy(&accmData->actionTypeParameter3, rdPtr, sizeof (accmData->actionTypeParameter3));
	rdPtr += sizeof (accmData->actionTypeParameter3);

	memcpy(&accmData->ip, rdPtr, sizeof (accmData->ip));
	rdPtr += sizeof (accmData->ip);

	return 0;
}


/*!
 * \brief getPIDuptime reads the proc file of the pid in question and returns start time of the process after boot. In kernels before Linux 2.6, this value was expressed in
jiffies. Since Linux 2.6, the value is expressed in
clock ticks (divide by sysconf(_SC_CLK_TCK)).
 * \param pid the pid in question.
 * \return The time the process started after system boot.
 */
struct timeval UtilGetPIDUptime(pid_t pID) {
	FILE *pidstat = NULL;
	struct timeval timeSinceStart;
	char filename[PATH_MAX] = { 0 };
	snprintf(filename, sizeof (filename), "/proc/%d/stat", pID);

	pidstat = fopen(filename, "r");
	if (pidstat == NULL) {
		fprintf(stderr, "Error: Couldn't open [%s]\n", filename);
		timeSinceStart.tv_sec = -1;
		timeSinceStart.tv_usec = -1;
		return timeSinceStart;
	}

	char strval1[255] = { 0 };
	fgets(strval1, 255, pidstat);

	fclose(pidstat);
	// Get start time from proc/pid/stat
	char *token = strtok(strval1, " ");
	int loopCounter = 0;

	char uptime[247];

	while (token != NULL) {
		if (loopCounter == startTime) {	//Get  starttime  %llu from proc file for pid.
			sprintf(uptime, "%s", token);
		}
		token = strtok(NULL, " ");
		loopCounter++;
	}


	timeSinceStart.tv_sec = (strtoul(uptime, NULL, 10));
	return timeSinceStart;
}


/*!
 * \brief UtilgetDistance calculates the distance between two log lat positions usgin haversine formula.
 * \param lat1 Latitude of first coordinate
 * \param log1 Latitude of first coordinate
 * \param lat2 Longitude of second coordinate
 * \param log2 Longitude of second coordinate
 * \return Distance in km
 */
double UtilGetDistance(double th1, double ph1, double th2, double ph2) {
	int nRadius = 6371;			// Earth's radius in Kilometers

	printf("th1: %f \n", th1);
	printf("ph1: %f \n", ph1);

	printf("th2: %f \n", th2);
	printf("ph2: %f \n", ph2);

	// Get the difference between our two points
	// then convert the difference into radians
	double dx, dy, dz;

	ph1 -= ph2;
	ph1 *= (3.1415926536 / 180), th1 *= (3.1415926536 / 180), th2 *= (3.1415926536 / 180);

	dz = sin(th1) - sin(th2);
	dx = cos(ph1) * cos(th1) - cos(th2);
	dy = sin(ph1) * cos(th1);
	return asin(sqrt(dx * dx + dy * dy + dz * dz) / 2) * 2 * nRadius;
}

/*!
 * \brief UtilCoordinateDistance calculates the distance between two cordinates using distance formula.
 * \param x1 x of first coordinate
 * \param y1 y of first coordinate
 * \param z1 z of first coordinate
 * \param x2 x of second coordinate
 * \param y2 y of second coordinate
 * \param z2 z of second coordinate
 * \return Distance
 */
float UtilCoordinateDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) + pow(z2 - z1, 2));
}
