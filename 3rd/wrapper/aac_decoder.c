#include "codec.h"
#include "neaacdec.h"

static void *aac_decoder_create(uint8_t *adts, uint32_t adts_size, uint8_t *audio_config, uint32_t config_size)
{
    assert(adts != NULL || audio_config != NULL);

    NeAACDecHandle handle = NeAACDecOpen();

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(handle);
    config->outputFormat = FAAD_FMT_16BIT;
    // config->downMatrix = 1;

    if (!NeAACDecSetConfiguration(handle, config))
    {
        printf("AAC decoder configuration failed\n");
        return NULL;
    }

    long code = 0;
    unsigned long sample_size = 0;
    unsigned char channels = 0;
    if (adts != NULL)
    {
        code = NeAACDecInit(handle, adts, adts_size, &sample_size, &channels);
    }
    else if (audio_config != NULL)
    {
        code = NeAACDecInit2(handle, audio_config, config_size, &sample_size, &channels);
    }

    audio_codec_t *decoder = (audio_codec_t *)calloc(1, sizeof(audio_codec_t));
    decoder->handle = handle;
    decoder->sample_rate = sample_size;
    decoder->channels = channels;
    return decoder;
}

static void aac_decoder_destroy(void *decoder)
{
    NeAACDecClose(((audio_codec_t *)decoder)->handle);
    free(((audio_codec_t *)decoder));
}

static int aac_decoder_decode(void *decoder, uint8_t *frame, uint32_t size, uint8_t *pcm)
{
    NeAACDecFrameInfo info;
    void *sample_buffer = NeAACDecDecode(((audio_codec_t *)decoder)->handle, &info, frame, size);

    if (info.error != 0)
    {
        printf("AAC decoding error: %s\n", NeAACDecGetErrorMessage(info.error));
        return 0;
    }

    if (!sample_buffer || info.samples == 0)
    {
        return 0;
    }

    if (sample_buffer && pcm)
    {
        size_t pcm_size = info.samples * 2; // 16位PCM
        memcpy(pcm, sample_buffer, pcm_size);
        return pcm_size;
    }
    return 0;
}