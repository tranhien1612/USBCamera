#include <sys/resource.h>  
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <pthread.h>
#include "libiruvc.h"
#include "libirtemp.h"
#include "libirparse.h"
#include "libircmd.h"
/*
sudo apt install libopencv-dev
sudo apt update
sudo apt install gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
*/
#define OPENCV_ENABLE
#ifdef OPENCV_ENABLE
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp> 
#include <opencv2/highgui/highgui_c.h> 
#endif

#define COREECT_TABLE_VERSION_LEN	256
#define COREECT_TABLE_LEN			4*14*64
#define PID_TYPE1	0x5830 
#define PID_TYPE2	0x5840 
#define VID_TYPE	0x0BDA 

// #define IMAGE_AND_TEMP_OUTPUT	//normal mode:get 1 image frame and temp frame at the same time 
// #define IMAGE_OUTPUT	//only image frame
#define TEMP_OUTPUT		//only temp frame

#define STREAM_TIME 10000  //unit:s

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t byte_size;
}FrameInfo_t;

typedef struct {
    IruvcHandle_t* iruvc_handle;
    IrcmdHandle_t* ircmd_handle;
    UserCallback_t callback;
    uint8_t* raw_frame;
    uint8_t* image_frame;
    uint32_t image_byte_size;
    uint8_t* temp_frame;
    uint32_t temp_byte_size;
    FrameInfo_t image_info;
    FrameInfo_t temp_info;
    CameraParam_t camera_param;
    timeval timer;
    uint8_t* image_tmp_frame1;
    uint8_t* image_tmp_frame2;
    uint8_t is_streaming;
}StreamFrameInfo_t;

