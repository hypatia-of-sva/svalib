/** 
 * Graphics library abstraction
 * implemented in GL (ES) 2 and GLFW
 */


#ifndef GFX_H

/* TODO: differentiate between [0,1] and [-1,1] clamping with type names */
typedef float  gfx_clamp_f;

typedef enum gfx_result_t {
    GFX_OK = 0,
    GFX_ERROR_WINDOW = -1,
    GFX_ERROR_API_UNAVAILABLE = -2,
    GFX_ERROR_API_INIT_ERROR = -3,
    GFX_ERROR_API_OTHER = -4,
    GFX_ERROR_API_UPDATE_ERROR = -5,
    GFX_ERROR_CURSOR_NOT_AVAILABLE = -6,
    GFX_ERROR_INVALID_PARAM = -7,
    GFX_ERROR_OUT_OF_MEMORY = -8,
    GFX_ERROR_UNKNOWN = -9,
} gfx_result_t;
typedef enum gfx_event_type_t {
    GFX_EVENT_MONITOR_CONNECTED = 0,
    GFX_EVENT_MONITOR_DISCONNECTED = 1,
    GFX_EVENT_JOYSTICK_CONNECTED = 2,
    GFX_EVENT_JOYSTICK_DISCONNECTED = 3,
    GFX_EVENT_JOYSTICK_STATE_UPDATE = 4,
    GFX_EVENT_JOYSTICK_COUNTS_UPDATE = 5,
    GFX_EVENT_GAMEPAD_STATE_UPDATE = 6,
    GFX_EVENT_WINDOW_MOVE = 7,
    GFX_EVENT_WINDOW_RESIZE = 8,
    GFX_EVENT_WINDOW_CLOSE = 9,
    GFX_EVENT_WINDOW_CONTENT_REFRESH = 10,
    GFX_EVENT_WINDOW_FOCUSED = 11,
    GFX_EVENT_WINDOW_UNFOCUSED = 12,
    GFX_EVENT_WINDOW_ICONIFIED = 13,
    GFX_EVENT_WINDOW_RESTORED = 14,
    GFX_EVENT_WINDOW_MAXIMIZED = 15,
    GFX_EVENT_WINDOW_UNMAXIMIZED = 16,
    GFX_EVENT_FRAMEBUFFER_RESIZED = 17,
    GFX_EVENT_WINDOW_CONTENT_SCALE_CHANGED = 18,
    GFX_EVENT_KEY_PRESS = 19,
    GFX_EVENT_KEY_RELEASE = 20,
    GFX_EVENT_KEY_REPEAT = 21,
    GFX_EVENT_UNICODE_CHAR_INPUT = 22,
    GFX_EVENT_MOUSE_CLICK = 23,
    GFX_EVENT_MOUSE_RELEASE = 24,
    GFX_EVENT_MOUSE_MOVE = 25,
    GFX_EVENT_MOUSE_CURSOR_ENTER = 26,
    GFX_EVENT_MOUSE_CURSOR_LEAVE = 27,
    GFX_EVENT_SCROLL = 28,
    GFX_EVENT_PATH_DROP = 29,
    
    GFX_EVENT_MAX_ENUM = 0x7FFFFFFF,
} gfx_event_type_t;
typedef enum gfx_key_t {
    /* Keycode numbers taken verbatim from GLFW 3.4 */
/* Printable keys */
    GFX_KEY_SPACE             = 32,
    GFX_KEY_APOSTROPHE        = 39,  /* ' */
    GFX_KEY_COMMA             = 44,  /* , */
    GFX_KEY_MINUS             = 45,  /* - */
    GFX_KEY_PERIOD            = 46,  /* . */
    GFX_KEY_SLASH             = 47,  /* / */
    GFX_KEY_0                 = 48,
    GFX_KEY_1                 = 49,
    GFX_KEY_2                 = 50,
    GFX_KEY_3                 = 51,
    GFX_KEY_4                 = 52,
    GFX_KEY_5                 = 53,
    GFX_KEY_6                 = 54,
    GFX_KEY_7                 = 55,
    GFX_KEY_8                 = 56,
    GFX_KEY_9                 = 57,
    GFX_KEY_SEMICOLON         = 59,  /* ; */
    GFX_KEY_EQUAL             = 61,  /* = */
    GFX_KEY_A                 = 65,
    GFX_KEY_B                 = 66,
    GFX_KEY_C                 = 67,
    GFX_KEY_D                 = 68,
    GFX_KEY_E                 = 69,
    GFX_KEY_F                 = 70,
    GFX_KEY_G                 = 71,
    GFX_KEY_H                 = 72,
    GFX_KEY_I                 = 73,
    GFX_KEY_J                 = 74,
    GFX_KEY_K                 = 75,
    GFX_KEY_L                 = 76,
    GFX_KEY_M                 = 77,
    GFX_KEY_N                 = 78,
    GFX_KEY_O                 = 79,
    GFX_KEY_P                 = 80,
    GFX_KEY_Q                 = 81,
    GFX_KEY_R                 = 82,
    GFX_KEY_S                 = 83,
    GFX_KEY_T                 = 84,
    GFX_KEY_U                 = 85,
    GFX_KEY_V                 = 86,
    GFX_KEY_W                 = 87,
    GFX_KEY_X                 = 88,
    GFX_KEY_Y                 = 89,
    GFX_KEY_Z                 = 90,
    GFX_KEY_LEFT_BRACKET      = 91,  /* [ */
    GFX_KEY_BACKSLASH         = 92,  /* \ */
    GFX_KEY_RIGHT_BRACKET     = 93,  /* ] */
    GFX_KEY_GRAVE_ACCENT      = 96,  /* ` */
    GFX_KEY_WORLD_1           = 161, /* non-US #1 */
    GFX_KEY_WORLD_2           = 162, /* non-US #2 */
/* Function keys */
    GFX_KEY_ESCAPE            = 256,
    GFX_KEY_ENTER             = 257,
    GFX_KEY_TAB               = 258,
    GFX_KEY_BACKSPACE         = 259,
    GFX_KEY_INSERT            = 260,
    GFX_KEY_DELETE            = 261,
    GFX_KEY_RIGHT             = 262,
    GFX_KEY_LEFT              = 263,
    GFX_KEY_DOWN              = 264,
    GFX_KEY_UP                = 265,
    GFX_KEY_PAGE_UP           = 266,
    GFX_KEY_PAGE_DOWN         = 267,
    GFX_KEY_HOME              = 268,
    GFX_KEY_END               = 269,
    GFX_KEY_CAPS_LOCK         = 280,
    GFX_KEY_SCROLL_LOCK       = 281,
    GFX_KEY_NUM_LOCK          = 282,
    GFX_KEY_PRINT_SCREEN      = 283,
    GFX_KEY_PAUSE             = 284,
    GFX_KEY_F1                = 290,
    GFX_KEY_F2                = 291,
    GFX_KEY_F3                = 292,
    GFX_KEY_F4                = 293,
    GFX_KEY_F5                = 294,
    GFX_KEY_F6                = 295,
    GFX_KEY_F7                = 296,
    GFX_KEY_F8                = 297,
    GFX_KEY_F9                = 298,
    GFX_KEY_F10               = 299,
    GFX_KEY_F11               = 300,
    GFX_KEY_F12               = 301,
    GFX_KEY_F13               = 302,
    GFX_KEY_F14               = 303,
    GFX_KEY_F15               = 304,
    GFX_KEY_F16               = 305,
    GFX_KEY_F17               = 306,
    GFX_KEY_F18               = 307,
    GFX_KEY_F19               = 308,
    GFX_KEY_F20               = 309,
    GFX_KEY_F21               = 310,
    GFX_KEY_F22               = 311,
    GFX_KEY_F23               = 312,
    GFX_KEY_F24               = 313,
    GFX_KEY_F25               = 314,
    GFX_KEY_KP_0              = 320,
    GFX_KEY_KP_1              = 321,
    GFX_KEY_KP_2              = 322,
    GFX_KEY_KP_3              = 323,
    GFX_KEY_KP_4              = 324,
    GFX_KEY_KP_5              = 325,
    GFX_KEY_KP_6              = 326,
    GFX_KEY_KP_7              = 327,
    GFX_KEY_KP_8              = 328,
    GFX_KEY_KP_9              = 329,
    GFX_KEY_KP_DECIMAL        = 330,
    GFX_KEY_KP_DIVIDE         = 331,
    GFX_KEY_KP_MULTIPLY       = 332,
    GFX_KEY_KP_SUBTRACT       = 333,
    GFX_KEY_KP_ADD            = 334,
    GFX_KEY_KP_ENTER          = 335,
    GFX_KEY_KP_EQUAL          = 336,
    GFX_KEY_LEFT_SHIFT        = 340,
    GFX_KEY_LEFT_CONTROL      = 341,
    GFX_KEY_LEFT_ALT          = 342,
    GFX_KEY_LEFT_SUPER        = 343,
    GFX_KEY_RIGHT_SHIFT       = 344,
    GFX_KEY_RIGHT_CONTROL     = 345,
    GFX_KEY_RIGHT_ALT         = 346,
    GFX_KEY_RIGHT_SUPER       = 347,
    GFX_KEY_MENU              = 348,

    GFX_KEY_UNKNOWN           = -1,

    GFX_KEY_MAX_ENUM          = 0x7FFF
} gfx_key_t;
typedef enum gfx_key_mods_flagbits_t {
    GFX_KEY_MODS_SHIFT_BIT          = 0x0001,
    GFX_KEY_MODS_CONTROL_BIT        = 0x0002,
    GFX_KEY_MODS_ALT_BIT            = 0x0004,
    GFX_KEY_MODS_SUPER_BIT          = 0x0008,
    GFX_KEY_MODS_CAPS_LOCK_BIT      = 0x0010,
    GFX_KEY_MODS_NUM_LOCK_BIT       = 0x0020,
    GFX_KEY_MODS_MAX_BIT            = GFX_KEY_MODS_NUM_LOCK_BIT,
} gfx_key_mods_flagbits_t;
typedef enum gfx_mouse_button_t {
    GFX_MOUSE_BUTTON_LEFT = 0,
    GFX_MOUSE_BUTTON_RIGHT = 1,
    GFX_MOUSE_BUTTON_MIDDLE = 2,
    GFX_MOUSE_BUTTON_MAX_ENUM = 0x7f
} gfx_mouse_button_t;
typedef enum gfx_hat_state_t {
    GFX_HAT_STATE_CENTERED           = 0,
    GFX_HAT_STATE_UP                 = 1,
    GFX_HAT_STATE_RIGHT              = 2,
    GFX_HAT_STATE_DOWN               = 4,
    GFX_HAT_STATE_LEFT               = 8,
    GFX_HAT_STATE_RIGHT_UP           = (GLFW_HAT_RIGHT | GLFW_HAT_UP),
    GFX_HAT_STATE_RIGHT_DOWN         = (GLFW_HAT_RIGHT | GLFW_HAT_DOWN),
    GFX_HAT_STATE_LEFT_UP            = (GLFW_HAT_LEFT  | GLFW_HAT_UP),
    GFX_HAT_STATE_LEFT_DOWN          = (GLFW_HAT_LEFT  | GLFW_HAT_DOWN),
    GFX_HAT_STATE_MAX_ENUM           = 0x7F
} gfx_hat_state_t;
typedef enum gfx_standard_cursor_shape_t {
    GFX_STANDARD_CURSOR_SHAPE_ARROW         = 0,
    GFX_STANDARD_CURSOR_SHAPE_IBEAM         = 1,
    GFX_STANDARD_CURSOR_SHAPE_CROSSHAIR     = 2,
    GFX_STANDARD_CURSOR_SHAPE_POINTING_HAND = 3,
    GFX_STANDARD_CURSOR_SHAPE_RESIZE_EW     = 4,
    GFX_STANDARD_CURSOR_SHAPE_RESIZE_NS     = 5,
    GFX_STANDARD_CURSOR_SHAPE_RESIZE_NWSE   = 6,
    GFX_STANDARD_CURSOR_SHAPE_RESIZE_NESW   = 7,
    GFX_STANDARD_CURSOR_SHAPE_RESIZE_ALL    = 8,
    GFX_STANDARD_CURSOR_SHAPE_NOT_ALLOWED   = 9,
    GFX_STANDARD_CURSOR_SHAPE_MAX_ENUM      = 0x7f
} gfx_standard_cursor_shape_t;
typedef enum gfx_cursor_mode_t {
    GFX_CURSOR_MODE_NORMAL   = 0,
    GFX_CURSOR_MODE_HIDDEN   = 1,
    GFX_CURSOR_MODE_DISABLED = 2, /* also sets raw mouse input */
    GFX_CURSOR_MODE_CAPTURED = 3,
    GFX_CURSOR_MODE_MAX_ENUM = 0x7f
} gfx_cursor_mode_t;
typedef enum gfx_blend_equation_type_t {
    GFX_BLEND_EQUATION_TYPE_ADD = 0,
    GFX_BLEND_EQUATION_TYPE_SUBTRACT = 1,
    GFX_BLEND_EQUATION_TYPE_REVERSE_SUBTRACT = 2,
    GFX_BLEND_EQUATION_TYPE_MAX_ENUM = 0x7f
} gfx_blend_equation_type_t;
typedef enum gfx_blend_factor_type_t {
    GFX_BLEND_FACTOR_TYPE_CONSTANT_ZERO               = 0,
    GFX_BLEND_FACTOR_TYPE_CONSTANT_ONE                = 1,
    GFX_BLEND_FACTOR_TYPE_SRC_COLOR                   = 2,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_COLOR         = 3,
    GFX_BLEND_FACTOR_TYPE_DST_COLOR                   = 4,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_COLOR         = 5,
    GFX_BLEND_FACTOR_TYPE_SRC_ALPHA                   = 6,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_SRC_ALPHA         = 7,
    GFX_BLEND_FACTOR_TYPE_DST_ALPHA                   = 8,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_DST_ALPHA         = 9,
    GFX_BLEND_FACTOR_TYPE_CONSTANT_COLOR              = 10,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_COLOR    = 11,
    GFX_BLEND_FACTOR_TYPE_CONSTANT_ALPHA              = 12,
    GFX_BLEND_FACTOR_TYPE_ONE_MINUS_CONSTANT_ALPHA    = 13,
    /* this last one is the only one only usable for the source factors, all others can be used for both source and destination factors. */
    GFX_BLEND_FACTOR_TYPE_SRC_ALPHA_SATURATE          = 14,
    GFX_BLEND_FACTOR_TYPE_MAX_ENUM = 0x7f
} gfx_blend_factor_type_t;
typedef enum gfx_cull_face_type_t {
    GFX_CULL_FACE_TYPE_FRONT = 0,
    GFX_CULL_FACE_TYPE_BACK = 1,
    GFX_CULL_FACE_TYPE_BOTH = 2,
    GFX_CULL_FACE_TYPE_MAX_ENUM = 0x7f
} gfx_cull_face_type_t;
typedef enum gfx_test_comparison_function_type_t {
    GFX_TEST_COMPARISON_FUNCTION_NEVER_ACCEPT                               = 0,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_CURRENT                = 1,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_EQUAL_TO_CURRENT                 = 2,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_LESS_THAN_OR_EQUAL_TO_CURRENT    = 3,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_CURRENT             = 4,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_UNEQUAL_TO_CURRENT               = 5,
    GFX_TEST_COMPARISON_FUNCTION_ACCEPT_IF_GREATER_THAN_OR_EQUAL_TO_CURRENT = 6,
    GFX_TEST_COMPARISON_FUNCTION_ALWAYS_ACCEPT                              = 7,
    GFX_TEST_COMPARISON_FUNCTION_MAX_ENUM                                   = 0x7f
} gfx_test_comparison_function_type_t;
typedef enum gfx_face_orientation_t {
    GFX_FACE_ORIENTATION_CLOCKWISE = 0,
    GFX_FACE_ORIENTATION_COUNTER_CLOCKWISE = 1,
    GFX_FACE_ORIENTATION_MAX_ENUM = 0x7f
} gfx_face_orientation_t;
typedef enum gfx_pixel_storage_alignment_t {
    GFX_PIXEL_STORAGE_BYTE_ALIGNED = 1,
    GFX_PIXEL_STORAGE_DOUBLE_BYTE_ALIGNED = 2,
    GFX_PIXEL_STORAGE_WORD_ALIGNED = 4,
    GFX_PIXEL_STORAGE_DOUBLE_WORD_ALIGNED = 8,
    GFX_PIXEL_STORAGE_ALIGNMENT_MAX_ENUM = 0x7f
} gfx_pixel_storage_alignment_t;
typedef enum gfx_stencil_operation_type_t {
    GFX_STENCIL_OPERATION_KEEP_CURRENT_VALUE                         = 0,
    GFX_STENCIL_OPERATION_SET_TO_ZERO                                = 1,
    GFX_STENCIL_OPERATION_REPLACE_WITH_REFERENCE_VALUE               = 2,
    GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_CLAMP_TO_MAX   = 3,
    GFX_STENCIL_OPERATION_INCREMENT_CURRENT_VALUE_AND_WRAP_TO_0      = 4,
    GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_CLAMP_TO_0     = 5,
    GFX_STENCIL_OPERATION_DECREMENT_CURRENT_VALUE_AND_WRAP_TO_MAX    = 6,
    GFX_STENCIL_OPERATION_BIT_INVERT_CURRENT_VALUE                   = 7,
    GFX_STENCIL_OPERATION_MAX_ENUM                                   = 0x7f
} gfx_stencil_operation_type_t;
typedef enum gfx_buffer_type_t {
    GFX_BUFFER_TYPE_VERTEX_DATA_BUFFER = 0,
    GFX_BUFFER_TYPE_INDEX_BUFFER = 1,
    GFX_BUFFER_TYPE_MAX_ENUM = 0x7f
} gfx_buffer_type_t;
typedef enum gfx_buffer_usage_t {
    GFX_BUFFER_USAGE_MUTABLE = 0,
    GFX_BUFFER_USAGE_CONST = 1,
    GFX_BUFFER_USAGE_ONE_TIME = 2,
    GFX_BUFFER_USAGE_MAX_ENUM = 0x7f
} gfx_buffer_usage_t;

