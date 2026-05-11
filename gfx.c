#include "gfx.h"

/* uses standard glfw gl header instead of glad, but only for constants and typedefs, the function pointers are loaded manually. */
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <threads.h>


/* internal Event queue: */
struct g_events {
    gfx_event_t *data, *old_data;
    int size, capacity, base;
    mtx_t write_lock;
} g_events;
#define INITIAL_EQ_SIZE                           1024
#define MAX_EQ_SIZE                               (1<<24)
#define MAX_PROPORTION_OF_OLD_DATA_BEFORE_CLEANUP 0.5
void init_event_queue(void) {
    g_events.data = calloc(sizeof(gfx_event_t), INITIAL_EQ_SIZE);
    g_events.old_data = NULL;
    g_events.capacity = INITIAL_EQ_SIZE;
    g_events.size = 0;
    g_events.base = 0;
    assert(mtx_init(&g_events.write_lock, mtx_plain) == thrd_success);
}
void destroy_event_queue(void) {
    if(g_events.old_data != NULL && g_events.old_data != g_events.data) {
        free(g_events.old_data);
    }
    free(g_events.data);
    mtx_destroy(&g_events.write_lock);
    memset(g_events, 0, sizeof(g_events));
}
void cleanup_event_queue(void) {
    assert(mtx_lock(&g_events.write_lock) == thrd_success);
    
    if(g_events.old_data != NULL && g_events.old_data != g_events.data) {
        free(g_events.old_data);
    }
    
    g_events.old_data = NULL;
    
    if(g_events.base > ((double)g_events.capacity)*MAX_PROPORTION_OF_OLD_DATA_BEFORE_CLEANUP) {
        memmove(g_events.data, &(g_events.data[g_events.base]), g_events.size*sizeof(gfx_event_t));
        g_events.base = 0;
    }
    
    assert(mtx_unlock(&g_events.write_lock) == thrd_success);
}
void add_event(gfx_event_t e) {
    assert(mtx_lock(&g_events.write_lock) == thrd_success);
    
    assert(g_events.data != NULL);
    
    if(g_events.base + g_events.size == g_events.capacity) {
        g_events.capacity *= 2;
        assert(g_events.capacity <= MAX_EQ_SIZE);
        
        if (g_events.data == g_events.old_data) {
            /* the pointer was given out at poll_events, so we can't
               realloc the pointer as that would make the old pointer into a dangling
               pointer potentially read by the user; hence we must alloc a new array */
            gfx_event_t *new = calloc(sizeof(gfx_event_t), g_events.capacity);
            memmove(new, &(g_events.data[g_events.base]), sizeof(gfx_event_t)*g_events.size);
            g_events.data = new;
        } else {
            /* we can safely override since the data pointer was not given out */
            g_events.data = realloc(g_events.data, sizeof(gfx_event_t)*g_events.capacity);
        }
    }
    
    g_events.data[g_events.base+g_events.size] = e;
    g_events.size++;
    
    assert(mtx_unlock(&g_events.write_lock) == thrd_success);
}
void poll_events(int *nr_input_events, const gfx_event_t** events) {
    assert(nr_input_events != NULL && event != NULL);
    
    /* must be called _before_ locking since it also locks the mutex */
    cleanup_event_queue();
    
    assert(mtx_lock(&g_events.write_lock) == thrd_success);
    
    nr_input_events[0] = g_events.size;
    events[0] = &(g_events.data[g_events.base]);
    
    g_events.base += g_events.size;
    g_events.size = 0;
    
    g_events.old_data = g_events.data;
    
    assert(mtx_unlock(&g_events.write_lock) == thrd_success);
}

/* internal GLFW utils: */
#define GFX_MAX_JOYSTICK_COUNT GLFW_JOYSTICK_LAST+1
struct g_glfw {
    int monitor_count, monitor_capacity;
    GLFWmonitor** monitors;
    gfx_monitor_info_t *monitor_infos, *old_monitor_infos;
    
    gfx_joystick_info_t joystick_infos[GFX_MAX_JOYSTICK_COUNT];
    gfx_joystick_state_t joystick_states[GFX_MAX_JOYSTICK_COUNT];
    gfx_gamepad_state_t gamepad_states[GFX_MAX_JOYSTICK_COUNT];
    
    int paths_cap, num_paths, num_old_paths; char** paths, old_paths;
    
    GLFWwindow* window;
    int chosen_monitor_id;
    GLFWvideoMode usedVideoMode;
    
    GLFWcursor* used_cursor;
} g_glfw;
void* glfw_allocate(size_t size, void* user) {
    void* ptr = malloc(size);
#ifdef GFX_DEBUG
    fprintf(stderr, "GLFW allocation of %li bytes to %p (user = %p)", size, ptr, user);
#endif
    return ptr
}
void* glfw_reallocate(void* block, size_t size, void* user) {
    void *ptr = realloc(block, size);
#ifdef GFX_DEBUG
    fprintf(stderr, "GLFW reallocation of %li bytes at %p to %p (user = %p)", size, block, ptr, user);
#endif
    return ptr
}
void glfw_deallocate(void* block, void* user) {
#ifdef GFX_DEBUG
    fprintf(stderr, "GLFW deallocation at %p (user = %p)", block, user);
#endif
    free(block);
}
gfx_monitor_info_t get_info_from_glfw_handle(GLFWmonitor* m) {
    gfx_monitor_info_t info;
    
    info.connected = true;
    info.name = strdup(glfwGetMonitorName(m));
#ifndef GFX_NO_CHECKS
    assert(!glfwGetError(NULL));
#endif
    glfwGetMonitorPhysicalSize(m, &(info.physical_size.width_in_mm), &(info.physical_size.height_in_mm));
#ifndef GFX_NO_CHECKS
    assert(!glfwGetError(NULL));
#endif
    
    int mode_count;
    const GLFWvidmode* modes = glfwGetVideoModes(m, &mode_count);
#ifndef GFX_NO_CHECKS
    assert(!glfwGetError(NULL));
#endif
    const GLFWvidmode* curr_mode = glfwGetVideoMode(m);
#ifndef GFX_NO_CHECKS
    assert(!glfwGetError(NULL));
#endif
    bool found_mode = false;
    for(int i = 0; i < mode_count; i++) {
        if(modes[i] == curr_mode[0]) {
            info.current_video_mode_index = i;
            found_mode = true;
            break;
        }
    }
    assert(found_mode);
    
    info.nr_video_modes = mode_count;
    info.modes = calloc(sizeof(gfx_video_mode_t), mode_count);
    for(int i = 0; i < mode_count; i++) {
        info.modes[i].width       = modes[i].width;
        info.modes[i].height      = modes[i].height;
        info.modes[i].refresh_fps = modes[i].refreshRate;
    }
    
    return info;
}
void update_glfw_joystick_info(int jid) {
    if(glfwJoystickPresent(jid) == GLFW_TRUE) {
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        gfx_joystick_info_t* info = &g_glfw.joystick_infos[jid];
        
        info[0].connected    = true;        
        info[0].is_gamepad   = (glfwJoystickIsGamepad(jid) == GLFW_TRUE);
        info[0].name         = strdup(glfwGetJoystickName(jid));
        info[0].GUID         = strdup(glfwGetJoystickGUID(jid));
        info[0].pad_name     = info[0].is_gamepad ? strdup(glfwGetGamepadName(jid)) : NULL;
        
        (void) glfwGetJoystickButtons(jid, &(info[0].button_count));
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        (void) glfwGetJoystickHats(jid, &(info[0].hat_count));
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        (void) glfwGetJoystickAxes(jid, &(info[0].axis_count));
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
    }
}
void update_glfw_joystick_state(int jid) {
    if(g_glfw.joystick_infos[jid].connected) {
        int button_count, hat_count, axis_count;
        assert(glfwJoystickPresent(jid) == GLFW_TRUE);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        
        g_glfw.joystick_states[jid].id = jid;
        g_glfw.joystick_states[jid].button_states = glfwGetJoystickButtons(jid, &button_count);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        g_glfw.joystick_states[jid].hat_states = glfwGetJoystickHats(jid, &hat_count);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        g_glfw.joystick_states[jid].axis_states = glfwGetJoystickAxes(jid, &axis_count);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        
        gfx_event_t e;
        e.type = GFX_EVENT_JOYSTICK_STATE_UPDATE;
        e.state_ptr = &g_glfw.joystick_states[jid];
        add_event(e);
        
        if(g_glfw.joystick_infos[jid].button_count != button_count
          || g_glfw.joystick_infos[jid].hat_count != hat_count
          || g_glfw.joystick_infos[jid].axis_count != axis_count) {
            g_glfw.joystick_infos[jid].button_count = button_count;
            g_glfw.joystick_infos[jid].hat_count = hat_count;
            g_glfw.joystick_infos[jid].axis_count = axis_count;
            
            gfx_event_t e;
            e.type = GFX_EVENT_JOYSTICK_COUNTS_UPDATE;
            e.id = jid;
            add_event(e);
        }
    }
}
void update_glfw_gamepad_state(int jid) {
    if(g_glfw.joystick_infos[jid].connected && g_glfw.joystick_infos[jid].is_gamepad) {
        assert(glfwJoystickPresent(jid) == GLFW_TRUE);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        assert(glfwJoystickIsGamepad(jid) == GLFW_TRUE);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        
        g_glfw.gamepad_states[jid].id = jid;
        
        /* the rest of the struct is layed-out identically to GLFWgamepadstate, so we can just copy into it: */
        GLFWgamepadstate* ptr = (GLFWgamepadstate*) ((void*) &(g_glfw.gamepad_states[jid].A));
        glfwGetGamepadState(jid, ptr);
#ifndef GFX_NO_CHECKS
        assert(!glfwGetError(NULL));
#endif
        
        gfx_event_t e;
        e.type = GFX_EVENT_GAMEPAD_STATE_UPDATE;
        e.state_ptr = &g_glfw.gamepad_states[jid];
        add_event(e);
    }
}
/* every GLFW callback _except_ the error callback (and the deprecated charmods) will write to the queue */
void glfw_monitor(GLFWmonitor* monitor, int event) {
    gfx_event_t e;
    int index;
    switch(event) {
        case GLFW_CONNECTED:
            e.type = GFX_EVENT_MONITOR_CONNECTED;
            bool found = false;
            for(int i = 0; i < g_glfw.monitor_count; i++) {
                if(g_glfw.monitors[i] == monitor) {
                    g_glfw.monitor_infos[i].connected = true;
                    found = true;
                    index = i;
                    break;
                }
            }
            if(!found) {
                index = g_glfw.monitor_count;
                g_glfw.monitor_count++;
                if(g_glfw.monitor_count > g_glfw.monitor_capacity) {
                    g_glfw.monitor_capacity *= 2;
                    g_glfw.monitors = realloc(g_glfw.monitors, sizeof(GLFWmonitor*)*g_glfw.monitor_capacity);
                    /* same idea as for add_event */
                    if(g_glfw.old_monitor_infos == g_glfw.monitor_infos) {
                        gfx_monitor_info_t *new = calloc(sizeof(gfx_monitor_info_t), g_glfw.monitor_capacity);
                        memmove(new, g_glfw.monitor_infos, sizeof(gfx_event_t)*(g_glfw.monitor_count-1));
                        g_glfw.monitor_infos = new;
                    } else {
                        g_glfw.monitor_infos = realloc(g_glfw.monitor_infos, sizeof(gfx_monitor_info_t)*g_glfw.monitor_capacity);
                    }
                }
                
                g_glfw.monitors[index] = monitor;
            }
             
            /* always refresh monitor infos on connection to not keep stale data */
            g_glfw.monitor_infos[index] = get_info_from_glfw_handle(monitor);
            break;
        case GLFW_DISCONNECTED:
            e.type = GFX_EVENT_MONITOR_DISCONNECTED;
            bool found = false;
            for(int i = 0; i < g_glfw.monitor_count; i++) {
                if(g_glfw.monitors[i] == monitor) {
                    /* keep the old info data to be able to see what monitor disconnected with the index */
                    g_glfw.monitor_infos[i].connected = false;
                    found = true;
                    index = i;
                    break;
                }
            }
            assert(found);
            break;
        default:
            assert(false);
    }
    e.value.id = index;
    add_event(e);
}
void glfw_joystick(int jid, int event) {
    gfx_event_t e;
    switch(event) {
        case GLFW_CONNECTED:
            e.type = GFX_EVENT_JOYSTICK_CONNECTED;
            update_glfw_joystick_info(jid);
            break;
        case GLFW_DISCONNECTED:
            e.type = GFX_EVENT_JOYSTICK_DISCONNECTED;
            g_glfw.joystick_infos[jid].connected = false;
            break;
        default:
            assert(false);
    }
    e.value.id = jid;
    add_event(e);
    
}
void glfw_windowpos       (GLFWwindow* window, int xpos, int ypos) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_WINDOW_MOVE;
    e.value.ivec2.x = xpos;
    e.value.ivec2.y = ypos;
    add_event(e);
}
void glfw_windowsize      (GLFWwindow* window, int width, int height) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_WINDOW_RESIZE;
    e.value.ivec2.x = width;
    e.value.ivec2.y = height;
    add_event(e);
}
void glfw_windowclose     (GLFWwindow* window) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_WINDOW_CLOSE;
    add_event(e);
}
void glfw_windowrefresh   (GLFWwindow* window) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_WINDOW_CONTENT_REFRESH;
    add_event(e);
}
void glfw_windowfocus     (GLFWwindow* window, int focused) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    switch(focused) {
        case GLFW_TRUE:
            e.type = GFX_EVENT_WINDOW_FOCUSED;
            break;
        case GLFW_FALSE:
            e.type = GFX_EVENT_WINDOW_UNFOCUSED;
            break;
        default:
            assert(false);
    }
    add_event(e);
}
void glfw_windowiconify   (GLFWwindow* window, int iconified) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    switch(iconified) {
        case GLFW_TRUE:
            e.type = GFX_EVENT_WINDOW_ICONIFIED;
            break;
        case GLFW_FALSE:
            e.type = GFX_EVENT_WINDOW_RESTORED;
            break;
        default:
            assert(false);
    }
    add_event(e);
}
void glfw_windowmaximize  (GLFWwindow* window, int maximized) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    switch(maximized) {
        case GLFW_TRUE:
            e.type = GFX_EVENT_WINDOW_MAXIMIZED;
            break;
        case GLFW_FALSE:
            e.type = GFX_EVENT_WINDOW_UNMAXIMIZED;
            break;
        default:
            assert(false);
    }
    add_event(e);
}
void glfw_framebuffersize (GLFWwindow* window, int width, int height) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_FRAMEBUFFER_RESIZED;
    e.value.ivec2.x = width;
    e.value.ivec2.y = height;
    add_event(e);
}
void glfw_windowcontentscale    (GLFWwindow* window, float xscale, float yscale) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_WINDOW_CONTENT_SCALE_CHANGED;
    e.value.fvec2.x = xscale;
    e.value.fvec2.y = yscale;
    add_event(e);
}
void glfw_key         (GLFWwindow* window, int key, int scancode, int action, int mods) {
    assert(window == g_glfw.window);
    /* we ignore the scancode as it is platform dependent */
    gfx_event_t e;
    switch(action) {
        case GLFW_PRESS:
            e.type = GFX_EVENT_KEY_PRESS;
            break;
        case GLFW_RELEASE:
            e.type = GFX_EVENT_KEY_RELEASE;
            break;
        case GLFW_REPEAT:
            e.type = GFX_EVENT_KEY_REPEAT;
            break;
        default:
            assert(false);
    }
    e.value.keypress.key = key;
    e.value.keypress.mods = mods;
    add_event(e);
}
void glfw_char            (GLFWwindow* window, unsigned int codepoint) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_UNICODE_CHAR_INPUT;
    e.value.unicode_codepoint = codepoint;
    add_event(e);
}
void glfw_mousebutton     (GLFWwindow* window, int button, int action, int mods) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    switch(action) {
        case GLFW_PRESS:
            e.type = GFX_EVENT_MOUSE_CLICK;
            break;
        case GLFW_RELEASE:
            e.type = GFX_EVENT_MOUSE_RELEASE;
            break;
        default:
            assert(false);
    }
    e.value.mouseclick.button = button;
    e.value.mouseclick.mods = mods;
    add_event(e);
    
}
void glfw_cursorpos       (GLFWwindow* window, double xpos, double ypos) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_MOUSE_MOVE;
    e.value.dvec2.x = xpos;
    e.value.dvec2.y = ypos;
    add_event(e);
}
void glfw_cursorenter     (GLFWwindow* window, int entered) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    switch(entered) {
        case GLFW_TRUE:
            e.type = GFX_EVENT_MOUSE_CURSOR_ENTER;
            break;
        case GLFW_FALSE:
            e.type = GFX_EVENT_MOUSE_CURSOR_LEAVE;
            break;
        default:
            assert(false);
    }
    add_event(e);
}
void glfw_scroll          (GLFWwindow* window, double xoffset, double yoffset) {
    assert(window == g_glfw.window);
    gfx_event_t e;
    e.type = GFX_EVENT_SCROLL;
    e.value.dvec2.x = xoffset;
    e.value.dvec2.y = yoffset;
    add_event(e);
}
void glfw_drop            (GLFWwindow* window, int path_count, const char* paths[]) {
    assert(window == g_glfw.window);
    if(g_glfw.num_paths + path_count > g_glfw.paths_cap) {
        g_glfw.paths_cap = min(g_glfw.paths_cap*2, 1024);
        g_glfw.paths = realloc(g_glfw.paths, g_glfw.paths_cap*sizeof(char*));
    }
    
    for(int i = 0; i < path_count; i++) {
        char *path = strdup(paths[i]);
        g_glfw.paths[g_glfw.num_paths] = path;
        g_glfw.num_paths++;
        
        gfx_event_t e;
        e.type = GFX_EVENT_PATH_DROP;
        e.value.path = path;
        add_event(e);
    }
}
/* most things from GLFW used except

glfwSetWindowOpacity ?
glfwGetKeyName
glfwGetCurrentContext ?
glfwExtensionSupported ?

glfwSetWindowTitle ?
glfwSetWindowPos ?
glfwSetWindowSizeLimits, glfwSetWindowAspectRatio, glfwSetWindowSize ?
glfwSetWindowShouldClose, glfwIconifyWindow, glfwRestoreWindow, glfwMaximizeWindow, glfwShowWindow, glfwHideWindow, glfwFocusWindow, glfwRequestWindowAttention ?
glfwSetCursorPos

and functions already covered by the callbacks like glfwGetKey
*/

