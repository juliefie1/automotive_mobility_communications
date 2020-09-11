// -------------------------------------------------------------------------- //
//! @file   kineis_demo.c
//! @brief  how to integrate KIM1 and satelitte pass prediction libraries
//!         on mangOH red HW
//!
//! Refer to MAIN-APP detailled description to get information about the design
//! of this application code. The most import functions are:
//! * main
//! * argos_protocol_timer_handler
//!
//! @author Kinéis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //


// -------------------------------------------------------------------------- //
//! @addtogroup MAIN-APP
//! @brief  how to integrate KIM1 and satelitte pass prediction libraries
//!         on mangOH red HW
//!
//! Design is:
//! * get current time through one GPS acquisition
//! * compute next satellite pass
//! * schedule a timer to starting time of next pass. This timer is used as ARGOS protocol
//!   scheduler.
//! * once pass is started:
//!     * get GPS position and time
//!     * transmit position to KINEIS network
//!     * check next transmit (in TX_INTERVAL seconds) will be in satellite pass:
//!     * if yes, reprogram timer to next interval
//!     * if not, compute next satellite pass and program timer to it
//!
//! @{
// -------------------------------------------------------------------------- //

#define DEBUG 1

#include "legato.h"
#include "interfaces.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>
#include "previpass.h"
#include "mangOH_Kim1.h"
#include "gpslib.h"
#include "argosPublisher.h"

#ifdef DEBUG
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT(...)
#endif

#define MODEM_ENABLE            1
#define TX_INTERVAL             90      //!< time (seconds) between 2 TX during satellite pass
#define DEFAULT_POWER           1000    //!< transmit power (mW) over KINEIS network
#define DEFAULT_FREQ_BAND       1       //!< default ARGOS 2 band
#define DEFAULT_FREQ_OFFSET     0       //!< default frequency offset (kHz) for ARGOS 2 transmit
#define GPS_FRAME_LENGTH        11      //!< length of an ARGOS frame containing GPS data (bytes)

#define NUM_ELEMS(a) (sizeof(a)/sizeof(a[0]))

//! Use local AOP table
struct AopSatelliteEntry_t aopTable[] = {
		{ 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020,  3, 26, 22, 17, 58 },
				7195.543f,  98.5333f,  337.191f,  -25.341f,  101.3586f,   0.00f },
		{ 0x9, 3, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, { 2020,  3, 26, 23, 33, 23 },
				7195.595f,  98.7011f,  329.290f,  -25.340f,  101.3592f,   0.00f },
		{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020,  3, 26, 22, 45, 19 },
				7195.624f,  98.7079f,  341.538f,  -25.340f,  101.3598f,   0.00f },
		{ 0x5, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, { 2020,  3, 26, 22,  0, 17 },
				7180.518f,  98.7247f,  317.478f,  -25.259f,  101.0413f,  -1.78f },
		{ 0x8, 0, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A2, { 2020,  3, 26, 22,  9, 30 },
				7226.140f,  99.0541f,  345.355f,  -25.499f,  102.0034f,  -1.80 },
		{ 0xC, 6, SAT_DNLK_OFF,        SAT_UPLK_ON_WITH_A3, { 2020,  3, 26, 23, 48, 50 },
				7226.486f,  99.1948f,  268.101f,  -25.500f,  102.0103f,  -1.98f },
		{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020,  3, 26, 22, 32,  0 },
				7160.258f,  98.5403f,  110.937f,  -25.154f,  100.6151f,   0.00f }
};
uint8_t nbSatsInAopTable = NUM_ELEMS(aopTable);

//! Initial Prediction Pass configuration
struct PredictionPassConfiguration_t prepasConfiguration = {
	43.5497f,                     //!< Latitude of the beacon (deg.)
	1.485f,                       //!< East longitude of the beacon (deg.) [0, 360]
	{ 2020, 01, 27, 00, 00, 00 }, //!< Prediction start date (Y/M/D, hh:mm:ss)
	{ 2020, 01, 28, 00, 00, 00 }, //!< Prediction end date (Y/M/D, hh:mm:ss)
	5.0f,                         //!< Minimum site required (deg.) [0, 90]
	90.0f,                        //!< Maximum site required (deg.) [0, 90]
					//!< [site_max >= site_min]
	5.0f,                         //!< Minimum duration of a pass (minutes)
	2400,                         //!< Max number of satellite passes
	5,                            //!< Linear time margin (in minutes/6months)
	30                            //!< Step in sec
};

