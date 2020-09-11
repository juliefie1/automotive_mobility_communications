//! \file **************************************************************
//!
//! \brief GPS Formatter Header
//!
//! \author Alpwan
//! \version 1.0
//! \date 07/10/2019
//!
//! ********************************************************************


// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include "gpslib.h"


// -------------------------------------------------------------------------- //
//! @addtogroup GPS-LIBS
//! @{
// -------------------------------------------------------------------------- //

int gpsframe_parser(uint16_t day, uint8_t hour, uint8_t min,
		float longitude, float lat, float alt, uint8_t *gpsframe)
{
	unsigned char acq_period = 7;
	unsigned char temp;
	float enc_long;
	unsigned long longit;
	unsigned long enc_lat;
	unsigned short enc_alt;
	float reste;
	//unsigned char index=2;
	unsigned char min_high_3b;
	unsigned char long_low_bit;
	unsigned char lat_last6bit;
	unsigned char alt_high_2bit;
	unsigned char long_high_5bit;

	/** Shift gpsframe pointer to index 2 as gpsframe[0-1] are reserved for CRC */
	gpsframe++;
	gpsframe++;
	if (day >= 1 && day <= 31) {
		temp = (acq_period << 5) | day;
		//printf("1-temp=%X\n", temp);
		//gpsframe[index++]=temp;
		*gpsframe++ = temp;
		// Clear temp
		temp = 0;
	} else {
		printf("bad value: day\n");
		return -1;
	}
	if (hour >= 0 && hour <= 23)
		temp = hour << 3;
	else {
		printf("bad value: hour\n");
		return -1;
	}
	if (min >= 0 && min <= 59) {
		min_high_3b = ((min & 0x38) >> 3);		//b111000
		//printf("min_high_3b = %X\n", min_high_3b);
		temp |= min_high_3b;
		//printf("2-temp=%X\n", temp);
		//gpsframe[index++]=temp;
		*gpsframe++ = temp;
		// Clear temp
		temp = 0;
	} else {
		printf("bad value: minute\n");
		return -1;
	}
	if (longitude >= 0.0 && longitude <= 360) {
		enc_long = ((longitude * 3600000) / 360.0);
		reste = enc_long - (unsigned long)(enc_long);
		//printf("enc_long=%f\n", enc_long);
		//printf("reste=%f\n", reste);
		if (reste > 0.5)
			enc_long += 1;
		//printf("enc_long corrected=%lu\n", (unsigned long)enc_long);
		longit = (unsigned long) enc_long;
		//printf("longit=%lu\n", longit);
		long_high_5bit = (longit & 0x3E0000) >> 17;	//b11-1110-0000-0000-0000-0000
		//printf("long_high_5bit=%X\n", long_high_5bit );
		temp = ((min & 0x7) << 5) | long_high_5bit;
		//printf("3-temp=%X\n", temp);
		//gpsframe[index++]=temp;
		*gpsframe++ = temp;
		// Clear temp
		temp = 0;
		temp = (longit & 0x1FE00) >> 9;
		//gpsframe[index++]= temp;
		*gpsframe++ = temp;
		//printf("4-temp=%X\n", temp);
		// Clear temp
		temp = 0;
		temp = (longit & 0x1FE) >> 1;
		//gpsframe[index++]= temp;
		*gpsframe++ = temp;
		//printf("5-temp=%X\n", temp);
		long_low_bit = longit & 0x1;
		// Clear temp
		temp = 0;
	} else {
		printf("bad value: longitude\n");
		return -1;
	}
	if (lat >=  -90.0 && lat <= 90.0) {
		enc_lat = ((lat * 10000) + 900000);
		//printf("enc_lat=%lu\n", enc_lat);
		temp = (enc_lat & 0x1FC000) >> 14;	//b1-1111-1100-0000-0000-0000
		//gpsframe[index++]= (long_low_bit << 7) | temp;
		*gpsframe++ = (long_low_bit << 7) | temp;
		//printf("6-temp=%X\n", temp);
		// Clear temp
		temp = 0;
		temp = (enc_lat & 0x3FC0) >> 6;
		//gpsframe[index++]= temp;
		*gpsframe++ = temp;
		//printf("7-temp=%X\n", temp);
		// Clear temp
		temp = 0;
		lat_last6bit = (enc_lat & 0x3F);
		//printf("lat_last6bit%X\n", lat_last6bit);
	} else {
		printf("bad value: latitude\n");
		return -1;
	}
	if (alt >= 0 && alt <= 10230) {
		enc_alt = (alt / 10);
		//printf("enc_alt=%X\n", enc_alt);
		alt_high_2bit = (enc_alt & 0x300) >> 8;	//b11-0000-0000
		temp = (lat_last6bit << 2) | alt_high_2bit;
		//gpsframe[index++] = temp;
		*gpsframe++ = temp;
		//printf("8-temp=%X\n", temp);
		// Clear temp
		temp = 0;
		temp = enc_alt & 0xFF;
		//printf("9-temp=%X\n", temp);
		//gpsframe[index++] = temp;
		*gpsframe++ = temp;
		// Clear temp
		temp = 0;
		//printf("pointer %p\n", gpsframe);
		return 0;
	}
	printf("bad value: altitude\n");
	return -1;
}

