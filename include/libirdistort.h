#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#elif defined(linux) || defined(unix)
#define DLLEXPORT
#endif
/**
* @brief Log level definition in irdistort library
*/
typedef enum {
    IRDISTORT_LOG_DEBUG = 0,		///< print debug and error infomation
    IRDISTORT_LOG_ERROR = 1,		///< only print error infomation
    IRDISTORT_LOG_NO_PRINT = 2,	///< don't print debug and error infomation
}irdistort_log_level_t; 



/**
*@brief ���ظ�ʽö��
*/
typedef enum {
    IR_BYTE2_CH1,  /**< �����ڵ�ͨ����ÿ��ͨ��2�ֽڵ����ͣ���Y10��Y16�� */
    IR_BYTE1_CH3,  /**< ��������ͨ����ÿ��ͨ��1�ֽڵ����ͣ���RGB��BGR�� */
}IrUndistort_Pixel_Format;

/**
 * @brief ��ȡ�汾��Ϣ
 *
 * @return version
 */
DLLEXPORT const char* ir_undistort_version();

/**
 * @brief ���ò��м�����߳���
 *
 * @param threads �߳���
 */
DLLEXPORT void ir_undistort_set_threads(int threads);

/**
 * @brief ��ʼ��
 *
 * @param rows ͼ������
 * @param cols ͼ������
 * @param[in] params �������
 * @param pix_fmt ���ظ�ʽ
 */
DLLEXPORT void ir_undistort_init(int rows, int cols, const double* params, IrUndistort_Pixel_Format pix_fmt);

/**
 * @brief �����㷨
 *
 * @param[in] in ����ͼ���׵�ַ��
 * @param[out] out ���ͼ���׵�ַ������ǰ������ڴ��С
 */
DLLEXPORT void ir_undistort_run(const void* in, void* out);

/**
 * @brief ����
 *
 */
DLLEXPORT void ir_undistort_destroy();


/**
 * @brief ���ڰ�Ӣ�Ƶ��豸
 * @param[in] �豸���
 *
 * @return NULL
 */
DLLEXPORT void* ir_distort_bind_infisense_device(void* handle);

#ifdef __cplusplus
}
#endif