typedef struct gfx_event_t {
    gfx_event_type_t type;
    union {
        int id;
        void* state_ptr;
        struct { int x, y; } ivec2;
        struct { float x, y; } fvec2;
        struct { double x, y; } dvec2;
        struct { gfx_key_t key; int mods; } keypress;
        struct { gfx_mouse_button_t button; int mods; } mouseclick;
        uint32_t unicode_codepoint;
        const char* path;
    } value;
} gfx_event_t;
typedef struct gfx_video_mode_t {
    int width, height, refresh_fps;
} gfx_video_mode_t;
typedef struct gfx_monitor_info_t {
    bool connected;
    const char* name;
    struct {
        int width_in_mm, height_in_mm;
    } physical_size;
    int nr_video_modes;
    int current_video_mode_index;
    gfx_video_mode_t* modes;
} gfx_monitor_info_t;
typedef struct gfx_joystick_info_t {
    bool connected;
    bool is_gamepad;
    const char* name;
    const char* GUID;
    const char* pad_name;
    int button_count;
    int hat_count;
    int axis_count;
} gfx_joystick_info_t;
typedef struct gfx_joystick_state_t {
    int id;
    bool8_t* button_states;
    gfx_hat_state_t* hat_states;
    float* axis_states;
} gfx_joystick_state_t;
typedef struct gfx_gamepad_state_t {
    int id;
    bool8_t A, B, X, Y, LB, RB, back, start, guide, LThumb, RThumb,
        Up, Right, Down, Left;
    float left_x, left_y, right_x, right_y, LT, RT;
} gfx_gamepad_state_t;
typedef struct gfx_image_t {
    int width, height;
    uint32_t* RGBA_LE_data;
} gfx_image_t;
typedef struct gfx_image_alphaless_t {
    int width, height;
    uint32_t* RGB_LE_data;
} gfx_image_alphaless_t;
typedef struct gfx_cursor_shape_t {
    bool is_standard_shape;
    union {
        gfx_standard_cursor_shape_t standard_shape;
        struct {
            xhotspot, yhotspot;
            gfx_image_t image_data;
        } custom_shape_data;
    } data;
} gfx_cursor_shape_t;
typedef struct gfx_window_icon_t {
    int nr_icon_candidates;
    gfx_image_t *per_icon_candidate_data;
} gfx_window_icon_t;
typedef struct gfx_fixed_function_state_t {
    struct {
        bool enabled;
        struct { gfx_clamp_f r, g, b, a; } color;
        gfx_blend_equation_type_t rgb_equation, alpha_equation;
        gfx_blend_factor_type_t src_rgb, src_alpha, dst_rgb, dst_alpha;
    } blend;
    struct {
        bool color_enabled, depth_enabled, stencil_enabled;
        struct { gfx_clamp_f r, g, b, a; } color;
        gfx_clamp_f depth_value;
        int stencil_index;
    } clear;
    struct {
        struct { bool r, g, b, a; } color;
        bool depth;
    } mask;
    struct {
        bool enabled;
        gfx_face_orientation_t front_face_orientation;
        gfx_cull_face_type_t which_face;
    } cull;
    struct {
        bool test_enabled;
        gfx_test_comparison_function_type_t comparison_function;
        struct { gfx_clamp_f near, far; } range;
    } depth;
    struct {
        bool fill_enabled;
        float scale_factor, units;
    } polygon_offset;
    struct {
        bool enabled;
        int x, y, width, height;
    } scissor;
    struct {
        bool test_enabled;
        struct {
            gfx_test_comparison_function_type_t comparison_function;
            int reference_value;
            uint32_t compare_mask, write_mask;
            gfx_stencil_operation_type_t if_stencil_fails, if_stencil_passes_and_depth_fails, if_stencil_passes_and_depth_passes_or_is_not_available;
        } front_face, back_face;
    } stencil;
    struct {
        gfx_pixel_storage_alignment_t pack_alignment, unpack_alignment;
    } pixel_storage;
    struct {
        bool enabled, convert_alpha_to_coverage_value, invert_coverage_mask;
        gfx_clamp_f coverage_value;
    } sample_coverage;
    bool dithering_enabled;
    float line_width;
    struct {
        int x, y, width, height;
    } viewport;
} gfx_fixed_function_state_t;
typedef struct gfx_driver_limits_t {
    struct { float min, max; } line_width;
    struct { float x, y; } viewport_maximum;
    struct {
        struct { int vertex, fragment, combined; } maximum_usable_units;
        struct { int regular_2d, cube_map; } max_image_pixelbuffer_size;
    } texture;
    int max_vertex_attributes;
    int sample_buffers, sample_coverage_mask_size, subpixel_bits;
} gfx_driver_limits_t;
typedef struct gfx_buffer_t {
    uint32_t id;
    gfx_buffer_type_t type;
    gfx_buffer_usage_t usage;
    size_t size;
    int usages; /* to test for GFX_BUFFER_USAGE_ONE_TIME ? */
} gfx_buffer_t;

