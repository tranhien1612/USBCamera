#ifndef _LIBIRPARSE_H_
#define _LIBIRPARSE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#elif defined(linux) || defined(unix)
#define DLLEXPORT
#endif

/**
* @brief Log level definition in libirparse library
*/
typedef enum {
		IRPARSE_LOG_DEBUG = 0,		///< print debug and error infomation
		IRPARSE_LOG_ERROR = 1,		///< only print error infomation
		IRPARSE_LOG_NO_PRINT = 2,	///< don't print debug and error infomation
}irparse_log_level_t;


/**
* @brief Error type in libirparse library
*/
typedef enum {
	IRPARSE_SUCCESS = 0,			///< function excute success
	IRPARSE_ERROR_PARAM = -1		///< parameter is invalid
}irparse_error_t;


#if defined(_WIN32)
	#define IRPARSE_DEBUG(format, ...) irparse_debug_print("libirparse debug [%s:%d/%s] " format "\n", \
															__FILE__,__LINE__, __FUNCTION__, __VA_ARGS__)
	#define IRPARSE_ERROR(format, ...) irparse_error_print("libirparse error [%s:%d/%s] " format "\n", \
															__FILE__,__LINE__, __FUNCTION__, __VA_ARGS__)
#elif defined(linux) || defined(unix)
	#include <libgen.h>
	#define IRPARSE_DEBUG(format, ...) irparse_debug_print("libirparse debug [%s:%d/%s] " format "\n", \
															basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
	#define IRPARSE_ERROR(format, ...) irparse_error_print("libirparse error [%s:%d/%s] " format "\n", \
															basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif

extern void (*irparse_debug_print)(const char* fmt, ...);
extern void (*irparse_error_print)(const char* fmt, ...);


/**
 * @brief Get current libirparse library version
 *
 * @param NULL
 *
 * @return current libirparse library version
 */
DLLEXPORT char* irparse_version(void);


/**
 * @brief convert Y16 array to Y14 array
 *
 * @param[in] y16_data Y16 data source 
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] y14_data Y14 data after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t y16_to_y14(uint16_t* y16_data, int pixel_num, uint16_t* y14_data);


/**
 * @brief convert Y14 array to Y8 array
 *
 * @param[in] y14_data Y14 data source
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] y8_data Y8 data after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t y14_to_y8(uint16_t* y14_data, int pixel_num, uint8_t* y8_data);


/**
 * @brief convert Y14 array to RGB array
 *
 * @param[in] packet_data source Y14 image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] rgb_data RGB image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t y14_to_rgb(uint16_t* packet_data, int pixel_num, uint8_t* rgb_data);


/**
 * @brief convert Y14 array to YUV444 array. Y=Y14toY8 U=128 V=128
 *
 * @param[in] packet_data source Y14 image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] yuv_data YUV444 image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t y14_to_yuv444(uint16_t* packet_data, int pixel_num, uint8_t* yuv_data);


/**
 * @brief convert YUV444 array to YUV422 array
 *
 * @param[in] yuv444_data source YUV444 image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] yuv422_data YUV422 image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t yuv444_to_yuv422(uint8_t* yuv444_data, int pixel_num, uint8_t* yuv422_data);


/**
 * @brief convert YUV422 array to RGB array
 *
 * @param[in] yuv422 source YUV422 image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] rgb_data RGB image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t yuv422_to_rgb(uint8_t* yuv422, int pixel_num, uint8_t* rgb_data);


/**
 * @brief Convert YUV444 array to RGB array
 *
 * @param[in] packet_data source YUV444 image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] rgb_data RGB image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t yuv444_to_rgb(uint8_t* packet_data, int pixel_num, uint8_t* rgb_data);


/**
 * @brief Convert RGB array to BGR array
 *
 * @param[in] rgb_data source RGB image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[out] bgr_data BGR image frame after convertion
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t rgb_to_bgr(uint8_t* rgb_data, int pixel_num, uint8_t* bgr_data);


/**
 * @brief YUV422 array add a Y value
 *
 * @param[in] src source image frame
 * @param[in] pixel_num pixel number of source image frame
 * @param[in] y_value every pixel add y_value to Y component
 * @param[out] dst image frame after add y_value
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t yuv422_add_y(uint8_t* src, int pixel_num, int y_value, uint8_t* dst);


/**
 * @brief Cut the raw frame to image and temperature frames
 *
 * @param[in] raw_data raw data frame from device
 * @param[in] image_byte_size image_byte_size of image frame
 * @param[in] temp_byte_size temp_byte_size of temperature frame
 * @param[out] image_data image frame after cut
 * @param[out] temp_data temperature frame after cut
 *
 * @return see irparse_error_t
 */
DLLEXPORT irparse_error_t raw_data_cut(uint8_t* raw_data, int image_byte_size, int temp_byte_size, \
										uint8_t* image_data, uint8_t* temp_data);


/**
 * @brief Register log function depend on log level
 *
 * @param[in] log_level detail see irparse_log_level_t
 *
 */
DLLEXPORT void irparse_log_register(irparse_log_level_t log_level);

#ifdef __cplusplus
}
#endif

#endif