struct SatelliteNextPassPrediction_t satPass;
//! satPass refers to
//! 1) CurrentPass if currently, a satellite is available in visibility period,
//! 2) NextPass if currently, no satellite is available in visibility period
int fd_kineis;
uint8_t gpsframe[GPS_FRAME_LENGTH];
// -------------------------------------------------------------------------- //
//! Convert satellite code to two chars name.
// -------------------------------------------------------------------------- //
static void
	getSatName
(
	uint8_t satHexId,
	char _satNameTwoChars[]
)
{
	switch (satHexId) {
	case 0x6:
		strcpy(_satNameTwoChars, "A1");
		break;
	case 0xA:
		strcpy(_satNameTwoChars, "MA");
		break;
	case 0x9:
		strcpy(_satNameTwoChars, "MB");
		break;
	case 0xB:
		strcpy(_satNameTwoChars, "MC");
		break;
	case 0x5:
		strcpy(_satNameTwoChars, "NK");
		break;
	case 0x8:
		strcpy(_satNameTwoChars, "NN");
		break;
	case 0xC:
		strcpy(_satNameTwoChars, "NP");
		break;
	case 0xD:
		strcpy(_satNameTwoChars, "SR");
		break;
	default:
		strcpy(_satNameTwoChars, "XX");
	}
}

// -------------------------------------------------------------------------- //
//! @brief Display one pass
// -------------------------------------------------------------------------- //
static void
	writeOnePass
(
	struct SatelliteNextPassPrediction_t *_nextPass
)
{
	struct CalendarDateTime_t __ps_viewable_timedata;

	PREVIPASS_UTIL_date_stu90_calendar(_nextPass->epoch - EPOCH_90_TO_70_OFFSET,
			&__ps_viewable_timedata);

	//! Sat name
	char satNameTwoChars[3];

	getSatName(_nextPass->satHexId, satNameTwoChars);

	//! Display one pass
	printf(
			"sat %2s date begin %4hu/%02hhu/%02hu %02hhu:%02hhu:%02hhu duration (min) %f site Max (deg)  %i\n",
			satNameTwoChars,
			__ps_viewable_timedata.year,
			__ps_viewable_timedata.month,
			__ps_viewable_timedata.day,
			__ps_viewable_timedata.hour,
			__ps_viewable_timedata.minute,
			__ps_viewable_timedata.second,
			_nextPass->duration/60.,
			_nextPass->elevationMax);
}

//! @brief  binary uint8_t[] to hex-string (char[]) convert function.
//! @retval void
static void uint2hexString(uint8_t *input, uint8_t length, char *output)
{
	int idx = 0;

	idx = 0;

	for (idx = 0; idx < length; idx++)
		sprintf((char *)(output + idx * 2), "%02X", input[idx]);
	//! insert NULL at the end of the output string
	output[length * 2] = '\0';
}

//! @brief  crc16 ccitt function.
//! @retval crc16
unsigned short crc16(char *ptr, int count)
{
	int  crc;
	char i;

	crc = 0;
	while (--count >= 0) {
		crc = crc ^ (int) *ptr++ << 8;
		i = 8;
		do {
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
		} while (--i);
	}
	return (crc);
}

