/**
 * Sound library abstraction
 * implemented in OpenAL 1.1
 */

typedef struct float32_vec3_t {
    float32_t x, y, z;
} float32_vec3_t;

typedef uint32_t snd_source_t;
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
typedef struct snd_source_state_t {
    float32_vec3_t position, direction, velocity;
    bool32_t position_relative_to_listener, loop_queued_buffers;
    struct {
        float32_t multiplier, min, max, outer_angle_multiplier;
    } gain;
    struct {
        float32_t reference, max, rolloff_factor;
    } distance;
    struct {
        float32_t inner_angle, outer_angle;
    } cone;
    float32_t pitch_multiplier;
} snd_source_state_t;

snd_result_t snd_init(bool32_t start_default_device);
snd_result_t snd_exit(void);

snd_result_t snd_mic_infos(int *nr_mics, const snd_mic_info_t **infos);
snd_result_t snd_mic_record(int mic_id,  .. );


snd_result_t snd_speaker_infos(int *nr_speakers, const snd_speaker_info_t **infos);
snd_result_t snd_speaker_switch(int speaker_id);


snd_result_t snd_buffer_alloc(snd_format_t format, uint32_t frequency_hz, void* data, size_t size, snd_buffer_t* buffer);
snd_result_t snd_buffer_free(snd_buffer_t buffer);




