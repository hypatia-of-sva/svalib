/**
 * Sound library abstraction
 * implemented in OpenAL 1.1
 */

typedef uint32_t snd_buffer_t;

typedef enum snd_result_t {
    SND_OK = 0,
    SND_ERROR_AL_NOT_PRESENT = -1,
    SND_ERROR_OUT_OF_MEMORY = -2,
    SND_ERROR_UNKNOWN = -3,
    SND_ERROR_INVALID_PARAM = -4,
    SND_ERROR_BUFFER_STILL_IN_USE = -5,

    SND_RESULT_MAX_ENUM = 0x7fffffff
} snd_result_t;
typedef enum snd_format_t {
    SND_FORMAT_PCM_UINT8_MONO = 0,
    SND_FORMAT_PCM_INT16_MONO = 1,
    SND_FORMAT_PCM_UINT8_STEREO_INTERLEAVED_LR = 2,
    SND_FORMAT_PCM_INT16_STEREO_INTERLEAVED_LR = 3,
    SND_FORMAT_MAX_ENUM = 0x7f
}

typedef struct snd_mic_info_t {

} snd_mic_info_t;
typedef struct snd_speaker_info_t {

} snd_speaker_info_t;

snd_result_t snd_init(void);
snd_result_t snd_exit(void);

snd_result_t snd_mic_infos(int *nr_mics, const snd_mic_info_t **infos);
snd_result_t snd_mic_record(int mic_id,  .. );


snd_result_t snd_speaker_infos(int *nr_speakers, const snd_speaker_info_t **infos);
snd_result_t snd_speaker_switch(int speaker_id);


snd_result_t snd_buffer_alloc(snd_format_t format, uint32_t frequency_hz, void* data, size_t size, snd_buffer_t* buffer);
snd_result_t snd_buffer_free(snd_buffer_t buffer);




