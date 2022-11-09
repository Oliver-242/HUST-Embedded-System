#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

typedef struct {
	uint32_t numBytes;
	uint32_t sampleRate;
	uint16_t numChannels;
	uint16_t bitsPerSample;
} pcm_info_st;

static inline int pcm_get_frame_byte(pcm_info_st *pcm_info) {
	return ((pcm_info->numChannels * pcm_info->bitsPerSample) >> 3);
}

static inline int pcm_get_frame_num(pcm_info_st *pcm_info) {
	return pcm_info->numBytes / ((pcm_info->numChannels * pcm_info->bitsPerSample) >> 3);
}

#define pcm_free_buf(buf) do {if(buf) free(buf);} while(0)

/*-------------------------------------------------------------------------*/

int audio_record_init(char *dev, int rate, int channel, int sample_bit);

uint8_t * audio_record(int time_ms, pcm_info_st *pcm_info);
//返回pcm_buf需要free

/*-------------------------------------------------------------------------*/

uint8_t * pcm_read_wav_file(pcm_info_st *pcm_info, const char *wavFilename);
//返回pcm_buf需要free

void pcm_write_wav_file(uint8_t *pcm_buf, pcm_info_st *pcm_info, const char *wavFilename);

/*-------------------------------------------------------------------------*/

uint8_t * pcm_s16_mono_resample(uint8_t *src_buf, pcm_info_st *src_info, int dst_sample_rate, pcm_info_st *pcm_info);
//返回pcm_buf需要free
