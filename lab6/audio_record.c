#include <alsa/asoundlib.h>

#include "audio_util.h"

#define LogE	printf

/*==========================================================*/
static snd_pcm_t *alsa_handler;
static snd_pcm_hw_params_t *alsa_hwparams;
static snd_pcm_sw_params_t *alsa_swparams;
static int byte_per_frame;

static pcm_info_st alsa_info;

int audio_record_init(char *dev, int rate, int channel, int sample_bit)
{
	unsigned int alsa_rate;
	unsigned int alsa_channels;
	snd_pcm_format_t alsa_format;

	int modes;
	int err;

	if(alsa_handler != NULL) return 0;

	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buf_size;

	if(sample_bit == 16) {
		alsa_format = SND_PCM_FORMAT_S16;
	} else {
		LogE("alsa record sample_bit %d error, only 16\n", sample_bit);
		return -1;
	}

	alsa_channels = channel;
	if(alsa_channels != 1) {
		LogE("alsa record channel %d error, only 1\n", alsa_channels);
		return -1;
	}

	alsa_rate = rate;
	if((alsa_rate < 8000)||(alsa_rate > 96000)) {
		LogE("alsa record rate %d error\n", alsa_rate);
		return -1;
	}

	modes = 0; /*SND_PCM_NONBLOCK, SND_PCM_ASYNC*/
	if(dev == NULL) dev = "default";
	if((err = snd_pcm_open(&alsa_handler, dev, SND_PCM_STREAM_CAPTURE, modes)) < 0) {
		LogE("open alsa record device error: %s\n", snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_alloca(&alsa_hwparams);
	snd_pcm_sw_params_alloca(&alsa_swparams);

	//setting hw-parameters
	if((err = snd_pcm_hw_params_any(alsa_handler, alsa_hwparams)) < 0) {
		LogE("alsa record snd_pcm_hw_params_any error\n");
		return -1;
	}
	
	if((err = snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		LogE("alsa record snd_pcm_hw_params_set_access error\n");
		return -1;
	}
	
	if((err = snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams, alsa_format)) < 0) {
		LogE("alsa record snd_pcm_hw_params_set_format error\n");
		return -1;
	}

	if((err = snd_pcm_hw_params_set_channels(alsa_handler, alsa_hwparams, alsa_channels)) < 0) {
		LogE("alsa record snd_pcm_hw_params_set_channels error\n");
		return -1;
	}

	/* 频率允许重采用，还是限制只用硬件采样频率*/
	//if((err = snd_pcm_hw_params_set_rate_resample(alsa_handler, alsa_hwparams, 0)) < 0)
	//	return NULL;

	if((err = snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams, &alsa_rate, NULL)) < 0) {
		LogE("alsa record snd_pcm_hw_params_set_rate_near error\n");
		return -1;
	}

	if((err = snd_pcm_hw_params(alsa_handler, alsa_hwparams)) < 0) {
		LogE("alsa record snd_pcm_hw_params error\n");
		return -1;
	}

	/*----------------------------------------------------------------*/

	if((err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &buf_size)) < 0) {
		LogE("alsa record snd_pcm_hw_params_get_buffer_size error\n");
		return -1;
	}

	if((err = snd_pcm_hw_params_get_period_size(alsa_hwparams, &period_size, NULL)) < 0) {
		LogE("alsa record snd_pcm_hw_params_get_period_size error\n");
		return -1;
	}

	/*----------------------------------------------------------------*/

	if((err = snd_pcm_sw_params_current(alsa_handler, alsa_swparams)) < 0) {
		LogE("alsa record snd_pcm_sw_params_current error\n");
		return -1;
	}

	if((err = snd_pcm_sw_params_set_start_threshold(alsa_handler, alsa_swparams, 0)) < 0) {
		LogE("alsa record snd_pcm_sw_params_set_start_threshold error\n");
		return -1;
	}

	if((err = snd_pcm_sw_params_set_stop_threshold(alsa_handler, alsa_swparams, buf_size)) < 0) {
		LogE("alsa record snd_pcm_sw_params_set_stop_threshold error\n");
		return -1;
	}

	if((err = snd_pcm_sw_params_set_silence_size(alsa_handler, alsa_swparams, 0)) < 0) {
		LogE("alsa record snd_pcm_sw_params_set_silence_size error\n");
		return -1;
	}

	if((err = snd_pcm_sw_params(alsa_handler, alsa_swparams)) < 0) {
		LogE("alsa record snd_pcm_sw_params error\n");
		return -1;
	}

	/*----------------------------------------------------------------*/

	if(snd_pcm_format_width(alsa_format) != sample_bit) {
		LogE("alsa bits per sample %d error\n", snd_pcm_format_width(alsa_format));
		return -1;
	}
	if(alsa_channels != channel) {
		LogE("alsa channel %d error\n", alsa_channels);
		return -1;
	}

	printf("alsa input: rate=%d, channels=%d, format=%s, buffer_size=%d, period_size=%d\n",
		alsa_rate, alsa_channels, snd_pcm_format_name(alsa_format), (int)buf_size, (int)period_size);

	byte_per_frame = ((alsa_channels * sample_bit) >> 3);
	alsa_info.sampleRate = alsa_rate;
	alsa_info.numChannels = alsa_channels;
	alsa_info.bitsPerSample = sample_bit;
	return 0;
}

