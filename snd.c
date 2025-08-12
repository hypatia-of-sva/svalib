#include "snd.h"

#define AL_NO_PROTOTYPES
#define ALC_NO_PROTOTYPES
#include <AL/alext.h>
#include <AL/efx-presets.h>

/* Function loading facilities from alad: modelled after GLFW 3.3, see win32_module.c and posix_module.c specifically */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else /* Unix defaults otherwise */
#include <dlfcn.h>
#endif /* _WIN32 */


struct g_al {
    void*                           module;
    struct {
        LPALCCREATECONTEXT               CreateContext;
        LPALCMAKECONTEXTCURRENT          MakeContextCurrent;
        LPALCPROCESSCONTEXT              ProcessContext;
        LPALCSUSPENDCONTEXT              SuspendContext;
        LPALCDESTROYCONTEXT              DestroyContext;
        LPALCGETCURRENTCONTEXT           GetCurrentContext;
        LPALCGETCONTEXTSDEVICE           GetContextsDevice;
        LPALCOPENDEVICE                  OpenDevice;
        LPALCCLOSEDEVICE                 CloseDevice;
        LPALCGETERROR                    GetError;
        LPALCGETSTRING                   GetString;
        LPALCGETINTEGERV                 GetIntegerv;
        LPALCCAPTUREOPENDEVICE           CaptureOpenDevice;
        LPALCCAPTURECLOSEDEVICE          CaptureCloseDevice;
        LPALCCAPTURESTART                CaptureStart;
        LPALCCAPTURESTOP                 CaptureStop;
        LPALCCAPTURESAMPLES              CaptureSamples;
    } c;
    LPALGETSTRING                    GetString;
    LPALGETINTEGER                   GetInteger;
    LPALGETFLOAT                     GetFloat;
    LPALGETERROR                     GetError;
    LPALDOPPLERFACTOR                DopplerFactor;
    LPALSPEEDOFSOUND                 SpeedOfSound;
    LPALDISTANCEMODEL                DistanceModel;
    LPALLISTENERF                    Listenerf;
    LPALLISTENERFV                   Listenerfv;
    LPALGETLISTENERF                 GetListenerf;
    LPALGETLISTENERFV                GetListenerfv;
    LPALGENSOURCES                   GenSources;
    LPALDELETESOURCES                DeleteSources;
    LPALISSOURCE                     IsSource;
    LPALSOURCEF                      Sourcef;
    LPALSOURCEFV                     Sourcefv;
    LPALSOURCEI                      Sourcei;
    LPALGETSOURCEF                   GetSourcef;
    LPALGETSOURCEFV                  GetSourcefv;
    LPALGETSOURCEI                   GetSourcei;
    LPALSOURCEPLAYV                  SourcePlayv;
    LPALSOURCESTOPV                  SourceStopv;
    LPALSOURCEREWINDV                SourceRewindv;
    LPALSOURCEPAUSEV                 SourcePausev;
    LPALSOURCEQUEUEBUFFERS           SourceQueueBuffers;
    LPALSOURCEUNQUEUEBUFFERS         SourceUnqueueBuffers;
    LPALGENBUFFERS                   GenBuffers;
    LPALDELETEBUFFERS                DeleteBuffers;
    LPALISBUFFER                     IsBuffer;
    LPALBUFFERDATA                   BufferData;
    LPALGETBUFFERI                   GetBufferi;
} g_al;

