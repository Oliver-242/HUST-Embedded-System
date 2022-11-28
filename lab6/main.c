#include "../common/common.h"
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include "audio_util.h"
#include <math.h>

#define RED	FB_COLOR(255,0,0)
#define ORANGE	FB_COLOR(255,165,0)
#define YELLOW	FB_COLOR(255,255,0)
#define GREEN	FB_COLOR(0,255,0)
#define CYAN	FB_COLOR(0,127,255)
#define BLUE	FB_COLOR(0,0,255)
#define PURPLE	FB_COLOR(139,0,255)
#define WHITE   FB_COLOR(255,255,255)
#define BLACK   FB_COLOR(0,0,0)

/*语音识别要求的pcm采样频率*/
#define PCM_SAMPLE_RATE 16000 /* 16KHz */

static char * send_to_vosk_server(char *file);
extern void imagecentralize(imageplus* , fb_image* );
extern void imagescaling(imageplus* , int );
static void touch_event_cb(int fd);

static int touch_fd, point, type;
static int radius[5] = {45,47,49,51,53};
static struct old_pos{
	int x;
	int y;
} old[5];
static char* jpgs[3] = {"./test.jpg","./test1.jpg","./test2.jpg"};

static fb_image* img;
static imageplus* imgplus;
static pcm_info_st pcm_info;

int main(int argc, char *argv[])
{
	fb_init("/dev/fb0");
	fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,BLACK);
	
	imgplus = (imageplus*)malloc(sizeof(imageplus));
	point = 0;
	
	img = fb_read_jpeg_image(jpgs[point]);
	imagecentralize(imgplus,img);
	fb_draw_rect(0,0,100,100,PURPLE);
	fb_update();

	audio_record_init(NULL, PCM_SAMPLE_RATE, 1, 16); //单声道，S16采样
	
	//打开多点触摸设备文件, 返回文件fd
	touch_fd = touch_init("/dev/input/event1");
	//添加任务, 当touch_fd文件可读时, 会自动调用touch_event_cb函数
	task_add_file(touch_fd, touch_event_cb);
	task_loop(); //进入任务循环

	return 0;
}


static void touch_event_cb(int fd)
{
	int type1,x,y,finger, color;
	type1 = touch_read(fd, &x,&y,&finger);
	switch(type1){
	case TOUCH_PRESS:
		if(x<=100 && y<=100){
			point = (point+1)%3;
			img = fb_read_jpeg_image(jpgs[point]);
			fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,BLACK);
			imagecentralize(imgplus,img);
			fb_draw_rect(0,0,100,100,PURPLE);
		}
		else{
			type = -1;
			printf("\n1秒后开始录制:\n");
			sleep(1);
			printf("开始！\n");
			uint8_t *pcm_buf = audio_record(1800, &pcm_info); //录2秒音频

			if(pcm_info.sampleRate != PCM_SAMPLE_RATE) { //实际录音采用率不满足要求时 resample
				uint8_t *pcm_buf2 = pcm_s16_mono_resample(pcm_buf, &pcm_info, PCM_SAMPLE_RATE, &pcm_info);
				pcm_free_buf(pcm_buf);
				pcm_buf = pcm_buf2;
			}

			pcm_write_wav_file(pcm_buf, &pcm_info, "/tmp/test.wav");
			//printf("write wav end\n");

			pcm_free_buf(pcm_buf);

			char *rev = send_to_vosk_server("/tmp/test.wav");
			printf("recv from server: %s\n", rev);
			if(strcmp(rev, "放大") == 0) type = 0;
			else if(strcmp(rev, "缩小") == 0) type = 1;
			else if(strcmp(rev, "左") == 0) type = 2;
			else if(strcmp(rev, "右") == 0) type = 3;
			else if(strcmp(rev, "上移") == 0) type = 4;
			else if(strcmp(rev, "下移") == 0) type = 5;
			else if(strcmp(rev, "全屏") == 0) type = 6;
			else if(strcmp(rev, "恢复") == 0) type = 7;
			else type = -1;
			imagescaling(imgplus,type);
			printf("完毕!\n");
		}
		break;
	case TOUCH_MOVE:
		break;
	case TOUCH_RELEASE:
		break;
	case TOUCH_ERROR:
		printf("close touch fd\n");
		close(fd);
		task_delete_file(fd);
		break;
	default:
		return;
	}
	fb_update();
	return;
}


/*===============================================================*/	

#define IP "127.0.0.1"
#define PORT 8011

#define print_err(fmt, ...) \
	printf("%d:%s " fmt, __LINE__, strerror(errno), ##__VA_ARGS__);

static char * send_to_vosk_server(char *file)
{
	static char ret_buf[128]; //识别结果

	if((file == NULL)||(file[0] != '/')) {
		print_err("file %s error\n", file);
		return NULL;
	}

	int skfd = -1, ret = -1;
	skfd = socket(AF_INET, SOCK_STREAM, 0);
	if(skfd < 0) {
		print_err("socket failed\n");
		return NULL;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = inet_addr(IP);
	ret = connect(skfd,(struct sockaddr*)&addr, sizeof(addr));
	if(ret < 0) {
		print_err("connect failed\n");
		close(skfd);
		return NULL;
	}

	printf("send wav file name: %s\n", file);
	ret = send(skfd, file, strlen(file)+1, 0);
	if(ret < 0) {
		print_err("send failed\n");
		close(skfd);
		return NULL;
	}

	ret = recv(skfd, ret_buf, sizeof(ret_buf)-1, 0);
	if(ret < 0) {
		print_err("recv failed\n");
		close(skfd);
		return NULL;
	}
	ret_buf[ret] = '\0';
	return ret_buf;
}