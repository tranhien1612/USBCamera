#include <stdio.h> 
#include <netdb.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdint.h>
#include <malloc.h>
#include <pthread.h>
#include "libiruvc.h"
#include "libirtemp.h"
#include "libirparse.h"
#include "libircmd.h"
#include "libiri2c.h"

#define PID_TYPE1	0x5830 
#define PID_TYPE2	0x5840 
#define PID_TYPE3	0x5831
#define PID_TYPE4	0x10F9

#define VID_TYPE1	0x0BDA 
#define VID_TYPE2	0x20B4
 
/*------------------------------------------- UVC Camera --------------------------------------------*/
#define IMAGE_AND_TEMP_OUTPUT	//normal mode:get 1 image frame and temp frame at the same time 
//#define IMAGE_OUTPUT	//only image frame
//#define TEMP_OUTPUT		//only temp frame

#define STREAM_TIME 10000  //unit:s
extern uint8_t is_streaming;

typedef enum{
    NO_ROTATE = 0,
    LEFT_90D,
    RIGHT_90D,
    ROTATE_180D
}RotateSide_t;

typedef enum{
    STATUS_NO_MIRROR_FLIP = 0,
    STATUS_ONLY_MIRROR,
    STATUS_ONLY_FLIP,
    STATUS_MIRROR_FLIP
}MirrorFlipStatus_t;

typedef enum{
    INPUT_FMT_Y14 = 0,
    INPUT_FMT_Y16,
    INPUT_FMT_YUV422,
    INPUT_FMT_YUV444,
    INPUT_FMT_RGB888
}InputFormat_t;

typedef enum{
    OUTPUT_FMT_Y14 = 0,
    OUTPUT_FMT_YUV422,
    OUTPUT_FMT_YUV444,
    OUTPUT_FMT_RGB888,
    OUTPUT_FMT_BGR888,
}OutputFormat_t;

typedef enum
{
    PSEUDO_COLOR_ON=0,
    PSEUDO_COLOR_OFF,
}PseudoColor_t;

typedef enum{
    IMG_ENHANCE_ON = 0,
    IMG_ENHANCE_OFF,
}ImgEnhance_t;


typedef struct {
    float ems;
    float ta;
    float tu;
    float dist;
    float hum;
}NewCorrectParam_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t byte_size;
    RotateSide_t rotate_side;
    MirrorFlipStatus_t mirror_flip_status;
    InputFormat_t  input_format;
    OutputFormat_t  output_format;
    PseudoColor_t  pseudo_color_status;
    ImgEnhance_t   img_enhance_status;
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
    //thread's semaphore
    //sem_t image_sem;
    //sem_t image_done_sem;

    uint8_t* image_tmp_frame1;
    uint8_t* image_tmp_frame2;
    uint8_t is_streaming;
}StreamFrameInfo_t;

typedef enum {
	DEBUG_PRINT = 0,
	ERROR_PRINT,
	NO_PRINT,
}log_level_t;


void print_and_record_version(void){
	puts(irtemp_version());
  puts(irparse_version());
  puts(ircmd_version());
	puts(iruvc_version());
}

void log_level_register(void){
	iruvc_log_register(IRUVC_LOG_ERROR);
	irtemp_log_register(IRTEMP_LOG_ERROR);
	ircmd_log_register(IRCMD_LOG_ERROR);
	irparse_log_register(IRPARSE_LOG_ERROR);
}


int get_dev_index_with_pid_vid(DevCfg_t devs_cfg[]){
    int cur_dev_index = 0;
    for (int i = 0; i < 64; i++){
      if(devs_cfg[i].vid == VID_TYPE1){
        if((devs_cfg[i].pid == PID_TYPE1) || (devs_cfg[i].pid == PID_TYPE2) || (devs_cfg[i].pid == PID_TYPE3)){
          cur_dev_index = i;
          printf("pid: 0x%.04x, vid: 0x%.04x, name: %s\n", devs_cfg[i].pid, devs_cfg[i].vid, devs_cfg[i].name);
          return  cur_dev_index;
        }
      }
      if(devs_cfg[i].vid == VID_TYPE2 && devs_cfg[i].pid == PID_TYPE4){
          cur_dev_index = i;
          printf("name=%s\n", devs_cfg[i].name);
          return  cur_dev_index;
      }

    }
    printf("pid or vid is wrong\n");
    return -1;
}

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

