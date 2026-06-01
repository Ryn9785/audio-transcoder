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

    unsigned char *ppBuffer = NULL;
    unsigned long pSizeOfDecoderSpecificInfo = 0;
    faacEncGetDecoderSpecificInfo(encoder, &ppBuffer, &pSizeOfDecoderSpecificInfo);

    if (ppBuffer && pSizeOfDecoderSpecificInfo > 0)
    {
        codec->extra_data_size = pSizeOfDecoderSpecificInfo;
        codec->extra_data = (char *)malloc(codec->extra_data_size);
        if (codec->extra_data)
        {
            memcpy(codec->extra_data, ppBuffer, codec->extra_data_size);
        }
        else
        {
            printf("Failed to allocate memory for extra data\n");
            faacEncClose(encoder);
            free(codec);
            return NULL;
        }
    }
    else
    {
        codec->extra_data_size = 0;
        codec->extra_data = NULL;
    }
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