// -------------------------------------------------------------------------- //
//! @brief ARGOS protocol scheduler main function
//!
//! This function is actually a timer handler. The ARGOS protocol scheduler is
//! timer-based. It will be called each time an ARGOS message needs to be transmitted
//! on KINEIS network.
//!
//! Design is:
//! * get GPS position and time
//! * power ON KIM1
//! * transmit position to KINEIS network
//! * power OFF KIM1
//! * check next transmit (in TX_INTERVAL seconds) is still in a satellite pass:
//! * if yes, reprogram timer to next TX_INTERVAL
//! * if not, compute next satellite pass and program timer to the beginning of this new pass
//!
//! @param[in]  signum not used
//!
//! @returns void
// -------------------------------------------------------------------------- //
void argos_protocol_timer_handler(int signum)
{
	struct itimerval timer;
	time_t tnow;
	struct tm *local_tnow;
	unsigned long nxtTimerHandlerExec = 0;
	int j = 0;
	uint16_t crc = 0;
	char temp[256];
	float beacon_alt; //! Used Only to call gpslib APIs correctely

	//! Get position and current time from GPS receiver
	if (gps_pos(&(prepasConfiguration.beaconLatitude),
			&(prepasConfiguration.beaconLongitude),
			&beacon_alt,
			&(prepasConfiguration.start.year),
			&(prepasConfiguration.start.month),
			&(prepasConfiguration.start.day),
			&(prepasConfiguration.start.hour),
			&(prepasConfiguration.start.minute),
			&(prepasConfiguration.start.second))) {
		printf("[LOG_WARNING] issue when running GPS !!!\n");
		printf("[LOG_WARNING] GPS coordinates may not be the latest correct ones !!!\n");
		printf("[LOG_WARNING] Please check gps receiver\n");

		//! Get current local time if GPS acquisition failed
		time(&tnow);
		local_tnow = localtime(&tnow);
		prepasConfiguration.start.year = local_tnow->tm_year + 1900;
		prepasConfiguration.start.month = local_tnow->tm_mon + 1;
		prepasConfiguration.start.day = local_tnow->tm_mday;
		prepasConfiguration.start.hour = local_tnow->tm_hour;
		prepasConfiguration.start.minute = local_tnow->tm_min;
		prepasConfiguration.start.second = local_tnow->tm_sec;
	}

	//! SEND DATA and Parse response
	if (!gpsframe_parser(prepasConfiguration.start.day,
				prepasConfiguration.start.hour,
				prepasConfiguration.start.minute,
				prepasConfiguration.beaconLongitude,
				prepasConfiguration.beaconLatitude,
				beacon_alt,
				&gpsframe[0])) {
		DEBUG_PRINT("frame without CRC:\n");
		for (j = 0; j < GPS_FRAME_LENGTH; j++)
			DEBUG_PRINT("%02X", gpsframe[j]);
		printf("\n");

		//! compute crc
		crc = (uint16_t) crc16((char *) &gpsframe[2], 9);
		DEBUG_PRINT("crc %04X\n", crc);

		//! add crc16 to the frame
		gpsframe[0] = (crc & 0xFF00) >> 8;
		gpsframe[1] = crc & 0xFF;

		DEBUG_PRINT("frame with CRC:");
		for (j = 0; j < GPS_FRAME_LENGTH; j++)
			DEBUG_PRINT("%02X", gpsframe[j]);
		printf("\n");
	}
	uint2hexString(&gpsframe[0], GPS_FRAME_LENGTH, &temp[0]);
	DEBUG_PRINT("After string2HexString call %s\n", temp);
	DEBUG_PRINT("length of temp: %d\n", strlen((const char *) temp));

#if MODEM_ENABLE
	//! Initialise serial port wired to Kineis modem
	printf("[DEBUG_LOG] Open /dev/ttyHS0 and set serial port parameters\n");
	fd_kineis = mangOH_kim_open(NULL);
	if (fd_kineis != -1) {
		//! Send message
		if (mangOH_kim_uart_tx_data(fd_kineis, &temp[0])) {
			time(&tnow);
			printf(">> Frame transmission at %s (PASS)\n", ctime(&tnow));
		} else {
			time(&tnow);
			printf(">> Frame transmission at %s (FAIL) !!!\n", ctime(&tnow));
		}

		//! Close KIM KIM1 fd
		printf("[DEBUG_LOG] Close /dev/ttyHS0\n");
		mangOH_kim_close(fd_kineis);
	} else {
		printf("[LOG_ERROR] Open /dev/ttyHS0 FAILED, skip frame transmission\n");
	}
#endif

	//! Check current Satellite status
	//! Get timenow
	tnow = time(NULL);
	if (difftime(tnow, satPass.epoch) < satPass.duration) {
		//! Current satellite is in visibilty period
		nxtTimerHandlerExec = TX_INTERVAL;
	} else {
		//! Current satellite is no more in visibilty period
		//! So compute next Pass
		if (!PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
				aopTable,
				nbSatsInAopTable,
				SAT_DNLK_OFF,		//! Kim1 does not support reception
				SAT_UPLK_ON_WITH_A2,	//! Kim1 only support Argos-2 transmission
				&satPass)) {
			printf("[LOG_ERROR] Failed to compute next pass\n");
			return;
		}

		writeOnePass(&satPass);

		//! get latest date before starting timer
		tnow = time(NULL);
		printf("t_now = %s\n", ctime(&tnow));
                printf("t_now = %ld\n", tnow);
                printf("satPass.epoch = %d\n", satPass.epoch);

		//! Compare timeprediction with tnow and configure nxtTimerHandlerExec
		nxtTimerHandlerExec = (unsigned long) difftime(satPass.epoch, tnow);
		printf("diff between nextPass and now = %lu\n", nxtTimerHandlerExec);
		if (nxtTimerHandlerExec <= 0) {
			//! Is already in next pass
			if (difftime(tnow, satPass.epoch) >= satPass.duration) {
				printf("[LOG_ERROR] It's too late, you missed the satellite...\n");
				printf("[LOG_ERROR] Data transmission skipped\n");
				return;
			}
			/** The new satelite is currently in visibility period
			 * In order to protect the kim1 module from two successive SendData
			 * affect TX_INTERVAL to delta_t
			 */
			nxtTimerHandlerExec = TX_INTERVAL;
		}
	}

	//! Configure the timer to expire after nxtTimerHandlerExec (for one shot)
	timer.it_value.tv_sec = nxtTimerHandlerExec;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