int ir_camera_open(IruvcHandle_t* handle, CameraParam_t* camera_param, int same_idx, int resolution_idx){
    DevCfg_t devs_cfg[64] = {0};
    CameraStreamInfo_t camera_stream_info[32] = { 0 };
    int rst = iruvc_camera_init(handle);
    if (rst < 0){
        printf("uvc_camera_init:%d\n", rst);
        return rst;
    }

    memset(devs_cfg, 0, sizeof(DevCfg_t) * 64); //clear the device list before get list
    rst = iruvc_camera_list(handle, devs_cfg);
    if (rst < 0){
        printf("uvc_camera_list:%d\n", rst);
        return rst;
    }

    int dev_index = get_dev_index_with_pid_vid(devs_cfg);
    
    printf("dev_index = %d\n", dev_index);
    if (dev_index < 0){
        printf("can not get this device!\n");
        return dev_index;
    }
    rst = iruvc_camera_info_get(handle, &(devs_cfg[dev_index]), camera_stream_info);
    if (rst < 0){
        printf("uvc_camera_info_get:%d\n", rst);
        return rst;
    }
    
    rst = iruvc_camera_open_same(handle, devs_cfg[dev_index], same_idx);
    if (rst < 0){
        printf("uvc_camera_open:%d\n", rst);
        return rst;
    }
    
    int i = 0;
    while (camera_stream_info[i].width != 0 && camera_stream_info[i].height != 0){
        printf("width: %d,height: %d\n", camera_stream_info[i].width, camera_stream_info[i].height);
        i++;
    }
    *camera_param = camera_para_set(devs_cfg[dev_index], resolution_idx, camera_stream_info);
    
    return 0;
}

int create_data_demo(StreamFrameInfo_t* info){
	if (info != NULL){
		if (info->raw_frame == NULL && info->image_frame == NULL && \
			info->temp_frame == NULL){
			info->raw_frame = (uint8_t*)malloc(info->camera_param.frame_size);
			info->image_frame = (uint8_t*)malloc(info->image_byte_size);
			info->temp_frame = (uint8_t*)malloc(info->temp_byte_size);
		}
	}
	return 0;
}

void load_stream_frame_info(StreamFrameInfo_t* info){
#if defined(IMAGE_AND_TEMP_OUTPUT)
    info->image_info.width = info->camera_param.width;
    info->image_info.height = info->camera_param.height / 2;
    info->image_info.rotate_side = NO_ROTATE;
    info->image_info.mirror_flip_status = STATUS_NO_MIRROR_FLIP;
    info->image_info.pseudo_color_status = PSEUDO_COLOR_OFF;
    info->image_info.img_enhance_status = IMG_ENHANCE_OFF;
    info->image_info.input_format = INPUT_FMT_YUV422; //only Y14 or Y16 mode can use enhance and pseudo color
    info->image_info.output_format = OUTPUT_FMT_BGR888; //if display on opencv,please select BGR888

    info->temp_info.width = info->camera_param.width;
    info->temp_info.height = info->camera_param.height / 2;
    info->temp_info.rotate_side = NO_ROTATE;
    info->temp_info.mirror_flip_status = STATUS_NO_MIRROR_FLIP;
    info->image_byte_size = info->image_info.width * info->image_info.height * 2;
    info->temp_byte_size = info->image_info.width * info->image_info.height * 2;//no temp frame input
#elif defined(IMAGE_OUTPUT) || defined(TEMP_OUTPUT)
    info->image_info.width = info->camera_param.width;
    info->image_info.height = info->camera_param.height;
    info->image_info.rotate_side = NO_ROTATE;
    info->image_info.mirror_flip_status = STATUS_NO_MIRROR_FLIP;
    info->image_info.pseudo_color_status = PSEUDO_COLOR_OFF;
    info->image_info.img_enhance_status = IMG_ENHANCE_OFF;
    info->image_info.input_format = INPUT_FMT_YUV422; //only Y14 or Y16 mode can use enhance and pseudo color
    info->image_info.output_format = OUTPUT_FMT_BGR888; //if display on opencv,please select BGR888

    info->image_byte_size = info->image_info.width * info->image_info.height * 2;
    info->temp_byte_size = 0;
#endif
    create_data_demo(info);
}


/*
void uvc_zoom(float level){
  basic_zoom_center_factor_set(stream_frame_info->ircmd_handle, level);
  printf("zoomLevel: %f\n", level);
}

uint16_t uvc_point_temp(IrcmdPoint_t point){
  uint16_t result;
  basic_tpd_get_point_temp_info(stream_frame_info->ircmd_handle, point, &result);
  printf("tpd_get_point_temp_info:%.2f\n", (result / 16 - 273.15));
  return result;     
}

TpdLineRectTempInfo_t uvc_rect_temp(IrcmdRect_t rect){
  TpdLineRectTempInfo_t result = {0};
  basic_tpd_get_rect_temp_info(stream_frame_info->ircmd_handle, rect, &result);
 	printf("tpd_get_rect_temp_info:min(%d,%d):%.2f, max(%d,%d):%.2f\n", \
		result.min_temp_point.x, result.min_temp_point.y, \
		(result.temp_info_value.min_temp / 16 - 273.15), \
		result.max_temp_point.x, result.max_temp_point.y, \
		(result.temp_info_value.max_temp / 16 - 273.15));
  return result;
}*/