void audio_recored_uninit(void)
{
	snd_pcm_hw_params_free(alsa_hwparams);
	snd_pcm_sw_params_free(alsa_swparams);
	snd_pcm_close(alsa_handler);
	alsa_handler = NULL;
	return;
}

/*==========================================================*/

int audio_record_start(void)
{
	snd_pcm_prepare(alsa_handler);
	//printf("alsa_input_start\n");
	return 0;
}

int audio_record_stop(void)
{
	snd_pcm_drop(alsa_handler);
	//printf("alsa_input_stop\n");
	return 0;
}

int audio_record_read(void *frame, int frame_num)
{
	snd_pcm_sframes_t num_frames;
	snd_pcm_sframes_t res;
	int bytes_per_sample = byte_per_frame;

	num_frames = frame_num;
	while(num_frames > 0)
	{
		res = snd_pcm_readi(alsa_handler, frame, num_frames);
		if((res == -EINTR)||(res == -EAGAIN)) continue;
		if(res < 0) {
			res = snd_pcm_recover(alsa_handler, res, 0);
			if(res < 0) {
				LogE("read snd_pcm_recover error\n");
				return -1;
			}
			continue;
		}
		if(res == 0) break;
		num_frames -= res;
		frame += res*bytes_per_sample;
	}
	return frame_num-num_frames;
}

/*==========================================================*/

uint8_t * audio_record(int time_ms, pcm_info_st *pcm_info)
{
	memset(pcm_info, 0, sizeof(pcm_info_st));

	if(alsa_handler == NULL) {
		LogE("audio record not init\n");
		return NULL;
	}

	if(byte_per_frame == 0) {
		LogE("audio record init fail\n");
		return NULL;
	}

	int frame_num, n;

	frame_num = (alsa_info.sampleRate * time_ms)/1000;
	if(frame_num <= 0) {
		LogE("audio record frame_num %d\n", frame_num);
		return NULL;
	}

	uint8_t *buf = malloc(frame_num * byte_per_frame);

	audio_record_start();
	n = audio_record_read(buf, frame_num);
	audio_record_stop();

	if(n != frame_num)
	{
		LogE("audio record frame %d != %d\n", n, frame_num);
		if(n <= 0) {
			free(buf);
			return NULL;
		}
	}

	pcm_info->numBytes = n * byte_per_frame;
	pcm_info->sampleRate = alsa_info.sampleRate;
	pcm_info->numChannels = alsa_info.numChannels;
	pcm_info->bitsPerSample = alsa_info.bitsPerSample;
	return buf;
}
