#include "../common/common.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "audio_util.h"

/*语音识别要求的pcm采样频率*/
#define PCM_SAMPLE_RATE 16000 /* 16KHz */

static char * send_to_vosk_server(char *file);

int main(int argc, char *argv[])
{
	audio_record_init(NULL, PCM_SAMPLE_RATE, 1, 16); //单声道，S16采样

	pcm_info_st pcm_info;
	uint8_t *pcm_buf = audio_record(2000, &pcm_info); //录2秒音频

	if(pcm_info.sampleRate != PCM_SAMPLE_RATE) { //实际录音采用率不满足要求时 resample
		uint8_t *pcm_buf2 = pcm_s16_mono_resample(pcm_buf, &pcm_info, PCM_SAMPLE_RATE, &pcm_info);
		pcm_free_buf(pcm_buf);
		pcm_buf = pcm_buf2;
	}

	pcm_write_wav_file(pcm_buf, &pcm_info, "/tmp/test.wav");
	printf("write wav end\n");

	pcm_free_buf(pcm_buf);

	char *rev = send_to_vosk_server("/tmp/test.wav");
	printf("recv from server: %s\n", rev);
	return 0;
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