/* internal GL utils: */
struct g_gl {
    bool                                initialized;
    enum { GLES2, GL3Core, GL2}         version;
    gfx_fixed_function_state_t          state;
    gfx_image_data_rgba_t               screenshot;
    gfx_driver_limits_t                 limits;
    /* all functions supported by all three of GL ES 2.0, GL 3+ Core and GL 2.1 */
    PFNGLACTIVETEXTUREPROC              ActiveTexture;
    PFNGLBINDATTRIBLOCATIONPROC         AttachShader;
    PFNGLBINDATTRIBLOCATIONPROC         BindAttribLocation;
    PFNGLBINDBUFFERPROC                 BindBuffer;
    PFNGLBINDTEXTUREPROC                BindTexture;
    PFNGLBLENDCOLORPROC                 BlendColor;
    PFNGLBLENDEQUATIONPROC              BlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC      BlendEquationSeparate;
    PFNGLBLENDFUNCPROC                  BlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC          BlendFuncSeparate;
    PFNGLBUFFERDATAPROC                 BufferData;
    PFNGLBUFFERSUBDATAPROC              BufferSubData;
    PFNGLCLEARPROC                      Clear;
    PFNGLCLEARCOLORPROC                 ClearColor;
    PFNGLCLEARDEPTHPROC                 ClearDepth;
    PFNGLCLEARDEPTHFPROC                ClearDepthf;
    PFNGLCLEARSTENCILPROC               ClearStencil;
    PFNGLCOLORMASKPROC                  ColorMask;
    PFNGLCOMPILESHADERPROC              CompileShader;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC       CompressedTexImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC    CompressedTexSubImage2D;
    PFNGLCOPYTEXIMAGE2DPROC             CopyTexImage2D;
    PFNGLCOPYTEXSUBIMAGE2DPROC          CopyTexSubImage2D;
    PFNGLCREATEPROGRAMPROC              CreateProgram;
    PFNGLCREATESHADERPROC               CreateShader;
    PFNGLCULLFACEPROC                   CullFace;
    PFNGLDELETEBUFFERSPROC              DeleteBuffers;
    PFNGLDELETEPROGRAMPROC              DeleteProgram;
    PFNGLDELETESHADERPROC               DeleteShader;
    PFNGLDELETETEXTURESPROC             DeleteTextures;
    PFNGLDEPTHFUNCPROC                  DepthFunc;
    PFNGLDEPTHMASKPROC                  DepthMask;
    PFNGLDEPTHRANGEPROC                 DepthRange;
    PFNGLDEPTHRANGEFPROC                DepthRangef;
    PFNGLDETACHSHADERPROC               DetachShader;
    PFNGLDISABLEPROC                    Disable;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC   DisableVertexAttribArray;
    PFNGLDRAWARRAYSPROC                 DrawArrays;
    PFNGLDRAWELEMENTSPROC               DrawElements;
    PFNGLENABLEPROC                     Enable;
    PFNGLENABLEVERTEXATTRIBARRAYPROC    EnableVertexAttribArray;
    PFNGLFINISHPROC                     Finish;
    PFNGLFLUSHPROC                      Flush;
    PFNGLFRONTFACEPROC                  FrontFace;
    PFNGLGENBUFFERSPROC                 GenBuffers;
    PFNGLGENERATEMIPMAPPROC             GenerateMipmap;
    PFNGLGENTEXTURESPROC                GenTextures;
    PFNGLGETACTIVEATTRIBPROC            GetActiveAttrib;
    PFNGLGETACTIVEUNIFORMPROC           GetActiveUniform;
    PFNGLGETATTACHEDSHADERSPROC         GetAttachedShaders;
    PFNGLGETATTRIBLOCATIONPROC          GetAttribLocation;
    PFNGLGETBOOLEANVPROC                GetBooleanv;
    PFNGLGETBUFFERPARAMETERIVPROC       GetBufferParameteriv;
    PFNGLGETERRORPROC                   GetError;
    PFNGLGETFLOATVPROC                  GetFloatv;
    PFNGLGETINTEGERVPROC                GetIntegerv;
    PFNGLGETPROGRAMIVPROC               GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC          GetProgramInfoLog;
    PFNGLGETSHADERIVPROC                GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC           GetShaderInfoLog;
    PFNGLGETSHADERSOURCEPROC            GetShaderSource;
    PFNGLGETSTRINGPROC                  GetString;
    PFNGLGETTEXPARAMETERFVPROC          GetTexParameterfv;
    PFNGLGETTEXPARAMETERIVPROC          GetTexParameteriv;
    PFNGLGETUNIFORMFVPROC               GetUniformfv;
    PFNGLGETUNIFORMIVPROC               GetUniformiv;
    PFNGLGETUNIFORMLOCATIONPROC         GetUniformLocation;
    PFNGLGETVERTEXATTRIBFVPROC          GetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIVPROC          GetVertexAttribiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC    GetVertexAttribPointerv;
    PFNGLHINTPROC                       Hint;
    PFNGLISBUFFERPROC                   IsBuffer;
    PFNGLISENABLEDPROC                  IsEnabled;
    PFNGLISPROGRAMPROC                  IsProgram;
    PFNGLISSHADERPROC                   IsShader;
    PFNGLISTEXTUREPROC                  IsTexture;
    PFNGLLINEWIDTHPROC                  LineWidth;
    PFNGLLINKPROGRAMPROC                LinkProgram;
    PFNGLPIXELSTOREIPROC                PixelStorei;
    PFNGLPOLYGONOFFSETPROC              PolygonOffset;
    PFNGLREADPIXELSPROC                 ReadPixels;
    PFNGLSAMPLECOVERAGEPROC             SampleCoverage;
    PFNGLSCISSORPROC                    Scissor;
    PFNGLSHADERSOURCEPROC               ShaderSource;
    PFNGLSTENCILFUNCPROC                StencilFunc;
    PFNGLSTENCILFUNCSEPARATEPROC        StencilFuncSeparate;
    PFNGLSTENCILMASKPROC                StencilMask;
    PFNGLSTENCILMASKSEPARATEPROC        StencilMaskSeparate;
    PFNGLSTENCILOPPROC                  StencilOp;
    PFNGLSTENCILOPSEPARATEPROC          StencilOpSeparate;
    PFNGLTEXIMAGE2DPROC                 TexImage2D;
    PFNGLTEXPARAMETERFPROC              TexParameterf;
    PFNGLTEXPARAMETERFVPROC             TexParameterfv;
    PFNGLTEXPARAMETERIPROC              TexParameteri;
    PFNGLTEXPARAMETERIVPROC             TexParameteriv;
    PFNGLTEXSUBIMAGE2DPROC              TexSubImage2D;
    PFNGLUNIFORM1FPROC                  Uniform1f;
    PFNGLUNIFORM1FVPROC                 Uniform1fv;
    PFNGLUNIFORM1IPROC                  Uniform1i;
    PFNGLUNIFORM1IVPROC                 Uniform1iv;
    PFNGLUNIFORM2FPROC                  Uniform2f;
    PFNGLUNIFORM2FVPROC                 Uniform2fv;
    PFNGLUNIFORM2IPROC                  Uniform2i;
    PFNGLUNIFORM2IVPROC                 Uniform2iv;
    PFNGLUNIFORM3FPROC                  Uniform3f;
    PFNGLUNIFORM3FVPROC                 Uniform3fv;
    PFNGLUNIFORM3IPROC                  Uniform3i;
    PFNGLUNIFORM3IVPROC                 Uniform3iv;
    PFNGLUNIFORM4FPROC                  Uniform4f;
    PFNGLUNIFORM4FVPROC                 Uniform4fv;
    PFNGLUNIFORM4IPROC                  Uniform4i;
    PFNGLUNIFORM4IVPROC                 Uniform4iv;
    PFNGLUNIFORMMATRIX2FVPROC           UniformMatrix2fv;
    PFNGLUNIFORMMATRIX3FVPROC           UniformMatrix3fv;
    PFNGLUNIFORMMATRIX4FVPROC           UniformMatrix4fv;
    PFNGLUSEPROGRAMPROC                 UseProgram;
    PFNGLVALIDATEPROGRAMPROC            ValidateProgram;
    PFNGLVERTEXATTRIB1FPROC             VertexAttrib1f;
    PFNGLVERTEXATTRIB1FVPROC            VertexAttrib1fv;
    PFNGLVERTEXATTRIB2FPROC             VertexAttrib2f;
    PFNGLVERTEXATTRIB2FVPROC            VertexAttrib2fv;
    PFNGLVERTEXATTRIB3FPROC             VertexAttrib3f;
    PFNGLVERTEXATTRIB3FVPROC            VertexAttrib3fv;
    PFNGLVERTEXATTRIB4FPROC             VertexAttrib4f;
    PFNGLVERTEXATTRIB4FVPROC            VertexAttrib4fv;
    PFNGLVERTEXATTRIBPOINTERPROC        VertexAttribPointer;
    PFNGLVIEWPORTPROC                   Viewport;
} g_gl;
void load_gl(void) {
    /* We don't do error checking here since not available functions will just become NULL pointers */
    g_gl.ActiveTexture                  = (PFNGLACTIVETEXTUREPROC             ) glfwGetProcAddress("glActiveTexture");
    g_gl.AttachShader                   = (PFNGLBINDATTRIBLOCATIONPROC        ) glfwGetProcAddress("glAttachShader");
    g_gl.BindAttribLocation             = (PFNGLBINDATTRIBLOCATIONPROC        ) glfwGetProcAddress("glBindAttribLocation");
    g_gl.BindBuffer                     = (PFNGLBINDBUFFERPROC                ) glfwGetProcAddress("glBindBuffer");
    g_gl.BindTexture                    = (PFNGLBINDTEXTUREPROC               ) glfwGetProcAddress("glBindTexture");
    g_gl.BlendColor                     = (PFNGLBLENDCOLORPROC                ) glfwGetProcAddress("glBlendColor");
    g_gl.BlendEquation                  = (PFNGLBLENDEQUATIONPROC             ) glfwGetProcAddress("glBlendEquation");
    g_gl.BlendEquationSeparate          = (PFNGLBLENDEQUATIONSEPARATEPROC     ) glfwGetProcAddress("glBlendEquationSeparate");
    g_gl.BlendFunc                      = (PFNGLBLENDFUNCPROC                 ) glfwGetProcAddress("glBlendFunc");
    g_gl.BlendFuncSeparate              = (PFNGLBLENDFUNCSEPARATEPROC         ) glfwGetProcAddress("glBlendFuncSeparate");
    g_gl.BufferData                     = (PFNGLBUFFERDATAPROC                ) glfwGetProcAddress("glBufferData");
    g_gl.BufferSubData                  = (PFNGLBUFFERSUBDATAPROC             ) glfwGetProcAddress("glBufferSubData");
    g_gl.Clear                          = (PFNGLCLEARPROC                     ) glfwGetProcAddress("glClear");
    g_gl.ClearColor                     = (PFNGLCLEARCOLORPROC                ) glfwGetProcAddress("glClearColor");
    g_gl.ClearDepth                     = (PFNGLCLEARDEPTHPROC                ) glfwGetProcAddress("glClearDepth");
    g_gl.ClearDepthf                    = (PFNGLCLEARDEPTHFPROC               ) glfwGetProcAddress("glClearDepthf");
    g_gl.ClearStencil                   = (PFNGLCLEARSTENCILPROC              ) glfwGetProcAddress("glClearStencil");
    g_gl.ColorMask                      = (PFNGLCOLORMASKPROC                 ) glfwGetProcAddress("glColorMask");
    g_gl.CompileShader                  = (PFNGLCOMPILESHADERPROC             ) glfwGetProcAddress("glCompileShader");
    g_gl.CompressedTexImage2D           = (PFNGLCOMPRESSEDTEXIMAGE2DPROC      ) glfwGetProcAddress("glCompressedTexImage2D");
    g_gl.CompressedTexSubImage2D        = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC   ) glfwGetProcAddress("glCompressedTexSubImage2D");
    g_gl.CopyTexImage2D                 = (PFNGLCOPYTEXIMAGE2DPROC            ) glfwGetProcAddress("glCopyTexImage2D");
    g_gl.CopyTexSubImage2D              = (PFNGLCOPYTEXSUBIMAGE2DPROC         ) glfwGetProcAddress("glCopyTexSubImage2D");
    g_gl.CreateProgram                  = (PFNGLCREATEPROGRAMPROC             ) glfwGetProcAddress("glCreateProgram");
    g_gl.CreateShader                   = (PFNGLCREATESHADERPROC              ) glfwGetProcAddress("glCreateShader");
    g_gl.CullFace                       = (PFNGLCULLFACEPROC                  ) glfwGetProcAddress("glCullFace");
    g_gl.DeleteBuffers                  = (PFNGLDELETEBUFFERSPROC             ) glfwGetProcAddress("glDeleteBuffers");
    g_gl.DeleteProgram                  = (PFNGLDELETEPROGRAMPROC             ) glfwGetProcAddress("glDeleteProgram");
    g_gl.DeleteShader                   = (PFNGLDELETESHADERPROC              ) glfwGetProcAddress("glDeleteShader");
    g_gl.DeleteTextures                 = (PFNGLDELETETEXTURESPROC            ) glfwGetProcAddress("glDeleteTextures");
    g_gl.DepthFunc                      = (PFNGLDEPTHFUNCPROC                 ) glfwGetProcAddress("glDepthFunc");
    g_gl.DepthMask                      = (PFNGLDEPTHMASKPROC                 ) glfwGetProcAddress("glDepthMask");
    g_gl.DepthRange                     = (PFNGLDEPTHRANGEPROC                ) glfwGetProcAddress("glDepthRange");
    g_gl.DepthRangef                    = (PFNGLDEPTHRANGEFPROC               ) glfwGetProcAddress("glDepthRangef");
    g_gl.DetachShader                   = (PFNGLDETACHSHADERPROC              ) glfwGetProcAddress("glDetachShader");
    g_gl.Disable                        = (PFNGLDISABLEPROC                   ) glfwGetProcAddress("glDisable");
    g_gl.DisableVertexAttribArray       = (PFNGLDISABLEVERTEXATTRIBARRAYPROC  ) glfwGetProcAddress("glDisableVertexAttribArray");
    g_gl.DrawArrays                     = (PFNGLDRAWARRAYSPROC                ) glfwGetProcAddress("glDrawArrays");
    g_gl.DrawElements                   = (PFNGLDRAWELEMENTSPROC              ) glfwGetProcAddress("glDrawElements");
    g_gl.Enable                         = (PFNGLENABLEPROC                    ) glfwGetProcAddress("glEnable");
    g_gl.EnableVertexAttribArray        = (PFNGLENABLEVERTEXATTRIBARRAYPROC   ) glfwGetProcAddress("glEnableVertexAttribArray");
    g_gl.Finish                         = (PFNGLFINISHPROC                    ) glfwGetProcAddress("glFinish");
    g_gl.Flush                          = (PFNGLFLUSHPROC                     ) glfwGetProcAddress("glFlush");
    g_gl.FrontFace                      = (PFNGLFRONTFACEPROC                 ) glfwGetProcAddress("glFrontFace");
    g_gl.GenBuffers                     = (PFNGLGENBUFFERSPROC                ) glfwGetProcAddress("glGenBuffers");
    g_gl.GenerateMipmap                 = (PFNGLGENERATEMIPMAPPROC            ) glfwGetProcAddress("glGenerateMipmap");
    g_gl.GenTextures                    = (PFNGLGENTEXTURESPROC               ) glfwGetProcAddress("glGenTextures");
    g_gl.GetActiveAttrib                = (PFNGLGETACTIVEATTRIBPROC           ) glfwGetProcAddress("glGetActiveAttrib");
    g_gl.GetActiveUniform               = (PFNGLGETACTIVEUNIFORMPROC          ) glfwGetProcAddress("glGetActiveUniform");
    g_gl.GetAttachedShaders             = (PFNGLGETATTACHEDSHADERSPROC        ) glfwGetProcAddress("glGetAttachedShaders");
    g_gl.GetAttribLocation              = (PFNGLGETATTRIBLOCATIONPROC         ) glfwGetProcAddress("glGetAttribLocation");
    g_gl.GetBooleanv                    = (PFNGLGETBOOLEANVPROC               ) glfwGetProcAddress("glGetBooleanv");
    g_gl.GetBufferParameteriv           = (PFNGLGETBUFFERPARAMETERIVPROC      ) glfwGetProcAddress("glGetBufferParameteriv");
    g_gl.GetError                       = (PFNGLGETERRORPROC                  ) glfwGetProcAddress("glGetError");
    g_gl.GetFloatv                      = (PFNGLGETFLOATVPROC                 ) glfwGetProcAddress("glGetFloatv");
    g_gl.GetIntegerv                    = (PFNGLGETINTEGERVPROC               ) glfwGetProcAddress("glGetIntegerv");
    g_gl.GetProgramiv                   = (PFNGLGETPROGRAMIVPROC              ) glfwGetProcAddress("glGetProgramiv");
    g_gl.GetProgramInfoLog              = (PFNGLGETPROGRAMINFOLOGPROC         ) glfwGetProcAddress("glGetProgramInfoLog");
    g_gl.GetShaderiv                    = (PFNGLGETSHADERIVPROC               ) glfwGetProcAddress("glGetShaderiv");
    g_gl.GetShaderInfoLog               = (PFNGLGETSHADERINFOLOGPROC          ) glfwGetProcAddress("glGetShaderInfoLog");
    g_gl.GetShaderSource                = (PFNGLGETSHADERSOURCEPROC           ) glfwGetProcAddress("glGetShaderSource");
    g_gl.GetString                      = (PFNGLGETSTRINGPROC                 ) glfwGetProcAddress("glGetString");
    g_gl.GetTexParameterfv              = (PFNGLGETTEXPARAMETERFVPROC         ) glfwGetProcAddress("glGetTexParameterfv");
    g_gl.GetTexParameteriv              = (PFNGLGETTEXPARAMETERIVPROC         ) glfwGetProcAddress("glGetTexParameteriv");
    g_gl.GetUniformfv                   = (PFNGLGETUNIFORMFVPROC              ) glfwGetProcAddress("glGetUniformfv");
    g_gl.GetUniformiv                   = (PFNGLGETUNIFORMIVPROC              ) glfwGetProcAddress("glGetUniformiv");
    g_gl.GetUniformLocation             = (PFNGLGETUNIFORMLOCATIONPROC        ) glfwGetProcAddress("glGetUniformLocation");
    g_gl.GetVertexAttribfv              = (PFNGLGETVERTEXATTRIBFVPROC         ) glfwGetProcAddress("glGetVertexAttribfv");
    g_gl.GetVertexAttribiv              = (PFNGLGETVERTEXATTRIBIVPROC         ) glfwGetProcAddress("glGetVertexAttribiv");
    g_gl.GetVertexAttribPointerv        = (PFNGLGETVERTEXATTRIBPOINTERVPROC   ) glfwGetProcAddress("glGetVertexAttribPointerv");
    g_gl.Hint                           = (PFNGLHINTPROC                      ) glfwGetProcAddress("glHint");
    g_gl.IsBuffer                       = (PFNGLISBUFFERPROC                  ) glfwGetProcAddress("glIsBuffer");
    g_gl.IsEnabled                      = (PFNGLISENABLEDPROC                 ) glfwGetProcAddress("glIsEnabled");
    g_gl.IsProgram                      = (PFNGLISPROGRAMPROC                 ) glfwGetProcAddress("glIsProgram");
    g_gl.IsShader                       = (PFNGLISSHADERPROC                  ) glfwGetProcAddress("glIsShader");
    g_gl.IsTexture                      = (PFNGLISTEXTUREPROC                 ) glfwGetProcAddress("glIsTexture");
    g_gl.LineWidth                      = (PFNGLLINEWIDTHPROC                 ) glfwGetProcAddress("glLineWidth");
    g_gl.LinkProgram                    = (PFNGLLINKPROGRAMPROC               ) glfwGetProcAddress("glLinkProgram");
    g_gl.PixelStorei                    = (PFNGLPIXELSTOREIPROC               ) glfwGetProcAddress("glPixelStorei");
    g_gl.PolygonOffset                  = (PFNGLPOLYGONOFFSETPROC             ) glfwGetProcAddress("glPolygonOffset");
    g_gl.ReadPixels                     = (PFNGLREADPIXELSPROC                ) glfwGetProcAddress("glReadPixels");
    g_gl.SampleCoverage                 = (PFNGLSAMPLECOVERAGEPROC            ) glfwGetProcAddress("glSampleCoverage");
    g_gl.Scissor                        = (PFNGLSCISSORPROC                   ) glfwGetProcAddress("glScissor");
    g_gl.ShaderSource                   = (PFNGLSHADERSOURCEPROC              ) glfwGetProcAddress("glShaderSource");
    g_gl.StencilFunc                    = (PFNGLSTENCILFUNCPROC               ) glfwGetProcAddress("glStencilFunc");
    g_gl.StencilFuncSeparate            = (PFNGLSTENCILFUNCSEPARATEPROC       ) glfwGetProcAddress("glStencilFuncSeparate");
    g_gl.StencilMask                    = (PFNGLSTENCILMASKPROC               ) glfwGetProcAddress("glStencilMask");
    g_gl.StencilMaskSeparate            = (PFNGLSTENCILMASKSEPARATEPROC       ) glfwGetProcAddress("glStencilMaskSeparate");
    g_gl.StencilOp                      = (PFNGLSTENCILOPPROC                 ) glfwGetProcAddress("glStencilOp");
    g_gl.StencilOpSeparate              = (PFNGLSTENCILOPSEPARATEPROC         ) glfwGetProcAddress("glStencilOpSeparate");
    g_gl.TexImage2D                     = (PFNGLTEXIMAGE2DPROC                ) glfwGetProcAddress("glTexImage2D");
    g_gl.TexParameterf                  = (PFNGLTEXPARAMETERFPROC             ) glfwGetProcAddress("glTexParameterf");
    g_gl.TexParameterfv                 = (PFNGLTEXPARAMETERFVPROC            ) glfwGetProcAddress("glTexParameterfv");
    g_gl.TexParameteri                  = (PFNGLTEXPARAMETERIPROC             ) glfwGetProcAddress("glTexParameteri");
    g_gl.TexParameteriv                 = (PFNGLTEXPARAMETERIVPROC            ) glfwGetProcAddress("glTexParameteriv");
    g_gl.TexSubImage2D                  = (PFNGLTEXSUBIMAGE2DPROC             ) glfwGetProcAddress("glTexSubImage2D");
    g_gl.Uniform1f                      = (PFNGLUNIFORM1FPROC                 ) glfwGetProcAddress("glUniform1f");
    g_gl.Uniform1fv                     = (PFNGLUNIFORM1FVPROC                ) glfwGetProcAddress("glUniform1fv");
    g_gl.Uniform1i                      = (PFNGLUNIFORM1IPROC                 ) glfwGetProcAddress("glUniform1i");
    g_gl.Uniform1iv                     = (PFNGLUNIFORM1IVPROC                ) glfwGetProcAddress("glUniform1iv");
    g_gl.Uniform2f                      = (PFNGLUNIFORM2FPROC                 ) glfwGetProcAddress("glUniform2f");
    g_gl.Uniform2fv                     = (PFNGLUNIFORM2FVPROC                ) glfwGetProcAddress("glUniform2fv");
    g_gl.Uniform2i                      = (PFNGLUNIFORM2IPROC                 ) glfwGetProcAddress("glUniform2i");
    g_gl.Uniform2iv                     = (PFNGLUNIFORM2IVPROC                ) glfwGetProcAddress("glUniform2iv");
    g_gl.Uniform3f                      = (PFNGLUNIFORM3FPROC                 ) glfwGetProcAddress("glUniform3f");
    g_gl.Uniform3fv                     = (PFNGLUNIFORM3FVPROC                ) glfwGetProcAddress("glUniform3fv");
    g_gl.Uniform3i                      = (PFNGLUNIFORM3IPROC                 ) glfwGetProcAddress("glUniform3i");
    g_gl.Uniform3iv                     = (PFNGLUNIFORM3IVPROC                ) glfwGetProcAddress("glUniform3iv");
    g_gl.Uniform4f                      = (PFNGLUNIFORM4FPROC                 ) glfwGetProcAddress("glUniform4f");
    g_gl.Uniform4fv                     = (PFNGLUNIFORM4FVPROC                ) glfwGetProcAddress("glUniform4fv");
    g_gl.Uniform4i                      = (PFNGLUNIFORM4IPROC                 ) glfwGetProcAddress("glUniform4i");
    g_gl.Uniform4iv                     = (PFNGLUNIFORM4IVPROC                ) glfwGetProcAddress("glUniform4iv");
    g_gl.UniformMatrix2fv               = (PFNGLUNIFORMMATRIX2FVPROC          ) glfwGetProcAddress("glUniformMatrix2fv");
    g_gl.UniformMatrix3fv               = (PFNGLUNIFORMMATRIX3FVPROC          ) glfwGetProcAddress("glUniformMatrix3fv");
    g_gl.UniformMatrix4fv               = (PFNGLUNIFORMMATRIX4FVPROC          ) glfwGetProcAddress("glUniformMatrix4fv");
    g_gl.UseProgram                     = (PFNGLUSEPROGRAMPROC                ) glfwGetProcAddress("glUseProgram");
    g_gl.ValidateProgram                = (PFNGLVALIDATEPROGRAMPROC           ) glfwGetProcAddress("glValidateProgram");
    g_gl.VertexAttrib1f                 = (PFNGLVERTEXATTRIB1FPROC            ) glfwGetProcAddress("glVertexAttrib1f");
    g_gl.VertexAttrib1fv                = (PFNGLVERTEXATTRIB1FVPROC           ) glfwGetProcAddress("glVertexAttrib1fv");
    g_gl.VertexAttrib2f                 = (PFNGLVERTEXATTRIB2FPROC            ) glfwGetProcAddress("glVertexAttrib2f");
    g_gl.VertexAttrib2fv                = (PFNGLVERTEXATTRIB2FVPROC           ) glfwGetProcAddress("glVertexAttrib2fv");
    g_gl.VertexAttrib3f                 = (PFNGLVERTEXATTRIB3FPROC            ) glfwGetProcAddress("glVertexAttrib3f");
    g_gl.VertexAttrib3fv                = (PFNGLVERTEXATTRIB3FVPROC           ) glfwGetProcAddress("glVertexAttrib3fv");
    g_gl.VertexAttrib4f                 = (PFNGLVERTEXATTRIB4FPROC            ) glfwGetProcAddress("glVertexAttrib4f");
    g_gl.VertexAttrib4fv                = (PFNGLVERTEXATTRIB4FVPROC           ) glfwGetProcAddress("glVertexAttrib4fv");
    g_gl.VertexAttribPointer            = (PFNGLVERTEXATTRIBPOINTERPROC       ) glfwGetProcAddress("glVertexAttribPointer");
    g_gl.Viewport                       = (PFNGLVIEWPORTPROC                  ) glfwGetProcAddress("glViewport");
}
gfx_fixed_function_state_t default_state(int width, int height) {
    gfx_fixed_function_state_t s;
    s.blend.enabled = false;
    s.blend.color.r = 0;
    s.blend.color.g = 0;
    s.blend.color.b = 0;
    s.blend.color.a = 0;
    s.blend.rgb_equation = GFX_BLEND_EQUATION_TYPE_ADD;
    s.blend.alpha_equation = GFX_BLEND_EQUATION_TYPE_ADD;
    s.blend.src_rgb = GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE;
    s.blend.dst_rgb = GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO;
    s.blend.src_alpha = GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE;
    s.blend.dst_alpha = GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO;
    s.clear.color_enabled = true;
    s.clear.depth_enabled = true;
    s.clear.stencil_enabled = true;
    s.clear.color.r = 0;
    s.clear.color.g = 0;
    s.clear.color.b = 0;
    s.clear.color.a = 0;
    s.clear.depth_value = 1.0f;
    s.clear.stencil_index = 0;
    s.mask.color.r = true;
    s.mask.color.g = true;
    s.mask.color.b = true;
    s.mask.color.a = true;
    s.mask.depth = true;
    s.cull.enabled = false;
    s.cull.front_face_orientation = GFX_FACE_ORIENTATION_COUNTER_CLOCKWISE;
    s.cull.which_face = GFX_CULL_FACE_TYPE_BACK;
    s.depth.test_enabled = false;
    s.depth.comparison_function = GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT;
    s.depth.range.near = 0.0f;
    s.depth.range.far = 1.0f;
    s.polygon_offset.fill_enabled = false;
    s.polygon_offset.scale_factor = 0.0f;
    s.polygon_offset.units = 0.0f;
    s.scissor.enabled = false;
    s.scissor.x = 0;
    s.scissor.y = 0;
    s.scissor.width = width;
    s.scissor.height = height;
    s.stencil.test_enabled = false;
    s.stencil.front_face.comparison_function = GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT;
    s.stencil.front_face.reference_value = 0;
    s.stencil.front_face.compare_mask = 0xFFFFFFFF;
    s.stencil.front_face.write_mask = 0xFFFFFFFF;
    s.stencil.front_face.if_stencil_fails = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.stencil.front_face.if_stencil_passes_and_depth_fails = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.stencil.back_face.comparison_function = GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT;
    s.stencil.back_face.reference_value = 0;
    s.stencil.back_face.compare_mask = 0xFFFFFFFF;
    s.stencil.back_face.write_mask = 0xFFFFFFFF;
    s.stencil.back_face.if_stencil_fails = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.stencil.back_face.if_stencil_passes_and_depth_fails = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available = GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE;
    s.pixel_storage.pack_alignment = GFX_PIXEL_STORAGE_WORD_ALIGNED;
    s.pixel_storage.unpack_alignment = GFX_PIXEL_STORAGE_WORD_ALIGNED;
    s.sample_coverage.enabled = false;
    s.sample_coverage.convert_alpha_to_coverage_value = false;
    s.sample_coverage.invert_coverage_mask = false;
    s.sample_coverage.coverage_value = 1.0f;
    s.dithering_enabled = true;
    s.line_width = 1.0f;
    s.viewport.x = 0;
    s.viewport.y = 0;
    s.viewport.width = width;
    s.viewport.height = height;
    return s;
}
bool load_or_check_limits(void) {
    gfx_driver_limits_t limits;
    g_gl.GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, &limits.line_width);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetFloatv(GL_MAX_VIEWPORT_DIMS, &limits.viewport_maximum);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.vertex);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.fragment);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.combined);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.texture.max_image_pixelbuffer_size.regular_2d);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limits.texture.max_image_pixelbuffer_size.cube_map);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limits.max_vertex_attributes);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_SAMPLE_BUFFERS, &limits.sample_buffers);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_SAMPLES, &limits.sample_coverage_mask_size);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    g_gl.GetIntegerv(GL_SUBPIXEL_BITS, &limits.subpixel_bits);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return false;
    }