#ifdef MODEM_ENABLE
// -------------------------------------------------------------------------- //
//! @brief configure KIM module for TX on KINEIS network with ARGOS 2 protocol
//! @returns error status (true: OK, false: FAILED to configure)
// -------------------------------------------------------------------------- //
static bool mangOH_kim_set_tx_cfg(int fd)
{
	char *rxbuffer;
	char ret[64];

	//! Init power to 1000mW
	//! Call API function to set tx power
	rxbuffer = mangOH_kim_set_pwr(fd, DEFAULT_POWER);
	if (rxbuffer == NULL)
		return false;
	strcpy(ret, rxbuffer);
	if (!strcmp(ret, "OK\n\n"))
		printf("[DEBUG_LOG] Setting PWR with success (value : %d mW)\n", DEFAULT_POWER);
	else {
		printf("[ERROR_LOG] Fail to set PWR value\n");
		return false;
	}

	//! Init frequency band to 1
	//! Call API function to set frequency band
	rxbuffer = mangOH_kim_set_band(fd, DEFAULT_FREQ_BAND);
	if (rxbuffer == NULL)
		return false;

	strcpy(ret, rxbuffer);
	if (!strcmp(ret, "OK\n\n"))
		printf("[DEBUG_LOG] Setting FREQUENCY_BAND with success (value : B%d)\n",
			DEFAULT_FREQ_BAND);
	else {
		printf("[ERROR_LOG] Fail to set FREQUENCY_BAND value\n");
		return false;
	}

	//! Init frequency to 401620 Hz
	//! Call API function to set frequency
	rxbuffer = mangOH_kim_set_freq(fd, DEFAULT_FREQ_OFFSET, DEFAULT_FREQ_BAND);
	if (rxbuffer == NULL)
		return EXIT_FAILURE;

	strcpy(ret, rxbuffer);
	if (!strcmp(ret, "OK\n\n"))
		printf("[DEBUG_LOG] Setting FREQUENCY OFFSET with success (value : %d kHz)\n",
			DEFAULT_FREQ_OFFSET);
	else {
		printf("[ERROR_LOG] Fail to set FREQUENCY value\n");
		return false;
	}

	return true;
}
#endif

