#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <vosk_api.h>
#include "yyjson.c"

#define IP "127.0.0.1"
#define PORT 8011
#define CONF_WORDS "words.txt"

const int BACKLOG = 5;
const int BUFFER_LEN = 1024;
const int WAVE_BUFFER_LEN = 3200;

VoskModel *model;
VoskRecognizer *recognizer;

void print_err(char *str, int line, int err_no)
{
	printf("%d, %s :%s\n", line, str, strerror(err_no));
	_exit(-1);
}

void *receive(void *pth_arg)
{
	int ret = 0;
	long cfd = (long)pth_arg;
	char buf[BUFFER_LEN];
    FILE *wavin;
    char wave_buf[WAVE_BUFFER_LEN];
    int nread, final;
    const char* json;
    const char* parseText;

	while(1) {
		sleep(1);
		bzero(&buf, sizeof(buf));
		ret = recv(cfd, &buf, sizeof(buf), 0);	
		if (ret < 0) {
			print_err("recv failed", __LINE__, errno);
			close(cfd);
			pthread_exit(NULL);
		} else if (ret > 0) {
            printf("recv from client %s \n", buf);
            if(access(buf, F_OK)) {
                printf("%d, %s :%s\n", __LINE__, "file not exists", strerror(errno));
                parseText = "file not exists";
            } else {
                wavin = fopen(buf, "rb");
                fseek(wavin, 44, SEEK_SET);
                while (!feof(wavin)) {
                    nread = fread(wave_buf, 1, sizeof(wave_buf), wavin);
                    final = vosk_recognizer_accept_waveform(recognizer, wave_buf, nread);
                }
                json = vosk_recognizer_final_result(recognizer);

                yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
                yyjson_val *root = yyjson_doc_get_root(doc);

                yyjson_val *text = yyjson_obj_get(root, "text");
                parseText = yyjson_get_str(text);
                printf("recognize_text: %s\n", yyjson_get_str(text));
            }
			ret = send(cfd, parseText, strlen(parseText) + 1, 0);
			if (ret == -1) print_err("send failed", __LINE__, errno);
        } else {
			shutdown(cfd, SHUT_RDWR);
		}
	}
	if(cfd) close(cfd);
}

int main()
{
    FILE * fp;
    char * words = NULL;
    size_t len = 0;
    size_t read;

    fp = fopen(CONF_WORDS, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    read = getline(&words, &len, fp);
    if(read != -1) {
        printf("load words: %s", words);
    } else {
		printf("load words error");
		exit(EXIT_FAILURE);
	}

    // vosk_set_log_level(-1);
    model = vosk_model_new("model");
    recognizer = vosk_recognizer_new_grm(model, 16000.0, words);

	int skfd = -1, ret = -1;
	skfd = socket(AF_INET, SOCK_STREAM, 0);
	if (skfd < 0) {
		print_err("socket failed", __LINE__, errno);
		exit(EXIT_FAILURE);
	}

	int on = 1;
	ret = setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = inet_addr(IP);

	ret = bind(skfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		print_err("bind failed", __LINE__, errno);
		exit(EXIT_FAILURE);
	}
 
	ret = listen(skfd, BACKLOG);
	if (ret < 0) {
		print_err("listen failed", __LINE__, errno);
		exit(EXIT_FAILURE);
	}
	
	long cfd = -1;
	pthread_t id;
	while (1) {
		sleep(1);
		struct sockaddr_in caddr = {0};
		int csize = sizeof(caddr);
		cfd = accept(skfd, (struct sockaddr*)&caddr, &csize);
		if (cfd < 0) {
			print_err("accept failed", __LINE__, errno);
			continue;
		}
		printf("client_port = %d, client_ip = %s\n", ntohs(caddr.sin_port), inet_ntoa(caddr.sin_addr));
	
		int ret = pthread_create(&id, NULL, receive, (void*)cfd);
		if(ret < 0) {
			print_err("accept failed", __LINE__, errno);
			continue;
		} else if (ret == 0) {
			pthread_detach(id);
		}
	}

	close(skfd);
	fclose(fp);
	if (words) free(words);
    vosk_recognizer_free(recognizer);
    vosk_model_free(model);
	return 0;
}
