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
    glfwGetMonitorPhysicalSize(m, &(info.physical_size.width_in_mm), &(info.physical_size.height_in_mm));
    
    int mode_count;
    const GLFWvidmode* modes = glfwGetVideoModes(m, &mode_count);
    const GLFWvidmode* curr_mode = glfwGetVideoMode(m);
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
        gfx_joystick_info_t* info = &g_glfw.joystick_infos[jid];
        
        info[0].connected    = true;        
        info[0].is_gamepad   = (glfwJoystickIsGamepad(jid) == GLFW_TRUE);
        info[0].name         = strdup(glfwGetJoystickName(jid));
        info[0].GUID         = strdup(glfwGetJoystickGUID(jid));
        info[0].pad_name     = info[0].is_gamepad ? strdup(glfwGetGamepadName(jid)) : NULL;
        
        (void) glfwGetJoystickButtons(jid, &(info[0].button_count));
        (void) glfwGetJoystickHats(jid, &(info[0].hat_count));
        (void) glfwGetJoystickAxes(jid, &(info[0].axis_count));
    }
}
void update_glfw_joystick_state(int jid) {
    if(g_glfw.joystick_infos[jid].connected) {
        int button_count, hat_count, axis_count;
        assert(glfwJoystickPresent(jid) == GLFW_TRUE);
        
        g_glfw.joystick_states[jid].id = jid;
        g_glfw.joystick_states[jid].button_states = glfwGetJoystickButtons(jid, &button_count);
        g_glfw.joystick_states[jid].hat_states = glfwGetJoystickHats(jid, &hat_count);
        g_glfw.joystick_states[jid].axis_states = glfwGetJoystickAxes(jid, &axis_count);
        
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
        assert(glfwJoystickIsGamepad(jid) == GLFW_TRUE);
        
        g_glfw.gamepad_states[jid].id = jid;
        
        /* the rest of the struct is layed-out identically to GLFWgamepadstate, so we can just copy into it: */
        GLFWgamepadstate* ptr = (GLFWgamepadstate*) ((void*) &(g_glfw.gamepad_states[jid].A));
        glfwGetGamepadState(jid, ptr);
        
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
    gfx_image_t                         screenshot;
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
    g_gl.GetFloatv(GL_MAX_VIEWPORT_DIMS, &limits.viewport_maximum);
    g_gl.GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.vertex);
    g_gl.GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.fragment);
    g_gl.GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limits.texture.maximum_usable_units.combined);
    g_gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.texture.max_image_pixelbuffer_size.regular_2d);
    g_gl.GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limits.texture.max_image_pixelbuffer_size.cube_map);
    g_gl.GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limits.max_vertex_attributes);
    g_gl.GetIntegerv(GL_SAMPLE_BUFFERS, &limits.sample_buffers);
    g_gl.GetIntegerv(GL_SAMPLES, &limits.sample_coverage_mask_size);
    g_gl.GetIntegerv(GL_SUBPIXEL_BITS, &limits.subpixel_bits);
    
    if(g_gl.initialized) {
        return g_gl.limits == limits;
    } else {
        g_gl.limits = limits;
        return true;
    }
}
bool check_params(gfx_fixed_function_state_t state) {
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
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
        
    glfwSetMonitorCallback(glfw_monitor);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    
    glfwSetJoystickCallback(glfw_joystick);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    
    GLFWmonitor** glfw_allocated_monitor_array = glfwGetMonitors(&g_glfw.monitor_count);
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
    glfwWindowHint(GLFW_RED_BITS,     mode[0].redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode[0].greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode[0].blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode[0].refreshRate);
    
    /* center the window */
    glfwWindowHint(GLFW_POSITION_X , mode[0].width/2);
    glfwWindowHint(GLFW_POSITION_Y , mode[0].height/2);
    
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    
    
    
    /* first test GL ES 2 */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
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
    
    assert(g_glfw.window != NULL);
    
    g_glfw.paths_cap = g_glfw.num_paths = g_glfw.num_old_paths = 0;
    g_glfw.paths = g_glfw.old_paths = NULL;
    g_glfw.used_cursor = NULL;
    
    /* now to all the callbacks: */
    glfwSetWindowPosCallback(g_glfw.window, glfw_pos);
    glfwSetWindowSizeCallback(g_glfw.window, glfw_size);
    glfwSetWindowCloseCallback(g_glfw.window, glfw_close);
    glfwSetWindowRefreshCallback(g_glfw.window, glfw_refresh);
    glfwSetWindowFocusCallback(g_glfw.window, glfw_focus);
    glfwSetWindowIconifyCallback(g_glfw.window, glfw_iconify);
    glfwSetWindowMaximizeCallback(g_glfw.window, glfw_maximize);
    glfwSetFramebufferSizeCallback(g_glfw.window, glfw_framebuffersize);
    glfwSetWindowContentScaleCallback(g_glfw.window, glfw_contentscale);
    glfwSetKeyCallback(g_glfw.window, glfw_key);
    glfwSetCharCallback(g_glfw.window, glfw_char);
    glfwSetMouseButtonCallback(g_glfw.window, glfw_mousebutton);
    glfwSetCursorPosCallback(g_glfw.window, glfw_cursorpos);
    glfwSetCursorEnterCallback(g_glfw.window, glfw_cursorenter);
    glfwSetScrollCallback(g_glfw.window, glfw_scroll);
    glfwSetDropCallback(g_glfw.window, glfw_drop);
    
    /* Since this will only take effect when the cursor is deactivated, we will turn it on preemtively */
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(_glfw.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    
    glfwSetInputMode(_glfw.window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
    
    glfwMakeContextCurrent(g_glfw.window);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_INIT_ERROR;
    }
    
    load_gl();
    assert(load_or_check_limits());
    
    glfwSwapInterval(1);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_INIT_ERROR;
    }
    
    /* calls glViewport and sets variables like glLineWidth to default values */
    gfx_result_t code = gfx_params_reset();
    if(code != GFX_OK) return code;
    
    if(icon != NULL) {
        glfwSetWindowIcon(g_glfw.window, icon[0].nr_icon_candidates, (const GLFWimage*) icon[0].per_icon_candidate_data);
        /* we explicitly don't check for GLFW errors here since this is not available on all platforms and will fail gracefully. */
    }
    
    return GFX_OK;
}
gfx_result_t gfx_exit(void) {
    g_gl.Finish();
    
    if(g_gl.screenshot.RGBA_LE_data != NULL) {
        free(g_gl.screenshot.RGBA_LE_data);
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
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    
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
    if(gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_API_INIT_ERROR;
    }
    g_gl.state.viewport.x = 0;
    g_gl.state.viewport.y = 0;
    g_gl.state.viewport.width = fb_width;
    g_gl.state.viewport.height = fb_height;
                /* TODO: should this portion be factorized to be called here and in gfx_params_set? but how to handle the error codes? */
    /* (re)set screenshot buffer for gfx_screenshot */
    g_gl.screenshot.width = g_gl.state.viewport.width;
    g_gl.screenshot.height = g_gl.state.viewport.height;
    g_gl.screenshot.RGBA_LE_data = realloc(g_gl.screenshot.RGBA_LE_data, sizeof(uint32_t)*g_gl.screenshot.width*g_gl.screenshot.height);
    
    
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
                assert(false);
        }
        g_glfw.used_cursor = glfwCreateStandardCursor(shape);
    } else {
        g_glfw.used_cursor = glfwCreateCursor((GLFWimage*) &(shape.data.custom_shape_data.image_data),
            shape.data.custom_shape_data.xhotspot, shape.data.custom_shape_data.yhotspot);
    }
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_CURSOR_NOT_AVAILABLE;
    }
    
    glfwSetCursor(g_glfw.window, g_glfw.used_cursor);
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_WINDOW;
    }
    
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
            assert(false);
    }
    glfwSetInputMode(g_glfw.window, GLFW_CURSOR, value);
    if(glfwGetError(NULL) || g_glfw.used_cursor == NULL) {
        return GFX_ERROR_WINDOW;
    }
    
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
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    return GFX_OK;
}
gfx_result_t gfx_clipboard_set(const char* string) {
    glfwSetClipboardString(g_glfw.window, string);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    return GFX_OK;
}