/* minimal version of alad for just core AL/ALC since we don't use more: */
typedef void (*snd_func) (void);
typedef snd_func (*snd_loader) (void* module, const char *name);
void snd_load_al(snd_loader loader) {
    assert(g_al.module != NULL);
    g_al.c.CreateContext      = (LPALCCREATECONTEXT)       loader(g_al.module, "alcCreateContext"));
    g_al.c.MakeContextCurrent = (LPALCMAKECONTEXTCURRENT)  loader(g_al.module, "alcMakeContextCurrent"));
    g_al.c.ProcessContext     = (LPALCPROCESSCONTEXT)      loader(g_al.module, "alcProcessContext"));
    g_al.c.SuspendContext     = (LPALCSUSPENDCONTEXT)      loader(g_al.module, "alcSuspendContext"));
    g_al.c.DestroyContext     = (LPALCDESTROYCONTEXT)      loader(g_al.module, "alcDestroyContext"));
    g_al.c.GetCurrentContext  = (LPALCGETCURRENTCONTEXT)   loader(g_al.module, "alcGetCurrentContext"));
    g_al.c.GetContextsDevice  = (LPALCGETCONTEXTSDEVICE)   loader(g_al.module, "alcGetContextsDevice"));
    g_al.c.OpenDevice         = (LPALCOPENDEVICE)          loader(g_al.module, "alcOpenDevice"));
    g_al.c.CloseDevice        = (LPALCCLOSEDEVICE)         loader(g_al.module, "alcCloseDevice"));
    g_al.c.GetError           = (LPALCGETERROR)            loader(g_al.module, "alcGetError"));
    g_al.c.GetString          = (LPALCGETSTRING)           loader(g_al.module, "alcGetString"));
    g_al.c.GetIntegerv        = (LPALCGETINTEGERV)         loader(g_al.module, "alcGetIntegerv"));
    g_al.c.CaptureOpenDevice  = (LPALCCAPTUREOPENDEVICE)   loader(g_al.module, "alcCaptureOpenDevice"));
    g_al.c.CaptureCloseDevice = (LPALCCAPTURECLOSEDEVICE)  loader(g_al.module, "alcCaptureCloseDevice"));
    g_al.c.CaptureStart       = (LPALCCAPTURESTART)        loader(g_al.module, "alcCaptureStart"));
    g_al.c.CaptureStop        = (LPALCCAPTURESTOP)         loader(g_al.module, "alcCaptureStop"));
    g_al.c.CaptureSamples     = (LPALCCAPTURESAMPLES)      loader(g_al.module, "alcCaptureSamples"));
    g_al.GetString            = (LPALGETSTRING)            loader(g_al.module, "alGetString");
    g_al.GetInteger           = (LPALGETINTEGER)           loader(g_al.module, "alGetInteger");
    g_al.GetFloat             = (LPALGETFLOAT)             loader(g_al.module, "alGetFloat");
    g_al.GetError             = (LPALGETERROR)             loader(g_al.module, "alGetError");
    g_al.DopplerFactor        = (LPALDOPPLERFACTOR)        loader(g_al.module, "alDopplerFactor"));
    g_al.SpeedOfSound         = (LPALSPEEDOFSOUND)         loader(g_al.module, "alSpeedOfSound"));
    g_al.DistanceModel        = (LPALDISTANCEMODEL)        loader(g_al.module, "alDistanceModel"));
    g_al.Listenerf            = (LPALLISTENERF)            loader(g_al.module, "alListenerf"));
    g_al.Listenerfv           = (LPALLISTENERFV)           loader(g_al.module, "alListenerfv"));
    g_al.GetListenerf         = (LPALGETLISTENERF)         loader(g_al.module, "alGetListenerf"));
    g_al.GetListenerfv        = (LPALGETLISTENERFV)        loader(g_al.module, "alGetListenerfv"));
    g_al.GenSources           = (LPALGENSOURCES)           loader(g_al.module, "alGenSources"));
    g_al.DeleteSources        = (LPALDELETESOURCES)        loader(g_al.module, "alDeleteSources"));
    g_al.IsSource             = (LPALISSOURCE)             loader(g_al.module, "alIsSource"));
    g_al.Sourcef              = (LPALSOURCEF)              loader(g_al.module, "alSourcef"));
    g_al.Sourcefv             = (LPALSOURCEFV)             loader(g_al.module, "alSourcefv"));
    g_al.Sourcei              = (LPALSOURCEI)              loader(g_al.module, "alSourcei"));
    g_al.GetSourcef           = (LPALGETSOURCEF)           loader(g_al.module, "alGetSourcef"));
    g_al.GetSourcefv          = (LPALGETSOURCEFV)          loader(g_al.module, "alGetSourcefv"));
    g_al.GetSourcei           = (LPALGETSOURCEI)           loader(g_al.module, "alGetSourcei"));
    g_al.SourcePlayv          = (LPALSOURCEPLAYV)          loader(g_al.module, "alSourcePlayv"));
    g_al.SourceStopv          = (LPALSOURCESTOPV)          loader(g_al.module, "alSourceStopv"));
    g_al.SourceRewindv        = (LPALSOURCEREWINDV)        loader(g_al.module, "alSourceRewindv"));
    g_al.SourcePausev         = (LPALSOURCEPAUSEV)         loader(g_al.module, "alSourcePausev"));
    g_al.SourceQueueBuffers   = (LPALSOURCEQUEUEBUFFERS)   loader(g_al.module, "alSourceQueueBuffers"));
    g_al.SourceUnqueueBuffers = (LPALSOURCEUNQUEUEBUFFERS) loader(g_al.module, "alSourceUnqueueBuffers"));
    g_al.GenBuffers           = (LPALGENBUFFERS)           loader(g_al.module, "alGenBuffers"));
    g_al.DeleteBuffers        = (LPALDELETEBUFFERS)        loader(g_al.module, "alDeleteBuffers"));
    g_al.IsBuffer             = (LPALISBUFFER)             loader(g_al.module, "alIsBuffer"));
    g_al.BufferData           = (LPALBUFFERDATA)           loader(g_al.module, "alBufferData"));
    g_al.GetBufferi           = (LPALGETBUFFERI)           loader(g_al.module, "alGetBufferi"));
}
snd_loader snd_load_al_dll(void) {
/* since we only use core functions, it's enough to load all function pointers once at init from the DLL.
 * This would be different for direct functions, but since they're newer we avoid them here.
 */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
    g_al.module = LoadLibraryA ("OpenAL32.dll");
    if (g_al.module == NULL) {
        g_al.module = LoadLibraryA ("soft_oal.dll");
    }
    if(g_al.module == NULL) {
        return NULL;
    }
    return (snd_loader) GetProcAddress;
#else /* Unix defaults otherwise */
#if defined(__APPLE__)
    /* not tested myself; the only references I could find are https://github.com/ToweOPrO/sadsad and https://pastebin.com/MEmh3ZFr, which is at least tenuous */
    g_al.module = dlopen ("libopenal.1.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (g_al.module == NULL) {
        g_al.module = dlopen ("libopenal.dylib", RTLD_LAZY | RTLD_LOCAL);
    }
    if(g_al.module == NULL) {
        return NULL;
    }
#else /* Linux and BSD */
    g_al.module = dlopen ("libopenal.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (g_al.module == NULL) {
        g_al.module = dlopen ("libopenal.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if(g_al.module == NULL) {
        return NULL;
    }
#endif
    /* there are also libopenal.so.1.[X].[Y] and libopenal.1.[X].[Y].dylib respectively, but it would be difficult to look all of those up */
    return (snd_loader) dlsym;
#endif /* _WIN32 */
}
void snd_unload_al_dll(void) {
    assert(g_al.module != NULL);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
    FreeLibrary(g_al.module);
#else /* Unix defaults otherwise */
    dlclose(g_al.module);
#endif
}


snd_result_t snd_init(void) {
    snd_loader loader = snd_load_al_dll();
    if(loader == NULL) {
        return SND_ERROR_AL_NOT_PRESENT;
    }
    snd_load_al(loader);
    return SND_OK;
}
snd_result_t snd_exit(void) {
    snd_unload_al_dll();
    return SND_OK;
}








snd_result_t snd_buffer_alloc(snd_format_t format, uint32_t frequency_hz, void* data, size_t size, snd_buffer_t* buffer) {
    snd_buffer_t id;
    ALenum al_format;
    ALint bits, channels, i;

    if(buffer == NULL || data == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
    switch(format) {
    case SND_FORMAT_PCM_UINT8_MONO:
        al_format = AL_FORMAT_MOMO8;
        bits = 8; channels = 1;
        break;
    case SND_FORMAT_PCM_INT16_MONO:
        al_format = AL_FORMAT_MOMO16;
        if(size%2 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 16; channels = 1;
        break;
    case SND_FORMAT_PCM_UINT8_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO8;
        if(size%2 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 8; channels = 2;
        break;
    case SND_FORMAT_PCM_INT16_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO16;
        if(size%4 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 16; channels = 2;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.GenBuffers(&id);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(g_al.IsBuffer(id) != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }

    g_al.BufferData(id, al_format, data, size, frequency_hz);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }

    g_al.GetBufferi(id, AL_FREQUENCY, &i)
    if(i != frequency_hz) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_BITS, &i)
    if(i != bits) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_CHANNELS, &i)
    if(i != channels) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_SIZE, &i)
    if(i != size) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_DATA, &i)
    if(i != data) {
        return SND_ERROR_UNKNOWN;
    }

    buffer[0] = id;
    return SND_OK;
}
snd_result_t snd_buffer_free(snd_buffer_t buffer) {
    if(g_al.IsBuffer(buffer) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.DeleteBuffers(1, &buffer);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_INVALID_OPERATION:
        return SND_ERROR_BUFFER_STILL_IN_USE;
    default:
        return SND_ERROR_UNKNOWN;
    }

    return SND_OK;
}

typedef struct float32_vec3_t {
    float32_t x, y, z;
} float32_vec3_t;
typedef uint32_t snd_source_t;
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
/* context check? or adding in context mark in snd_source_t ? */
snd_result_t snd_source_create(snd_source_state_t state, snd_source_t* source) {
    snd_source_t id;

    if(source == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.GenSources(&id);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(g_al.IsSource(id) != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }

    source[0] = id;
    return SND_OK;
}
snd_result_t snd_source_state_set(snd_source_t source, snd_source_state_t state) {
    float32_vec3_t fv;
    float32_t f;
    int32_t i;

    if(g_al.IsSource(source) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(float32_is_nan(state.position.x) || float32_is_nan(state.position.y) || float32_is_nan(state.position.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(float32_is_nan(state.direction.x) || float32_is_nan(state.direction.y) || float32_is_nan(state.direction.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(float32_is_nan(state.velocity.x) || float32_is_nan(state.velocity.y) || float32_is_nan(state.velocity.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.gain.multiplier < 0.0f || state.pitch_multiplier <= 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.gain.min < 0.0f || state.gain.min > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.gain.max < 0.0f || state.gain.max > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.gain.outer_angle_multiplier < 0.0f || state.gain.outer_angle_multiplier > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.distance.reference < 0.0f || state.distance.max < 0.0f
            || state.distance.rolloff_factor < 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.cone.inner_angle < 0.0f || state.cone.inner_angle > 360.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.cone.outer_angle < 0.0f || state.cone.outer_angle > 360.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.position_relative_to_listener != true && state.position_relative_to_listener != false) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.loop_queued_buffers != true && state.loop_queued_buffers != false) {
        return SND_ERROR_INVALID_PARAM;
    }



    g_al.Sourcefv(source, AL_POSITION, &state.position);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcefv(source, AL_DIRECTION, &state.direction);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcefv(source, AL_VELOCITY, &state.velocity);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    g_al.Sourcef(source, AL_PITCH, state.pitch_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_GAIN, state.gain.multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_MIN_GAIN, state.gain.min);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_MAX_GAIN, state.gain.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_CONE_OUTER_GAIN, state.gain.outer_angle_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_REFERENCE_DISTANCE, state.distance.reference);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_MAX_DISTANCE, state.distance.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_ROLLOFF_FACTOR, state.distance.rolloff_factor);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_CONE_INNER_ANGLE, state.cone.inner_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.Sourcef(source, AL_CONE_OUTER_ANGLE, state.cone.outer_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.position_relative_to_listener) {
        g_al.Sourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    } else {
        g_al.Sourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    }
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(state.loop_queued_buffers) {
        g_al.Sourcei(source, AL_LOOPING, AL_TRUE);
    } else {
        g_al.Sourcei(source, AL_LOOPING, AL_FALSE);
    }
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }




    g_al.GetSourcefv(source, AL_POSITION, &fv);
    if(g_al.GetError() != AL_NO_ERROR || fv != state.position) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcefv(source, AL_DIRECTION, &fv);
    if(g_al.GetError() != AL_NO_ERROR || fv != state.direction) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcefv(source, AL_VELOCITY, &fv);
    if(g_al.GetError() != AL_NO_ERROR || fv != state.velocity) {
        return SND_ERROR_UNKNOWN;
    }

    g_al.GetSourcef(source, AL_PITCH, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.pitch_multiplier) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_GAIN, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.gain.multiplier) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MIN_GAIN, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.gain.min) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MAX_GAIN, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.gain.max) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_OUTER_GAIN, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.gain.outer_angle_multiplier) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_REFERENCE_DISTANCE, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.distance.reference) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MAX_DISTANCE, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.distance.max) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_ROLLOFF_FACTOR, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.distance.rolloff_factor) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_INNER_ANGLE, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.cone.inner_angle) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_OUTER_ANGLE, &f);
    if(g_al.GetError() != AL_NO_ERROR || f != state.cone.outer_angle) {
        return SND_ERROR_UNKNOWN;
    }


    g_al.GetSourcei(source, AL_LOOPING, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    switch(i) {
    case AL_TRUE:
        if(!state.position_relative_to_listener) {
            return SND_ERROR_UNKNOWN;
        }
        break;
    case AL_FALSE:
        if(state.position_relative_to_listener) {
            return SND_ERROR_UNKNOWN;
        }
        break;
    default:
            return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcei(source, AL_SOURCE_RELATIVE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    switch(i) {
    case AL_TRUE:
        if(!state.loop_queued_buffers) {
            return SND_ERROR_UNKNOWN;
        }
        break;
    case AL_FALSE:
        if(state.loop_queued_buffers) {
            return SND_ERROR_UNKNOWN;
        }
        break;
    default:
            return SND_ERROR_UNKNOWN;
    }













    alSourcei


AL_BUFFER
AL_SOURCE_STATE






    return SND_OK;
}
snd_result_t snd_source_destroy(snd_source_t source) {
    if(g_al.IsSource(source) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.DeleteSources(1, &buffer);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    default:
        return SND_ERROR_UNKNOWN;
    }

    return SND_OK;
}
