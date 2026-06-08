#include "codec.h"
#include "neaacdec.h"

static void *aac_decoder_create(uint8_t *adts, uint32_t adts_size, uint8_t *audio_config, uint32_t config_size)
{
    assert(adts != NULL || audio_config != NULL);

    NeAACDecHandle handle = NeAACDecOpen();

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(handle);
    config->outputFormat = FAAD_FMT_16BIT;
    // JT1078 audio is plain AAC-LC (typically 8kHz mono). faad2 otherwise applies
    // implicit SBR upsampling (doubling the rate) and Parametric Stereo, emitting e.g.
    // 16kHz/2ch PCM that no longer matches the stream's declared rate/channels — the
    // downstream Opus/AAC encoder then reinterprets it and the audio becomes garbage.
    config->dontUpSampleImplicitSBR = 1;
    config->downMatrix = 1;

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
        // faad2 can emit Parametric-Stereo 2ch PCM from a mono AAC-LC stream (JT1078 audio
        // is mono voice). Downmix back to mono so the output matches the stream's declared
        // mono layout; otherwise the interleaved stereo is fed to a mono encoder as twice as
        // many samples and the audio is garbled.
        if (info.channels == 2)
        {
            const int16_t *s = (const int16_t *)sample_buffer;
            int16_t *d = (int16_t *)pcm;
            unsigned long frames = info.samples / 2; // samples per channel
            for (unsigned long i = 0; i < frames; i++)
            {
                d[i] = (int16_t)(((int)s[2 * i] + (int)s[2 * i + 1]) / 2);
            }
            return (int)(frames * 2); // mono 16-bit bytes
        }

        size_t pcm_size = info.samples * 2; // 16位PCM
        memcpy(pcm, sample_buffer, pcm_size);
        return pcm_size;
    }
    return 0;
}