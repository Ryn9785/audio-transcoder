#include "codec.h"
#include "opus.h"

// 创建Opus解码器
static void *opus_decoder_create2(int sample_rate, int channels)
{
    int err;
    OpusDecoder *dec = opus_decoder_create(sample_rate, channels, &err);

    if (err != OPUS_OK || dec == NULL)
    {
        printf("Failed to create Opus decoder: %s\n", opus_strerror(err));
        return NULL;
    }

    audio_codec_t *codec = (audio_codec_t *)calloc(1, sizeof(audio_codec_t));
    codec->handle = dec;
    codec->sample_rate = sample_rate;
    codec->channels = channels;
    return codec;
}

// 解码Opus数据为PCM
static int opus_decoder_decode(void *dec,
                               const uint8_t *data,
                               int data_size,
                               uint8_t *pcm,
                               int frame_size)
{
    int samples = opus_decode(((audio_codec_t *)dec)->handle, data, data_size, (int16_t *)pcm, frame_size, 0);
    if (samples < 0)
    {
        printf("Opus decode error: %s data_size: %d frame_size: %d\n", opus_strerror(samples), data_size, frame_size);
        return 0;
    }

    return samples * ((audio_codec_t *)dec)->channels * 2; // 通道数*采样率
}

// 销毁解码器
static void opus_decoder_destroy2(void *dec)
{
    opus_decoder_destroy(((audio_codec_t *)dec)->handle);
    free(((audio_codec_t *)dec));
}