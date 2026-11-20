#pragma once

/// @file types.hpp
/// @brief Minimal self-contained OpenGL type and constant definitions.
///
/// FuhGLCore does not include any Khronos headers: the few GL types and the
/// constants actually used are defined here, so the library builds without
/// OpenGL dev packages installed. Values follow the OpenGL specification.

#include <cstddef>

namespace fuhgl::gl {

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLvoid = void;
using GLbyte = signed char;
using GLshort = short;
using GLint = int;
using GLubyte = unsigned char;
using GLushort = unsigned short;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLclampf = float;
using GLdouble = double;
using GLclampd = double;
using GLchar = char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;
using GLint64 = long long;
using GLuint64 = unsigned long long;
using GLsync = struct __GLsync*;

// --- boolean / error ----------------------------------------------------------
inline constexpr GLboolean GL_FALSE = 0;
inline constexpr GLboolean GL_TRUE = 1;
inline constexpr GLenum GL_NO_ERROR = 0;
inline constexpr GLenum GL_INVALID_ENUM = 0x0500;
inline constexpr GLenum GL_INVALID_VALUE = 0x0501;
inline constexpr GLenum GL_INVALID_OPERATION = 0x0502;
inline constexpr GLenum GL_INVALID_FRAMEBUFFER_OPERATION = 0x0506;
inline constexpr GLenum GL_OUT_OF_MEMORY = 0x0505;

// --- data types ---------------------------------------------------------------
inline constexpr GLenum GL_BYTE = 0x1400;
inline constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum GL_SHORT = 0x1402;
inline constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
inline constexpr GLenum GL_INT = 0x1404;
inline constexpr GLenum GL_UNSIGNED_INT = 0x1405;
inline constexpr GLenum GL_FLOAT = 0x1406;
inline constexpr GLenum GL_HALF_FLOAT = 0x140B;
inline constexpr GLenum GL_DOUBLE = 0x140A;

// --- buffer bits / state ------------------------------------------------------
inline constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;
inline constexpr GLbitfield GL_STENCIL_BUFFER_BIT = 0x00000400;
inline constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;

inline constexpr GLenum GL_POINTS = 0x0000;
inline constexpr GLenum GL_LINES = 0x0001;
inline constexpr GLenum GL_LINE_STRIP = 0x0003;
inline constexpr GLenum GL_TRIANGLES = 0x0004;
inline constexpr GLenum GL_TRIANGLE_STRIP = 0x0005;

inline constexpr GLenum GL_NEVER = 0x0200;
inline constexpr GLenum GL_LESS = 0x0201;
inline constexpr GLenum GL_EQUAL = 0x0202;
inline constexpr GLenum GL_LEQUAL = 0x0203;
inline constexpr GLenum GL_GREATER = 0x0204;
inline constexpr GLenum GL_NOTEQUAL = 0x0205;
inline constexpr GLenum GL_GEQUAL = 0x0206;
inline constexpr GLenum GL_ALWAYS = 0x0207;

inline constexpr GLenum GL_ZERO = 0;
inline constexpr GLenum GL_ONE = 1;
inline constexpr GLenum GL_SRC_ALPHA = 0x0302;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
inline constexpr GLenum GL_DST_ALPHA = 0x0304;
inline constexpr GLenum GL_ONE_MINUS_DST_ALPHA = 0x0305;

inline constexpr GLenum GL_FRONT = 0x0404;
inline constexpr GLenum GL_BACK = 0x0405;
inline constexpr GLenum GL_FRONT_AND_BACK = 0x0408;
inline constexpr GLenum GL_CCW = 0x0901;
inline constexpr GLenum GL_CW = 0x0900;

inline constexpr GLenum GL_DEPTH_TEST = 0x0B71;
inline constexpr GLenum GL_BLEND = 0x0BE2;
inline constexpr GLenum GL_CULL_FACE = 0x0B44;
inline constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
inline constexpr GLenum GL_STENCIL_TEST = 0x0B90;
inline constexpr GLenum GL_DEBUG_OUTPUT = 0x92E0;
inline constexpr GLenum GL_DEBUG_OUTPUT_SYNCHRONOUS = 0x8242;

// --- shaders / programs -------------------------------------------------------
inline constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
inline constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
inline constexpr GLenum GL_GEOMETRY_SHADER = 0x8DD9;
inline constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
inline constexpr GLenum GL_LINK_STATUS = 0x8B82;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;
inline constexpr GLenum GL_CURRENT_PROGRAM = 0x8B8D;

// --- buffers ------------------------------------------------------------------
inline constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum GL_UNIFORM_BUFFER = 0x8A11;
inline constexpr GLenum GL_SHADER_STORAGE_BUFFER = 0x90D2;
inline constexpr GLenum GL_STATIC_DRAW = 0x88E4;
inline constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
inline constexpr GLenum GL_STREAM_DRAW = 0x88E0;
inline constexpr GLenum GL_READ_ONLY = 0x88B8;
inline constexpr GLenum GL_WRITE_ONLY = 0x88B9;
inline constexpr GLenum GL_READ_WRITE = 0x88BA;
inline constexpr GLbitfield GL_MAP_READ_BIT = 0x0001;
inline constexpr GLbitfield GL_MAP_WRITE_BIT = 0x0002;

// --- textures -----------------------------------------------------------------
inline constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
inline constexpr GLenum GL_TEXTURE0 = 0x84C0;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum GL_TEXTURE_WRAP_R = 0x8072;
inline constexpr GLenum GL_NEAREST = 0x2600;
inline constexpr GLenum GL_LINEAR = 0x2601;
inline constexpr GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;
inline constexpr GLenum GL_LINEAR_MIPMAP_NEAREST = 0x2701;
inline constexpr GLenum GL_NEAREST_MIPMAP_LINEAR = 0x2702;
inline constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;
inline constexpr GLenum GL_REPEAT = 0x2901;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum GL_MIRRORED_REPEAT = 0x8370;
inline constexpr GLenum GL_RED = 0x1903;
inline constexpr GLenum GL_RGB = 0x1907;
inline constexpr GLenum GL_RGBA = 0x1908;
inline constexpr GLenum GL_RGB8 = 0x8051;
inline constexpr GLenum GL_RGBA8 = 0x8058;
inline constexpr GLenum GL_DEPTH_COMPONENT = 0x1902;
inline constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6;
inline constexpr GLenum GL_DEPTH24_STENCIL8 = 0x88F0;
inline constexpr GLenum GL_UNSIGNED_INT_24_8 = 0x84FA;
inline constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
inline constexpr GLenum GL_TEXTURE_BINDING_2D = 0x8069;

// --- framebuffer / renderbuffer ------------------------------------------------
inline constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
inline constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
inline constexpr GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
inline constexpr GLenum GL_RENDERBUFFER = 0x8D41;
inline constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
inline constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
inline constexpr GLenum GL_STENCIL_ATTACHMENT = 0x8D20;
inline constexpr GLenum GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;
inline constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
inline constexpr GLenum GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6;
inline constexpr GLenum GL_FRAMEBUFFER_UNDEFINED = 0x8219;

// --- misc / info ---------------------------------------------------------------
inline constexpr GLenum GL_VENDOR = 0x1F00;
inline constexpr GLenum GL_RENDERER = 0x1F01;
inline constexpr GLenum GL_VERSION = 0x1F02;
inline constexpr GLenum GL_SHADING_LANGUAGE_VERSION = 0x8B8C;
inline constexpr GLenum GL_EXTENSIONS = 0x1F03;
inline constexpr GLenum GL_MAX_TEXTURE_SIZE = 0x0D33;
inline constexpr GLenum GL_VIEWPORT = 0x0BA2;
inline constexpr GLenum GL_NUM_EXTENSIONS = 0x821D;

// --- debug (GL 4.3 / KHR_debug) -------------------------------------------------
inline constexpr GLenum GL_DEBUG_SOURCE_API = 0x8246;
inline constexpr GLenum GL_DEBUG_SOURCE_WINDOW_SYSTEM = 0x8247;
inline constexpr GLenum GL_DEBUG_SOURCE_SHADER_COMPILER = 0x8248;
inline constexpr GLenum GL_DEBUG_SOURCE_THIRD_PARTY = 0x8249;
inline constexpr GLenum GL_DEBUG_SOURCE_APPLICATION = 0x824A;
inline constexpr GLenum GL_DEBUG_SOURCE_OTHER = 0x824B;
inline constexpr GLenum GL_DEBUG_TYPE_ERROR = 0x824C;
inline constexpr GLenum GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR = 0x824D;
inline constexpr GLenum GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR = 0x824E;
inline constexpr GLenum GL_DEBUG_TYPE_PORTABILITY = 0x824F;
inline constexpr GLenum GL_DEBUG_TYPE_PERFORMANCE = 0x8250;
inline constexpr GLenum GL_DEBUG_TYPE_OTHER = 0x8251;
inline constexpr GLenum GL_DEBUG_SEVERITY_HIGH = 0x9146;
inline constexpr GLenum GL_DEBUG_SEVERITY_MEDIUM = 0x9147;
inline constexpr GLenum GL_DEBUG_SEVERITY_LOW = 0x9148;
inline constexpr GLenum GL_DEBUG_SEVERITY_NOTIFICATION = 0x826B;

// --- compute (GL 4.3) -----------------------------------------------------------
inline constexpr GLenum GL_COMPUTE_SHADER = 0x91B9;
inline constexpr GLenum GL_DISPATCH_INDIRECT_BUFFER = 0x90EE;
inline constexpr GLenum GL_DRAW_INDIRECT_BUFFER = 0x8F3F;
inline constexpr GLbitfield GL_SHADER_STORAGE_BARRIER_BIT = 0x00002000;
inline constexpr GLbitfield GL_ALL_BARRIER_BITS = 0xFFFFFFFF;

// --- queries / sync --------------------------------------------------------------
inline constexpr GLenum GL_TIME_ELAPSED = 0x88BF;
inline constexpr GLenum GL_TIMESTAMP = 0x8E28;
inline constexpr GLenum GL_QUERY_RESULT = 0x8866;
inline constexpr GLenum GL_QUERY_RESULT_AVAILABLE = 0x8867;
inline constexpr GLenum GL_SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
inline constexpr GLenum GL_ALREADY_SIGNALED = 0x911A;
inline constexpr GLenum GL_TIMEOUT_EXPIRED = 0x911B;
inline constexpr GLenum GL_CONDITION_SATISFIED = 0x911C;
inline constexpr GLenum GL_WAIT_FAILED = 0x911D;
inline constexpr GLbitfield GL_SYNC_FLUSH_COMMANDS_BIT = 0x00000001;
inline constexpr GLuint64 GL_TIMEOUT_IGNORED = 0xFFFFFFFFFFFFFFFFull;

// --- more texture targets / formats ----------------------------------------------
inline constexpr GLenum GL_TEXTURE_3D = 0x806F;
inline constexpr GLenum GL_TEXTURE_2D_ARRAY = 0x8C1A;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP = 0x8513;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP_ARRAY = 0x9009;
inline constexpr GLenum GL_TEXTURE_2D_MULTISAMPLE = 0x9100;
inline constexpr GLenum GL_TEXTURE_2D_MULTISAMPLE_ARRAY = 0x9102;
inline constexpr GLenum GL_PROXY_TEXTURE_CUBE_MAP = 0x851B;
inline constexpr GLenum GL_SAMPLER_2D = 0x8B5E;
inline constexpr GLenum GL_SAMPLER_3D = 0x8B5F;
inline constexpr GLenum GL_SAMPLER_CUBE = 0x8B60;
inline constexpr GLenum GL_SAMPLER_2D_ARRAY = 0x8DC1;
inline constexpr GLenum GL_SAMPLER_2D_MULTISAMPLE = 0x9108;
inline constexpr GLenum GL_SAMPLER_CUBE_MAP_ARRAY = 0x900C;
inline constexpr GLenum GL_RGBA32F = 0x8814;
inline constexpr GLenum GL_RGB32F = 0x8815;
inline constexpr GLenum GL_RGBA16F = 0x881A;
inline constexpr GLenum GL_DEPTH_COMPONENT32F = 0x8CAC;
inline constexpr GLenum GL_DEPTH32F_STENCIL8 = 0x8CAD;
inline constexpr GLenum GL_FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD;
inline constexpr GLenum GL_TEXTURE_BASE_LEVEL = 0x813C;
inline constexpr GLenum GL_TEXTURE_MAX_LEVEL = 0x813D;
inline constexpr GLenum GL_GENERATE_MIPMAP = 0x8191;
inline constexpr GLenum GL_TEXTURE_COMPARE_MODE = 0x884C;
inline constexpr GLenum GL_TEXTURE_COMPARE_FUNC = 0x884D;
inline constexpr GLenum GL_COMPARE_REF_TO_TEXTURE = 0x884E;
inline constexpr GLenum GL_CLAMP_TO_BORDER = 0x812D;
inline constexpr GLenum GL_BORDER_COLOR = 0x1004;
inline constexpr GLenum GL_MAX_SAMPLES = 0x8D57;
inline constexpr GLenum GL_FRAMEBUFFER_SRGB = 0x8DB9;
inline constexpr GLenum GL_COLOR = 0x1800;
inline constexpr GLenum GL_DEPTH = 0x1801;
inline constexpr GLenum GL_COLOR_ATTACHMENT1 = 0x8CE1;
inline constexpr GLenum GL_COLOR_ATTACHMENT2 = 0x8CE2;
inline constexpr GLenum GL_COLOR_ATTACHMENT3 = 0x8CE3;
inline constexpr GLenum GL_RGB16F = 0x881B;

/// @brief Signature of the GL debug message callback (GLDEBUGPROC).
using GLDEBUGPROC = void (*)(GLenum source, GLenum type, GLuint id, GLenum severity,
                             GLsizei length, const GLchar* message, const void* userParam);

} // namespace fuhgl::gl
