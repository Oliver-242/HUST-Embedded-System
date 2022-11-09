#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "audio_util.h"

/*=================================================================================*/
#define AUDIO_FORMAT_PCM 1
// WAV文件头(44字节)
typedef struct {
    /* wav header chunk */
    uint8_t riffChunkId[4]; // RIFF chunk的id "RIFF"
    uint32_t riffChunkDataSize; //RIFF chunk的data大小,
    //下一个字节到文件尾的长度(文件总长度减去8字节)
    uint8_t format[4]; // "WAVE"

    /* fmt chunk */
    uint8_t fmtChunkId[4]; // fmt chunk的id "fmt "
    uint32_t fmtChunkDataSize; //fmt chunk的data大小,
    //下一个字节到该chunk尾的长度(存储PCM数据时，是16)
    uint16_t audioFormat; // 音频编码，1表示PCM，3表示Floating Point
    uint16_t numChannels; // 声道数
    uint32_t sampleRate; // 采样率
    uint32_t byteRate; // 字节率 = sampleRate * blockAlign
    uint16_t blockAlign; // 一个样本的字节数 = numChannels*bitsPerSample>>3
    uint16_t bitsPerSample; // 位深度

    /* data chunk */
    uint8_t dataChunkId[4]; // data chunk的id  "data"
    uint32_t dataChunkDataSize; //data chunk的data大小,
    //下一个字节到该chunk尾的长度, 音频数据的总长度, 即文件总长度减去该文件头的长度(44)
} WAVHeader;

void pcm_write_wav_file(uint8_t *pcm_buf, pcm_info_st *pcm_info, const char *wavFilename)
{
	if((pcm_info->numChannels < 1)||(pcm_info->numChannels > 2)) {
		printf("%s numChannels %d error\n", __func__, pcm_info->numChannels);
		return;
	}
	if((pcm_info->sampleRate < 8000)||(pcm_info->sampleRate > 96000)) {
		printf("%s sampleRate %d error\n", __func__, pcm_info->sampleRate);
		return;
	}
	if((pcm_info->bitsPerSample != 8)&&(pcm_info->bitsPerSample != 16)&&
		(pcm_info->bitsPerSample != 24)&&(pcm_info->bitsPerSample != 32))
	{
		printf("%s bitsPerSample %d error\n", __func__, pcm_info->bitsPerSample);
		return;
	}

	WAVHeader header = {
        .riffChunkId = {'R', 'I', 'F', 'F'},
        .riffChunkDataSize = 0,
        .format = {'W', 'A', 'V', 'E'},
        .fmtChunkId = {'f', 'm', 't', ' '},
        .fmtChunkDataSize = 16,
        .audioFormat = AUDIO_FORMAT_PCM,
        .numChannels = pcm_info->numChannels,
        .sampleRate = pcm_info->sampleRate,
        .byteRate = 0,
        .blockAlign = 0,
        .bitsPerSample = pcm_info->bitsPerSample,
        .dataChunkId = {'d', 'a', 't', 'a'},
        .dataChunkDataSize = 0,
    };

    header.blockAlign = (pcm_info->numChannels * pcm_info->bitsPerSample) >> 3;
    header.byteRate = pcm_info->sampleRate * header.blockAlign;
    header.dataChunkDataSize = pcm_info->numBytes;
    header.riffChunkDataSize = header.dataChunkDataSize + sizeof (WAVHeader) - 8;

    FILE *wavOut = fopen(wavFilename, "wb");
    if(!wavOut) {
        printf("%s open file %s error\n", __func__, wavFilename);
        return;
    }
    size_t res = fwrite(&header, sizeof(WAVHeader), 1, wavOut);
    if(res != 1) {
        printf("%s write file %s head error\n", __func__, wavFilename);
        fclose(wavOut);
        return;
    }
    res = fwrite(pcm_buf, 1, header.dataChunkDataSize, wavOut);
    if(res != header.dataChunkDataSize) {
        printf("%s write file %s data error\n", __func__, wavFilename);
    }
    fclose(wavOut);
    return;
}

uint8_t * pcm_read_wav_file(pcm_info_st *pcm_info, const char *wavFilename)
{
	memset(pcm_info, 0, sizeof(pcm_info_st));

	FILE *fp = fopen(wavFilename, "rb");
	if(!fp) {
		printf("%s open file %s error\n", __func__, wavFilename);
		return NULL;
	}

	WAVHeader header;
	int ret = fread(&header, sizeof(header), 1, fp);
	if(ret != 1) {
		fclose(fp);
		printf("%s read file %s head error\n", __func__, wavFilename);
		return NULL;
	}

	if(header.audioFormat != AUDIO_FORMAT_PCM) {
		fclose(fp);
		printf("%s file %s head audioFormat error\n", __func__, wavFilename);
		return NULL;
	}
	if(header.blockAlign != (header.numChannels * header.bitsPerSample) >> 3) {
		fclose(fp);
		printf("%s file %s head blockAlign error\n", __func__, wavFilename);
		return NULL;
	}

	pcm_info->bitsPerSample = header.bitsPerSample;
   	pcm_info->numChannels = header.numChannels;
	pcm_info->sampleRate = header.sampleRate;
	int len = (header.dataChunkDataSize / header.blockAlign) * header.blockAlign;
	pcm_info->numBytes = len;

	uint8_t *pcm_buf = malloc(len);
	if(pcm_buf == NULL) {
		fclose(fp);
		printf("%s file %s malloc %d error\n", __func__, wavFilename, len);
		return NULL;
	}

	ret = fread(pcm_buf, 1, len, fp);
	if(ret != len) {
		fclose(fp);
		printf("%s file %s read data %d error\n", __func__, wavFilename, len);
		free(pcm_buf);
		return NULL;
	}
	fclose(fp);
	return pcm_buf;
}

/*=================================================================================*/

uint8_t * pcm_s16_mono_resample(uint8_t *src_buf, pcm_info_st *src_info, int dst_sample_rate, pcm_info_st *pcm_info)
{
	memset(pcm_info, 0, sizeof(pcm_info_st));

	if(src_info->numChannels != 1) {
		printf("resample src channels %d error\n", src_info->numChannels);
		return NULL;
	}
	if(src_info->bitsPerSample != 16) {
		printf("resample src bitsPerSample %d error\n", src_info->bitsPerSample);
		return NULL;
	}

	uint32_t src_sample_rate = src_info->sampleRate;
	uint32_t src_frame_n = pcm_get_frame_num(src_info);
	uint32_t dst_frame_n =  src_frame_n * dst_sample_rate / src_sample_rate;

	pcm_info->numChannels = 1;
	pcm_info->bitsPerSample = 16;
	pcm_info->sampleRate = dst_sample_rate;
	pcm_info->numBytes = dst_frame_n * 2;

	int16_t *src = (int16_t *)src_buf;
	int16_t *dst = malloc(pcm_info->numBytes);
	if(dst == NULL) {
		printf("malloc %d error\n", pcm_info->numBytes);
		return NULL;
	}

	int di, si;
	for(di=0; di<dst_frame_n; ++di) {
		si = di * src_sample_rate / dst_sample_rate;
		dst[di] = src[si];
	}
	return (uint8_t *)dst;
}