#endif
    
    if(g_gl.initialized) {
        return g_gl.limits == limits;
    } else {
        g_gl.limits = limits;
        return true;
    }
}
bool check_params(gfx_fixed_function_state_t state) {
#ifndef GFX_NO_CHECKS
    /* TODO: Is there any way to request the current value of glFrontFace ? 
        also: is GL_SCISSOR_BOX or GL_VIEWPORT with integer an issue?
    */
    bool b[4]; float f[4]; int i;
    g_gl.GetBooleanv(GL_BLEND, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.blend.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.blend.enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_BLEND)) {
        case GL_TRUE:
            if(!state.blend.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.blend.enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_BLEND_COLOR, f);
    if(f[0] != state.blend.color.r || f[1] != state.blend.color.g
        || f[2] != state.blend.color.b || f[3] != state.blend.color.a) {
        return false;
    }
    g_gl.GetIntegerv(GL_BLEND_EQUATION_RGB, &i);
    switch(i) {
        case GL_FUNC_ADD:
            if(state.blend.rgb_equation != GFX_BLEND_EQUATION_TYPE_ADD)
                return false;
            break;
        case GL_FUNC_SUBTRACT:
            if(state.blend.rgb_equation != GFX_BLEND_EQUATION_TYPE_SUBTRACT)
                return false;
            break;
        case GL_FUNC_REVERSE_SUBTRACT:
            if(state.blend.rgb_equation != GFX_BLEND_EQUATION_TYPE_REVERSE_SUBTRACT)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &i);
    switch(i) {
        case GL_FUNC_ADD:
            if(state.blend.alpha_equation != GFX_BLEND_EQUATION_TYPE_ADD)
                return false;
            break;
        case GL_FUNC_SUBTRACT:
            if(state.blend.alpha_equation != GFX_BLEND_EQUATION_TYPE_SUBTRACT)
                return false;
            break;
        case GL_FUNC_REVERSE_SUBTRACT:
            if(state.blend.alpha_equation != GFX_BLEND_EQUATION_TYPE_REVERSE_SUBTRACT)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_BLEND_SRC_RGB, &i);
    switch(i) {
        case GL_ZERO:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO)
                return false;
            break;
        case GL_ONE:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE)
                return false;
            break;
        case GL_SRC_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_SRC_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR)
                return false;
            break;
        case GL_DST_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_DST_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR)
                return false;
            break;
        case GL_SRC_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA)
                return false;
            break;
        case GL_DST_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_DST_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA)
                return false;
            break;
        case GL_CONSTANT_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR)
                return false;
            break;
        case GL_CONSTANT_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA)
                return false;
            break;
        case GL_SRC_ALPHA_SATURATE:
            if(state.blend.src_rgb != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA_SATURATE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_BLEND_SRC_ALPHA, &i);
    switch(i) {
        case GL_ZERO:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO)
                return false;
            break;
        case GL_ONE:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE)
                return false;
            break;
        case GL_SRC_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_SRC_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR)
                return false;
            break;
        case GL_DST_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_DST_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR)
                return false;
            break;
        case GL_SRC_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA)
                return false;
            break;
        case GL_DST_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_DST_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA)
                return false;
            break;
        case GL_CONSTANT_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR)
                return false;
            break;
        case GL_CONSTANT_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA)
                return false;
            break;
        case GL_SRC_ALPHA_SATURATE:
            if(state.blend.src_alpha != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA_SATURATE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_BLEND_DST_RGB, &i);
    switch(i) {
        case GL_ZERO:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO)
                return false;
            break;
        case GL_ONE:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE)
                return false;
            break;
        case GL_SRC_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_SRC_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR)
                return false;
            break;
        case GL_DST_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_DST_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR)
                return false;
            break;
        case GL_SRC_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA)
                return false;
            break;
        case GL_DST_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_DST_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA)
                return false;
            break;
        case GL_CONSTANT_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR)
                return false;
            break;
        case GL_CONSTANT_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            if(state.blend.dst_rgb != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_BLEND_DST_ALPHA, &i);
    switch(i) {
        case GL_ZERO:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO)
                return false;
            break;
        case GL_ONE:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE)
                return false;
            break;
        case GL_SRC_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_SRC_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR)
                return false;
            break;
        case GL_DST_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_DST_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR)
                return false;
            break;
        case GL_SRC_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_SRC_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA)
                return false;
            break;
        case GL_DST_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_DST_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA)
                return false;
            break;
        case GL_CONSTANT_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR)
                return false;
            break;
        case GL_CONSTANT_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA)
                return false;
            break;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            if(state.blend.dst_alpha != GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_COLOR_CLEAR_VALUE, f);
    if(f[0] != state.clear.color.r || f[1] != state.clear.color.g
        || f[2] != state.clear.color.b || f[3] != state.clear.color.a) {
        return false;
    }
    g_gl.GetFloatv(GL_DEPTH_CLEAR_VALUE, f);
    if(f[0] != state.clear.depth_value) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_CLEAR_VALUE, &i);
    if(i != state.clear.stencil_index) {
        return false;
    }
    g_gl.GetBooleanv(GL_COLOR_WRITEMASK, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.mask.color.r)
                return false;
            break;
        case GL_FALSE:
            if(state.mask.color.r)
                return false;
            break;
        default:
            return false;
    }
    switch(b[1]) {
        case GL_TRUE:
            if(!state.mask.color.g)
                return false;
            break;
        case GL_FALSE:
            if(state.mask.color.g)
                return false;
            break;
        default:
            return false;
    }
    switch(b[2]) {
        case GL_TRUE:
            if(!state.mask.color.b)
                return false;
            break;
        case GL_FALSE:
            if(state.mask.color.b)
                return false;
            break;
        default:
            return false;
    }
    switch(b[3]) {
        case GL_TRUE:
            if(!state.mask.color.a)
                return false;
            break;
        case GL_FALSE:
            if(state.mask.color.a)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetBooleanv(GL_DEPTH_WRITEMASK, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.mask.depth)
                return false;
            break;
        case GL_FALSE:
            if(state.mask.depth)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetBooleanv(GL_CULL_FACE, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.cull.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.cull.enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_CULL_FACE)) {
        case GL_TRUE:
            if(!state.cull.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.cull.enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_CULL_FACE_MODE, &i);
    switch(i) {
        case GL_FRONT:
            if(state.cull.which_face != GFX_CULL_FACE_TYPE_FRONT)
                return false;
            break;
        case GL_BACK:
            if(state.cull.which_face != GFX_CULL_FACE_TYPE_BACK)
                return false;
            break;
        case GL_FRONT_AND_BACK:
            if(state.cull.which_face != GFX_CULL_FACE_TYPE_BOTH)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetBooleanv(GL_DEPTH_TEST, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.depth.test_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.depth.test_enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_DEPTH_TEST)) {
        case GL_TRUE:
            if(!state.depth.test_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.depth.test_enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_DEPTH_FUNC, &i);
    switch(i) {
        case GL_NEVER:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT)
                return false;
            break;
        case GL_LESS:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT)
                return false;
            break;
        case GL_EQUAL:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_LEQUAL:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GREATER:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT)
                return false;
            break;
        case GL_NOTEQUAL:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GEQUAL:
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_ALWAYS :
            if(state.depth.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_DEPTH_RANGE, f);
    if(f[0] != state.depth.range.near || f[1] != state.depth.range.far) {
        return false;
    }
    g_gl.GetBooleanv(GL_POLYGON_OFFSET_FILL, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.polygon_offset.fill_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.polygon_offset.fill_enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_POLYGON_OFFSET_FILL)) {
        case GL_TRUE:
            if(!state.polygon_offset.fill_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.polygon_offset.fill_enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_POLYGON_OFFSET_FACTOR, f);
    if(f[0] != state.polygon_offset.scale_factor) {
        return false;
    }
    g_gl.GetFloatv(GL_POLYGON_OFFSET_UNITS, f);
    if(f[0] != state.polygon_offset.units) {
        return false;
    }
    g_gl.GetBooleanv(GL_SCISSOR_TEST, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.scissor.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.scissor.enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_SCISSOR_TEST)) {
        case GL_TRUE:
            if(!state.scissor.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.scissor.enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_SCISSOR_BOX, f);
    if(f[0] != state.scissor.x || f[1] != state.scissor.y
        || f[2] != state.scissor.width || f[3] != state.scissor.height) {
        return false;
    }
    g_gl.GetBooleanv(GL_STENCIL_TEST, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.stencil.test_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.stencil.test_enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_STENCIL_TEST)) {
        case GL_TRUE:
            if(!state.stencil.test_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.stencil.test_enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_FUNC, &i);
    switch(i) {
        case GL_NEVER:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT)
                return false;
            break;
        case GL_LESS:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT)
                return false;
            break;
        case GL_EQUAL:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_LEQUAL:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GREATER:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT)
                return false;
            break;
        case GL_NOTEQUAL:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GEQUAL:
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_ALWAYS :
            if(state.stencil.front_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_FUNC, &i);
    switch(i) {
        case GL_NEVER:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT)
                return false;
            break;
        case GL_LESS:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT)
                return false;
            break;
        case GL_EQUAL:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_LEQUAL:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GREATER:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT)
                return false;
            break;
        case GL_NOTEQUAL:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT)
                return false;
            break;
        case GL_GEQUAL:
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT)
                return false;
            break;
        case GL_ALWAYS :
            if(state.stencil.back_face.comparison_function != GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_REF, &i);
    if(i != state.stencil.front_face.reference_value) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_REF, &i);
    if(i != state.stencil.back_face.reference_value) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_VALUE_MASK, &i);
    if(i != state.stencil.front_face.compare_mask) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &i);
    if(i != state.stencil.back_face.compare_mask) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_WRITEMASK, &i);
    if(i != state.stencil.front_face.write_mask) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_WRITEMASK, &i);
    if(i != state.stencil.back_face.write_mask) {
        return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_FAIL, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.front_face.if_stencil_fails != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_FAIL, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.back_face.if_stencil_fails != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.front_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.back_face.if_stencil_passes_and_depth_fails != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &i);
    switch(i) {
        case GL_KEEP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE)
                return false;
            break;
        case GL_ZERO:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_SET_TO_ZERO)
                return false;
            break;
        case GL_REPLACE:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE)
                return false;
            break;
        case GL_INCR:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX)
                return false;
            break;
        case GL_INCR_WRAP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0)
                return false;
            break;
        case GL_DECR:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0)
                return false;
            break;
        case GL_DECR_WRAP:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX)
                return false;
            break;
        case GL_INVERT:
            if(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_PACK_ALIGNMENT, &i);
    switch(i) {
        case 1:
            if(state.pixel_storage.pack_alignment != GFX_PIXEL_STORAGE_BYTE_ALIGNED)
                return false;
            break;
        case 2:
            if(state.pixel_storage.pack_alignment != GFX_PIXEL_STORAGE_DOUBLE_BYTE_ALIGNED)
                return false;
            break;
        case 4:
            if(state.pixel_storage.pack_alignment != GFX_PIXEL_STORAGE_WORD_ALIGNED)
                return false;
            break;
        case 8:
            if(state.pixel_storage.pack_alignment != GFX_PIXEL_STORAGE_DOUBLE_WORD_ALIGNED)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetIntegerv(GL_UNPACK_ALIGNMENT, &i);
    switch(i) {
        case 1:
            if(state.pixel_storage.unpack_alignment != GFX_PIXEL_STORAGE_BYTE_ALIGNED)
                return false;
            break;
        case 2:
            if(state.pixel_storage.unpack_alignment != GFX_PIXEL_STORAGE_DOUBLE_BYTE_ALIGNED)
                return false;
            break;
        case 4:
            if(state.pixel_storage.unpack_alignment != GFX_PIXEL_STORAGE_WORD_ALIGNED)
                return false;
            break;
        case 8:
            if(state.pixel_storage.unpack_alignment != GFX_PIXEL_STORAGE_DOUBLE_WORD_ALIGNED)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetBooleanv(GL_SAMPLE_COVERAGE_INVERT, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.sample_coverage.invert_coverage_mask)
                return false;
            break;
        case GL_FALSE:
            if(state.sample_coverage.invert_coverage_mask)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_SAMPLE_COVERAGE_VALUE, f);
    if(f[0] != state.sample_coverage.coverage_value) {
        return false;
    }
    g_gl.GetBooleanv(GL_DITHER, b);
    switch(b[0]) {
        case GL_TRUE:
            if(!state.dithering_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.dithering_enabled)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_DITHER)) {
        case GL_TRUE:
            if(!state.dithering_enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.dithering_enabled)
                return false;
            break;
        default:
            return false;
    }
    g_gl.GetFloatv(GL_LINE_WIDTH, f);
    if(f[0] != state.line_width) {
        return false;
    }
    g_gl.GetIntegerv(GL_VIEWPORT, f);
    if(f[0] != state.viewport.x || f[1] != state.viewport.y
        || f[2] != state.viewport.width || f[3] != state.viewport.height) {
        return false;
    }
    switch(glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)) {
        case GL_TRUE:
            if(!state.sample_coverage.convert_alpha_to_coverage_value)
                return false;
            break;
        case GL_FALSE:
            if(state.sample_coverage.convert_alpha_to_coverage_value)
                return false;
            break;
        default:
            return false;
    }
    switch(glIsEnabled(GL_SAMPLE_COVERAGE)) {
        case GL_TRUE:
            if(!state.sample_coverage.enabled)
                return false;
            break;
        case GL_FALSE:
            if(state.sample_coverage.enabled)
                return false;
            break;
        default:
            return false;
    }