// -------------------------------------------------------------------------- //
//! @brief main function of the example integrating KIM library and satellite pass
//! prediction library
//!
//! Design is:
//! * get current time through one GPS acquisition
//! * compute next satellite pass
//! * schedule a timer to starting time of next pass
//!
//! @param[in]  argc not used
//! @param[in]  argv not used
//!
//! @returns error status (0: OK, 1 FAIL)
// -------------------------------------------------------------------------- //
LE_SHARED int argos_publisher (void)
{
	struct itimerval timer;
	float beacon_alt; //! Used Only to call gpslib APIs correctely
	unsigned long nxtTimerHandlerExec;
	int j = 0;
	uint16_t crc = 0;
	char temp[256];
	//! timenow
	time_t tnow = time(NULL);
	int try = 0 ;
	char cmd[30];
	FILE *output = NULL;

        LE_INFO("Sending data through KIM1 IoT Card to ArgosWeb");

	//! Init beacon_lat, beacon_long and beacon_alt with gps coordinates
	if (!gps_pos(&(prepasConfiguration.beaconLatitude),
			&(prepasConfiguration.beaconLongitude),
			&beacon_alt,
			&(prepasConfiguration.start.year),
			&(prepasConfiguration.start.month),
			&(prepasConfiguration.start.day),
			&(prepasConfiguration.start.hour),
			&(prepasConfiguration.start.minute),
			&(prepasConfiguration.start.second))) {
		//! Update System date
		snprintf(cmd, sizeof(cmd), "date %02d%02d%02d%02d%04d",
				prepasConfiguration.start.month,
				prepasConfiguration.start.day,
				prepasConfiguration.start.hour,
				prepasConfiguration.start.minute,
				prepasConfiguration.start.year);
		output = popen(cmd, "r");
		if (output == NULL)
			fprintf(stderr, "Erreur popen %d\n", errno);

		pclose(output);
	} else {
		printf("1st GPS coordinates unavailable !!!\n");
		printf("It is impossible to update the UTC date of the module !!!\n");
		printf("Please check gps receiver and restart the application.\n");
	}
 
        LE_INFO("Point1");

        //! SEND DATA and Parse response
	if (!gpsframe_parser(prepasConfiguration.start.day,
				prepasConfiguration.start.hour,
				prepasConfiguration.start.minute,
				prepasConfiguration.beaconLongitude,
				prepasConfiguration.beaconLatitude,
				beacon_alt,
				&gpsframe[0])) {
		DEBUG_PRINT("frame without CRC:\n");
		for (j = 0; j < GPS_FRAME_LENGTH; j++)
			DEBUG_PRINT("%02X", gpsframe[j]);
		printf("\n");

		//! compute crc
		crc = (uint16_t) crc16((char *) &gpsframe[2], 9);
		DEBUG_PRINT("crc %04X\n", crc);

		//! add crc16 to the frame
		gpsframe[0] = (crc & 0xFF00) >> 8;
		gpsframe[1] = crc & 0xFF;

		DEBUG_PRINT("frame with CRC:");
		for (j = 0; j < GPS_FRAME_LENGTH; j++)
			DEBUG_PRINT("%02X", gpsframe[j]);
		printf("\n");
	}
        LE_INFO("Point2");
	uint2hexString(&gpsframe[0], GPS_FRAME_LENGTH, &temp[0]);
	DEBUG_PRINT("After string2HexString call %s\n", temp);
	DEBUG_PRINT("length of temp: %d\n", strlen((const char *) temp));

//	//! Conversion of AOP data to generic format. Only needed when received raw AOP data from
//	//! argos web or donwlink all cast messages. Here 'aopTable' struct is already filled-up
//	//! with generic format
//	uint8_t satIdx = 0;
//	for (satIdx = 0; satIdx < nbSatsInAopTable; satIdx++) {
//		PREVIPASS_statusFormatAtoGeneric(aopTable[satIdx].downlinkStatus,
//				aopTable[satIdx].uplinkStatus,
//				aopTable[satIdx].satHexId,
//				&aopTable[satIdx].downlinkStatus,
//				&aopTable[satIdx].uplinkStatus);
//	}
	//! Print next pass with criteria
	if (!PREVIPASS_compute_next_pass_with_status(&prepasConfiguration,
			aopTable,
			nbSatsInAopTable,
			SAT_DNLK_OFF,
			SAT_UPLK_ON_WITH_A2,
			&satPass)) {
		printf("ERROR : failed to compute next pass\n");
		return 1;
	}

	writeOnePass(&satPass);

        printf("t_now = %s\n", ctime(&tnow));

	//! timeprediction - tnow
	nxtTimerHandlerExec = (unsigned long) difftime(satPass.epoch, tnow) - 7200;
	printf("diff between nextPass and now = %lu\n", nxtTimerHandlerExec);
	if (nxtTimerHandlerExec <= 0) {
		if (difftime(tnow, satPass.epoch) >= satPass.duration) {
			printf("Error: it's too late, you missed the satellite...\n");
			printf("Data transmission skipped\n");
			printf("Please restart the application\n");
			return 1;
		}
		//! This satelite is currently in visibility period
		//! In order to be able to send data now, nxtTimerHandlerExec
		//! should bigger than 0
		nxtTimerHandlerExec = 1;
	}

#if MODEM_ENABLE
	//! Initialise serial port wired to Kineis modem
	printf("[DEBUG_LOG] Open /dev/ttyHS0 and set serial port parameters\n");
	fd_kineis = mangOH_kim_open(NULL);
	if (fd_kineis != -1) {
		//! Set TX Configuration
		// mangOH_kim_set_tx_cfg(fd_kineis);
                //! Send message
		while (try<5){
			if (mangOH_kim_uart_tx_data(fd_kineis, &temp[0])) {
				time(&tnow);
				printf(">> Frame transmission at %s (PASS)\n", ctime(&tnow));
			} else {
				time(&tnow);
				printf(">> Frame transmission at %s (FAIL) !!!\n", ctime(&tnow));
			}
                sleep(15);
		try++;
                }

		//! Close KIM KIM1 fd
		printf("[DEBUG_LOG] Close /dev/ttyHS0\n");
		mangOH_kim_close(fd_kineis);
	} else {
		printf("[LOG_ERROR] Open /dev/ttyHS0 FAILED, skip frame transmission\n");
	}
#endif

	//! Configure the timer to expire after (prediction - timenow) ...
	timer.it_value.tv_sec = nxtTimerHandlerExec;
	timer.it_value.tv_usec = 0;

	//! One shot
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;

	setitimer(ITIMER_VIRTUAL, &timer, NULL);

	//! Close KIM KIM1 fd
	mangOH_kim_close(fd_kineis);

	return 0;
}

COMPONENT_INIT
{
	struct sigaction sa;

	//! Install timer_handler as the signal handler for SIGVTALRM.
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &argos_protocol_timer_handler;
	sigaction(SIGVTALRM, &sa, NULL);


	//! Initialise serial port wired to Kineis modem
	printf("[DEBUG_LOG] Open /dev/ttyHS0 and set serial port parameters\n");
	fd_kineis = mangOH_kim_open(NULL);
	if (fd_kineis != -1) {
		//! Set TX Configuration
		mangOH_kim_set_tx_cfg(fd_kineis);
		//! Close KIM KIM1 fd
		printf("[DEBUG_LOG] Close /dev/ttyHS0\n");
		mangOH_kim_close(fd_kineis);
	} else {
		printf("[LOG_ERROR] Open /dev/ttyHS0 FAILED, skip frame transmission\n");
	}

}
// -------------------------------------------------------------------------- //
//! @} (end addtogroup MAIN-APP)
// -------------------------------------------------------------------------- //