/*
gfx_result_t gfx_time(uint64_t* ticks, uint64_t* frequency_in_hz) {
    assert(ticks != NULL && frequency_in_hz != 0);
    ticks[0] = glfwGetTimerValue();
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    frequency_in_hz[0] = glfwGetTimerFrequency();
    if(glfwGetError(NULL)) {
        return GFX_ERROR_WINDOW;
    }
    return GFX_OK;
}
*/


gfx_result_t gfx_params_get(gfx_fixed_function_state_t* state) {
    assert(state != NULL);
    
    if(!load_or_check_limits() || !check_params(g_gl.state)) {
        return GFX_ERROR_API_OTHER;
    }
    
    state[0] = g_gl.state;
    return GFX_OK;
}
gfx_result_t gfx_params_set(gfx_fixed_function_state_t state) {
    bool set_all = !g_gl.initialized;
    
    if(!set_all) {
        if(!load_or_check_limits() || !check_params(g_gl.state)) {
            return GFX_ERROR_API_OTHER;
        }
    }
    
    if(set_all || state.blend.enabled != g_gl.state.blend.enabled) {
        if(state.blend.enabled == true) {
            g_gl.Enable(GL_BLEND);
        } else if (state.blend.enabled == false) {
            g_gl.Disable(GL_BLEND);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.blend.color != g_gl.state.blend.color) {
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
        g_gl.BlendColor(state.blend.color.r, state.blend.color.g, state.blend.color.b, state.blend.color.a);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.clear.color != g_gl.state.clear.color) {
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
        g_gl.ClearColor(state.clear.color.r, state.clear.color.g, state.clear.color.b, state.clear.color.a);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.clear.depth_value != g_gl.state.clear.depth_value) {
        if(state.clear.depth_value < 0.0f || state.clear.depth_value > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.version == GLES2) {
            g_gl.ClearDepthf(state.clear.depth_value);
        } else {
            g_gl.ClearDepth(state.clear.depth_value);
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.clear.stencil_index != g_gl.state.clear.stencil_index) {
        g_gl.ClearStencil(state.clear.stencil_index);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.cull.enabled != g_gl.state.cull.enabled) {
        if(state.cull.enabled == true) {
            g_gl.Enable(GL_CULL_FACE);
        } else if (state.cull.enabled == false) {
            g_gl.Disable(GL_CULL_FACE);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.depth.test_enabled != g_gl.state.depth.test_enabled) {
        if(state.depth.test_enabled == true) {
            g_gl.Enable(GL_DEPTH_TEST);
        } else if (state.depth.test_enabled == false) {
            g_gl.Disable(GL_DEPTH_TEST);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.depth.range != g_gl.state.depth.range) {
        if(state.depth.range.near < 0.0f || state.depth.range.near > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.depth.range.far < 0.0f || state.depth.range.far > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.version == GLES2) {
            g_gl.DepthRangef(state.depth.range.near, state.depth.range.far);
        } else {
            g_gl.DepthRange(state.clear.depth_value);
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.polygon_offset.fill_enabled != g_gl.state.polygon_offset.fill_enabled) {
        if(state.polygon_offset.fill_enabled == true) {
            g_gl.Enable(GL_POLYGON_OFFSET_FILL);
        } else if (state.polygon_offset.fill_enabled == false) {
            g_gl.Disable(GL_POLYGON_OFFSET_FILL);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.polygon_offset.scale_factor != g_gl.state.polygon_offset.scale_factor
        || state.polygon_offset.units != g_gl.state.polygon_offset.units) {
        g_gl.PolygonOffset(state.polygon_offset.scale_factor, state.polygon_offset.units);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.scissor.enabled != g_gl.state.scissor.enabled) {
        if(state.scissor.enabled == true) {
            g_gl.Enable(GL_SCISSOR_TEST);
        } else if (state.scissor.enabled == false) {
            g_gl.Disable(GL_SCISSOR_TEST);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.scissor.x != g_gl.state.scissor.x
               || state.scissor.y != g_gl.state.scissor.y
               || state.scissor.width != g_gl.state.scissor.width
               || state.scissor.height != g_gl.state.scissor.height) {
        if(state.scissor.x < 0 || state.scissor.y < 0 || state.scissor.width < 0 || state.scissor.height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        
        /* also check viewport size? */
        g_gl.Scissor(state.scissor.x, state.scissor.y, state.scissor.width, state.scissor.height);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.stencil.test_enabled != g_gl.state.stencil.test_enabled) {
        if(state.stencil.test_enabled == true) {
            g_gl.Enable(GL_STENCIL_TEST);
        } else if (state.stencil.test_enabled == false) {
            g_gl.Disable(GL_STENCIL_TEST);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.stencil.front_face.write_mask != g_gl.state.stencil.front_face.write_mask) {
        g_gl.StencilMaskSeparate(GL_FRONT, state.stencil.front_face.write_mask);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.stencil.back_face.write_mask != g_gl.state.stencil.back_face.write_mask) {
        g_gl.StencilMaskSeparate(GL_BACK, state.stencil.back_face.write_mask);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.sample_coverage.enabled != g_gl.state.sample_coverage.enabled) {
        if(state.sample_coverage.enabled == true) {
            g_gl.Enable(GL_SAMPLE_COVERAGE);
        } else if (state.sample_coverage.enabled == false) {
            g_gl.Disable(GL_SAMPLE_COVERAGE);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.sample_coverage.convert_alpha_to_coverage_value != g_gl.state.sample_coverage.convert_alpha_to_coverage_value) {
        if(state.sample_coverage.convert_alpha_to_coverage_value == true) {
            g_gl.Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        } else if (state.sample_coverage.convert_alpha_to_coverage_value == false) {
            g_gl.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
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
        if(state.sample_coverage.coverage_value < 0.0f || state.sample_coverage.coverage_value > 1.0f) {
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.SampleCoverage(state.sample_coverage.coverage_value, invert);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.dithering_enabled != g_gl.state.dithering_enabled) {
        if(state.dithering_enabled == true) {
            g_gl.Enable(GL_DITHER);
        } else if (state.dithering_enabled == false) {
            g_gl.Disable(GL_DITHER);
        } else {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.line_width != g_gl.state.line_width) {
        if(state.line_width <= 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.line_width < g_gl.limits.line_width.min || state.line_width > g_gl.limits.line_width.max) {
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.LineWidth(state.line_width);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return GFX_ERROR_UNKNOWN;
        }
    }
    if(set_all || state.viewport != g_gl.state.viewport) {
        if(state.viewport.x < 0 || state.viewport.y < 0 || state.viewport.width < 0 || state.viewport.height < 0) {
            return GFX_ERROR_INVALID_PARAM;
        }
        if(state.viewport.x + state.viewport.width > limits.viewport_maximum.x || state.viewport.y + state.viewport.height > limits.viewport_maximum.y) {
            return GFX_ERROR_INVALID_PARAM;
        }
        g_gl.Viewport(state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height);
        if(g_gl.GetError() != GL_NO_ERROR) {
            return set_all ? GFX_ERROR_API_INIT_ERROR : GFX_ERROR_API_UPDATE_ERROR;
        }
        /* (re)set screenshot buffer for gfx_screenshot */
        g_gl.screenshot.width = g_gl.state.viewport.width;
        g_gl.screenshot.height = g_gl.state.viewport.height;
        g_gl.screenshot.RGBA_LE_data = realloc(g_gl.screenshot.RGBA_LE_data, sizeof(uint32_t)*g_gl.screenshot.width*g_gl.screenshot.height);
    }

    /* check if we actually could update the state */
    if(!load_or_check_limits() || !check_params(state)) {
        return GFX_ERROR_API_OTHER;
    }
    
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
    g_gl.Flush();
    
    glfwSwapBuffers(g_glfw.window);
    if(glfwGetError(NULL)) {
        return GFX_ERROR_API_OTHER;
    }
    
    return GFX_OK;
}

gfx_result_t gfx_clear(void) {
    GLbitfield mask = 0;
    if(g_gl.state.clear.color_enabled) {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if(g_gl.state.clear.depth_enabled) {
        mask |= stencil_enabled;
    }
    if(g_gl.state.clear.stencil_enabled) {
        mask |= stencil_enabled;
    }
    g_gl.Clear(mask);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    return GFX_OK;
}


gfx_result_t gfx_screenshot(gfx_image_t* img) {
    assert(img != NULL);+
    /* we are limited to RGBA32 bc GL ES 2 only has this and non-standard formats
       we only do full screenshots since users need to copy out the data anyway,
       and this way we can maintain an internal buffer with minimal reallocing
    */
    g_gl.ReadPixels(g_gl.state.viewport.x, g_gl.state.viewport.y, g_gl.screenshot.width, g_gl.screenshot.width, GL_RGBA, GL_UNSIGNED_BYTE, (void*) g_gl.screenshot.RGBA_LE_data);
    img[0] = g_gl.screenshot;
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    return GFX_OK;
}

gfx_result_t gfx_driver_limits(const gfx_driver_limits_t** limits) {
    assert(limits != NULL);
    limits[0] = &(g_gl.limits);
    
    if(!load_or_check_limits()) {
        return GFX_ERROR_API_OTHER;
    }
    
    return GFX_OK;
}




gfx_result_t gfx_buffer_create(gfx_buffer_t* buffer, size_t size, void* ptr) {
    assert(buffer != NULL && ptr != NULL && size > 0);
    
    GLenum target, targetinfo, usage_pattern;
    GLuint id;
    switch(buffer[0].type) {
        case GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER:
            target = GL_ARRAY_BUFFER;
            targetinfo = GL_ARRAY_BUFFER_BINDING;
            break;
        case GFX_BUFFER_TYPE_INDEX_BUFFER:
            target = GL_ELEMENT_ARRAY_BUFFER;
            targetinfo = GL_ELEMENT_ARRAY_BUFFER_BINDING;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    switch(buffer[0].usage) {
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
    
    g_gl.GetIntegerv(targetinfo, &id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(id != 0) {
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.GenBuffers(1, &id);
    if(g_gl.GetError() != GL_NO_ERROR || id == 0) {
        return GFX_ERROR_UNKNOWN;
    }
    
    g_gl.BindBuffer(target, id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    g_gl.BufferData(target, size, ptr, usage_pattern);
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
    
    g_gl.BindBuffer(target, 0);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    buffer[0].id = id;
    buffer[0].size = size;
    
    return GFX_OK;
}
gfx_result_t gfx_buffer_rewrite(gfx_buffer_t buffer, size_t offset, size_t size, void* ptr) {
    assert(offset > 0 && size > 0 && ptr != NULL);
    
    GLenum target, targetinfo, usage_pattern;
    GLuint id;
    switch(buffer[0].type) {
        case GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER:
            target = GL_ARRAY_BUFFER;
            targetinfo = GL_ARRAY_BUFFER_BINDING;
            break;
        case GFX_BUFFER_TYPE_INDEX_BUFFER:
            target = GL_ELEMENT_ARRAY_BUFFER;
            targetinfo = GL_ELEMENT_ARRAY_BUFFER_BINDING;
            break;
        default:
            return GFX_ERROR_INVALID_PARAM;
    }
    
    if(offset+size > buffer.size
            || buffer.usage != GFX_BUFFER_USAGE_MUTABLE
            || g_gl.IsBuffer(buffer.id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    g_gl.GetIntegerv(targetinfo, &id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    if(id != 0) {
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.BindBuffer(target, id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    int i;
    g_gl.GetBufferParameteriv(target, GL_BUFFER_SIZE, &i);
    if(i != buffer.size) {
        g_gl.BindBuffer(target, 0);
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.GetBufferParameteriv(target, GL_BUFFER_USAGE, &i);
    if(i != GL_DYNAMIC_DRAW) {
        g_gl.BindBuffer(target, 0);
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.BufferSubData(target, offset, size, ptr);
    if(i != GL_DYNAMIC_DRAW) {
        g_gl.BindBuffer(target, 0);
        return GFX_ERROR_API_OTHER;
    }
    
    g_gl.BindBuffer(target, 0);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    return GFX_OK;
}
gfx_result_t gfx_buffer_destroy(gfx_buffer_t buffer) {
    if(g_gl.IsBuffer(buffer.id) != GL_TRUE) {
        return GFX_ERROR_INVALID_PARAM;
    }
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    g_gl.DeleteBuffers(1, &buffer.id);
    if(g_gl.GetError() != GL_NO_ERROR) {
        return GFX_ERROR_UNKNOWN;
    }
    
    return GFX_OK;
}






typedef enum gfx_texture_type_t {
    GFX_TEXTURE_TYPE_2D = 0,
    GFX_TEXTURE_TYPE_CUBE_MAP = 1,
    GFX_TEXTURE_TYPE_MAX_ENUM = 0x7f
}
typedef enum gfx_texture_zoom_mode_t {
    GFX_TEXTURE_ZOOM_MODE_NEAREST_ELEMENT = 0,
    GFX_TEXTURE_ZOOM_MODE_LINEAR_AVERAGE_OF_FOUR = 1,
    /* mip-map modes are only used for zooming out/minifying, not zooming in/magnifying  */
    GFX_TEXTURE_ZOOM_MODE_MATCHING_MIPMAP_NEAREST_ELEMENT = 2,
    GFX_TEXTURE_ZOOM_MODE_MATCHING_MIPMAP_LINEAR_AVERAGE_OF_FOUR = 3,
    GFX_TEXTURE_ZOOM_MODE_TWO_MIPMAP_AVERAGE_NEAREST_ELEMENT = 4,
    GFX_TEXTURE_ZOOM_MODE_TWO_MIPMAP_AVERAGE_LINEAR_AVERAGE_OF_FOUR = 5,
    GFX_TEXTURE_ZOOM_MODE_MAX_ENUM = 0x7f
} gfx_texture_zoom_mode_t;
typedef enum gfx_texture_wrapping_mode_t {
    GFX_TEXTURE_WRAPPING_MODE_CLAMP_TO_EDGE = 0,
    GFX_TEXTURE_WRAPPING_MODE_REPEAT = 1,
    GFX_TEXTURE_WRAPPING_MODE_MIRRORED_REPEAT = 2,
    GFX_TEXTURE_WRAPPING_MODE_MAX_ENUM = 0x7f
} gfx_texture_wrapping_mode_t;
typedef enum gfx_texture_image_data_format_t {
    GFX_TEXTURE_IMAGE_DATA_FORMAT_RGBA = 0,
    GFX_TEXTURE_IMAGE_DATA_FORMAT_RGB = 1,
    GFX_TEXTURE_IMAGE_DATA_FORMAT_MAX_ENUM = 0x7f
} gfx_texture_image_data_format_t;

typedef struct gfx_texture_image_data_t {
    gfx_texture_image_data_format_t format;
    union {
        gfx_image_t rgba_data;
        gfx_image_alphaless_t rgb_data;
    } data;
} gfx_texture_image_data_t;
typedef struct gfx_texture_initialization_data_t {
    gfx_texture_type_t type;
    struct {
        gfx_texture_image_data_t data;
    } surface_2d;
    struct {
        gfx_texture_image_data_t xp, yp, zp, xn, yn, zn;
    } faces_cube_map;
} gfx_texture_initialization_data_t


typedef struct gfx_texture_t {
    
    gfx_texture_zoom_mode_t magnifying_mode, minifying_mode;
    gfx_texture_wrapping_mode_t horizontal_wrap, vertical_wrap;
} gfx_texture_t;



/* unused GL functions and variables:





texture portion:

glActiveTexture , glBindTexture ,  , glCopyTexImage2D , glCopyTexSubImage2D , glDeleteTextures , glTexImage2D , , glTexSubImage2D 

glTexParameterf, glTexParameteri, glTexParameterfv, glTexParameteriv
    sets:
    GL_TEXTURE_MIN_FILTER to GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR or GL_LINEAR_MIPMAP_LINEAR
    GL_TEXTURE_MAG_FILTER to GL_NEAREST, GL_LINEAR
    GL_TEXTURE_WRAP_S (i.e. x) to GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, or GL_REPEAT
    GL_TEXTURE_WRAP_T (i.e. y) to GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT, or GL_REPEAT
    

glGenTextures 
glGetTexParameter , glIsTexture



GL_ACTIVE_TEXTURE               1e      for checking if state change worked
GL_TEXTURE_BINDING_2D           1i      for checking if state change worked
GL_TEXTURE_BINDING_CUBE_MAP     1i      for checking if state change worked



shader portion:

glAttachShader , glCompileShader , glCreateProgram , glCreateShader , glDeleteProgram , glDeleteShader , glDetachShader, glLinkProgram, glShaderSource , glUniform[1|2|3|4][f|i][v], glUniformMatrix[2|3|4]fv , glUseProgram , glValidateProgram
glGetActiveAttrib , glGetActiveUniform , glGetAttachedShaders , glGetAttribLocation , glGetProgramInfoLog , glGetProgramiv , glGetShaderInfoLog , glGetShaderSource , glGetShaderiv , glGetUniformfv /  glGetUniformiv , glGetUniformLocation, glIsProgram, glIsShader
GL_CURRENT_PROGRAM              1n      for checking if state change worked
GL_SHADING_LANGUAGE_VERSION     1s       __CONSTANT__

glBindAttribLocation , glEnableVertexAttribArray , glDisableVertexAttribArray , glVertexAttrib[1|2|3|4]f[v], glVertexAttribPointer
glGetVertexAttribfv, glGetVertexAttribiv , glGetVertexAttribPointerv
        should glVertexAttrib and glGetVertexAttrib be generally be not used bc of WebGL only allowing the Pointers / this being more compatible with the vertex array anyway?



actual draw calls:
    

glDrawArrays , glDrawElements






unnecessary:
glBlendEquation because glBlendEquationSeparate 
glBlendFunc because glBlendFuncSeparate 
glStencilFunc because glStencilFuncSeparate 
glStencilMask because glStencilMaskSeparate 
glStencilOp because glStencilOpSeparate 


these strings: can only really be used for a debug printout; glGetString might still be used to parse the shader language internally.
GL_VENDOR                       1s       __CONSTANT__
GL_RENDERER                     1s       __CONSTANT__
GL_VERSION                      1s       __CONSTANT__



unusable:
glHint because no hints left
glCompressedTexImage2D , glCompressedTexSubImage2D because it's really unclear which formats are supported
GL_COMPRESSED_TEXTURE_FORMATS   var.e   __CONSTANT__; MAYBE usable, but very unclear
GL_NUM_COMPRESSED_TEXTURE_FORMATS   1i  __CONSTANT__; MAYBE usable, but very unclear

 */
