//! \file **************************************************************
//!
//! \brief Library functions
//!
//! \author Alpwan
//! \version 1.0
//! \date 07/10/2019
//!
//! ********************************************************************


// -------------------------------------------------------------------------- //
//! @addtogroup KIM1-LIBS
//! @{
// -------------------------------------------------------------------------- //

// -- INCLUDE ----------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "mangOH_Kim1.h"

// -- DEFINE -----------------------------------------------------------
// _MANGOH_KIM_GPIO_ONOFF	= GPIO_1  (Expansion board pin 24) = GPIO42
// _MANGOH_KIM_GPIO_RESET	= n_RESET (Expansion board pin 32) = GPIO2
#define _MANGOH_KIM_GPIO_ONOFF				"42"
#define _MANGOH_KIM_GPIO_RESET				"2"
#define _MANGOH_KIM_SIZE_BUF				64
#define _MANGOH_KIM_READ_TIMEOUT			50000		// In us
#define _MANGOH_KIM_SERIAL_PORT				"/dev/ttyHS0"
#define _MANGOH_KIM_SIZE_BUF				64

const char cr = '\n';

// -- PROTOTYPE --------------------------------------------------------


/**
 * @brief This function allows to switch ON the Kineis module
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_enable_onoff(void)
{
	// Open gpio
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_ONOFF"/value", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set gpio to '1'.
	if (write(gpio_fd, "1", 1) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function allows to switch OFF the Kineis module
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_disable_onoff(void)
{
	// Open gpio
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_ONOFF"/value", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set gpio to '0'.
	if (write(gpio_fd, "0", 1) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function allows to put the Kineis module in reset
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_enable_reset(void)
{
	// Open gpio
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_RESET"/value", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set gpio to '0'.
	if (write(gpio_fd, "0", 1) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function allows to release the Kineis module reset
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_disable_reset(void)
{
	// Open gpio
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_RESET"/value", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set gpio to '1'.
	if (write(gpio_fd, "1", 1) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief This function initializes the serial port (4800,n,8,1)
 * @param[in] fd serial port file descriptor
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_set_fd_parameter(int fd)
{
	struct termios cfg;

	// Get current attributes
	if (tcgetattr(fd, &cfg) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set speed
	cfsetispeed(&cfg, B4800);
	cfsetospeed(&cfg, B4800);


	// 8bits of data
	cfg.c_cflag &= ~CSIZE;
	cfg.c_cflag |= CS8;

	// No flow.
	cfg.c_cflag &= ~CRTSCTS;

	// Set parity
	cfg.c_cflag &= ~PARENB;

	// Set bit stop
	cfg.c_cflag &= ~CSTOPB;

	cfg.c_lflag &= ~(ICANON | ISIG | ECHO);
	cfg.c_oflag |= OPOST;
	cfg.c_iflag &= ~(IXON | IXOFF | IXANY);

	// Set new attributes.
	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &cfg) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief This function select the file descriptor for the serial port.
 *        It allows to received early answers from KIM1 after power on.
 * @param[in] fd serial port file descriptor
 * @retval returns the status when selecting the serial port
 *         0 if ok
 *         -1 when select fails
 *         x in case of timeout
 */
static ssize_t mangOH_kim_config_read(int fd)
{
	// int escape = 0;

	struct timeval _timeout;

	_timeout.tv_sec = 0;
	_timeout.tv_usec = _MANGOH_KIM_READ_TIMEOUT;

	fd_set fd_read;

	FD_ZERO(&fd_read);
	FD_SET(fd, &fd_read);

	// Wait for reading.
	int read_status = select(fd+1, &fd_read, NULL, NULL, &_timeout);

	if (read_status == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}
	// Read data.
	else if (read_status == 0)
		printf("config READ OK\n");

	// Or Timeout
	else {
		printf("config READ timeout\n");
		// printf("Timeout !!!\n");
	}

	return read_status;
}

/**
 * @brief	This function reads 'nbyte' bytes from the serial port
 * @param[in] fd serial port file descriptor
 * @param[in] buf read buffer pointer
 * @param[in] nbyte number of byte to read
 * @param[in] firstTimeout
 * @param[in] timeout
 * @retval return 0 if ok otherwise return -1
 */
