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
//! @addtogroup GPS-LIBS
//! @brief  gps data manpulation
//! @{
// -------------------------------------------------------------------------- //

#ifndef GPSLIB_H
#define GPSLIB_H

/**
 * @brief  GPS parameters
 */
#define TIMEOUT	60	// 60 * TIMEOUT_PERIOD (1s) = 60s
#define TIMEOUT_PERIOD	1

/**
 * @brief  Compute hexadecimal payload to send GPS data overs ARGOS
 * @param[in] day
 * @param[in] hour
 * @param[in] min
 * @param[in] longitude
 * @param[in] lat
 * @param[in] alt
 * @param[out] gpsframe ARGOS frame containing GPS data
 * @retval return 0 if ok otherwise return -1
 */
int gpsframe_parser(uint16_t day, uint8_t hour, uint8_t min,
		float longitude, float lat, float alt, uint8_t *gpsframe);

/**
 * @brief  Get gps coordinates function (MangOH red).
 *         This function uses the gnss tools
 *         https://docs.legato.io/17_08/toolsTarget_gnss.html
 * @param[out] latitude
 * @param[out] longitude
 * @param[out] altitude
 * @param[out] dat_year
 * @param[out] dat_month
 * @param[out] dat_day
 * @param[out] dat_hour
 * @param[out] dat_min
 * @param[out] dat_sec
 * @retval 0 succeed ; 1 fail
 */
int gps_pos(float *latitude, float *longitude, float *altitude,
	uint16_t *dat_year, uint8_t *dat_month, uint16_t *dat_day,
	uint8_t *dat_hour, uint8_t *dat_min, uint8_t *dat_sec);
#endif // GPSLIB_H

// -------------------------------------------------------------------------- //
//! @} (end addtogroup GPS-LIBS)
// -------------------------------------------------------------------------- //