int gps_pos(float *latitude, float *longitude, float *altitude,
	uint16_t *dat_year, uint8_t *dat_month, uint16_t *dat_day,
	uint8_t *dat_hour, uint8_t *dat_min, uint8_t *dat_sec)
{
	const char *gnss_start = "/legato/systems/current/bin/gnss start";
	const char *get_posState = "/legato/systems/current/bin/gnss get posState";
	const char *get_loc3d = "/legato/systems/current/bin/gnss get loc3d";
	const char *get_time = "/legato/systems/current/bin/gnss get time";
	const char *get_date = "/legato/systems/current/bin/gnss get date";
	const char *check_gnssservice = "Success!\n";
	const char *check_ifready = "The GNSS device is already started\n";
	const char *fix_2d = "Position state: 2D Fix\n";
	const char *fix_3d = "Position state: 3D Fix\n";
	unsigned short fix2d = 0;
	unsigned short fix3d = 0;
	unsigned short timer = 1;
	char line[128];
	int line_number = 0;
	char lat_header[32];
	char lon_header[32];
	char alt_header[32];
	char dp[10];
	char *date_ptr;
	char year[5];
	char month[3];
	char day[3];
	char hour[3];
	char min[3];
	char sec[3];
	char time_header[32];
	char time[32];
	FILE *output;

	/** Start gnss service */
	output = popen(gnss_start, "r");
	if (output == NULL)
		fprintf(stderr, "Erreur popen %d\n", errno);

	while (fgets(line, 127, output) != NULL) {
		if (strcmp(line, check_gnssservice) == 0) {
			printf("GPS service started\n");
			pclose(output);
			break;
		} else if (strcmp(line, check_ifready) == 0) {
			printf("GPS service already started\n");
			pclose(output);
			break;
		}
	}

	/** Get posState (no fix, 2D, 3D) */
	do {
		output = popen(get_posState, "r");
		if (output == NULL)
			fprintf(stderr, "Erreur popen %d\n", errno);
		while (fgets(line, 127, output) != NULL) {
			//printf("%s\n", line);
			//printf("size of line %d\n", strlen(line));
			if (strcmp(line, fix_2d) == 0) {
				pclose(output);
				fix2d = 1;
				break;
			} else if (strcmp(line, fix_3d) == 0) {
				pclose(output);
				fix3d = 1;
				break;
			}
		}
		/** TTF */
		sleep(TIMEOUT_PERIOD);
		timer++;

	} while (!fix2d && !fix3d && (timer <= TIMEOUT));

	/** force 3D to get altitude info */
	/** Please check : "https://forum.mangoh.io/t/gnss-no-3d-fix-with-wp7702/1904/3" */
	fix2d = 0;
	fix3d = 1;

	if (timer > TIMEOUT) {
		printf("timeout ... (check gps antenna, etc ..)\n");
		return 1;
	} else if (fix3d) {
		printf("Position state: 3D Fix\n");
		/** Get day of month for gps frame */
		output = popen(get_date, "r");
		if (output == NULL) {
			fprintf(stderr, "Erreur popen %d\n", errno);
			return 1;
		}
		while (fgets(line, 127, output) != NULL) {
			printf("%s\n", line);
			/** looking for ")" in the returned string */
			date_ptr = strstr(line, ")");
			if (date_ptr != NULL) {
				/** move date_ptr to point to "year" info */
				date_ptr = date_ptr + 2;
				/** copy the year */
				strncpy(&year[0], date_ptr, 4);
				year[4] = '\0';
				//printf("Year = %s\n", year);
				/** move date_ptr to point to "month" info */
				date_ptr = date_ptr+5;
				/** copy the month */
				strncpy(&month[0], date_ptr, 2);
				month[2] = '\0';
				//printf("Month = %s\n", month);
				/** move date_ptr to point to "day" info */
				date_ptr = date_ptr+3;
				/** copy the day */
				strncpy(&day[0], date_ptr, 2);
				day[2] = '\0';
				//printf("Day of month = %s\n", day);
			} else {
				/* Close output stream before exit */
				pclose(output);
				return 1;
			}
		}
		/* Close output steam */
		if (output) {
			pclose(output);
			output = NULL;
		}

		/** Get time for gps frame */
		output = popen(get_time, "r");
		if (output == NULL) {
			fprintf(stderr, "Erreur popen %d\n", errno);
			return 1;
		}


		while (fgets(line, 127, output) != NULL) {
			//printf("returned time string = %s\n", line);
			if (sscanf(line, "%s %s", time_header, time) != 2)
				return 1;
			//printf("time_header = %s\n", time_header);
			//printf("time = %s\n", time);
			strncpy(&hour[0], &time[0], 2);
			/** Get hour info */
			hour[2] = '\0';
			//printf("hour = %s\n", hour);
			/** Get min info */
			strncpy(&min[0], &time[3], 2);
			min[2] = '\0';
			//printf("min = %s\n", min);
			/** Get sec info */
			strncpy(&sec[0], &time[6], 2);
			sec[2] = '\0';
			//printf("seconds = %s\n", sec);
		}

		pclose(output);

		/** convert string to int */
		*dat_year = atoi(&year[0]);
		*dat_month = atoi(&month[0]);
		*dat_day = atoi(&day[0]);
		*dat_hour = atoi(&hour[0]);
		*dat_min = atoi(&min[0]);
		*dat_sec = atoi(&sec[0]);
		printf("year=%d; month=%d; day=%d; hour=%d; min=%d, sec=%d\n", *dat_year,
			*dat_month, *dat_day, *dat_hour, *dat_min, *dat_sec);

		/** Get location info */
		output = popen(get_loc3d, "r");
		if (output == NULL) {
			fprintf(stderr, "Erreur popen %d\n", errno);
			return 1;
		}


		while (fgets(line, 127, output) != NULL) {
			if (line_number == 0) {
				if (sscanf(line, "%s %s %f", lat_header, dp, latitude) != 3)
					return 1;
				printf("Header = %s\n", lat_header);
				printf("separator = %s\n", dp);
				printf("Latitude = %f\n", *latitude);
				/** check the sign for latitude */
				if (strstr(lat_header, "positive") != NULL)
					printf("Latitude positive -> North\n");
				else {
					printf("Latitude negative -> South\n");
					*latitude = -(*latitude);
				}
			} else if (line_number == 1) {
				if (sscanf(line, "%s %s %f", lon_header, dp, longitude) != 3)
					return 1;
				printf("Header = %s\n", lon_header);
				printf("separator = %s\n", dp);
				printf("Longitude = %f\n", *longitude);
				/** check the sign for the longitude */
				if (strstr(lon_header, "positive") != NULL)
					printf("Longitude positive -> east\n");
				else {
					printf("Longitude negative -> west\n");
					*longitude = -(*longitude);
				}
			} else if (line_number == 3) {
				if (sscanf(line, "%s %s %f", alt_header, dp, altitude) != 3)
					return 1;
				printf("Header = %s\n", alt_header);
				printf("separator = %s\n", dp);
				printf("Altitude = %f\n", *altitude);
			} else if (line_number > 4) {
				break;
			}
			line_number++;
		}
		if (output) {
			pclose(output);
			output = NULL;
		}
	} else {
		printf("Position state: no Fix\n");
		return 1;
	}
	return 0;
}

// -------------------------------------------------------------------------- //
//! @} (end addtogroup GPS-LIBS)
// -------------------------------------------------------------------------- //