#endif

    return true;
}


/* public API: */
gfx_result_t gfx_init(const char* window_name, int width, int height, gfx_window_icon_t* icon) {
    memset(g_gl, 0, sizeof(g_gl));
    memset(g_glfw, 0, sizeof(g_glfw));
    
    init_event_queue();
    
    int major, minor;
    glfwGetVersion(&major, &minor, NULL);
    assert(major == 3 && minor >= 4);
    
    GLFWallocator allocator;
    allocator.allocate = glfw_allocate;
    allocator.reallocate = glfw_reallocate;
    allocator.deallocate = glfw_deallocate;
    allocator.user = NULL;
    glfwInitAllocator(allocator);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_FALSE);
    if(glfwPlatformSupported(GLFW_PLATFORM_WIN32)) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_D3D11);
    } else if(glfwPlatformSupported(GLFW_PLATFORM_X11)) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
    } else if(glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
        glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_PREFER_LIBDECOR);
    } else if(glfwPlatformSupported(GLFW_PLATFORM_COCOA)) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_COCOA);
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_METAL);
        glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_TRUE);
    } else {
        return GFX_ERROR_WINDOW;
    }
    
    if ( !glfwInit() ) {
        return GFX_ERROR_WINDOW;
    }
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
        
    glfwSetMonitorCallback(glfw_monitor);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    
    glfwSetJoystickCallback(glfw_joystick);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    
    GLFWmonitor** glfw_allocated_monitor_array = glfwGetMonitors(&g_glfw.monitor_count);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    g_glfw.monitor_capacity = round_up_to_power_of_2(g_glfw.monitor_count);
    g_glfw.monitors = calloc(sizeof(GLFWmonitor*), g_glfw.monitor_capacity);
    memmove(g_glfw.monitors, glfw_allocated_monitor_array, sizeof(GLFWmonitor*)*g_glfw.monitor_count);
    g_glfw.monitor_infos = calloc(sizeof(gfx_monitor_info_t), g_glfw.monitor_capacity);
    for(int i = 0; i < g_glfw.monitor_count; i++) {
        g_glfw.monitor_infos[i] = get_info_from_glfw_handle(g_glfw.monitors[i]);
    }
    g_glfw.old_monitor_infos = NULL;
    
    /* choose primary monitor by default */
    g_glfw.chosen_monitor_id = 0;
    
    GLFWmonitor* chosen_monitor = g_glfw.monitors[g_glfw.chosen_monitor_id];
    
    /* use current settings of main monitor */
    const GLFWvidmode* mode = glfwGetVideoMode(chosen_monitor);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_RED_BITS,     mode[0].redBits);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_GREEN_BITS,   mode[0].greenBits);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_BLUE_BITS,    mode[0].blueBits);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_REFRESH_RATE, mode[0].refreshRate);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    /* center the window */
    glfwWindowHint(GLFW_POSITION_X , mode[0].width/2);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_POSITION_Y , mode[0].height/2);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    
    /* first test GL ES 2 */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    g_gl.version = GLES2;
    
    /* we _don't_ use chosen_monitor since we start in windowed mode */
    g_glfw.window = glfwCreateWindow(width, height, window_name, NULL, NULL);
    
    bool api_failure = false;
    switch(glfwGetError(NULL)) {
        case: GLFW_NO_ERROR:
            break;
        case GLFW_API_UNAVAILABLE:
        case GLFW_VERSION_UNAVAILABLE:
            api_failure = true;
            break:
        default:
            return GFX_ERROR_WINDOW;
    }
    
    if(api_failure) {
        /* first then GL 3+ Core */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        g_gl.version = GL3Core;
        g_glfw.window = glfwCreateWindow(width, height, window_name, NULL, NULL);
        
        bool api_failure = false;
        switch(glfwGetError(NULL)) {
            case: GLFW_NO_ERROR:
                break;
            case GLFW_API_UNAVAILABLE:
            case GLFW_VERSION_UNAVAILABLE:
                api_failure = true;
                break:
            default:
                return GFX_ERROR_WINDOW;
        }
        
        if(api_failure) {
            /* Then the old GL 2.1 */
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
            g_gl.version = GL2;
            g_glfw.window = glfwCreateWindow(width, height, window_name, NULL, NULL);
            
            bool api_failure = false;
            switch(glfwGetError(NULL)) {
                case: GLFW_NO_ERROR:
                    break;
                case GLFW_API_UNAVAILABLE:
                case GLFW_VERSION_UNAVAILABLE:
                    api_failure = true;
                    break:
                default:
                    return GFX_ERROR_WINDOW;
            }
            
            if(api_failure) {
                return GFX_ERROR_API_UNAVAILABLE;
            }
        }
    }
    
#ifndef GFX_NO_CHECKS
    assert(g_glfw.window != NULL);
#endif
    
    g_glfw.paths_cap = g_glfw.num_paths = g_glfw.num_old_paths = 0;
    g_glfw.paths = g_glfw.old_paths = NULL;
    g_glfw.used_cursor = NULL;
    
    /* now to all the callbacks: */
    glfwSetWindowPosCallback(g_glfw.window, glfw_pos);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowSizeCallback(g_glfw.window, glfw_size);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowCloseCallback(g_glfw.window, glfw_close);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowRefreshCallback(g_glfw.window, glfw_refresh);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowFocusCallback(g_glfw.window, glfw_focus);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowIconifyCallback(g_glfw.window, glfw_iconify);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowMaximizeCallback(g_glfw.window, glfw_maximize);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetFramebufferSizeCallback(g_glfw.window, glfw_framebuffersize);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetWindowContentScaleCallback(g_glfw.window, glfw_contentscale);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetKeyCallback(g_glfw.window, glfw_key);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetCharCallback(g_glfw.window, glfw_char);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetMouseButtonCallback(g_glfw.window, glfw_mousebutton);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetCursorPosCallback(g_glfw.window, glfw_cursorpos);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetCursorEnterCallback(g_glfw.window, glfw_cursorenter);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetScrollCallback(g_glfw.window, glfw_scroll);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glfwSetDropCallback(g_glfw.window, glfw_drop);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    /* Since this will only take effect when the cursor is deactivated, we will turn it on preemtively */
    if (glfwRawMouseMotionSupported()) {
#ifndef GFX_NO_CHECKS
        if(glfwGetError(NULL)) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
        glfwSetInputMode(_glfw.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
#ifndef GFX_NO_CHECKS
        if(glfwGetError(NULL)) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    
    glfwSetInputMode(_glfw.window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    glfwMakeContextCurrent(g_glfw.window);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_INIT_ERROR;
    }
#endif
    
    load_gl();
    bool loaded_limits = load_or_check_limits();
#ifndef GFX_NO_CHECKS
    if(!loaded_limits) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    glfwSwapInterval(1);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_INIT_ERROR;
    }
#endif
    
    /* calls glViewport and sets variables like glLineWidth to default values */
    gfx_result_t code = gfx_params_reset();
#ifndef GFX_NO_CHECKS
    if(code != GFX_OK) return code;
#endif
    
    if(icon != NULL) {
        glfwSetWindowIcon(g_glfw.window, icon[0].nr_icon_candidates, (const GLFWimage*) icon[0].per_icon_candidate_data);
        /* we explicitly don't check for GLFW errors here since this is not available on all platforms and will fail gracefully. */
    }
    
    return GFX_OK;
}
gfx_result_t gfx_exit(void) {
    g_gl.Finish();
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    if(g_gl.screenshot.pixel_data != NULL) {
        free(g_gl.screenshot.pixel_data);
    }
    
    glfwDestroyWindow(g_glfw.window);
    
    free(g_glfw.monitors);
    for(int i = 0; i < g_glfw.monitor_count; i++) {
        if(g_glfw.monitor_infos[i].connected) {
            free(g_glfw.monitor_infos[i].name);
            free(g_glfw.monitor_infos[i].modes);
        }
    }
    free(g_glfw.monitor_infos);
    if(g_glfw.old_monitor_infos != NULL && g_glfw.old_monitor_infos != g_glfw.monitor_infos) {
        free(g_glfw.old_monitor_infos);
    }
    
    glfwTerminate();
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    
    destroy_event_queue();
    
    return GFX_OK;
}

gfx_result_t gfx_joystick_infos(int *nr_joysticks, const gfx_joystick_info_t **infos) {
    assert(nr_joysticks != NULL && infos != NULL);
    
    nr_monitors[0] = GFX_MAX_JOYSTICK_COUNT;
    infos[0] = g_glfw.joystick_infos;
    
    return GFX_OK;
}
gfx_result_t gfx_monitor_infos(int *nr_monitors, const gfx_monitor_info_t **infos) {
    assert(nr_monitors != NULL && infos != NULL);
    
    if(g_glfw.old_monitor_infos != NULL && g_glfw.old_monitor_infos != g_glfw.monitor_infos) {
        free(g_glfw.old_monitor_infos);
    }
    
    nr_monitors[0] = g_glfw.monitor_count;
    infos[0] = g_glfw.monitor_infos;
    
    g_glfw.old_monitor_infos = g_glfw.monitor_infos;
    
    return GFX_OK;
}
gfx_result_t gfx_monitor_switch(int monitor_id, gfx_video_mode_t mode, bool fullscreen, float gamma) {
    g_glfw.chosen_monitor_id = monitor_id;
    
    /* centeres the window on the new monitor; NULL is set for windowed mode compatibility */
    glfwSetWindowMonitor(g_glfw.window, fullscreen ? g_glfw.monitors[monitor_id] : NULL, mode.width/2, mode.height/2, mode.width, mode.height, mode.refresh_fps);
    
    glfwSetGamma(g_glfw.monitors[monitor_id], gamma);
    
    int fb_width, fb_height;
    glfwGetFramebufferSize(g_glfw.window, &fb_width, &fb_height);
    g_gl.Viewport(0, 0, fb_width, fb_height);
#ifndef GFX_NO_CHECKS
    if(gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_API_INIT_ERROR;
    }
#endif
    g_gl.state.viewport.x = 0;
    g_gl.state.viewport.y = 0;
    g_gl.state.viewport.width = fb_width;
    g_gl.state.viewport.height = fb_height;
                /* TODO: should this portion be factorized to be called here and in gfx_params_set? but how to handle the error codes? */
    /* (re)set screenshot buffer for gfx_screenshot */
    g_gl.screenshot.width = g_gl.state.viewport.width;
    g_gl.screenshot.height = g_gl.state.viewport.height;
    g_gl.screenshot.pixel_data = realloc(g_gl.screenshot.pixel_data, sizeof(uint32_t)*g_gl.screenshot.width*g_gl.screenshot.height);
    
    
    return GFX_OK;
}
gfx_result_t gfx_cursor_change(gfx_cursor_shape_t shape, gfx_cursor_mode_t mode) {
    if(g_glfw.used_cursor != NULL) {
        glfwDestroyCursor(g_glfw.used_cursor);
    }
    
    if(shape.is_standard_shape) {
        int shape;
        switch(shape.data.standard_shape) {
            case GFX_STANDARD_CURSOR_SHAPE_ARROW         : 
                shape = GLFW_ARROW_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_IBEAM         : 
                shape = GLFW_IBEAM_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_CROSSHAIR     : 
                shape = GLFW_CROSSHAIR_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_POINTING_HAND : 
                shape = GLFW_POINTING_HAND_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_RESIZE_EW     : 
                shape = GLFW_RESIZE_EW_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_RESIZE_NS     : 
                shape = GLFW_RESIZE_NS_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_RESIZE_NWSE   : 
                shape = GLFW_RESIZE_NWSE_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_RESIZE_NESW   : 
                shape = GLFW_RESIZE_NESW_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_RESIZE_ALL    : 
                shape = GLFW_RESIZE_ALL_CURSOR;
                break
            case GFX_STANDARD_CURSOR_SHAPE_NOT_ALLOWED   : 
                shape = GLFW_NOT_ALLOWED_CURSOR;
                break
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_glfw.used_cursor = glfwCreateStandardCursor(shape);
    } else {
        g_glfw.used_cursor = glfwCreateCursor((GLFWimage*) &(shape.data.custom_shape_data.image_data),
            shape.data.custom_shape_data.xhotspot, shape.data.custom_shape_data.yhotspot);
    }
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_CURSOR_NOT_AVAILABLE;
    }
#endif
    
    glfwSetCursor(g_glfw.window, g_glfw.used_cursor);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_WINDOW;
    }
#endif
    
    int value;
    switch(mode) {
        case GFX_CURSOR_MODE_NORMAL  :
            value = GLFW_CURSOR_NORMAL  ;
            break;
        case GFX_CURSOR_MODE_HIDDEN  :
            value = GLFW_CURSOR_HIDDEN  ;
            break;
        case GFX_CURSOR_MODE_DISABLED:
            value = GLFW_CURSOR_DISABLED;
            break;
        case GFX_CURSOR_MODE_CAPTURED:
            value = GLFW_CURSOR_CAPTURED;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    glfwSetInputMode(g_glfw.window, GLFW_CURSOR, value);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_WINDOW;
    }
#endif
    
    return GFX_OK;
}

gfx_result_t gfx_events_read(int *nr_events, const gfx_event_t** events) {
    assert(nr_events != NULL && events != NULL);
    
    glfwPollEvents();
    
    for(int jid = 0; jid < GFX_MAX_JOYSTICK_COUNT; jid++) {
        update_glfw_joystick_state(jid);
        update_glfw_gamepad_state(jid);
    }
    
    if(g_glfw.old_paths != NULL) {
        for(int i = 0; i < g_glfw.num_old_paths; i++) {
            free(g_glfw.old_paths[i]);
        }
        free(g_glfw.old_paths);
    }
    g_glfw.old_paths = g_glfw.paths;
    g_glfw.num_old_paths = g_glf.num_paths;
    g_glfw.paths = NULL;
    g_glf.num_paths = 0;
    
    poll_events(nr_input_events, events);
    
    return GFX_OK;
}
gfx_result_t gfx_events_done_processing(void) {
    if(g_glfw.old_paths != NULL) {
        for(int i = 0; i < g_glfw.num_old_paths; i++) {
            free(g_glfw.old_paths[i]);
        }
        free(g_glfw.old_paths);
    }
    
    cleanup_event_queue();
    
    return GFX_OK;
}


gfx_result_t gfx_clipboard_get(const char** string) {
    assert(string != NULL);
    string[0] = glfwGetClipboardString(g_glfw.window);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    return GFX_OK;
}
gfx_result_t gfx_clipboard_set(const char* string) {
    glfwSetClipboardString(g_glfw.window, string);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    return GFX_OK;
}