#ifdef __cplusplus
extern "C" {
#endif

    std::string pipeline = "appsrc ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast ! rtph264pay config-interval=1 pt=96 ! udpsink host=192.168.1.57 port=5600";

    //create the raw frame/image frame/temperature frame's buffer
    int create_data_demo(StreamFrameInfo_t* stream_frame_info){
        if (stream_frame_info != NULL){
            if (stream_frame_info->raw_frame == NULL && stream_frame_info->image_frame == NULL && \
                stream_frame_info->temp_frame == NULL){
                stream_frame_info->raw_frame = (uint8_t*)malloc(stream_frame_info->camera_param.frame_size);
                stream_frame_info->image_frame = (uint8_t*)malloc(stream_frame_info->image_byte_size);
                stream_frame_info->temp_frame = (uint8_t*)malloc(stream_frame_info->temp_byte_size);
            }
        }
        return 0;
    }

    //recycle the raw frame/image frame/temperature frame's buffer
    int destroy_data_demo(StreamFrameInfo_t* stream_frame_info){
        if (stream_frame_info != NULL){
            if (stream_frame_info->raw_frame != NULL){
                free(stream_frame_info->raw_frame);
                stream_frame_info->raw_frame = NULL;
            }

            if (stream_frame_info->image_frame != NULL){
                free(stream_frame_info->image_frame);
                stream_frame_info->image_frame = NULL;
            }

            if (stream_frame_info->temp_frame != NULL){
                free(stream_frame_info->temp_frame);
                stream_frame_info->temp_frame = NULL;
            }
        }
        return 0;
    }

    //init the display parameters
    void display_init(StreamFrameInfo_t* stream_frame_info){
        int pixel_size = stream_frame_info->image_info.width * stream_frame_info->image_info.height;
        if (stream_frame_info->image_tmp_frame1 == NULL){
            stream_frame_info->image_tmp_frame1 = (uint8_t*)malloc(pixel_size * 3);
        }
        if (stream_frame_info->image_tmp_frame2 == NULL){
            stream_frame_info->image_tmp_frame2 = (uint8_t*)malloc(pixel_size * 3);
        }
    }

    //recyle the display parameters
    void display_release(StreamFrameInfo_t* stream_frame_info){
        if (stream_frame_info->image_tmp_frame1 != NULL){
            free(stream_frame_info->image_tmp_frame1);
            stream_frame_info->image_tmp_frame1 = NULL;
        }

        if (stream_frame_info->image_tmp_frame2 != NULL){
            free(stream_frame_info->image_tmp_frame2);
            stream_frame_info->image_tmp_frame2 = NULL;
        }
    }

	//load the stream frame info
	void load_stream_frame_info(StreamFrameInfo_t* stream_frame_info){
#if defined(IMAGE_AND_TEMP_OUTPUT)
		stream_frame_info->image_info.width = stream_frame_info->camera_param.width;
		stream_frame_info->image_info.height = stream_frame_info->camera_param.height / 2;

		stream_frame_info->temp_info.width = stream_frame_info->camera_param.width;
		stream_frame_info->temp_info.height = stream_frame_info->camera_param.height / 2;
		stream_frame_info->image_byte_size = stream_frame_info->image_info.width * stream_frame_info->image_info.height * 2;
		stream_frame_info->temp_byte_size = stream_frame_info->image_info.width * stream_frame_info->image_info.height * 2; //no temp frame input

#elif defined(IMAGE_OUTPUT) || defined(TEMP_OUTPUT)
        stream_frame_info->image_info.width = stream_frame_info->camera_param.width;
        stream_frame_info->image_info.height = stream_frame_info->camera_param.height;
        stream_frame_info->image_byte_size = stream_frame_info->image_info.width * stream_frame_info->image_info.height * 2;
        stream_frame_info->temp_byte_size = 0;
#endif
		create_data_demo(stream_frame_info);
	}

    //temperature value to actual temp(Celsius)
    float temp_value_converter(uint16_t temp_val){
        return ((double)temp_val / 64 - 273.15);
    }

    //detect the point's temperature
    void point_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res, uint16_t* correct_table, uint32_t table_len){
        Dot_t point = { 200,100 };
        uint16_t old_temp = 0;
        float new_temp = 0;
        EnvCorrectParam env_correct_param;
        env_correct_param = { 0.5, 1, 1, 25, 26};
        if (get_point_temp(temp_data, temp_res, point, &old_temp) == IRTEMP_SUCCESS){
            printf("point(%d,%d)old_temp:%.2f\n", point.x, point.y, temp_value_converter(old_temp));
            enhance_distance_temp_correct(&env_correct_param, correct_table, table_len, temp_value_converter(old_temp), &new_temp);
            printf("point(%d,%d)new_temp:%.2f\n", point.x, point.y, new_temp);
        }
    }


    void line_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res){
        Line_t line = { 128,191, 128,0, };
        //Line_t line = { 191,128,0,128 };
        //Line_t line = { 20,50,128,30, };
        TempInfo_t temp_info = { 0 };

        if (get_line_temp(temp_data, temp_res, line, &temp_info) == IRTEMP_SUCCESS)
        {
            printf("current line temp: max=%f, min=%f, avr=%f\n", \
                temp_value_converter(temp_info.max_temp), \
                temp_value_converter(temp_info.min_temp), \
                temp_value_converter(temp_info.avr_temp));
            //printf("maxtemp_cord(%d,%d)\n", temp_info.max_cord.x, temp_info.max_cord.y);
            //printf("mintemp_cord(%d,%d)\n", temp_info.min_cord.x, temp_info.min_cord.y);
        }
    }

    //detect the rectangle's temperature
    void rect_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res){
        Area_t rect = { 50,50,20,20 };
        TempInfo_t temp_info = { 0 };

        if (get_rect_temp(temp_data, temp_res, rect, &temp_info) == IRTEMP_SUCCESS){
            printf("rectangle temp: max=%f, min=%f, avr=%f\n", \
                temp_value_converter(temp_info.max_temp), \
                temp_value_converter(temp_info.min_temp), \
                temp_value_converter(temp_info.avr_temp));
        }
    }


    //get specific device via pid&vid from all devices
    int get_dev_index_with_pid_vid(DevCfg_t devs_cfg[]){
        int cur_dev_index = 0;
        for (int i = 0; i < 64; i++){
            if ((devs_cfg[i].vid == VID_TYPE) && ((devs_cfg[i].pid == PID_TYPE1) || (devs_cfg[i].pid == PID_TYPE2))){
                cur_dev_index = i;
                printf("name=%s\n", devs_cfg[i].name);
                return  cur_dev_index;
            }
        }
        printf("PID or VID is wrong!\n");
        return -1;
    }

    //display the frame by opencv 
    void display_one_frame(StreamFrameInfo_t* stream_frame_info, const char* title){
        if (stream_frame_info == NULL){
            return;
        }

        int pix_num = stream_frame_info->image_info.width * stream_frame_info->image_info.height;
        int width = stream_frame_info->image_info.width;
        int height = stream_frame_info->image_info.height;
        //yuv422_to_rgb(stream_frame_info->temp_frame, pix_num, stream_frame_info->image_tmp_frame2);
        stream_frame_info->image_info.byte_size = pix_num * 3;
        yuv422_to_rgb(stream_frame_info->image_frame, pix_num, stream_frame_info->image_tmp_frame2);
#ifdef OPENCV_ENABLE
        cv::Mat image = cv::Mat(height, width, CV_8UC3, stream_frame_info->image_tmp_frame2);
        // cv::imshow(title, image);
        // cvWaitKey(5);
        cv::VideoWriter writer(pipeline, 0, 30, image.size(), true);
        if (!writer.isOpened()) {
            std::cerr << "Error: Could not open GStreamer pipeline!" << std::endl;
        }
        writer.write(image);
#endif
     }


    //set the camera_param from camera_stream_info and stream_index
    CameraParam_t camera_para_set(DevCfg_t dev_cfg, int stream_index, CameraStreamInfo_t camera_stream_info[])
    {
        CameraParam_t camera_param = { 0 };
        camera_param.dev_cfg = dev_cfg;
        camera_param.format = camera_stream_info[stream_index].format;
        camera_param.width = camera_stream_info[stream_index].width;
        camera_param.height = camera_stream_info[stream_index].height;
        camera_param.frame_size = camera_param.width * camera_param.height * 2;
        camera_param.fps = camera_stream_info[stream_index].fps[0];
        camera_param.timeout_ms_delay = 1000;

        return camera_param;
    }

    //open camera device by camera_param
    int ir_camera_open(IruvcHandle_t* handle, CameraParam_t* camera_param, int same_idx, int resolution_idx){
        DevCfg_t devs_cfg[64] = { 0 };
        CameraStreamInfo_t camera_stream_info[32] = { 0 };
        int rst = iruvc_camera_init(handle);
        if (rst < 0)
        {
            printf("uvc_camera_init:%d\n", rst);
            return rst;
        }
        memset(devs_cfg, 0, sizeof(DevCfg_t) * 64); //clear the device list before get list
        rst = iruvc_camera_list(handle, devs_cfg);
        if (rst < 0)
        {
            printf("uvc_camera_list:%d\n", rst);
            return rst;
        }

        int dev_index = 0;

        dev_index = get_dev_index_with_pid_vid(devs_cfg);

        if (dev_index < 0)
        {
            printf("can not get this device!\n");
            return dev_index;
        }
        printf("cur_index=%d\n", dev_index);

        rst = iruvc_camera_info_get(handle, &(devs_cfg[dev_index]), camera_stream_info);
        if (rst < 0)
        {
            printf("uvc_camera_info_get:%d\n", rst);
            return rst;
        }

        rst = iruvc_camera_open_same(handle, devs_cfg[dev_index], same_idx);
        if (rst < 0)
        {
            printf("uvc_camera_open:%d\n", rst);
            return rst;
        }

        int i = 0;
        while (camera_stream_info[i].width != 0 && camera_stream_info[i].height != 0)
        {
            printf("width: %d,height: %d\n", camera_stream_info[i].width, camera_stream_info[i].height);
            i++;
        }
        *camera_param = camera_para_set(devs_cfg[dev_index], resolution_idx, camera_stream_info);
        return 0;
    }

    //stream start by stream_frame_info
    int ir_camera_stream_on(StreamFrameInfo_t* stream_frame_info){
        int rst;

        stream_frame_info->callback.iruvc_handle = stream_frame_info->iruvc_handle;
        stream_frame_info->callback.usr_func = NULL;
        stream_frame_info->callback.usr_param = NULL;

        rst = iruvc_camera_stream_start(stream_frame_info->iruvc_handle, stream_frame_info->camera_param, \
            & stream_frame_info->callback);
        if (rst < 0)
        {
            printf("uvc_camera_stream_start:%d\n", rst);
            return rst;
        }
        stream_frame_info->is_streaming = 1;
        return rst;
    }

    //stream stop
    int ir_camera_stream_off(StreamFrameInfo_t* stream_frame_info){
        int rst = 0;

        rst = iruvc_camera_stream_close(stream_frame_info->iruvc_handle, CLOSE_CAM_SIDE_PREVIEW);
        if (rst < 0)
        {
            return rst;
        }

        destroy_data_demo(stream_frame_info);
        stream_frame_info->is_streaming = 0;
        iruvc_camera_close(stream_frame_info->iruvc_handle);
        return rst;
    }

    void* stream_operation(StreamFrameInfo_t* stream_frame_info, uint16_t* correct_tablel, uint32_t table_len){
        TempDataRes_t temp_res;
        char title[80];
        int same_idx = 0;
        same_idx = iruvc_get_same_idx(stream_frame_info->iruvc_handle);
        sprintf(title, "Image%d", same_idx);
        if (stream_frame_info == NULL){
            return NULL;
        }

        printf("fps=%d\n", stream_frame_info->camera_param.fps);
        int i = 0;
        int r = 0;
        int overtime_cnt = 0;
        int overtime_threshold = 2;     
        while (stream_frame_info->is_streaming && (i <= STREAM_TIME * stream_frame_info->camera_param.fps)){//display stream_time seconds
            r = iruvc_frame_get(stream_frame_info->iruvc_handle, stream_frame_info->raw_frame);
            if (r < 0){
                overtime_cnt++;
            }else{
                overtime_cnt = 0;
            }
            if (r < 0 && overtime_cnt >= overtime_threshold){
                ir_camera_stream_off(stream_frame_info);
                printf("uvc_frame_get failed\n ");
                return NULL;
            }
            if (stream_frame_info->raw_frame != NULL){
                raw_data_cut((uint8_t*)stream_frame_info->raw_frame, stream_frame_info->image_byte_size, \
                    stream_frame_info->temp_byte_size, (uint8_t*)stream_frame_info->image_frame, \
                    (uint8_t*)stream_frame_info->temp_frame);
#if defined(IMAGE_AND_TEMP_OUTPUT)                
                display_one_frame(stream_frame_info, title);
                if (i > 100 ){
                 temp_res = { stream_frame_info->temp_info.width, stream_frame_info->temp_info.height };
                 point_temp_demo((uint16_t*)stream_frame_info->temp_frame, temp_res, correct_tablel, table_len);
                }
#elif defined(IMAGE_OUTPUT)
                basic_y16_preview(stream_frame_info->ircmd_handle, BASIC_Y16_MODE_YUV);
                display_one_frame(stream_frame_info, title);
#elif defined(TEMP_OUTPUT) 
                basic_y16_preview(stream_frame_info->ircmd_handle, BASIC_Y16_MODE_TEMPERATURE); //BASIC_Y16_MODE_YUV, BASIC_Y16_MODE_TEMPERATURE
                display_one_frame(stream_frame_info, title);
                // if (i > 3){
                //     temp_res = { stream_frame_info->image_info.width, stream_frame_info->image_info.height };
                //     point_temp_demo((uint16_t*)stream_frame_info->image_frame, temp_res, correct_tablel, table_len);
                // }
#endif
            }
            i++;
        }

        ir_camera_stream_off(stream_frame_info);
        printf("stream thread exit!!\n");
        return NULL;
    }

	int main(void)
	{
		//set priority to highest level
		setpriority(PRIO_PROCESS, 0, -20);
		int rst, same_idx, resolution_idx;

#if defined(IMAGE_AND_TEMP_OUTPUT)
        resolution_idx = 1;
#elif defined(IMAGE_OUTPUT) || defined(TEMP_OUTPUT)
        resolution_idx = 0;
#endif
		//open camera 0
		same_idx = 0;
		StreamFrameInfo_t stream_frame_info = { 0 };
		IruvcHandle_t* iruvc_handle = iruvc_create_handle();
		printf("thread_function same index:%d\n", same_idx);
		rst = ir_camera_open(iruvc_handle, &stream_frame_info.camera_param, same_idx, resolution_idx);
		if (rst < 0)
		{
			puts("ir camera open failed!\n");
			getchar();
			return 0;
		}

		load_stream_frame_info(&stream_frame_info);

		stream_frame_info.iruvc_handle = iruvc_handle;
        stream_frame_info.ircmd_handle = ircmd_create_handle(iruvc_handle, VDCMD_I2C_USB_VDCMD);

		display_init(&stream_frame_info);
        
		rst = ir_camera_stream_on(&stream_frame_info);
		if (rst < 0){
			puts("ir camera stream on failed!\n");
			getchar();
			return 0;
		} 
        //get correct table from tau_H.bin
        uint16_t* correct_table = NULL;
        uint32_t table_len;
        FILE* fp;
        if ((fp = fopen("tau_H.bin", "rb")) == NULL){
            puts("Fail to open file!");
        }
        fseek(fp, 0, SEEK_END);
        table_len = ftell(fp);
        correct_table = (uint16_t*)malloc((table_len));
        fseek(fp, 0, SEEK_SET);
        fread(correct_table, sizeof(uint8_t), table_len, fp);
        fclose(fp);

        //start display frame and get temperature for temperature correction
		stream_operation(&stream_frame_info, correct_table, table_len);

		display_release(&stream_frame_info);
        free(correct_table);
		puts("EXIT");
		getchar();
		return 0;
	}

#ifdef __cplusplus
}
#endif
