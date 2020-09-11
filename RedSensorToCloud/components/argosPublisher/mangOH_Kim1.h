//! \file **************************************************************
//!
//! \brief Header
//!
//! \author Alpwan
//! \version 1.0
//! \date 07/10/2019
//!
//! ********************************************************************


// -------------------------------------------------------------------------- //
//! @addtogroup KIM1-LIBS
//! @brief  KIM1 library
//! @{
// -------------------------------------------------------------------------- //

#ifndef MANGOH_KIM1_H
#define MANGOH_KIM1_H

// -- FUNCTION ---------------------------------------------------------

/**
 * @brief  This function initializes the reset and onoff gpio
 * and opens the serial port and waits for startup message.
 * @param[in] path to serial port device name
 * @retval return 0 if ok otherwise return -1
 */
int mangOH_kim_open(const char *path);

/**
 * @brief  This function returns the firmware version
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the fw version
 */
char *mangOH_kim_get_fw_version(int fd);

/**
 * @brief  This function returns the serial number
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the SN
 */
char *mangOH_kim_get_sn(int fd);

/**
 * @brief  This function returns the argos ID
 * @param[in] fd serial port file descriptor
 * @retval pointer to the string containing the argos ID
 */
char *mangOH_kim_get_argos_id(int fd);

/**
 * @brief  This function sets the argos ID
 * @param[in] fd serial port file descriptor
 * @param[in] argos_id pointer to argos_id string
 * @retval return 0 if pass else return 1
 */
unsigned int mangOH_kim_set_argos_id(int fd, char *argos_id);

/**
 * @brief This function sets the frequency
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the band frequency
 */
char *mangOH_kim_get_band(int fd);
/**
 * @brief  This function sets the band frequency
 * @param[in] fd serial port file descriptor
 * @param[in] band frequency (1<=band<=9)
 * @retval return a pointer to a string containing the band frequency
 */
char *mangOH_kim_set_band(int fd, unsigned char band);

/**
 * @brief This function gets the frequency setting
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the frequency
 */
char *mangOH_kim_get_freq(int fd);

/**
 * @brief  This function sets the frequency
 * @param[in] fd serial port file descriptor
 * @param[in] freq_offset frequency offset (kHz)
 * @param[in] band band frequency (1 < band < 9)
 * @retval return a pointer to a string containing the frequency
 */
char *mangOH_kim_set_freq(int fd, unsigned int freq_offset, unsigned int band);

/**
 * @brief  This function returns the power
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the power setting
 */
char *mangOH_kim_get_pwr(int fd);

/**
 * @brief  This function sets the power
 * @param[in] fd serial port file descriptor
 * @param[in] pwr value (250, 500, 750, 1000 or 1500)
 * @retval return a pointer to a string containing the power setting
 */
char *mangOH_kim_set_pwr(int fd, unsigned short pwr);

/**
 * @brief  This function transmits data
 * @param[in] fd serial port file descriptor
 * @param[in] fd pointer to the string containing buffer to transmit
 * @retval return 0 if pass else return -1 (fail)
 */
int mangOH_kim_uart_tx_data(int fd, char *data);

/**
 * @brief This function closes the serial port and switch off the module.
 * @param[in] fd serial port file descriptor
 * @retval return 0 if ok otherwise return -1
 */
int mangOH_kim_close(int fd);

#endif //MANGOH_KIM1_H


// -------------------------------------------------------------------------- //
//! @} (end addtogroup KIM1-LIBS)
// -------------------------------------------------------------------------- //