/* icon is optional and can be left NULL */
gfx_result_t gfx_init(const char* window_name, int width, int height, gfx_window_icon_t* icon);
gfx_result_t gfx_exit(void);

gfx_result_t gfx_joystick_infos(int *nr_joysticks, const gfx_joystick_info_t **infos);
gfx_result_t gfx_monitor_infos(int *nr_monitors, const gfx_monitor_info_t **infos);
gfx_result_t gfx_monitor_switch(int monitor_id, gfx_video_mode_t mode, bool fullscreen, float gamma);
gfx_result_t gfx_cursor_change(gfx_cursor_shape_t shape, gfx_cursor_mode_t mode);

/* events, and linked paths in it, are valid pointers until next call to gfx_events_read or gfx_events_done_processing */
gfx_result_t gfx_events_read(int *nr_events, const gfx_event_t **events);
gfx_result_t gfx_events_done_processing(void);

gfx_result_t gfx_clipboard_get(const char** string);
gfx_result_t gfx_clipboard_set(const char* string);

gfx_result_t gfx_params_get(gfx_fixed_function_state_t* state);
gfx_result_t gfx_params_set(gfx_fixed_function_state_t state);
gfx_result_t gfx_params_reset(void);
gfx_result_t gfx_driver_limits(const gfx_driver_limits_t** limits);

gfx_result_t gfx_render(void);
gfx_result_t gfx_clear(void);

/* data in img is only valid before next call to gfx_screenshot */
gfx_result_t gfx_screenshot(gfx_image_t* img);


gfx_result_t gfx_buffer_create(gfx_buffer_t* buffer, size_t size, void* ptr);
gfx_result_t gfx_buffer_rewrite(gfx_buffer_t buffer, size_t offset, size_t size, void* ptr);
gfx_result_t gfx_buffer_destroy(gfx_buffer_t buffer);

#endif