float zoomLevel = 0.0;
IrcmdPoint_t p = {0, 0};
uint16_t pointValue = 0;
IrcmdRect_t rect = {0, 0, 0, 0};
TpdLineRectTempInfo_t rectValue = {0};

void cmd_handle(StreamFrameInfo_t* handle, int mode){
  if(zoomLevel != 0.0){
 	  basic_zoom_center_factor_set(handle->ircmd_handle, zoomLevel);
	  printf("zoomLevel: %f\n", zoomLevel);
  }
  /*
  if(p != {0, 0}){
    basic_tpd_get_point_temp_info(handle->ircmd_handle, p, &pointValue);
    printf("tpd_get_point_temp_info:%.2f\n", (pointValue / 16 - 273.15));
  }
  if(rect != {0, 0, 0, 0}){
    basic_tpd_get_rect_temp_info(handle->ircmd_handle, rect, &rectValue);
   	printf("tpd_get_rect_temp_info:min(%d,%d):%.2f, max(%d,%d):%.2f\n", \
  		rectValue.min_temp_point.x, rectValue.min_temp_point.y, \
  		(rectValue.temp_info_value.min_temp / 16 - 273.15), \
  		rectValue.max_temp_point.x, rectValue.max_temp_point.y, \
  		(rectValue.temp_info_value.max_temp / 16 - 273.15));
  }*/
}

void* cmd_function(void* threadarg){
  int mode = 0;
	while (1){
		cmd_handle(((StreamFrameInfo_t*)threadarg), mode);
	}
	printf("cmd thread exit!!\n");
	return NULL;
}

void uvc_init(){
  setpriority(PRIO_PROCESS, 0, -20);
  print_and_record_version();
  log_level_register();
  
  int rst, same_idx;
  int resolution_idx = 1;
  
  same_idx = 0;
  StreamFrameInfo_t stream_frame_info = {0};
  IruvcHandle_t* iruvc_handle = iruvc_create_handle();
  printf("thread_function same index:%d\n", same_idx);
  rst = ir_camera_open(iruvc_handle, &stream_frame_info.camera_param, same_idx, resolution_idx);
  if (rst < 0){
		puts("ir camera open failed!\n");
		//getchar();
		return;
	}
  load_stream_frame_info(&stream_frame_info);
  stream_frame_info.iruvc_handle = iruvc_handle;
  stream_frame_info.ircmd_handle = ircmd_create_handle(iruvc_handle, VDCMD_I2C_USB_VDCMD);

  pthread_t tid;
  pthread_create(&tid, NULL, cmd_function, (void*)&stream_frame_info);
  pthread_cancel(tid);
  iruvc_camera_close(iruvc_handle);
}
/*------------------------------------------- UVC Camera --------------------------------------------*/

/*------------------------------------------- TCP Server --------------------------------------------*/
int socket_desc, client_sock;
void* connection_handler(void *client_sock){
  struct sockaddr_in client;
  int len = sizeof(struct sockaddr_in); int sock;
  char buffer[2000];
  while(sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&len)){ 
    puts("Connection accepted");
    while( recv(sock , buffer , 2000 , 0) > 0){
      printf("Rec: %s\n", buffer);
		  memset(buffer, 0, 2000);
    }
  }
  return 0;
}

void tcpServer_init(){
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1){
    printf("Could not create socket");
  }
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8000);
  
  if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
    perror("bind failed. Error");
    return;
  }
  listen(socket_desc , 3);
  puts("Waiting for incoming connections...");
  
  pthread_t thread_id;
  if( pthread_create( &thread_id , NULL , connection_handler, (void*) &client_sock) < 0){
    perror("could not create thread");
    return;
  }
        
}
/*------------------------------------------- TCP Server --------------------------------------------*/

/*------------------------------------------- TCP Client --------------------------------------------*/

void* tcp_handler(void *sock){
  int sockfd = *(int*)sock;
  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5765);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  char buf[1024];
  bool isConnect = false;
  while(1){
   	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) >= 0) {
      printf("connected to the server..\n");
      isConnect = true;
  	}else isConnect = false;
   
    while(isConnect){
      printf("isConnect: %d \n", isConnect);
      int len = read(sockfd, buf, sizeof(buf) - 1);
      for(int i = 0; i < len; i++){
        printf("%.02x ", buf[i]);
      }
  	  //printf("Rev: %x\n", buf);
      memset(buf, 0, 1024);
    }
  }
}

void tcpClient_init(){
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		return;
	}
  pthread_t thread_id;
  if( pthread_create( &thread_id , NULL , tcp_handler, (void*) &sockfd) < 0){
    perror("could not create thread");
    return;
  }
  
}
/*------------------------------------------- TCP Client --------------------------------------------*/


int main(){
  //tcpClient_init();
  //tcpServer_init();
  
  uvc_init();
  
	return 0;
}