gfx_result_t gfx_timestamp_frequency(uint64_t* frequency_in_hz) {
    uint64_t val
#ifndef GFX_NO_CHECKS
    if(frequency_in_hz == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    val = glfwGetTimerFrequency();
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    frequency_in_hz[0] = val;
    return GFX_OK;
}
gfx_result_t gfx_timestamp(uint64_t* ticks) {
    uint64_t val
#ifndef GFX_NO_CHECKS
    if(ticks == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    val = glfwGetTimerValue();
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
#endif
    ticks[0] = val;
    return GFX_OK;
}



gfx_result_t gfx_params_get(gfx_fixed_function_state_t* state) {
    assert(state != NULL);
    
#ifndef GFX_NO_CHECKS
    if(!load_or_check_limits() || !check_params(g_gl.state)) {
        return GFX_ERROR_API_OTHER;
    }
#endif
    
    state[0] = g_gl.state;
    return GFX_OK;
}
gfx_result_t gfx_params_set(gfx_fixed_function_state_t state) {
    bool set_all = !g_gl.initialized;
    
#ifndef GFX_NO_CHECKS
    if(!set_all) {
        if(!load_or_check_limits() || !check_params(g_gl.state)) {
            return GFX_ERROR_API_OTHER;
        }
    }
#endif
    
    if(set_all || state.blend.enabled != g_gl.state.blend.enabled) {
        if(state.blend.enabled == true) {
            g_gl.Enable(GL_BLEND);
        } else if (state.blend.enabled == false) {
            g_gl.Disable(GL_BLEND);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.blend.color != g_gl.state.blend.color) {
#ifndef GFX_NO_CHECKS
        if(state.blend.color.r < 0.0f || state.blend.color.r > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.blend.color.g < 0.0f || state.blend.color.g > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.blend.color.b < 0.0f || state.blend.color.b > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.blend.color.a < 0.0f || state.blend.color.a > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        g_gl.BlendColor(state.blend.color.r, state.blend.color.g, state.blend.color.b, state.blend.color.a);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.blend.rgb_equation != g_gl.state.blend.rgb_equation
               || state.blend.alpha_equation != g_gl.state.blend.alpha_equation) {
        GLenum modeRGB, modeAlpha;
        switch(state.blend.rgb_equation) {
            case GFX_BLEND_EQUATION_TYPE_ADD:
                modeRGB = GL_FUNC_ADD;
                break;
            case GFX_BLEND_EQUATION_TYPE_SUBTRACT:
                modeRGB = GL_FUNC_SUBTRACT;
                break;
            case GFX_BLEND_EQUATION_TYPE_REVERSE_SUBTRACT:
                modeRGB = GL_FUNC_REVERSE_SUBTRACT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.blend.alpha_equation) {
            case GFX_BLEND_EQUATION_TYPE_ADD:
                modeAlpha = GL_FUNC_ADD;
                break;
            case GFX_BLEND_EQUATION_TYPE_SUBTRACT:
                modeAlpha = GL_FUNC_SUBTRACT;
                break;
            case GFX_BLEND_EQUATION_TYPE_REVERSE_SUBTRACT:
                modeAlpha = GL_FUNC_REVERSE_SUBTRACT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.BlendEquationSeparate(modeRGB, modeAlpha);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.blend.src_rgb != g_gl.state.blend.src_rgb
               || state.blend.src_alpha != g_gl.state.blend.src_alpha
               || state.blend.dst_rgb != g_gl.state.blend.dst_rgb
               || state.blend.dst_alpha != g_gl.state.blend.dst_alpha) {
        GLenum src_rgb, src_alpha, dst_rgb, dst_alpha;
        switch(state.blend.src_rgb) {
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO            :
                src_rgb = GL_ZERO;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE             :
                src_rgb = GL_ONE;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_COLOR                :
                src_rgb = GL_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR      :
                src_rgb = GL_ONE_MINUS_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_COLOR                :
                src_rgb = GL_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR      :
                src_rgb = GL_ONE_MINUS_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA                :
                src_rgb = GL_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA      :
                src_rgb = GL_ONE_MINUS_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_ALPHA                :
                src_rgb = GL_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA      :
                src_rgb = GL_ONE_MINUS_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR           :
                src_rgb = GL_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR :
                src_rgb = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA           :
                src_rgb = GL_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA :
                src_rgb = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA_SATURATE       :
                src_rgb = GL_SRC_ALPHA_SATURATE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.blend.src_alpha) {
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO            :
                src_alpha = GL_ZERO;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE             :
                src_alpha = GL_ONE;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_COLOR                :
                src_alpha = GL_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR      :
                src_alpha = GL_ONE_MINUS_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_COLOR                :
                src_alpha = GL_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR      :
                src_alpha = GL_ONE_MINUS_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA                :
                src_alpha = GL_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA      :
                src_alpha = GL_ONE_MINUS_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_ALPHA                :
                src_alpha = GL_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA      :
                src_alpha = GL_ONE_MINUS_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR           :
                src_alpha = GL_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR :
                src_alpha = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA           :
                src_alpha = GL_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA :
                src_alpha = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA_SATURATE       :
                src_alpha = GL_SRC_ALPHA_SATURATE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.blend.dst_rgb) {
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO            :
                dst_rgb = GL_ZERO;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE             :
                dst_rgb = GL_ONE;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_COLOR                :
                dst_rgb = GL_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR      :
                dst_rgb = GL_ONE_MINUS_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_COLOR                :
                dst_rgb = GL_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR      :
                dst_rgb = GL_ONE_MINUS_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA                :
                dst_rgb = GL_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA      :
                dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_ALPHA                :
                dst_rgb = GL_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA      :
                dst_rgb = GL_ONE_MINUS_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR           :
                dst_rgb = GL_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR :
                dst_rgb = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA           :
                dst_rgb = GL_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA :
                dst_rgb = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.blend.dst_alpha) {
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO            :
                dst_alpha = GL_ZERO;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE             :
                dst_alpha = GL_ONE;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_COLOR                :
                dst_alpha = GL_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR      :
                dst_alpha = GL_ONE_MINUS_SRC_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_COLOR                :
                dst_alpha = GL_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR      :
                dst_alpha = GL_ONE_MINUS_DST_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_SRC_ALPHA                :
                dst_alpha = GL_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA      :
                dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_DST_ALPHA                :
                dst_alpha = GL_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA      :
                dst_alpha = GL_ONE_MINUS_DST_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR           :
                dst_alpha = GL_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR :
                dst_alpha = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            case GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA           :
                dst_alpha = GL_CONSTANT_ALPHA;
                break;
            case GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA :
                dst_alpha = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.BlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.clear.color != g_gl.state.clear.color) {
#ifndef GFX_NO_CHECKS
        if(state.clear.color.r < 0.0f || state.clear.color.r > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.clear.color.g < 0.0f || state.clear.color.g > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.clear.color.b < 0.0f || state.clear.color.b > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.clear.color.a < 0.0f || state.clear.color.a > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        g_gl.ClearColor(state.clear.color.r, state.clear.color.g, state.clear.color.b, state.clear.color.a);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.clear.depth_value != g_gl.state.clear.depth_value) {
#ifndef GFX_NO_CHECKS
        if(state.clear.depth_value < 0.0f || state.clear.depth_value > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        if(g_gl.version == GLES2) {
            g_gl.ClearDepthf(state.clear.depth_value);
        } else {
            g_gl.ClearDepth(state.clear.depth_value);
        }
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.clear.stencil_index != g_gl.state.clear.stencil_index) {
        g_gl.ClearStencil(state.clear.stencil_index);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.mask.color != g_gl.state.mask.color) {
        GLboolean r,g,b,a;
        switch(state.mask.color.r) {
            case true:
                r = GL_TRUE;
                break;
            case false:
                r = GL_FALSE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.mask.color.g) {
            case true:
                g = GL_TRUE;
                break;
            case false:
                g = GL_FALSE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.mask.color.b) {
            case true:
                b = GL_TRUE;
                break;
            case false:
                b = GL_FALSE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.mask.color.a) {
            case true:
                a = GL_TRUE;
                break;
            case false:
                a = GL_FALSE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.ColorMask(r,g,b,a);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.mask.depth != g_gl.state.mask.depth) {
        GLboolean flag;
        switch(state.mask.depth) {
            case true:
                flag = GL_TRUE;
                break;
            case false:
                flag = GL_FALSE;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.DepthMask(flag);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.cull.enabled != g_gl.state.cull.enabled) {
        if(state.cull.enabled == true) {
            g_gl.Enable(GL_CULL_FACE);
        } else if (state.cull.enabled == false) {
            g_gl.Disable(GL_CULL_FACE);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.cull.front_face_orientation != g_gl.state.cull.front_face_orientation) {
        GLenum mode;
        switch(state.cull.front_face_orientation) {
            case GFX_FACE_ORIENTATION_CLOCKWISE:
                mode = GL_CW;
                break;
            case GFX_FACE_ORIENTATION_COUNTER_CLOCKWISE:
                mode = GL_CCW;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.FrontFace(mode);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.cull.which_face != g_gl.state.cull.which_face) {
        GLenum mode;
        switch(state.cull.which_face) {
            case GFX_CULL_FACE_TYPE_FRONT:
                mode = GL_FRONT;
                break;
            case GFX_CULL_FACE_TYPE_BACK:
                mode = GL_BACK;
                break;
            case GFX_CULL_FACE_TYPE_BOTH:
                mode = GL_FRONT_AND_BACK;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.CullFace(mode);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.depth.test_enabled != g_gl.state.depth.test_enabled) {
        if(state.depth.test_enabled == true) {
            g_gl.Enable(GL_DEPTH_TEST);
        } else if (state.depth.test_enabled == false) {
            g_gl.Disable(GL_DEPTH_TEST);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.depth.comparison_function != g_gl.state.depth.comparison_function) {
        GLenum func;
        switch(state.depth.comparison_function) {
        case GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT                             :
            func = GL_NEVER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT              :
            func = GL_LESS;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT               :
            func = GL_EQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT  :
            func = GL_LEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT           :
            func = GL_GREATER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT             :
            func = GL_NOTEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT:
            func = GL_GEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT                             :
            func = GL_ALWAYS;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.DepthFunc(func);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.depth.range != g_gl.state.depth.range) {
#ifndef GFX_NO_CHECKS
        if(state.depth.range.near < 0.0f || state.depth.range.near > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.depth.range.far < 0.0f || state.depth.range.far > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        if(g_gl.version == GLES2) {
            g_gl.DepthRangef(state.depth.range.near, state.depth.range.far);
        } else {
            g_gl.DepthRange(state.depth.range.near, state.depth.range.far);
        }
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.polygon_offset.fill_enabled != g_gl.state.polygon_offset.fill_enabled) {
        if(state.polygon_offset.fill_enabled == true) {
            g_gl.Enable(GL_POLYGON_OFFSET_FILL);
        } else if (state.polygon_offset.fill_enabled == false) {
            g_gl.Disable(GL_POLYGON_OFFSET_FILL);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.polygon_offset.scale_factor != g_gl.state.polygon_offset.scale_factor
        || state.polygon_offset.units != g_gl.state.polygon_offset.units) {
        g_gl.PolygonOffset(state.polygon_offset.scale_factor, state.polygon_offset.units);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.scissor.enabled != g_gl.state.scissor.enabled) {
        if(state.scissor.enabled == true) {
            g_gl.Enable(GL_SCISSOR_TEST);
        } else if (state.scissor.enabled == false) {
            g_gl.Disable(GL_SCISSOR_TEST);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.scissor.x != g_gl.state.scissor.x
               || state.scissor.y != g_gl.state.scissor.y
               || state.scissor.width != g_gl.state.scissor.width
               || state.scissor.height != g_gl.state.scissor.height) {
#ifndef GFX_NO_CHECKS
        if(state.scissor.x < 0 || state.scissor.y < 0 || state.scissor.width < 0 || state.scissor.height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        
        /* also check viewport size? */
        g_gl.Scissor(state.scissor.x, state.scissor.y, state.scissor.width, state.scissor.height);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.test_enabled != g_gl.state.stencil.test_enabled) {
        if(state.stencil.test_enabled == true) {
            g_gl.Enable(GL_STENCIL_TEST);
        } else if (state.stencil.test_enabled == false) {
            g_gl.Disable(GL_STENCIL_TEST);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.front_face.comparison_function != g_gl.state.stencil.front_face.comparison_function
         || state.stencil.front_face.reference_value != g_gl.state.stencil.front_face.reference_value
         || state.stencil.front_face.compare_mask != g_gl.state.stencil.front_face.compare_mask) {
        GLenum func;
        switch(state.stencil.front_face.comparison_function) {
        case GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT                             :
            func = GL_NEVER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT              :
            func = GL_LESS;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT               :
            func = GL_EQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT  :
            func = GL_LEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT           :
            func = GL_GREATER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT             :
            func = GL_NOTEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT:
            func = GL_GEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT                             :
            func = GL_ALWAYS;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.StencilFuncSeparate(GL_FRONT,func,state.stencil.front_face.reference_value, state.stencil.front_face.compare_mask);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.back_face.comparison_function != g_gl.state.stencil.back_face.comparison_function
         || state.stencil.back_face.reference_value != g_gl.state.stencil.back_face.reference_value
         || state.stencil.back_face.compare_mask != g_gl.state.stencil.back_face.compare_mask) {
        GLenum func;
        switch(state.stencil.back_face.comparison_function) {
        case GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT                             :
            func = GL_NEVER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT              :
            func = GL_LESS;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT               :
            func = GL_EQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT  :
            func = GL_LEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT           :
            func = GL_GREATER;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT             :
            func = GL_NOTEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT:
            func = GL_GEQUAL;
            break;
        case GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT                             :
            func = GL_ALWAYS;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.StencilFuncSeparate(GL_BACK,func,state.stencil.back_face.reference_value, state.stencil.back_face.compare_mask);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.front_face.write_mask != g_gl.state.stencil.front_face.write_mask) {
        g_gl.StencilMaskSeparate(GL_FRONT, state.stencil.front_face.write_mask);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.back_face.write_mask != g_gl.state.stencil.back_face.write_mask) {
        g_gl.StencilMaskSeparate(GL_BACK, state.stencil.back_face.write_mask);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.front_face.if_stencil_fails != g_gl.state.stencil.front_face.if_stencil_fails
         || state.stencil.front_face.if_stencil_passes_and_depth_fails != g_gl.state.stencil.front_face.if_stencil_passes_and_depth_fails
         || state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available != g_gl.state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available) {
        GLenum sfail, dpfail, dppass;
        switch(state.stencil.front_face.if_stencil_fails) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                sfail = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                sfail = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                sfail = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                sfail = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                sfail = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                sfail = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                sfail = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                sfail = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.stencil.front_face.if_stencil_passes_and_depth_fails) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                dpfail = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                dpfail = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                dpfail = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                dpfail = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                dpfail = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                dpfail = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                dpfail = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                dpfail = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.stencil.front_face.if_stencil_passes_and_depth_passes_or_is_not_available) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                dppass = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                dppass = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                dppass = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                dppass = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                dppass = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                dppass = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                dppass = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                dppass = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.StencilOpSeparate(GL_FRONT,sfail,dpfail,dppass);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.stencil.back_face.if_stencil_fails != g_gl.state.stencil.back_face.if_stencil_fails
         || state.stencil.back_face.if_stencil_passes_and_depth_fails != g_gl.state.stencil.back_face.if_stencil_passes_and_depth_fails
         || state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available != g_gl.state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available) {
        GLenum sfail, dpfail, dppass;
        switch(state.stencil.back_face.if_stencil_fails) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                sfail = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                sfail = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                sfail = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                sfail = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                sfail = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                sfail = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                sfail = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                sfail = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.stencil.back_face.if_stencil_passes_and_depth_fails) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                dpfail = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                dpfail = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                dpfail = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                dpfail = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                dpfail = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                dpfail = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                dpfail = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                dpfail = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        switch(state.stencil.back_face.if_stencil_passes_and_depth_passes_or_is_not_available) {
            case GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                      :
                dppass = GL_KEEP;
                break;
            case GFX_STENCIL_OPERATION_SET_TO_ZERO                             :
                dppass = GL_ZERO;
                break;
            case GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE            :
                dppass = GL_REPLACE;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX:
                dppass = GL_INCR;
                break;
            case GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0   :
                dppass = GL_INCR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0  :
                dppass = GL_DECR;
                break;
            case GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX :
                dppass = GL_DECR_WRAP;
                break;
            case GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                :
                dppass = GL_INVERT;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.StencilOpSeparate(GL_BACK,sfail,dpfail,dppass);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.pixel_storage.pack_alignment != g_gl.state.pixel_storage.pack_alignment) {
        GLint param;
        switch(state.pixel_storage.pack_alignment) {
            case GFX_PIXEL_STORAGE_BYTE_ALIGNED:
                param = 1;
                break;
            case GFX_PIXEL_STORAGE_DOUBLE_BYTE_ALIGNED:
                param = 2;
                break;
            case GFX_PIXEL_STORAGE_WORD_ALIGNED:
                param = 4;
                break;
            case GFX_PIXEL_STORAGE_DOUBLE_WORD_ALIGNED:
                param = 8;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.PixelStorei(GL_PACK_ALIGNMENT, param);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.pixel_storage.unpack_alignment != g_gl.state.pixel_storage.unpack_alignment) {
        GLint param;
        switch(state.pixel_storage.unpack_alignment) {
            case GFX_PIXEL_STORAGE_BYTE_ALIGNED:
                param = 1;
                break;
            case GFX_PIXEL_STORAGE_DOUBLE_BYTE_ALIGNED:
                param = 2;
                break;
            case GFX_PIXEL_STORAGE_WORD_ALIGNED:
                param = 4;
                break;
            case GFX_PIXEL_STORAGE_DOUBLE_WORD_ALIGNED:
                param = 8;
                break;
            default:
                return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.PixelStorei(GL_UNPACK_ALIGNMENT, param);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.sample_coverage.enabled != g_gl.state.sample_coverage.enabled) {
        if(state.sample_coverage.enabled == true) {
            g_gl.Enable(GL_SAMPLE_COVERAGE);
        } else if (state.sample_coverage.enabled == false) {
            g_gl.Disable(GL_SAMPLE_COVERAGE);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.sample_coverage.convert_alpha_to_coverage_value != g_gl.state.sample_coverage.convert_alpha_to_coverage_value) {
        if(state.sample_coverage.convert_alpha_to_coverage_value == true) {
            g_gl.Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        } else if (state.sample_coverage.convert_alpha_to_coverage_value == false) {
            g_gl.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.sample_coverage.invert_coverage_mask != g_gl.state.sample_coverage.invert_coverage_mask
             || state.sample_coverage.coverage_value != g_gl.state.sample_coverage.coverage_value) {
        GLboolean invert;
        if(state.sample_coverage.invert_coverage_mask == true) {
            invert = GL_TRUE;
        } else if (state.sample_coverage.invert_coverage_mask == false) {
            invert = GL_FALSE;
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
#ifndef GFX_NO_CHECKS
        if(state.sample_coverage.coverage_value < 0.0f || state.sample_coverage.coverage_value > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        g_gl.SampleCoverage(state.sample_coverage.coverage_value, invert);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.dithering_enabled != g_gl.state.dithering_enabled) {
        if(state.dithering_enabled == true) {
            g_gl.Enable(GL_DITHER);
        } else if (state.dithering_enabled == false) {
            g_gl.Disable(GL_DITHER);
        }
#ifndef GFX_NO_CHECKS
        else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.line_width != g_gl.state.line_width) {
#ifndef GFX_NO_CHECKS
        if(state.line_width <= 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.line_width < g_gl.limits.line_width.min || state.line_width > g_gl.limits.line_width.max) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        g_gl.LineWidth(state.line_width);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    if(set_all || state.viewport != g_gl.state.viewport) {
#ifndef GFX_NO_CHECKS
        if(state.viewport.x < 0 || state.viewport.y < 0 || state.viewport.width < 0 || state.viewport.height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.viewport.x + state.viewport.width > limits.viewport_maximum.x || state.viewport.y + state.viewport.height > limits.viewport_maximum.y) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        g_gl.Viewport(state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return set_all ? GFX_ERROR_API_INIT_ERROR : GFX_ERROR_API_UPDATE_ERROR;
        }
#endif
        /* (re)set screenshot buffer for gfx_screenshot */
        g_gl.screenshot.width = g_gl.state.viewport.width;
        g_gl.screenshot.height = g_gl.state.viewport.height;
        g_gl.screenshot.pixel_data = realloc(g_gl.screenshot.pixel_data, sizeof(uint32_t)*g_gl.screenshot.width*g_gl.screenshot.height);
    }

#ifndef GFX_NO_CHECKS
    /* check if we actually could update the state */
    if(!load_or_check_limits() || !check_params(state)) {
        return GFX_ERROR_API_OTHER;
    }
#endif
    
    g_gl.state = state;
    g_gl.initialized = true;
    return GFX_OK;
}
gfx_result_t gfx_params_reset(void) {
    int fb_width, fb_height;
    glfwGetFramebufferSize(g_glfw.window, &fb_width, &fb_height);
    return gfx_params_set(default_state(fb_width, fb_height));
}





gfx_result_t gfx_render(void) {
    g_gl.Flush(); /* This might be unnecessary but we keep it in just in case */
    
    glfwSwapBuffers(g_glfw.window);
#ifndef GFX_NO_CHECKS
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_OTHER;
    }
#endif
    
    return GFX_OK;
}

gfx_result_t gfx_clear(void) {
    GLbitfield mask = 0;
    if(g_gl.state.clear.color_enabled) {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if(g_gl.state.clear.depth_enabled) {
        mask |=  GL_DEPTH_BUFFER_BIT;
    }
    if(g_gl.state.clear.stencil_enabled) {
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    g_gl.Clear(mask);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}


gfx_result_t gfx_screenshot(gfx_image_data_rgba_t* img) {
#ifndef GFX_NO_CHECKS
    assert(img != NULL);
#endif
    /* we are limited to RGBA32 bc GL ES 2 only has this and non-standard formats
       we only do full screenshots since users need to copy out the data anyway,
       and this way we can maintain an internal buffer with minimal reallocing
    */
    g_gl.ReadPixels(g_gl.state.viewport.x, g_gl.state.viewport.y, g_gl.screenshot.width, g_gl.screenshot.width, GL_RGBA, GL_UNSIGNED_BYTE, (void*) g_gl.screenshot.pixel_data);
    img[0] = g_gl.screenshot;
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}

gfx_result_t gfx_driver_limits(const gfx_driver_limits_t** limits) {
#ifndef GFX_NO_CHECKS
    assert(limits != NULL);
#endif
    limits[0] = &(g_gl.limits);
    
    if(!load_or_check_limits()) {
        return GFX_ERROR_API_OTHER;
    }
    
    return GFX_OK;
}






typedef enum gfx_internal_buffer_type_t {
    GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER = 0,
    GFX_BUFFER_TYPE_INDEX_BUFFER = 1,
    GFX_BUFFER_TYPE_MAX_ENUM = 0x7f
} gfx_internal_buffer_type_t;


static gfx_result_t gfx_buffer_bind_safe(GLenum target, GLuint old_id, GLuint new_id) {
    GLenum targetinfo;
    GLuint i;
    switch(target) {
        case GL_ARRAY_BUFFER;
            targetinfo = GL_ARRAY_BUFFER_BINDING;
            break;
        case GL_ELEMENT_ARRAY_BUFFER;
            targetinfo = GL_ELEMENT_ARRAY_BUFFER_BINDING;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }

#ifdef GFX_DEBUG
    g_gl.GetIntegerv(targetinfo, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != old_id) {
        return GFX_ERROR_API_OTHER;
    }
#endif

    g_gl.BindBuffer(target, new_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

#ifdef GFX_DEBUG
    g_gl.GetIntegerv(targetinfo, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != new_id) {
        return GFX_ERROR_API_OTHER;
    }
#endif

    return GFX_OK;
}
static gfx_result_t gfx_buffer_create_generic(gfx_internal_buffer_type_t type, gfx_buffer_usage_t usage, size_t size, void* ptr, GLuint* id) {
#ifndef GFX_NO_CHECKS
    assert(ptr != NULL && size > 0);
#endif
    
    GLenum target, usage_pattern;
    GLuint id;
    gfx_result_t r;

    switch(type) {
        case GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER:
            target = GL_ARRAY_BUFFER;
            break;
        case GFX_BUFFER_TYPE_INDEX_BUFFER:
            target = GL_ELEMENT_ARRAY_BUFFER;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    switch(usage) {
        case GFX_BUFFER_USAGE_MUTABLE:
            usage_pattern = GL_DYNAMIC_DRAW;
            break;
        case GFX_BUFFER_USAGE_CONST:
            usage_pattern = GL_STATIC_DRAW;
            break;
        case GFX_BUFFER_USAGE_ONE_TIME:
            usage_pattern = GL_STREAM_DRAW;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    
    
    g_gl.GenBuffers(1, &id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR || id == 0) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_buffer_bind_safe(target, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    g_gl.BufferData(target, size, ptr, usage_pattern);
#ifndef GFX_NO_CHECKS
    switch(g_gl.GetError()) {
        case GL_NO_ERROR:
            break;
        case GL_OUT_OF_MEMORY:
            g_gl.BindBuffer(target, 0);
            return GFX_ERROR_OUT_OF_MEMORY;
        default:
            g_gl.BindBuffer(target, 0);
            return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_buffer_bind_safe(target, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_buffer_rewrite_generic(gfx_internal_buffer_type_t type, gfx_buffer_usage_t init_usage, size_t init_size, GLuint id, size_t offset, size_t size, void* ptr) {
    assert(offset > 0 && size > 0 && ptr != NULL);
    int i;
    GLenum target;
    gfx_result_t r;

    switch(type) {
        case GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER:
            target = GL_ARRAY_BUFFER;
            break;
        case GFX_BUFFER_TYPE_INDEX_BUFFER:
            target = GL_ELEMENT_ARRAY_BUFFER;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(offset+size > init_size
            || init_usage != GFX_BUFFER_USAGE_MUTABLE
            || g_gl.IsBuffer(id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_buffer_bind_safe(target, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
#ifdef GFX_DEBUG
    g_gl.GetBufferParameteriv(target, GL_BUFFER_SIZE, &i);
    if(i != init_size) {
        g_gl.BindBuffer(target, 0);
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.GetBufferParameteriv(target, GL_BUFFER_USAGE, &i);
    if(i != GL_DYNAMIC_DRAW) {
        g_gl.BindBuffer(target, 0);
        return GFX_ERROR_API_OTHER;
    }
#endif
    
    g_gl.BufferSubData(target, offset, size, ptr);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_buffer_bind_safe(target, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_buffer_destroy_generic(GLuint id) {
#ifndef GFX_NO_CHECKS
    if(g_gl.IsBuffer(id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    g_gl.DeleteBuffers(1, &id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}


gfx_result_t gfx_vertex_buffer_create(gfx_buffer_usage_t usage, size_t size, void* ptr, gfx_vertex_buffer_t* buffer) {
    assert(buffer != NULL);
    buffer[0].usage = usage;
    buffer[0].size = size;
    return gfx_buffer_create_generic(GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER, usage, size, ptr, &(buffer[0].id));
}
gfx_result_t gfx_vertex_buffer_rewrite(gfx_vertex_buffer_t buffer, size_t offset, size_t size, void* ptr) {
    return gfx_buffer_rewrite_generic(GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER, buffer.usage, buffer.size, buffer.id, offset, size, ptr);
}
gfx_result_t gfx_vertex_buffer_destroy(gfx_vertex_buffer_t buffer) {
    return gfx_buffer_destroy_generic(buffer.id);
}

gfx_result_t gfx_index_buffer_create(gfx_buffer_usage_t usage, size_t size, void* ptr, gfx_index_buffer_t* buffer) {
    assert(buffer != NULL);
    buffer[0].usage = usage;
    buffer[0].size = size;
    return gfx_buffer_create_generic(GFX_BUFFER_TYPE_INDEX_BUFFER, usage, size, ptr, &(buffer[0].id));
}
gfx_result_t gfx_index_buffer_rewrite(gfx_index_buffer_t buffer, size_t offset, size_t size, void* ptr) {
    return gfx_buffer_rewrite_generic(GFX_BUFFER_TYPE_INDEX_BUFFER, buffer.usage, buffer.size, buffer.id, offset, size, ptr);
}
gfx_result_t gfx_index_buffer_destroy(gfx_index_buffer_t buffer) {
    return gfx_buffer_destroy_generic(buffer.id);
}





static gfx_result_t gfx_texture_bind_safe(GLenum texture_unit, GLenum target, GLuint old_id, GLuint new_id) {
    GLenum targetinfo, e;
    GLuint i;
    switch(target) {
        case GL_TEXTURE_2D;
            targetinfo = GL_TEXTURE_BINDING_2D;
            break;
        case GL_TEXTURE_CUBE_MAP;
            targetinfo = GL_TEXTURE_BINDING_CUBE_MAP;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }


    g_gl.ActiveTexture(texture_unit);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

#ifdef GFX_DEBUG
    g_gl.GetIntegerv(GL_ACTIVE_TEXTURE, &e);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(e != texture_unit) {
        return GFX_ERROR_API_OTHER;
    }

    g_gl.GetIntegerv(targetinfo, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != old_id) {
        return GFX_ERROR_API_OTHER;
    }
#endif

    g_gl.BindTexture(target, new_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

#ifdef GFX_DEBUG
    g_gl.GetIntegerv(targetinfo, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != new_id) {
        return GFX_ERROR_API_OTHER;
    }
#endif

    return GFX_OK;
}
static gfx_result_t gfx_texture_image_safe(GLenum image_target, gfx_texture_image_data_t image_data, gfx_texture_dimensions_t* dimensions_out) {
    GLenum format; GLsizei width, height; const void* ptr;
    switch(image_data.format) {
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGBA:
        format = GL_RGBA;
        width = image_data.data.rgba_data.width;
        height = image_data.data.rgba_data.height;
        ptr = image_data.data.rgba_data.pixel_data;
        break;
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGB:
        format = GL_RGB;
        width = image_data.data.rgb_data.width;
        height = image_data.data.rgb_data.height;
        ptr = image_data.data.rgb_data.pixel_data;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }


#ifndef GFX_NO_CHECKS
    int max_2d_size = g_gl.limits.texture.max_image_pixelbuffer_size.regular_2d;
    int max_cube_size = g_gl.limits.texture.max_image_pixelbuffer_size.cube_map;
    switch(image_target) {
    case GL_TEXTURE_2D:
        if(width < 0 || height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(width > max_2d_size || height > max_2d_size) {
            return GFX_ERROR_TEXTURE_TOO_BIG;
        }
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if(width < 0 || height < 0 || width != height) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(width > max_cube_size || height > max_cube_size) {
            return GFX_ERROR_TEXTURE_TOO_BIG;
        }
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.TexImage2D(image_target, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, ptr);
#ifndef GFX_NO_CHECKS
    switch(g_gl.GetError()) {
    case GL_NO_ERROR:
        break;
    case GL_INVALID_VALUE:
        /* This is the only case that generates this error code that we did not exclude already */
        if(!is_power_of_two(width) ||!is_power_of_two(height))
            return GFX_ERROR_TEXTURE_NOT_POWER_OF_TWO_UNSUPPORTED;
        else return GFX_ERROR_UNKNOWN;
    default:
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    if(dimensions_out != NULL) {
        dimensions_out[0].width = width;
        dimensions_out[0].height = height;
    }
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_copy_image_safe(GLenum image_target, gfx_screen_rect_t rect, gfx_texture_image_data_format_t format) {
    GLenum format;
    switch(format) {
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGBA:
        format = GL_RGBA;
        break;
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGB:
        format = GL_RGB;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }


#ifndef GFX_NO_CHECKS
    int max_2d_size = g_gl.limits.texture.max_image_pixelbuffer_size.regular_2d;
    int max_cube_size = g_gl.limits.texture.max_image_pixelbuffer_size.cube_map;
    switch(image_target) {
    case GL_TEXTURE_2D:
        if(rect.width < 0 || rect.height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(rect.width > max_2d_size || rect.height > max_2d_size) {
            return GFX_ERROR_TEXTURE_TOO_BIG;
        }
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if(rect.width < 0 || rect.height < 0 || rect.width != rect.height) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(rect.width > max_cube_size || rect.height > max_cube_size) {
            return GFX_ERROR_TEXTURE_TOO_BIG;
        }
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.CopyTexImage2D(image_target, 0, format, rect.x, rect.y, rect.width, rect.height, 0);
#ifndef GFX_NO_CHECKS
    switch(g_gl.GetError()) {
    case GL_NO_ERROR:
        break;
    case GL_INVALID_VALUE:
        /* This is the only case that generates this error code that we did not exclude already */
        if(!is_power_of_two(width) ||!is_power_of_two(height))
            return GFX_ERROR_TEXTURE_NOT_POWER_OF_TWO_UNSUPPORTED;
        else return GFX_ERROR_UNKNOWN;
    default:
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_subimage_safe(GLenum image_target, gfx_texture_image_data_t image_data, gfx_texture_dimensions_t offset_rect, gfx_texture_dimensions_t creation_size) {
    GLenum format; GLsizei width, height; const void* ptr;
    switch(image_data.format) {
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGBA:
        format = GL_RGBA;
        width = image_data.data.rgba_data.width;
        height = image_data.data.rgba_data.height;
        ptr = image_data.data.rgba_data.pixel_data;
        break;
    case GFX_TEXTURE_IMAGE_DATA_FORMAT_RGB:
        format = GL_RGB;
        width = image_data.data.rgb_data.width;
        height = image_data.data.rgb_data.height;
        ptr = image_data.data.rgb_data.pixel_data;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(width < 0 || height < 0 || offset_rect.width < 0 || offset_rect.height < 0 ||
        width + offset_rect.width > creation_size.width || height + offset_rect.height > creation_size.height) {
        return GFX_ERROR_INVALID_PARAM;
    }
    
    switch(image_target) {
    case GL_TEXTURE_2D:
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.TexSubImage2D(image_target, 0, offset_rect.width, offset_rect.height, width, height, format, GL_UNSIGNED_BYTE, ptr);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_copy_subimage_safe(GLenum image_target, gfx_screen_rect_t rect, gfx_texture_dimensions_t offset_rect, gfx_texture_dimensions_t creation_size) {
#ifndef GFX_NO_CHECKS
    if(rect.width < 0 || rect.height < 0 || offset_rect.width < 0 || offset_rect.height < 0 ||
        rect.width + offset_rect.width > creation_size.width || rect.height + offset_rect.height > creation_size.height) {
        return GFX_ERROR_INVALID_PARAM;
    }
    
    switch(image_target) {
    case GL_TEXTURE_2D:
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.CopyTexSubImage2D(image_target, 0, offset_rect.width, offset_rect.height, rect.x, rect.y, rect.width, rect.height);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_set_zooming_modes(GLenum image_target, gfx_texture_zooming_in_mode_t magnifying_mode, gfx_texture_zooming_out_mode_t minifying_mode) {
    GLenum mag_filter, min_filter;
    
    switch(magnifying_mode) {
    case GFX_TEXTURE_ZOOMING_IN_MODE_NEAREST_ELEMENT:
        mag_filter = GL_NEAREST;
        break;
    case GFX_TEXTURE_ZOOMING_IN_MODE_LINEAR_AVERAGE_OF_FOUR:
        mag_filter = GL_LINEAR;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    g_gl.TexParameteri(image_target, GL_TEXTURE_MAG_FILTER, mag_filter);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    switch(minifying_mode) {
    case GFX_TEXTURE_ZOOMING_OUT_MODE_NEAREST_ELEMENT:
        min_filter = GL_NEAREST;
        break;
    case GFX_TEXTURE_ZOOMING_OUT_MODE_LINEAR_AVERAGE_OF_FOUR:
        min_filter = GL_LINEAR;
        break;
    case GFX_TEXTURE_ZOOMING_OUT_MODE_MATCHING_MIPMAP_NEAREST_ELEMENT:
        min_filter = GL_NEAREST_MIPMAP_NEAREST;
        break;
    case GFX_TEXTURE_ZOOMING_OUT_MODE_MATCHING_MIPMAP_LINEAR_AVERAGE_OF_FOUR:
        min_filter = GL_LINEAR_MIPMAP_NEAREST;
        break;
    case GFX_TEXTURE_ZOOMING_OUT_MODE_TWO_MIPMAP_AVERAGE_NEAREST_ELEMENT:
        min_filter = GL_NEAREST_MIPMAP_LINEAR;
        break;
    case GFX_TEXTURE_ZOOMING_OUT_MODE_TWO_MIPMAP_AVERAGE_LINEAR_AVERAGE_OF_FOUR:
        min_filter = GL_LINEAR_MIPMAP_LINEAR;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    g_gl.TexParameteri(image_target, GL_TEXTURE_MIN_FILTER, min_filter);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_check_zooming_modes(GLenum image_target, gfx_texture_zooming_in_mode_t magnifying_mode, gfx_texture_zooming_out_mode_t minifying_mode) {
#ifndef GFX_NO_CHECKS
    GLint i;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    switch(i) {
    case GL_NEAREST:
        if(magnifying_mode != GFX_TEXTURE_ZOOMING_IN_MODE_NEAREST_ELEMENT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_LINEAR:
        if(magnifying_mode != GFX_TEXTURE_ZOOMING_IN_MODE_LINEAR_AVERAGE_OF_FOUR)
            return GFX_ERROR_INVALID_PARAM;
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
    
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    switch(i) {
    case GL_NEAREST:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_NEAREST_ELEMENT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_LINEAR:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_LINEAR_AVERAGE_OF_FOUR)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_NEAREST_MIPMAP_NEAREST:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_MATCHING_MIPMAP_NEAREST_ELEMENT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_LINEAR_MIPMAP_NEAREST:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_MATCHING_MIPMAP_LINEAR_AVERAGE_OF_FOUR)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_NEAREST_MIPMAP_LINEAR:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_TWO_MIPMAP_AVERAGE_NEAREST_ELEMENT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_LINEAR_MIPMAP_LINEAR:
        if(minifying_mode != GFX_TEXTURE_ZOOMING_OUT_MODE_TWO_MIPMAP_AVERAGE_LINEAR_AVERAGE_OF_FOUR)
            return GFX_ERROR_INVALID_PARAM;
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
#endif

    return GFX_OK;
}
static gfx_result_t gfx_texture_set_wrapping_modes(GLenum image_target, gfx_texture_wrapping_mode_t horizontal_wrap, gfx_texture_wrapping_mode_t vertical_wrap) {
    GLenum wrap_s, wrap_t;
    
    switch(horizonatal_wrap) {
    case GFX_TEXTURE_WRAPPING_MODE_CLAMP_TO_EDGE:
        wrap_s = GL_CLAMP_TO_EDGE;
        break;
    case GFX_TEXTURE_WRAPPING_MODE_REPEAT:
        wrap_s = GL_REPEAT;
        break;
    case GFX_TEXTURE_WRAPPING_MODE_MIRRORED_REPEAT:
        wrap_s = GL_MIRRORED_REPEAT;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    g_gl.TexParameteri(image_target, GL_TEXTURE_WRAP_S, wrap_s);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    switch(vertical_wrap) {
    case GFX_TEXTURE_WRAPPING_MODE_CLAMP_TO_EDGE:
        wrap_t = GL_CLAMP_TO_EDGE;
        break;
    case GFX_TEXTURE_WRAPPING_MODE_REPEAT:
        wrap_t = GL_REPEAT;
        break;
    case GFX_TEXTURE_WRAPPING_MODE_MIRRORED_REPEAT:
        wrap_t = GL_MIRRORED_REPEAT;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    g_gl.TexParameteri(image_target, GL_TEXTURE_WRAP_T, wrap_t);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_check_wrapping_modes(GLenum image_target, gfx_texture_wrapping_mode_t horizontal_wrap, gfx_texture_wrapping_mode_t vertical_wrap) {
#ifndef GFX_NO_CHECKS
    GLint i;
    
    glGetTexParameteriv(image_target, GL_TEXTURE_WRAP_S, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    switch(i) {
    case GL_CLAMP_TO_EDGE:
        if(horizontal_wrap != GFX_TEXTURE_WRAPPING_MODE_CLAMP_TO_EDGE)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_REPEAT:
        if(horizontal_wrap != GFX_TEXTURE_WRAPPING_MODE_REPEAT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_MIRRORED_REPEAT:
        if(horizontal_wrap != GFX_TEXTURE_WRAPPING_MODE_MIRRORED_REPEAT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
    
    glGetTexParameteriv(image_target, GL_TEXTURE_WRAP_T, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    switch(i) {
    case GL_CLAMP_TO_EDGE:
        if(vertical_wrap != GFX_TEXTURE_WRAPPING_MODE_CLAMP_TO_EDGE)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_REPEAT:
        if(vertical_wrap != GFX_TEXTURE_WRAPPING_MODE_REPEAT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    case GL_MIRRORED_REPEAT:
        if(vertical_wrap != GFX_TEXTURE_WRAPPING_MODE_MIRRORED_REPEAT)
            return GFX_ERROR_INVALID_PARAM;
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}
static gfx_result_t gfx_texture_destroy_generic(GLuint id) {
#ifndef GFX_NO_CHECKS
    if(g_gl.IsTexture(id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
#ifndef GFX_NO_CHECKS
    g_gl.DeleteTextures(1, &id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}

gfx_result_t gfx_texture_create(gfx_texture_image_data_t data, gfx_texture_config_t config, gfx_texture_t* texture) {
    GLuint id, i;
    gfx_result_t r;
    gfx_texture_dimensions_t dim;
    
#ifndef GFX_NO_CHECKS
    if(texture == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.GenTextures(1, &id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR || id == 0) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    /* For the different mipmap generation hints see https://www.khronos.org/opengl/wiki/Common_Mistakes#Automatic_mipmap_generation */
    /* GL 2.1 mipmap generation hint: */
    if(g_gl.version == GL2) {
        g_gl.TexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }

    r = gfx_texture_image_safe(GL_TEXTURE_2D, data, &dim);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    /* GL 3+ and GL ES 2 mipmap generation: */
    if(g_gl.version == GLES2 || g_gl.version == GL3Core) {
        g_gl.GenerateMipmap(GL_TEXTURE_2D);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    
    r = gfx_texture_set_zooming_modes(GL_TEXTURE_2D, config.magnifying_mode, config.minifying_mode);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_set_wrapping_modes(GL_TEXTURE_2D, config.horizontal_wrap, config.vertical_wrap);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    texture[0].id = id;
    texture[0].format = data.format;
    texture[0].dimensions = dim;
    texture[0].config = config;

    return GFX_OK;
}
gfx_result_t gfx_texture_create_from_screen(gfx_screen_rect_t rect, gfx_texture_image_data_format_t format. gfx_texture_config_t config, gfx_texture_t* texture) {
    GLuint id, i;
    gfx_result_t r;
    gfx_texture_dimensions_t dim;
    
#ifndef GFX_NO_CHECKS
    if(texture == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.GenTextures(1, &id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR || id == 0) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif


    /* GL 2.1 mipmap generation hint: */
    if(g_gl.version == GL2) {
        g_gl.TexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }

    r = gfx_texture_copy_image_safe(GL_TEXTURE_2D, rect, format);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    /* GL 3+ and GL ES 2 mipmap generation: */
    if(g_gl.version == GLES2 || g_gl.version == GL3Core) {
        g_gl.GenerateMipmap(GL_TEXTURE_2D);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    
    r = gfx_texture_set_zooming_modes(GL_TEXTURE_2D, config.magnifying_mode, config.minifying_mode);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_set_wrapping_modes(GL_TEXTURE_2D, config.horizontal_wrap, config.vertical_wrap);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    texture[0].id = id;
    texture[0].format = format;
    texture[0].dimensions.width = rect.width;
    texture[0].dimensions.height = rect.height;
    texture[0].config = config;

    return GFX_OK;
}
gfx_result_t gfx_texture_rewrite(gfx_texture_t texture, gfx_texture_dimensions_t offset_rect, gfx_texture_image_data_t data) {
    gfx_result_t r;
    
#ifndef GFX_NO_CHECKS
    if(texture.format != data.format) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, 0, texture.id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, texture.config.magnifying_mode, texture.config.minifying_mode);
    if(r != GFX_OK) { return r; }

    r = gfx_texture_check_wrapping_modes(GL_TEXTURE_2D, texture.config.horizontal_wrap, texture.config.vertical_wrap);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_subimage_safe(GL_TEXTURE_2D, data, offset_rect, texture.dimensions);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    return GFX_OK;
}
gfx_result_t gfx_texture_rewrite_from_screen(gfx_texture_t texture, gfx_texture_dimensions_t offset_rect, gfx_screen_rect_t rect) {
    gfx_result_t r;
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, 0, texture.id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, texture.config.magnifying_mode, texture.config.minifying_mode);
    if(r != GFX_OK) { return r; }

    r = gfx_texture_check_wrapping_modes(GL_TEXTURE_2D, texture.config.horizontal_wrap, texture.config.vertical_wrap);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_copy_subimage_safe(GL_TEXTURE_2D, rect, offset_rect, texture.dimensions);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_2D, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    return GFX_OK;    
}
gfx_result_t gfx_texture_destroy(gfx_texture_t texture) {
    return gfx_texture_destroy_generic(texture.id);
}


gfx_result_t gfx_cubemap_create(gfx_texture_image_data_t* x_pos_data, gfx_texture_image_data_t* x_neg_data,
                                gfx_texture_image_data_t* y_pos_data, gfx_texture_image_data_t* y_neg_data,
                                gfx_texture_image_data_t* z_pos_data, gfx_texture_image_data_t* z_neg_data, gfx_cubemap_config_t config, gfx_cubemap_t* cubemap) {
    GLuint id, i;
    gfx_result_t r;
    
#ifndef GFX_NO_CHECKS
    if(cubemap == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    g_gl.GenTextures(1, &id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR || id == 0) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif


    /* GL 2.1 mipmap generation hint: */
    if(g_gl.version == GL2) {
        g_gl.TexParameteri(GL_TEXTURE_CUBE_MAP, GL_GENERATE_MIPMAP, GL_TRUE);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    
    r = gfx_texture_set_zooming_modes(GL_TEXTURE_2D, config.magnifying_mode, config.minifying_mode);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    g_gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    g_gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    g_gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#ifndef GFX_NO_CHECKS
    switch(g_gl.GetError() != GL_NO_ERROR) {
    case GL_NO_ERROR:
        break;
    case GL_INVALID_ENUM:
        /* explicitly allowed since some GL versions need the GL_TEXTURE_WRAP_R param and some don't allow it */
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    
    cubemap[0].id = id;
    cubemap[0].config = config;
    cubemap[0].face_data = {0};
    
    if(x_pos_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_POSITIVE_X, x_pos_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }
    if(x_neg_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_NEGATIVE_X, x_neg_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }
    if(y_pos_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_POSITIVE_Y, y_pos_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }
    if(y_neg_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_NEGATIVE_Y, y_neg_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }
    if(z_pos_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_POSITIVE_Z, z_pos_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }
    if(z_neg_data != NULL) {
        r = gfx_cubemap_add_face(cubemap, GFX_CUBE_MAP_FACETYPE_NEGATIVE_Z, z_neg_data[0]);
#ifndef GFX_NO_CHECKS
        if(r != GFX_OK) { return r; }
#endif
    }

    return GFX_OK;
}
gfx_result_t gfx_cubemap_add_face(gfx_cubemap_t* cubemap, gfx_cubemap_facetype_t face, gfx_texture_image_data_t data) {
    GLenum image_target;
    gfx_cubemap_face_data_t* data_ptr;
    gfx_texture_dimensions_t dim;
    bool other_faces_fully_created;
    
#ifndef GFX_NO_CHECKS
    if(cubemap == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
        
    switch(face) {
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        data_ptr = &(cubemap[0].face_data.positive_x);
        other_faces_fully_created = (cubemap[0].face_data.negative_x.created
            && cubemap[0].face_data.positive_y.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        data_ptr = &(cubemap[0].face_data.negative_x);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.positive_y.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        data_ptr = &(cubemap[0].face_data.positive_y);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        data_ptr = &(cubemap[0].face_data.negative_y);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        data_ptr = &(cubemap[0].face_data.positive_z);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.negative_y.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        data_ptr = &(cubemap[0].face_data.negative_z);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.negative_y.created && cubemap[0].face_data.positive_z.created);
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(data_ptr[0].created) {
        return GFX_ERROR_OPERATION_INVALID;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, cubemap[0].config.magnifying_mode, cubemap[0].config.minifying_mode);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_image_safe(image_target, data, &dim);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    if(other_faces_fully_created) {
        /* GL 3+ and GL ES 2 mipmap generation _only when cubemap face-complete_: */
        if(g_gl.version == GLES2 || g_gl.version == GL3Core) {
            g_gl.GenerateMipmap(GL_TEXTURE_CUBE_MAP);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
        }
    }

    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    data_ptr[0].created = true;
    data_ptr[0].format = data.format;
    data_ptr[0].dimensions = dim;
    
    return GFX_OK;
}
gfx_result_t gfx_cubemap_add_face_from_screen(gfx_cubemap_t* cubemap, gfx_cubemap_facetype_t face, gfx_screen_rect_t rect, gfx_texture_image_data_format_t format) {
    GLenum image_target;
    gfx_cubemap_face_data_t* data_ptr;
    bool other_faces_fully_created;
    
#ifndef GFX_NO_CHECKS
    if(cubemap == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
        
    switch(face) {
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        data_ptr = &(cubemap[0].face_data.positive_x);
        other_faces_fully_created = (cubemap[0].face_data.negative_x.created
            && cubemap[0].face_data.positive_y.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        data_ptr = &(cubemap[0].face_data.negative_x);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.positive_y.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        data_ptr = &(cubemap[0].face_data.positive_y);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.negative_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        data_ptr = &(cubemap[0].face_data.negative_y);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.positive_z.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        data_ptr = &(cubemap[0].face_data.positive_z);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.negative_y.created && cubemap[0].face_data.negative_z.created);
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        data_ptr = &(cubemap[0].face_data.negative_z);
        other_faces_fully_created = (cubemap[0].face_data.positive_x.created
            && cubemap[0].face_data.negative_x.created && cubemap[0].face_data.positive_y.created
            && cubemap[0].face_data.negative_y.created && cubemap[0].face_data.positive_z.created);
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(data_ptr[0].created) {
        return GFX_ERROR_OPERATION_INVALID;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, cubemap[0].config.magnifying_mode, cubemap[0].config.minifying_mode);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_copy_image_safe(image_target, rect, format);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    if(other_faces_fully_created) {
        /* GL 3+ and GL ES 2 mipmap generation _only when cubemap face-complete_: */
        if(g_gl.version == GLES2 || g_gl.version == GL3Core) {
            g_gl.GenerateMipmap(GL_TEXTURE_CUBE_MAP);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
        }
    }

    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    data_ptr[0].created = true;
    data_ptr[0].format = format;
    data_ptr[0].dimensions.width = rect.width;
    data_ptr[0].dimensions.height = rect.height;
    
    return GFX_OK;
}
gfx_result_t gfx_cubemap_rewrite_face(gfx_cubemap_t cubemap, gfx_cubemap_facetype_t face, gfx_texture_dimensions_t offset_rect. gfx_texture_image_data_t data) {
    GLenum image_target;
    gfx_cubemap_face_data_t face_data;
    
#ifndef GFX_NO_CHECKS
    if(cubemap == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
    switch(face) {
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        face_data = cubemap[0].face_data.positive_x;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        face_data = cubemap[0].face_data.negative_x;
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        face_data = cubemap[0].face_data.positive_y;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        face_data = cubemap[0].face_data.negative_y;
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        face_data = cubemap[0].face_data.positive_z;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        face_data = cubemap[0].face_data.negative_z;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(!face_data.created) {
        return GFX_ERROR_OPERATION_INVALID;
    }
    
    if(!face_data.format != data.format) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, cubemap[0].config.magnifying_mode, cubemap[0].config.minifying_mode);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_subimage_safe(image_target, data, offset_rect, face_data.dimensions);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    return GFX_OK;
}
gfx_result_t gfx_cubemap_rewrite_face_from_screen(gfx_cubemap_t cubemap, gfx_cubemap_facetype_t face, gfx_texture_dimensions_t offset_rect. gfx_screen_rect_t rect) {
    GLenum image_target;
    gfx_cubemap_face_data_t face_data;
    
#ifndef GFX_NO_CHECKS
    if(cubemap == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
    switch(face) {
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        face_data = cubemap[0].face_data.positive_x;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_X:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        face_data = cubemap[0].face_data.negative_x;
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        face_data = cubemap[0].face_data.positive_y;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Y:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        face_data = cubemap[0].face_data.negative_y;
        break;
    case GFX_CUBE_MAP_FACETYPE_POSITIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        face_data = cubemap[0].face_data.positive_z;
        break;
    case GFX_CUBE_MAP_FACETYPE_NEGATIVE_Z:
        image_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        face_data = cubemap[0].face_data.negative_z;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
#ifndef GFX_NO_CHECKS
    if(!face_data.created) {
        return GFX_ERROR_OPERATION_INVALID;
    }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, 0, id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
#ifdef GFX_DEBUG
    r = gfx_texture_check_zooming_modes(GL_TEXTURE_2D, cubemap[0].config.magnifying_mode, cubemap[0].config.minifying_mode);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_texture_copy_subimage_safe(image_target, rect, offset_rect, face_data.dimensions);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    r = gfx_texture_bind_safe(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    return GFX_OK;
}
gfx_result_t gfx_cubemap_destroy(gfx_cubemap_t cubemap) {
    return gfx_texture_destroy_generic(cubemap.id);
}


static gfx_result_t gfx_shader_create_shader_object(GLenum type, const char* source, GLuint* out_id) {
    GLuint id;
    GLint source_len, i;
    char* str; GLsizei s;
    
#ifndef GFX_NO_CHECKS
    if(out_id == NULL) {
        return GFX_ERROR_UNKNOWN;
    }
    switch(type) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
        break;
    default:
        return GFX_ERROR_UNKNOWN;
    }
    
    source_len = strlen(source);
#endif
    
    id = g_gl.CreateShader(type);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(id == 0) {
        return GFX_ERROR_SHADER_CREATE;
    }
#endif
#ifdef GFX_DEBUG
    if(g_gl.IsShader(id) != GL_TRUE) {
        return GFX_ERROR_UNKNOWN;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetShaderiv(id, GL_SHADER_TYPE, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != type) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

#ifdef GFX_NO_CHECKS
    g_gl.ShaderSource(id, 1, source, NULL);
#else
    g_gl.ShaderSource(id, 1, source, &source_len);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
#ifdef GFX_DEBUG
    g_gl.GetShaderiv(id, GL_SHADER_SOURCE_LENGTH, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != source_len+1) {
        return GFX_ERROR_UNKNOWN;
    }
    
    str = calloc(source_len+1, 1);
    g_gl.GetShaderSource(id, source_len+1, &s, str);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(s != source_len || strcmp(str, source) != 0) {
        return GFX_ERROR_UNKNOWN;
    }
    free(str);
#endif

    g_gl.CompileShader(vertex_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetShaderiv(id, GL_COMPILE_STATUS, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != GL_TRUE) {
        return GFX_ERROR_SHADER_COMPILATION_FAILED;
    }
#endif

#ifdef GFX_DEBUG
    g_gl.GetShaderiv(id, GL_INFO_LOG_LENGTH, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    str = calloc(i, 1);
    g_gl.GetShaderInfoLog(id, i, &s, str);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(s != i-1) {
        return GFX_ERROR_UNKNOWN;
    }
    fprintf(stderr, "OpenGL Shader Info Log for object with id %i:\n %s", id, str);
    free(str);
#endif
    
    out_id[0] = id;

    return GFX_OK;
}
static gfx_result_t gfx_shader_check(gfx_shader_t shader) {
#ifdef GFX_DEBUG
    GLint i; GLsizei s; GLuint a[2];

    if(g_gl.IsShader(shader.vertex_id) != GL_TRUE) {
        return GFX_ERROR_UNKNOWN;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetShaderiv(shader.vertex_id, GL_SHADER_TYPE, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != GL_VERTEX_SHADER) {
        return GFX_ERROR_UNKNOWN;
    }
    
    if(g_gl.IsShader(shader.fragment_id) != GL_TRUE) {
        return GFX_ERROR_UNKNOWN;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetShaderiv(shader.fragment_id, GL_SHADER_TYPE, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != GL_FRAGMENT_SHADER) {
        return GFX_ERROR_UNKNOWN;
    }

    if(g_gl.IsProgram(shader.program_id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    g_gl.GetProgramiv(shader.program_id, GL_ATTACHED_SHADERS, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != 2) {
        return GFX_ERROR_UNKNOWN;
    }
    glGetAttachedShaders(shader.program_id, 2, &s, a);
    if(s != 2) {
        return GFX_ERROR_UNKNOWN;
    }
    if(!((a[0] == shader.vertex_id && a[1] == shader.fragment_id) || (a[0] == shader.fragment_id && a[1] == shader.vertex_id))) {
        return GFX_ERROR_UNKNOWN;
    }
    
    return GFX_OK;
#endif
}
static gfx_result_t gfx_shader_link_and_validate(GLuint program_id) {
    GLint i; 
    
    glLinkProgram(program_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetProgramiv(program_id, GL_LINK_STATUS, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != GL_TRUE) {
        return GFX_ERROR_SHADER_PROGRAM_LINK;
    }
#endif

    glValidateProgram(program_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    g_gl.GetProgramiv(program_id, GL_VALIDATE_STATUS, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != GL_TRUE) {
        return GFX_ERROR_SHADER_PROGRAM_VALIDATE;
    }
#endif

#ifdef GFX_DEBUG
    g_gl.GetProgramiv(program_id, GL_INFO_LOG_LENGTH, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    str = calloc(i, 1);
    g_gl.GetProgramInfoLog(id, i, &s, str);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(s != i-1) {
        return GFX_ERROR_UNKNOWN;
    }
    fprintf(stderr, "OpenGL Program Info Log for object with id %i:\n %s", program_id, str);
    free(str);
#endif
}


const char* GL_prefix = "#version 110\n";
const char* GLES_vertex_prefix = "#version 100\n";
const char* GLES_fragment_prefix =  "#version 100\n"
                                    "#if GL_FRAGMENT_PRECISION_HIGH\n"
                                    "precision highp int;\n"
                                    "precision highp float;\n"
                                    "#else\n"
                                    "precision mediump float;\n"
                                    "#endif\n";

gfx_result_t gfx_shader_create(const char* vertex_shader_source_versionless, const char* fragment_shader_source_versionless;, gfx_shader_t* shader) {
    GLuint vertex_id, fragment_id, program_id;
    GLint i; char* str; GLsizei s; GLuint a[2];
    gfx_result_t r;
    char* shader_source, prefix; int oldlen;
    
#ifndef GFX_NO_CHECKS
    if(vertex_shader_source_versionless == NULL || fragment_shader_source_versionless == NULL || shader == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    oldlen = strlen(vertex_shader_source_versionless)+1;
    if(g_gl.version == GL2 || g_gl.version == GL3Core) {
        prefix = GL_prefix;
    }
    if(g_gl.version == GLES2) {
        prefix = GLES_vertex_prefix;
    }
    shader_source = calloc(oldlen+strlen(prefix),1);
    memcpy(shader_source, prefix, strlen(prefix));
    memcpy(&shader_source[strlen(prefix)], vertex_shader_source_versionless, oldlen);
    
    r = gfx_shader_create_shader_object(GL_VERTEX_SHADER, shader_source, &vertex_id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    free(shader_source);
    
    

    oldlen = strlen(fragment_shader_source_versionless)+1;
    if(g_gl.version == GL2 || g_gl.version == GL3Core) {
        prefix = GL_prefix;
    }
    if(g_gl.version == GLES2) {
        prefix = GLES_fragment_prefix;
    }
    shader_source = calloc(oldlen+strlen(prefix),1);
    memcpy(shader_source, prefix, strlen(prefix));
    memcpy(&shader_source[strlen(prefix)], fragment_shader_source_versionless, oldlen);
    
    r = gfx_shader_create_shader_object(GL_FRAGMENT_SHADER, fragment_shader_source, &fragment_id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    free(shader_source);
    
    
    
    program_id = glCreateProgram();
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(program_id == 0) {
        return GFX_ERROR_SHADER_PROGRAM_CREATE;
    }
#endif
#ifdef GFX_DEBUG
    if(g_gl.IsProgram(program_id) != GL_TRUE) {
        return GFX_ERROR_UNKNOWN;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    glAttachShader(program_id, vertex_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glAttachShader(program_id, fragment_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
#ifdef GFX_DEBUG
    g_gl.GetProgramiv(program_id, GL_ATTACHED_SHADERS, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != 2) {
        return GFX_ERROR_UNKNOWN;
    }
    
    glGetAttachedShaders(program_id, 2, &s, a);
    if(s != 2) {
        return GFX_ERROR_UNKNOWN;
    }
    if(!((a[0] == vertex_id && a[1] == fragment_id) || (a[0] == fragment_id && a[1] == vertex_id))) {
        return GFX_ERROR_UNKNOWN;
    }
#endif


    r = gfx_shader_link_and_validate(program_id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif


    /* We explicitly keep the vertex and fragment shader id's to be able to detach them correctly later. */
    shader[0].vertex_id = program_id;
    shader[0].fragment_id = program_id;
    shader[0].program_id = program_id;
    return GFX_OK;
}
gfx_result_t gfx_shader_destroy(gfx_shader_t shader) {
#ifdef GFX_DEBUG
    gfx_result_t r = gfx_shader_check(shader);
    if(r != GFX_OK) { return r; }
#endif

    glDetachShader(shader.program_id, shader.vertex_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glDeleteShader(shader.vertex_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glDetachShader(shader.program_id, shader.fragment_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    glDeleteShader(shader.fragment_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    g_gl.DeleteProgram(shader.program_id);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    
    return GFX_OK;
}









gfx_result_t gfx_shader_associate_attributes_indices(gfx_shader_t shader_program, uint32_t* indices, const char** variable_names, size_t count) {
    GLuint id = shader_program.program_id;
    GLint i; GLsizei s; GLenum e; char* str; int len; gfx_result_t r;
    
    for(int i = 0; i < count; i++) {
        uint32_t index = indices[i];
        const char* name = variable_names[i];
        
#ifndef GFX_NO_CHECKS
        if(index < 0 || index >= g_gl.limits.max_vertex_attributes) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
#ifdef GFX_DEBUG
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != GL_TRUE) {
            return GFX_ERROR_UNKNOWN;
        }
#endif

        g_gl.BindAttribLocation(id, index, name);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
#ifdef GFX_DEBUG
        i = g_gl.GetAttribLocation(id, name);
        if(i != index) {
            return GFX_ERROR_UNKNOWN;
        }
        len = strlen(name);
        str = calloc(len+1,1);
        glGetActiveAttrib(id, index, len+1, &s, &i, &e, str);
        /* size is always 1 since attributes can't be arrays */
        if(s != len || strcmp(name, str) != 0 || i != 1) {
            return GFX_ERROR_UNKNOWN;
        }
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        switch(e) {
        case GL_FLOAT:
            if(i != 1) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_VEC2:
            if(i != 2) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_VEC3:
            if(i != 3) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_VEC4:
            if(i != 4) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_MAT2:
            if(i != 2) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_MAT3:
            if(i != 3) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        case GL_FLOAT_MAT4:
            if(i != 4) {
                return GFX_ERROR_UNKNOWN;
            }
            break;
        default:
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    
    r = gfx_shader_link_and_validate(id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif
    
    return GFX_OK;
}

gfx_result_t gfx_vertex_attribute_index_alloc(uint32_t index, gfx_attribute_data_type_t type, gfx_vertex_buffer_t* buffer, uint32_t offset, uint32_t stride) {
    int size_per_column, columns; void* p; GLint i;
    
#ifndef GFX_NO_CHECKS
    if(buffer == NULL || index < 0 || index >= g_gl.limits.max_vertex_attributes) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
        
    r = gfx_buffer_bind_safe(GL_ARRAY_BUFFER, 0, buffer[0].id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    switch(type) {
    case GFX_UNIFORM_DATA_TYPE_FLOAT:
        size_per_column = 1;
        columns = 1;
        break;
    case GFX_UNIFORM_DATA_TYPE_VEC2:
        size_per_column = 2;
        columns = 1;
        break;
    case GFX_UNIFORM_DATA_TYPE_VEC3:
        size_per_column = 3;
        columns = 1;
        break;
    case GFX_UNIFORM_DATA_TYPE_VEC4:
        size_per_column = 4;
        columns = 1;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT2:
        size_per_column = 2;
        columns = 2;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT3:
        size_per_column = 3;
        columns = 3;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT4:
        size_per_column = 4;
        columns = 4;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }

    for(int col = 0; col < columns; col++) {
        g_gl.EnableVertexAttribArray(index+col);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif

#ifdef GFX_DEBUG
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != GL_TRUE) {
            return GFX_ERROR_UNKNOWN;
        }
#endif

        g_gl.VertexAttribPointer(index+col, size_per_column, GL_FLOAT, GL_FALSE, stride, (void*)(offset + (sizeof(float)*col*size_per_column)));
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif

#ifdef GFX_DEBUG
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != buffer[0].id) {
            return GFX_ERROR_UNKNOWN;
        }
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_SIZE, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != size_per_column) {
            return GFX_ERROR_UNKNOWN;
        }
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != stride) {
            return GFX_ERROR_UNKNOWN;
        }
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_TYPE, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != GL_FLOAT) {
            return GFX_ERROR_UNKNOWN;
        }
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != GL_FALSE) {
            return GFX_ERROR_UNKNOWN;
        }

        g_gl.GetVertexAttribPointerv(index+col, GL_VERTEX_ATTRIB_ARRAY_POINTER, &p);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(p != (void*)(offset + (sizeof(float)*col*size_per_column))) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }

    r = gfx_buffer_bind_safe(GL_ARRAY_BUFFER, buffer[0].id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    buffer[0].usages++;
        
    return GFX_OK;
}
gfx_result_t gfx_vertex_attribute_index_free(uint32_t index, gfx_attribute_data_type_t type) {
    int columns;
    
#ifndef GFX_NO_CHECKS
    if(index < 0 || index >= g_gl.limits.max_vertex_attributes) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
        
    switch(type) {
    case GFX_UNIFORM_DATA_TYPE_FLOAT:
    case GFX_UNIFORM_DATA_TYPE_VEC2:
    case GFX_UNIFORM_DATA_TYPE_VEC3:
    case GFX_UNIFORM_DATA_TYPE_VEC4:
        columns = 1;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT2:
        columns = 2;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT3:
        columns = 3;
        break;
    case GFX_UNIFORM_DATA_TYPE_MAT4:
        columns = 4;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }

    for(int col = 0; col < columns; col++) {
        g_gl.DisableVertexAttribArray(index+col);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
#endif

#ifdef GFX_DEBUG
        glGetVertexAttribiv(index+col, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &i);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != GL_FALSE) {
            return GFX_ERROR_UNKNOWN;
        }
#endif
    }
    return GFX_OK;
}

static gfx_result_t gfx_switch_current_program_id(GLuint oldid, GLuint newid) {
#ifdef GFX_DEBUG
    GLint i;
    glGetIntegerv(GL_CURRENT_PROGRAM, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != oldid) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    g_gl.UseProgram(newid);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_COULD_NOT_ACTIVATE_SHADER;
    }
#endif
#ifdef GFX_DEBUG
    glGetIntegerv(GL_CURRENT_PROGRAM, &i);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(i != newid) {
        return GFX_ERROR_UNKNOWN;
    }
#endif
    return GFX_OK;
}



gfx_result_t gfx_setup_uniforms(gfx_shader_t shader_program, gfx_uniform_data_info_t* uniforms, size_t nr_uniforms) {
    char* str; int len; GLsizei s; GLint i; GLenum e;
    gfx_result_t r;
    GLuint id = shader_program.program_id;
    GLint check_iv[4]; GLfloat check_fv[16];
    int used_units_for_textures = 0, used_units_for_cubemaps = 0;
    
    if(nr_uniforms == 0) {
#ifndef GFX_NO_CHECKS
        if(uniforms != NULL) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        return GFX_OK;
    }
    
#ifndef GFX_NO_CHECKS
    if(uniforms == NULL || nr_uniforms < 0) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
#ifdef GFX_DEBUG
    r = gfx_shader_check(shader_program);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_switch_current_program_id(0, shader_program.program_id);
#ifdef GFX_DEBUG
    if(r != GFX_OK) { return r; }
#endif
    
    for(int i = 0; i < nr_uniforms; i++) {
        GLint loc;
        
        loc = glGetUniformLocation(id, uniforms[i].name);
#ifndef GFX_NO_CHECKS
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(loc == -1) {
            return GFX_ERROR_UNIFORM_NOT_FOUND;
        }
#endif

#ifdef GFX_DEBUG
        if(uniforms[i].name == NULL) {
            return GFX_ERROR_UNKNOWN; /* No param error bc it should have failed earlier */
        }
        len = strlen(uniforms[i].name);
        str = calloc(len+1,1);
        glGetActiveUniform(id, loc, len, &s, &i, &e, str);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
        if(s != len || strcmp(str, uniforms[i].name) != 0) {
            return GFX_ERROR_UNKNOWN;
        }
        if(i != uniforms[i].array_size) {
            return GFX_ERROR_INVALID_PARAM;
        }
        switch(e) {
        case GL_FLOAT:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_FLOAT)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_VEC2:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_VEC2)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_VEC3:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_VEC3)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_VEC4:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_VEC4)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_INT:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_INT)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_INT_VEC2:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_IVEC2)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_INT_VEC3:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_IVEC3)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_INT_VEC4:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_IVEC4)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_BOOL:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_BOOL)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_BOOL_VEC2:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_BVEC2)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_BOOL_VEC3:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_BVEC3)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_BOOL_VEC4:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_BVEC4)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_MAT2:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_MAT2)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_MAT3:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_MAT3)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_FLOAT_MAT4:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_MAT4)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_SAMPLER_2D:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_SAMPLER_2D)
                return GFX_ERROR_INVALID_PARAM;
            break;
        case GL_SAMPLER_CUBE:
            if(uniforms[i].type != GFX_UNIFORM_DATA_TYPE_SAMPLER_CUBE_MAP)
                return GFX_ERROR_INVALID_PARAM;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
        free(str);
#endif

        switch(uniforms[i].type) {
        case GFX_UNIFORM_DATA_TYPE_INT:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.iv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform1iv(loc, uniforms[i].array_size, uniforms[i].data.iv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.iv[i]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_IVEC2:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.iv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform2iv(loc, uniforms[i].array_size, uniforms[i].data.iv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.iv[2*i] || check_iv[1] != uniforms[i].data.iv[2*i+1]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_IVEC3:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.iv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform3iv(loc, uniforms[i].array_size, uniforms[i].data.iv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.iv[3*i] || check_iv[1] != uniforms[i].data.iv[3*i+1] || check_iv[2] != uniforms[i].data.iv[3*i+2]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_IVEC4:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.iv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform4iv(loc, uniforms[i].array_size, uniforms[i].data.iv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.iv[4*i] || check_iv[1] != uniforms[i].data.iv[4*i+1] || check_iv[2] != uniforms[i].data.iv[4*i+2] || check_iv[2] != uniforms[i].data.iv[4*i+3]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_BOOL:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.bv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform1iv(loc, uniforms[i].array_size, uniforms[i].data.bv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.bv[i]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_BVEC2:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.bv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform2iv(loc, uniforms[i].array_size, uniforms[i].data.bv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.bv[2*i] || check_iv[1] != uniforms[i].data.bv[2*i+1]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_BVEC3:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.bv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform3iv(loc, uniforms[i].array_size, uniforms[i].data.bv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.bv[3*i] || check_iv[1] != uniforms[i].data.bv[3*i+1] || check_iv[2] != uniforms[i].data.bv[3*i+2]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_BVEC4:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.bv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform4iv(loc, uniforms[i].array_size, uniforms[i].data.bv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != uniforms[i].data.bv[4*i] || check_iv[1] != uniforms[i].data.bv[4*i+1] || check_iv[2] != uniforms[i].data.bv[4*i+2] || check_iv[2] != uniforms[i].data.bv[4*i+3]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_FLOAT:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform1fv(loc, uniforms[i].array_size, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[i]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_VEC2:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform2fv(loc, uniforms[i].array_size, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[2*i] || check_fv[1] != uniforms[i].data.fv[2*i+1]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_VEC3:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform3fv(loc, uniforms[i].array_size, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[3*i] || check_fv[1] != uniforms[i].data.fv[3*i+1] || check_fv[2] != uniforms[i].data.fv[3*i+2]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_VEC4:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.Uniform4fv(loc, uniforms[i].array_size, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[4*i] || check_fv[1] != uniforms[i].data.fv[4*i+1] || check_fv[2] != uniforms[i].data.fv[4*i+2] || check_fv[2] != uniforms[i].data.fv[4*i+3]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_MAT2:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.UniformMatrix2fv(loc, uniforms[i].array_size, GL_FALSE, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[4*i] || check_fv[1] != uniforms[i].data.fv[4*i+1] || check_fv[2] != uniforms[i].data.fv[4*i+2] || check_fv[2] != uniforms[i].data.fv[4*i+3]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_MAT3:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.UniformMatrix3fv(loc, uniforms[i].array_size, GL_FALSE, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[4*i] || check_fv[1] != uniforms[i].data.fv[4*i+1] || check_fv[2] != uniforms[i].data.fv[4*i+2] || check_fv[2] != uniforms[i].data.fv[4*i+3]
                    || check_fv[0] != uniforms[i].data.fv[4*i+4] || check_fv[1] != uniforms[i].data.fv[4*i+5] || check_fv[2] != uniforms[i].data.fv[4*i+6]
                    || check_fv[2] != uniforms[i].data.fv[4*i+7] || check_fv[2] != uniforms[i].data.fv[4*i+8]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_MAT4:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.fv == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            g_gl.UniformMatrix4fv(loc, uniforms[i].array_size, GL_FALSE, uniforms[i].data.fv);
#ifndef GFX_NO_CHECKS
            if(g_gl.GetError() != GL_NO_ERROR) {
                return GFX_ERROR_UNKNOWN;
            }
#endif
#ifdef GFX_DEBUG
            for(int i = 0; i < uniforms[i].array_size; i++) {
                g_gl.GetUniformfv(id, loc+i, &check_fv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_fv[0] != uniforms[i].data.fv[4*i] || check_fv[1] != uniforms[i].data.fv[4*i+1] || check_fv[2] != uniforms[i].data.fv[4*i+2] || check_fv[2] != uniforms[i].data.fv[4*i+3]
                    || check_fv[0] != uniforms[i].data.fv[4*i+4] || check_fv[1] != uniforms[i].data.fv[4*i+5] || check_fv[2] != uniforms[i].data.fv[4*i+6]
                    || check_fv[2] != uniforms[i].data.fv[4*i+7] || check_fv[2] != uniforms[i].data.fv[4*i+8] || check_fv[2] != uniforms[i].data.fv[4*i+9] 
                    || check_fv[2] != uniforms[i].data.fv[4*i+10] || check_fv[2] != uniforms[i].data.fv[4*i+11] || check_fv[2] != uniforms[i].data.fv[4*i+12]
                    || check_fv[2] != uniforms[i].data.fv[4*i+13] || check_fv[2] != uniforms[i].data.fv[4*i+14] || check_fv[2] != uniforms[i].data.fv[4*i+15]) {
                    return GFX_ERROR_UNKNOWN;
                }
            }
#endif
            break;
        case GFX_UNIFORM_DATA_TYPE_SAMPLER_2D:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.texture == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            for(int i = 0; i < uniforms[i].array_size; i++) {
#ifndef GFX_NO_CHECKS
                if(used_units_for_textures > (g_gl.limits.texture.maximum_usable_units.combined)-1) {
                    return GFX_ERROR_NOT_ENOUGH_TEXTURE_UNITS;
                }
#endif
                GLenum e = GL_TEXTURE0 + used_units_for_textures;
            
                r = gfx_texture_bind_safe(e, GL_TEXTURE_2D, 0, uniforms[i].data.texture[i].id);
                if(r != GLX_OK) { return r; }
            
                g_gl.Uniform1iv(loc+i, 1, used_units_for_textures);
#ifndef GFX_NO_CHECKS
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
#endif
#ifdef GFX_DEBUG
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != used_units_for_textures) {
                    return GFX_ERROR_UNKNOWN;
                }
#endif
                used_units_for_textures++;
            }
            break;
        case GFX_UNIFORM_DATA_TYPE_SAMPLER_CUBE_MAP:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.cubemap == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            for(int i = 0; i < uniforms[i].array_size; i++) {
#ifndef GFX_NO_CHECKS
                if(used_units_for_cubemaps > (g_gl.limits.texture.maximum_usable_units.combined)-1) {
                    return GFX_ERROR_NOT_ENOUGH_TEXTURE_UNITS;
                }
#endif
                GLenum e = GL_TEXTURE0 + used_units_for_cubemaps;
            
                r = gfx_texture_bind_safe(e, GL_TEXTURE_CUBE_MAP, 0, uniforms[i].data.cubemap[i].id);
                if(r != GLX_OK) { return r; }
            
                g_gl.Uniform1iv(loc+i, 1, used_units_for_cubemaps);
#ifndef GFX_NO_CHECKS
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
#endif
#ifdef GFX_DEBUG
                g_gl.GetUniformiv(id, loc+i, &check_iv);
                if(g_gl.GetError() != GL_NO_ERROR) {
                    return GFX_ERROR_UNKNOWN;
                }
                if(check_iv[0] != used_units_for_cubemaps) {
                    return GFX_ERROR_UNKNOWN;
                }
#endif
                used_units_for_cubemaps++;
            }
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
    }
    
    r = gfx_switch_current_program_id(shader_program.program_id, 0);
#ifdef GFX_DEBUG
    if(r != GFX_OK) { return r; }
#endif

    return GFX_OK;
}
gfx_result_t gfx_cleanup_uniforms(gfx_shader_t shader_program, gfx_uniform_data_info_t* uniforms, size_t nr_uniforms) {
    GLenum e;
    gfx_result_t r;
    int used_units_for_textures = 0, used_units_for_cubemaps = 0;
    
    if(nr_uniforms == 0) {
#ifndef GFX_NO_CHECKS
        if(uniforms != NULL) {
            return GFX_ERROR_INVALID_PARAM;
        }
#endif
        return GFX_OK;
    }
    
#ifndef GFX_NO_CHECKS
    if(uniforms == NULL || nr_uniforms < 0) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif
    
    for(int i = 0; i < nr_uniforms; i++) {
        switch(uniforms[i].type) {
        case GFX_UNIFORM_DATA_TYPE_INT:
        case GFX_UNIFORM_DATA_TYPE_IVEC2:
        case GFX_UNIFORM_DATA_TYPE_IVEC3:
        case GFX_UNIFORM_DATA_TYPE_IVEC4:
        case GFX_UNIFORM_DATA_TYPE_BOOL:
        case GFX_UNIFORM_DATA_TYPE_BVEC2:
        case GFX_UNIFORM_DATA_TYPE_BVEC3:
        case GFX_UNIFORM_DATA_TYPE_BVEC4:
        case GFX_UNIFORM_DATA_TYPE_FLOAT:
        case GFX_UNIFORM_DATA_TYPE_VEC2:
        case GFX_UNIFORM_DATA_TYPE_VEC3:
        case GFX_UNIFORM_DATA_TYPE_VEC4:
        case GFX_UNIFORM_DATA_TYPE_MAT2:
        case GFX_UNIFORM_DATA_TYPE_MAT3:
        case GFX_UNIFORM_DATA_TYPE_MAT4:
        case GFX_UNIFORM_DATA_TYPE_SAMPLER_2D:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.texture == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            for(int i = 0; i < uniforms[i].array_size; i++) {
#ifndef GFX_NO_CHECKS
                if(used_units_for_textures > (g_gl.limits.texture.maximum_usable_units.combined)-1) {
                    return GFX_ERROR_NOT_ENOUGH_TEXTURE_UNITS;
                }
#endif
                GLenum e = GL_TEXTURE0 + used_units_for_textures;
            
                r = gfx_texture_bind_safe(e, GL_TEXTURE_2D, uniforms[i].data.texture[i].id, 0);
                if(r != GLX_OK) { return r; }
            
                used_units_for_textures++;
            }
            break;
        case GFX_UNIFORM_DATA_TYPE_SAMPLER_CUBE_MAP:
#ifndef GFX_NO_CHECKS
            if(uniforms[i].data.cubemap == NULL) {
                return GFX_ERROR_INVALID_PARAM;
            }
#endif
            for(int i = 0; i < uniforms[i].array_size; i++) {
#ifndef GFX_NO_CHECKS
                if(used_units_for_cubemaps > (g_gl.limits.texture.maximum_usable_units.combined)-1) {
                    return GFX_ERROR_NOT_ENOUGH_TEXTURE_UNITS;
                }
#endif
                GLenum e = GL_TEXTURE0 + used_units_for_cubemaps;
            
                r = gfx_texture_bind_safe(e, GL_TEXTURE_CUBE_MAP, uniforms[i].data.cubemap[i].id, 0);
                if(r != GLX_OK) { return r; }
            
                used_units_for_cubemaps++;
            }
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
        }
    }
    
    return GFX_OK;
}


static gfx_result_t gfx_draw_generic_setup(gfx_shader_t shader_program, gfx_draw_shape_t shape, GLenum* mode) {
    gfx_result_t r; GLenum m; GLint i;

#ifndef GFX_NO_CHECKS
    if(mode == NULL) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

#ifdef GFX_DEBUG
    r = gfx_shader_check(shader_program);
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_switch_current_program_id(0, shader_program.program_id);
#ifdef GFX_DEBUG
    if(r != GFX_OK) { return r; }
#endif

    switch(shape) {
    case GFX_DRAW_SHAPE_POINTS:
        m = GL_POINTS;
        break;
    case GFX_DRAW_SHAPE_LINE_STRIP:
        m = GL_LINE_STRIP;
        break;
    case GFX_DRAW_SHAPE_LINE_LOOP:
        m = GL_LINE_LOOP;
        break;
    case GFX_DRAW_SHAPE_LINES:
        m = GL_LINES;
        break;
    case GFX_DRAW_SHAPE_TRIANGLE_STRIP:
        m = GL_TRIANGLE_STRIP;
        break;
    case GFX_DRAW_SHAPE_TRIANGLE_FAN:
        m = GL_TRIANGLE_FAN;
        break;
    case GFX_DRAW_SHAPE_TRIANGLES:
        m = GL_TRIANGLES;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
    mode[0] = m;

    return GFX_OK;
}

gfx_result_t gfx_draw(gfx_shader_t shader_program, gfx_draw_shape_t shape, uint32_t first, uint32_t count) {
    gfx_result_t r; GLenum mode;
    
    r = gfx_draw_generic_setup(shader_program, shape, &mode);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    g_gl.DrawArrays(mode, first, count);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_switch_current_program_id(shader_program.program_id, 0);
#ifdef GFX_DEBUG
    if(r != GFX_OK) { return r; }
#endif
    return GFX_OK;
}
gfx_result_t gfx_draw_indexed(gfx_shader_t shader_program, gfx_draw_shape_t shape, gfx_index_buffer_t* indices, gfx_index_type_t index_type, uint32_t offset, uint32_t count) {
    gfx_result_t r; GLenum mode, index_type_enum;
    
#ifndef GFX_NO_CHECKS
    if(indices == NULL) {
        return GFX_ERROR_INVALID_PARAM;
    }
#endif

    switch(index_type) {
    case GFX_INDEX_TYPE_UINT8:
        index_type_enum = GL_UNSIGNED_BYTE;
        break;
    case GFX_INDEX_TYPE_UINT16:
        index_type_enum = GL_UNSIGNED_SHORT;
        break;
    default:
        return GFX_ERROR_INVALID_PARAM;
    }
    
    r = gfx_draw_generic_setup(shader_program, shape, &mode);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_buffer_bind_safe(GL_ELEMENT_ARRAY_BUFFER, 0, indices[0].id);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    g_gl.DrawElements(mode, count, index_type_enum, offset);
#ifndef GFX_NO_CHECKS
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
#endif

    r = gfx_buffer_bind_safe(GL_ELEMENT_ARRAY_BUFFER, indices[0].id, 0);
#ifndef GFX_NO_CHECKS
    if(r != GFX_OK) { return r; }
#endif

    r = gfx_switch_current_program_id(shader_program.program_id, 0);
#ifdef GFX_DEBUG
    if(r != GFX_OK) { return r; }
#endif

    indices[0].usages++;

    return GFX_OK;
}



/* unused GL functions and variables:

open features: allow explicit override on other mipmap levels of textures?



 
   



glLinkProgram




unnecessary:
glBlendEquation because glBlendEquationSeparate 
glBlendFunc because glBlendFuncSeparate 
glStencilFunc because glStencilFuncSeparate 
glStencilMask because glStencilMaskSeparate 
glStencilOp because glStencilOpSeparate 

glVertexAttrib[1|2|3|4]f[v]     because it'S effectively a uniform


GL_CURRENT_VERTEX_ATTRIB            4f      because it's unclear what the "current value" is for buffer pointer bindings

these strings: can only really be used for a debug printout; glGetString might still be used to parse the shader language internally.
GL_VENDOR                       1s       __CONSTANT__
GL_RENDERER                     1s       __CONSTANT__
GL_VERSION                      1s       __CONSTANT__

MIGHT be useful: but needs to be especially parsed
GL_SHADING_LANGUAGE_VERSION     1s       __CONSTANT__


unusable:
glHint because no hints left
glCompressedTexImage2D , glCompressedTexSubImage2D because it's really unclear which formats are supported
GL_COMPRESSED_TEXTURE_FORMATS   var.e   __CONSTANT__; MAYBE usable, but very unclear
GL_NUM_COMPRESSED_TEXTURE_FORMATS   1i  __CONSTANT__; MAYBE usable, but very unclear

 */