static ssize_t mangOH_kim_read(int fd, char *buf, ssize_t nbyte, int firstTimeout, int timeout)
{
	ssize_t sizeBuf = nbyte-1;
	char *ptrBuf = buf;
	ssize_t n = 0;
	int i = 0;
	// int escape = 0;

	struct timeval _timeout;

	_timeout.tv_sec = 0;
	_timeout.tv_usec = firstTimeout ==  -1?_MANGOH_KIM_READ_TIMEOUT:firstTimeout;

	timeout = timeout ==  -1?_MANGOH_KIM_READ_TIMEOUT:timeout;


	while (1) {
		fd_set fd_read;

		FD_ZERO(&fd_read);
		FD_SET(fd, &fd_read);

		// Wait for reading.
		if (select(fd+1, &fd_read, NULL, NULL, &_timeout) == -1) {
			fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
			return -1;
		}
		// Read data.
		else if (FD_ISSET(fd, &fd_read)) {
			n = read(fd, ptrBuf, sizeBuf);
			printf("%d bytes received\n", n);

			/** uncomment for debug */
			for (i = 0; i < n; i++) {
				printf("%c", *ptrBuf);
				ptrBuf++;
			}
			ptrBuf -= n;
			/** end */

			if (n == -1) {
				fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
				return -1;
			}

			if (n == 0)
				break;

			sizeBuf -= n;
			ptrBuf += n;

			// check if receive '\n'.
			*ptrBuf = '\0';

			// The buffer is full ?
			if ((ssize_t)(ptrBuf-buf) >= nbyte)
				break;
		}
		// Or Timeout
		else {
			// printf("Timeout !!!\n");
			break;
		}
		// Set timeout
		_timeout.tv_sec = 0;
		_timeout.tv_usec = timeout;
	}

	buf[ptrBuf-buf] = '\0';

	return ptrBuf-buf;
}

/**
 * @brief This function exports the ONOFF gpio and set the gpio direction
 * as output.
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_gpio_onoff_export_output(void)
{
	// Open gpio export.
	int gpio_fd = open("/sys/class/gpio/export", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Export gpio.
	if (write(gpio_fd, _MANGOH_KIM_GPIO_ONOFF, 2) == -1) {
		// If error is "device or resource bus", the export is already did.
		// So, there is no error.
		if (errno != EBUSY) {
			close(gpio_fd);
			fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
			return -1;
		}
	}

	// Close gpio export.
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Wait the create file direction.
	usleep(50000);

	// Open gpio direction.
	gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_ONOFF"/direction", O_WRONLY);
	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set to output
	if (write(gpio_fd, "out", 3) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio direction
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief This function sets the ONOFF gpio direction as input and unexport it
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_gpio_onoff_unexport_output(void)
{
	// Open gpio direction.
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_ONOFF"/direction", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set to input
	if (write(gpio_fd, "in", 2) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio direction
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Open gpio unexport
	gpio_fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Unexport gpio
	if (write(gpio_fd, _MANGOH_KIM_GPIO_ONOFF, 3) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio unexport
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief This function exports the reset gpio ans sets as output
 * it.
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_gpio_reset_export_output(void)
{
	// Open gpio export.
	int gpio_fd = open("/sys/class/gpio/export", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Export gpio.
	if (write(gpio_fd, _MANGOH_KIM_GPIO_RESET, 2) == -1) {
		// If error is "device or resource bus", the export is already did.
		// So, there is no error.
		if (errno != EBUSY) {
			close(gpio_fd);
			fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
			return -1;
		}
	}

	// Close gpio export.
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Wait the create file direction.
	usleep(50000);

	// Open gpio direction.
	gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_RESET"/direction", O_WRONLY);
	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set to output
	if (write(gpio_fd, "out", 3) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio direction
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function sets the reset gpio as input and unexports it
 * it.
 * @retval return 0 if ok otherwise return -1
 */
