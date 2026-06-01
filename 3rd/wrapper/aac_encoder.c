#include "codec.h"
#include "faac.h"

// 初始化AAC编码器
static void *aac_encoder_create(uint32_t sample_rate,
                                uint32_t channels,
                                int adts, // 编码输出的帧是否包含ADTS头 0:不包含/1:包含
                                int aot)  // 音频对象类型 1:MAIN/2:LC/3:SSR/4:LTP
{
    assert(adts == 0 || adts == 1);
    assert(aot >= MAIN && aot <= LTP);

    unsigned long samples_per_frame, max_bytes_per_frame;
    faacEncHandle encoder = faacEncOpen(sample_rate, channels, &samples_per_frame, &max_bytes_per_frame);
    if (!encoder)
    {
        printf("Failed to initialize AAC encoder\n");
        return NULL;
    }

    // 设置编码器参数
    faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(encoder);
    config->inputFormat = FAAC_INPUT_16BIT;
    config->outputFormat = adts;
    config->aacObjectType = aot;
    config->allowMidside = 1;
    config->useLfe = 0;
    config->useTns = 0;

    if (!faacEncSetConfiguration(encoder, config))
    {
        printf("Failed to set AAC encoder configuration\n");
        faacEncClose(encoder);
        return NULL;
    }

    audio_codec_t *codec = (audio_codec_t *)calloc(1, sizeof(audio_codec_t));
    codec->handle = encoder;
    codec->sample_rate = sample_rate;
    codec->channels = channels;
    codec->profile = aot;
    codec->sample_size = samples_per_frame;

    // Build AAC-LC AudioSpecificConfig manually.  FAAC promotes to HE-AAC
    // (SBR, audioObjectType 5) at low sample rates like 8 kHz, producing
    // mp4a.40.5 which Chrome MSE rejects.  The core frames are still AAC-LC
    // (config->aacObjectType = LOW), so the ASC must report audioObjectType 2.
    static const int sr_table[] = {96000,88200,64000,48000,44100,32000,
                                   24000,22050,16000,12000,11025,8000,7350};
    uint8_t freq_idx = 0x0F;
    for (int i = 0; i < 13; i++) {
        if ((int)sample_rate == sr_table[i]) { freq_idx = (uint8_t)i; break; }
    }
    codec->extra_data_size = 2;
    codec->extra_data = (char *)malloc(2);
    codec->extra_data[0] = (char)((aot << 3) | (freq_idx >> 1));
    codec->extra_data[1] = (char)(((freq_idx & 1) << 7) | ((channels & 0x0F) << 3));
    return codec;
}

// 编码PCM数据为AAC
static int aac_encoder_encode(void *encoder,
                              uint8_t *pcm,
                              uint32_t pcm_size,
                              uint8_t *aac_buffer,
                              uint32_t aac_buffer_size)
{
    int bytes_encoded = faacEncEncode((((audio_codec_t *)encoder)->handle),
                                      (int32_t *)pcm,
                                      ((audio_codec_t *)encoder)->sample_size,
                                      aac_buffer,
                                      aac_buffer_size);

    if (bytes_encoded < 0)
    {
        printf("AAC encoding error occurred\n");
        return 0;
    }

    return bytes_encoded;
}

// 销毁编码器
static void aac_encoder_destroy(void *encoder)
{
    faacEncClose(((audio_codec_t *)encoder)->handle);
    free((audio_codec_t *)encoder);
}