static int mangOH_kim_gpio_reset_unexport_output(void)
{
	// Open gpio direction.
	int gpio_fd = open("/sys/class/gpio/gpio"_MANGOH_KIM_GPIO_RESET"/direction", O_WRONLY);

	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set to input
	if (write(gpio_fd, "in", 2) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio direction
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Open gpio unexport
	gpio_fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (gpio_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Unexport gpio
	if (write(gpio_fd, _MANGOH_KIM_GPIO_RESET, 3) == -1) {
		close(gpio_fd);
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Close gpio unexport
	if (close(gpio_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function initializes the reset and onoff gpio
 * and opens the serial port and waits for startup message.
 * @param[in] path to serial port device name
 * @retval return 0 if ok otherwise return -1
 */
int mangOH_kim_open(const char *path)
{
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// force gpio resetof KIM1 before uart init
	// Note: it shuold be in this statue already before calling this functions
	if (mangOH_kim_gpio_reset_export_output() == -1)
		return -1;

	if (mangOH_kim_enable_reset() == -1) {
		mangOH_kim_gpio_reset_unexport_output();
		return -1;
	}

	// Open serial port
	int mangOH_kim_fd;

	if (path != NULL)
		mangOH_kim_fd = open(path, O_RDWR);
	else
		mangOH_kim_fd = open(_MANGOH_KIM_SERIAL_PORT, O_RDWR);

	if (mangOH_kim_fd == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	// Set parameter for configure the uart
	if (mangOH_kim_set_fd_parameter(mangOH_kim_fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		close(mangOH_kim_fd);
		return -1;
	}

	// Preparing readint the uart, before exiting reset
	mangOH_kim_config_read(mangOH_kim_fd);

	// gpio onoff in output
	if (mangOH_kim_gpio_onoff_export_output() == -1)
		return -1;

	// power-on the KIM1
	if (mangOH_kim_enable_onoff() == -1) {
		mangOH_kim_gpio_onoff_unexport_output();
		return -1;
	}

	// gpio reset in output.
	if (mangOH_kim_gpio_reset_export_output() == -1)
		return -1;

	// exit from reset
	if (mangOH_kim_disable_reset() == -1) {
		mangOH_kim_gpio_reset_unexport_output();
		return -1;
	}

	// Read response.
	printf("read startup message\n");
	mangOH_kim_read(mangOH_kim_fd, rx_buf, _MANGOH_KIM_SIZE_BUF,
			_MANGOH_KIM_READ_TIMEOUT*2, -1);

	return mangOH_kim_fd;
}

/**
 * @brief This function closes the serial port and switch off the module.
 * @param[in] fd serial port file descriptor
 * @retval return 0 if ok otherwise return -1
 */
int mangOH_kim_close(int fd)
{
	// gpio onoff in output.
	if (mangOH_kim_gpio_onoff_export_output() == -1)
		return -1;

	if (mangOH_kim_disable_onoff() == -1) {
		mangOH_kim_gpio_onoff_unexport_output();
		return -1;
	}

	// gpio reset in output.
	if (mangOH_kim_gpio_reset_export_output() == -1)
		return -1;

	if (mangOH_kim_enable_reset() == -1) {
		mangOH_kim_gpio_reset_unexport_output();
		return -1;
	}

	if (close(fd) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * @brief  This function returns the argos ID
 * @param[in] fd serial port file descriptor
 * @retval pointer to the string containing the argos ID
 */
char *mangOH_kim_get_argos_id(int fd)
{
	// Create AT command.
	char tx_buf[32] = "AT+ID=?\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];


	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return NULL;


	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function sets the argos ID
 * @param[in] fd serial port file descriptor
 * @param[in] argos_id pointer to argos_id string
 * @retval return 0 if pass else return 1
 */
unsigned int mangOH_kim_set_argos_id(int fd, char *argos_id)
{
	unsigned int index = 0;
	char ch;
	unsigned int counter = 0;

	// Create AT command.
	char tx_buf[32] = "AT+ID=\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	printf("%s\n", argos_id);
	while (*argos_id != '\0') {
		ch = *argos_id;
		if (isxdigit(ch)) {
			counter++;
			printf("ch=%c, counter=%d\n", ch, counter);
		}
		argos_id++;
	}

	if ((counter != 5) && (counter != 7)) {
		printf("argos_id size not supported\n");
		return 1;
	}

	// size is ok
	argos_id -= counter;
	printf("argo_id ptr= %p\n", argos_id);
	// concat
	strcat(tx_buf, argos_id);
	printf("tx_buf= %s\n", tx_buf);

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return 0;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF,
				_MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return 1;

	// Parse and check response.
	while (rx_buf[index] != '\0') {
		printf("rx_buf[%d]=%c", index, rx_buf[index]);
		index += 1;
	}

	/**
	 * unsigned int id;
	 * if(sscanf(buf, "%x", &id) == -1) {
	 * fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
	 * return 0;
	 * }
	 */
	return 0;
}

/**
 * @brief This function gets the frequency setting
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the frequency
 */
char *mangOH_kim_get_freq(int fd)
{
	// int index=0;
	// Create AT command.
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];
	char tx_buf[32] = "AT+FRQ=?\n";

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	printf("read response\n");
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*4, -1) == -1)
		return NULL;


	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function sets the frequency
 * @param[in] fd serial port file descriptor
 * @param[in] freq_offset frequency (399980<= freq <=401620)
 * @param[in] band band frequency (1 < band < 9)
 * @retval return a pointer to a string containing the frequency
 */
char *mangOH_kim_set_freq(int fd, unsigned int freq_offset, unsigned int band)
{
	char frequency[10];

	// Create AT command.
	char tx_buf[32] = "AT+FRQ=";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// check frequency value
	if (band == 4 || band == 5) {
		if (freq_offset > 800) {
			printf("frequency is not in band%u range\n", band);
			return NULL;
		}
	} else if (band != 4 && band != 5) {
		if (freq_offset > 700) {
			printf("frequency is not in band%u range\n", band);
			return NULL;
		}

		// frequency is in band
		printf("freq init");
		// strcpy
		sprintf(frequency, "%u", freq_offset);
		printf("frequency value for AT command = %s\n", frequency);

		// concat
		strcat(tx_buf, frequency);

		// add CR
		strcat(tx_buf, &cr);

		// Send AT command.
		printf("send %s\n", tx_buf);
		if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
			fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
			return NULL;
		}

		// Read response.
		if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF,
					_MANGOH_KIM_READ_TIMEOUT*4, -1) == -1)
			return NULL;

		return rx_buf;
	}
	return NULL;
}

/**
 * @brief This function sets the frequency
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the band frequency
 */
char *mangOH_kim_get_band(int fd)
{
	// int index=0;
	// Create AT command.
	char tx_buf[32] = "AT+BAND=?\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	printf("read response\n");
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return NULL;


	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function sets the band frequency
 * @param[in] fd serial port file descriptor
 * @param[in] band frequency (1<=band<=9)
 * @retval return a pointer to a string containing the band frequency
 */
char *mangOH_kim_set_band(int fd, unsigned char band)
{
	char f_band[4];

	// Create AT command.
	char tx_buf[32] = "AT+BAND=";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// Check frequency band value
	if (band >= 1 && band <= 9) {
		sprintf(f_band, "B%u\n", band);
		strcat(tx_buf, f_band);
		// Add CR
		strcat(tx_buf, &cr);
		printf("tx_buf = %s", tx_buf);
	} else {
		printf("error band %u not supported !!!\n", band);
		return NULL;
	}

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return 0;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*8, -1) == -1)
		return NULL;


	return rx_buf;
}

/**
 * @brief  This function returns the power
 * @param[in]	fd serial port file descriptor
 * @retval return a pointer to a string containing the power setting
 */
char *mangOH_kim_get_pwr(int fd)
{
	// int index=0;
	// Create AT command.
	char tx_buf[32] = "AT+PWR=?\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	printf("read response\n");
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return NULL;


	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function sets the power
 * @param[in] fd serial port file descriptor
 * @param[in] pwr value (250, 500, 750, 1000 or 1500)
 * @retval return a pointer to a string containing the power setting
 */
char *mangOH_kim_set_pwr(int fd, unsigned short pwr)
{
	char power[8];

	// Create AT command.
	static char tx_buf[32] = "AT+PWR=";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// check pwr value
	printf("pwr = %d", pwr);
	switch (pwr) {
	case 250:
		strcpy(power, "250");
		break;
	case 500:
		strcpy(power, "500");
		break;
	case 750:
		strcpy(power, "750");
		break;
	case 1000:
		strcpy(power, "1000");
		break;
	case 1500:
		strcpy(power, "1500");
		break;
	default:
		printf("Bad power value (250, 500, 750, 1000 and 1500mW only supported\n");
		return NULL;
	}

	printf("power = %s\n", power);
	snprintf(power, 8, "%d", pwr);

	if (power == NULL) {
		printf("pwr value not supported\n");
		return NULL;
	}

	// concat
	strcat(tx_buf, power);
	printf("tx_buf= %s\n", tx_buf);

	// Add CR at the end of tx_buf
	strcat(tx_buf, &cr);

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF,
				_MANGOH_KIM_READ_TIMEOUT*8, -1) == -1)
		return NULL;

	return rx_buf;
}

/**
 * @brief  This function returns the firmware version
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the fw version
 */
char *mangOH_kim_get_fw_version(int fd)
{
	// int index=0;
	// Create AT command.
	char tx_buf[32] = "AT+FW=?\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return NULL;


	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function returns the serial number
 * @param[in] fd serial port file descriptor
 * @retval return a pointer to a string containing the SN
 */
char *mangOH_kim_get_sn(int fd)
{
	// int index=0;
	// Create AT command.
	char tx_buf[32] = "AT+SN=?\n";
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];

	// Send AT command.
	printf("send %s\n", tx_buf);
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return NULL;
	}

	// Read response.
	printf("read response\n");
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF, _MANGOH_KIM_READ_TIMEOUT*2, -1) == -1)
		return NULL;

	printf("rx_buf=%s\n", rx_buf);
	return rx_buf;
}

/**
 * @brief  This function transmits data
 * @param[in] fd serial port file descriptor
 * @param[in] data pointer to the string containing buffer to transmit
 * @retval return 0 if pass else return -1 (fail)
 */
// return if tx done otherwise return 0 if tx fails
int mangOH_kim_uart_tx_data(int fd, char *data)
{
	// Create AT command.
	char tx_buf[100];
	static char rx_buf[_MANGOH_KIM_SIZE_BUF];
	char databuf[65];
	// char find_ch = ':';
	char *ptr;
	unsigned int bit_frame_length;
	unsigned int remaining_bitlen;


	strcpy(databuf, data);
	printf("Frame to transmit=%s\n", databuf);
	printf("tx_buf size in bytes =%d\n", strlen(databuf));

//	for(j=0;j<strlen(databuf);j++)
//		printf("databuf[%d]=%02X:", j,databuf[j]);

	/** New padding */
	bit_frame_length = strlen(databuf)*8;
	printf("frame length (bit) =%d\n", bit_frame_length);
	bit_frame_length = bit_frame_length - 24;
	// n = bit_frame_length / 32;
	remaining_bitlen = bit_frame_length % 32;
	printf("remaining_bitlen=%d\n", remaining_bitlen);
	if (remaining_bitlen) {
		switch (remaining_bitlen) {
		case 24: // add (4-3) byte
			strcat(databuf, "0");
			break;
		case 16: // add (4-2) byte
			strcat(databuf, "00");
			break;
		case 8: // add (4-1) byte
			strcat(databuf, "000");
			break;
		default:
			printf("ERROR remaining_bitlen is not valid\n");
			return 1;
		}
	}

//	for(j=0;j<strlen(databuf);j++)
//		printf("databuf[%d]=%02X:", j,databuf[j]);
//	printf("\n");

	printf("Frame to transmit after padding=%s\n", databuf);
	// printf("tx_buf size in bytes =%d\n", strlen(databuf));

	sprintf(tx_buf, "AT+TX=%s\n", databuf);

	// Send AT command.
	if (write(fd, tx_buf, strlen(tx_buf)) == -1) {
		fprintf(stderr, "ERROR - %s: %s\n", __func__, strerror(errno));
		return 1;
	}

	// Read response.
	if (mangOH_kim_read(fd, rx_buf, _MANGOH_KIM_SIZE_BUF,
				_MANGOH_KIM_READ_TIMEOUT*400, -1) == -1){
		printf("ERROR response after transmission\n");
		return 1;
	}

	// search +TX_INFO: in buf
	// ptr=strchr(rx_buf, find_ch);
	ptr = strstr(rx_buf, "+TX_INFO:");
	if (ptr != NULL) {
//		printf(": detected\n");
//		ptr++;
//		printf("ret := %d",(*ptr)-48);

		ptr += strlen("+TX_INFO:");
		// printf("char := %d\s", *ptr);
		return (int)(*ptr)-48;
	}
	return 0;
}


// -------------------------------------------------------------------------- //
//! @} (end addtogroup KIM1-LIBS)
// -------------------------------------------------------------------------- //
