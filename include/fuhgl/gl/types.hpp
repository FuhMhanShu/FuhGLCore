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
inline constexpr GLenum GL_NONE = 0;
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
inline constexpr GLenum GL_VALIDATE_STATUS = 0x8B81;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;
inline constexpr GLenum GL_CURRENT_PROGRAM = 0x8B8D;
inline constexpr GLuint GL_INVALID_INDEX = 0xFFFFFFFFu;

// --- buffers ------------------------------------------------------------------
inline constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum GL_UNIFORM_BUFFER = 0x8A11;
inline constexpr GLenum GL_SHADER_STORAGE_BUFFER = 0x90D2;
inline constexpr GLenum GL_COPY_READ_BUFFER = 0x8F36;
inline constexpr GLenum GL_COPY_WRITE_BUFFER = 0x8F37;
inline constexpr GLenum GL_PIXEL_PACK_BUFFER = 0x88EB;
inline constexpr GLenum GL_PIXEL_UNPACK_BUFFER = 0x88EC;
inline constexpr GLenum GL_STATIC_DRAW = 0x88E4;
inline constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
inline constexpr GLenum GL_STREAM_DRAW = 0x88E0;
inline constexpr GLenum GL_READ_ONLY = 0x88B8;
inline constexpr GLenum GL_WRITE_ONLY = 0x88B9;
inline constexpr GLenum GL_READ_WRITE = 0x88BA;
inline constexpr GLbitfield GL_MAP_READ_BIT = 0x0001;
inline constexpr GLbitfield GL_MAP_WRITE_BIT = 0x0002;
inline constexpr GLbitfield GL_MAP_INVALIDATE_RANGE_BIT = 0x0004;
inline constexpr GLbitfield GL_MAP_INVALIDATE_BUFFER_BIT = 0x0008;
inline constexpr GLbitfield GL_MAP_FLUSH_EXPLICIT_BIT = 0x0010;
inline constexpr GLbitfield GL_MAP_UNSYNCHRONIZED_BIT = 0x0020;
inline constexpr GLbitfield GL_MAP_PERSISTENT_BIT = 0x0040;
inline constexpr GLbitfield GL_MAP_COHERENT_BIT = 0x0080;
inline constexpr GLenum GL_BUFFER_SIZE = 0x8764;
inline constexpr GLenum GL_BUFFER_USAGE = 0x8765;
inline constexpr GLenum GL_BUFFER_ACCESS = 0x88BB;
inline constexpr GLenum GL_BUFFER_MAPPED = 0x88BC;
inline constexpr GLenum GL_BUFFER_MAP_POINTER = 0x88BD;

// --- vertex attrib binding (GL 4.3) ----------------------------------------------
inline constexpr GLenum GL_VERTEX_ATTRIB_BINDING = 0x82D4;
inline constexpr GLenum GL_VERTEX_ATTRIB_RELATIVE_OFFSET = 0x82D5;
inline constexpr GLenum GL_VERTEX_BINDING_DIVISOR = 0x82D6;
inline constexpr GLenum GL_VERTEX_BINDING_OFFSET = 0x82D7;
inline constexpr GLenum GL_VERTEX_BINDING_STRIDE = 0x82D8;
inline constexpr GLenum GL_MAX_VERTEX_ATTRIB_BINDINGS = 0x82DA;
inline constexpr GLenum GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET = 0x82D9;
inline constexpr GLenum GL_VERTEX_ATTRIB_ARRAY_INTEGER = 0x88FD;

// --- texture buffer / images / compressed ----------------------------------------
inline constexpr GLenum GL_TEXTURE_BUFFER = 0x8C2A;
inline constexpr GLenum GL_TEXTURE_BINDING_BUFFER = 0x8C2C;
inline constexpr GLenum GL_TEXTURE_BUFFER_DATA_STORE_BINDING = 0x8C2D;
inline constexpr GLenum GL_MAX_TEXTURE_BUFFER_SIZE = 0x8C2B;
inline constexpr GLenum GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT = 0x919F;
inline constexpr GLenum GL_COMPRESSED_RED = 0x8225;
inline constexpr GLenum GL_COMPRESSED_RG = 0x8226;
inline constexpr GLenum GL_COMPRESSED_RGB = 0x84ED;
inline constexpr GLenum GL_COMPRESSED_RGBA = 0x84EE;
inline constexpr GLenum GL_COMPRESSED_SRGB = 0x8C48;
inline constexpr GLenum GL_COMPRESSED_SRGB_ALPHA = 0x8C49;
inline constexpr GLenum GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0;
inline constexpr GLenum GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1;
inline constexpr GLenum GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2;
inline constexpr GLenum GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3;
inline constexpr GLenum GL_COMPRESSED_SRGB_S3TC_DXT1_EXT = 0x8C4C;
inline constexpr GLenum GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 0x8C4D;
inline constexpr GLenum GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 0x8C4E;
inline constexpr GLenum GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 0x8C4F;
inline constexpr GLenum GL_COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
inline constexpr GLenum GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM = 0x8E8D;
inline constexpr GLenum GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT = 0x8E8E;
inline constexpr GLenum GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT = 0x8E8F;
inline constexpr GLenum GL_TEXTURE_WIDTH = 0x1000;
inline constexpr GLenum GL_TEXTURE_HEIGHT = 0x1001;
inline constexpr GLenum GL_TEXTURE_DEPTH = 0x8071;
inline constexpr GLenum GL_TEXTURE_INTERNAL_FORMAT = 0x1003;
inline constexpr GLenum GL_TEXTURE_RED_SIZE = 0x805C;
inline constexpr GLenum GL_TEXTURE_GREEN_SIZE = 0x805D;
inline constexpr GLenum GL_TEXTURE_BLUE_SIZE = 0x805E;
inline constexpr GLenum GL_TEXTURE_ALPHA_SIZE = 0x805F;
inline constexpr GLenum GL_TEXTURE_COMPRESSED = 0x86A1;
inline constexpr GLenum GL_TEXTURE_COMPRESSED_IMAGE_SIZE = 0x86A0;
inline constexpr GLbitfield GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
inline constexpr GLbitfield GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;
inline constexpr GLenum GL_SHADER_STORAGE_BUFFER_BINDING = 0x90D3;
inline constexpr GLenum GL_SHADER_STORAGE_BUFFER_START = 0x90D4;
inline constexpr GLenum GL_SHADER_STORAGE_BUFFER_SIZE = 0x90D5;

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
inline constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;
inline constexpr GLenum GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
inline constexpr GLenum GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
inline constexpr GLenum GL_VERTEX_ARRAY_BINDING = 0x85B5;
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
inline constexpr GLenum GL_MAJOR_VERSION = 0x821B;
inline constexpr GLenum GL_MINOR_VERSION = 0x821C;
inline constexpr GLenum GL_CONTEXT_PROFILE_MASK = 0x9126;
inline constexpr GLenum GL_CONTEXT_CORE_PROFILE_BIT = 0x00000001;

// --- limits / capabilities (queried via glGetIntegerv / glGetInteger64v) --------
inline constexpr GLenum GL_MAX_3D_TEXTURE_SIZE = 0x8073;
inline constexpr GLenum GL_MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C;
inline constexpr GLenum GL_MAX_ARRAY_TEXTURE_LAYERS = 0x88FF;
inline constexpr GLenum GL_MAX_RENDERBUFFER_SIZE = 0x84E8;
inline constexpr GLenum GL_MAX_VIEWPORT_DIMS = 0x0D3A;
inline constexpr GLenum GL_MAX_VERTEX_ATTRIBS = 0x8869;
inline constexpr GLenum GL_MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB;
inline constexpr GLenum GL_MAX_VARYING_VECTORS = 0x8DFC;
inline constexpr GLenum GL_MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD;
inline constexpr GLenum GL_MAX_TEXTURE_IMAGE_UNITS = 0x8872;
inline constexpr GLenum GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C;
inline constexpr GLenum GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D;
inline constexpr GLenum GL_MAX_DRAW_BUFFERS = 0x8824;
inline constexpr GLenum GL_MAX_COLOR_ATTACHMENTS = 0x8CDF;
inline constexpr GLenum GL_MAX_UNIFORM_BUFFER_BINDINGS = 0x8A2F;
inline constexpr GLenum GL_MAX_UNIFORM_BUFFER_SIZE = 0x8A30;
inline constexpr GLenum GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS = 0x90DD;
inline constexpr GLenum GL_MAX_SHADER_STORAGE_BUFFER_SIZE = 0x90D5;
inline constexpr GLenum GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS = 0x90EB;
inline constexpr GLenum GL_MAX_COMPUTE_WORK_GROUP_COUNT = 0x91BE;
inline constexpr GLenum GL_MAX_COMPUTE_WORK_GROUP_SIZE = 0x91BF;
inline constexpr GLenum GL_MAX_ELEMENTS_VERTICES = 0x8D6A;
inline constexpr GLenum GL_MAX_ELEMENTS_INDICES = 0x8D6B;
inline constexpr GLenum GL_MAX_VERTEX_OUTPUT_COMPONENTS = 0x9122;
inline constexpr GLenum GL_MAX_FRAGMENT_INPUT_COMPONENTS = 0x9125;
inline constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

// --- framebuffer extras -------------------------------------------------------------
inline constexpr GLenum GL_FRAMEBUFFER_DEFAULT = 0x8218;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL = 0x8CD2;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE = 0x8CD3;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER = 0x8CD4;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING = 0x8210;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE = 0x8212;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE = 0x8213;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE = 0x8214;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE = 0x8215;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216;
inline constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217;
inline constexpr GLenum GL_STENCIL_INDEX = 0x1901;
inline constexpr GLenum GL_DEPTH_STENCIL = 0x84F9;

// --- blending / logic op / stencil ----------------------------------------------------
inline constexpr GLenum GL_FUNC_ADD = 0x8006;
inline constexpr GLenum GL_FUNC_SUBTRACT = 0x800A;
inline constexpr GLenum GL_FUNC_REVERSE_SUBTRACT = 0x800B;
inline constexpr GLenum GL_MIN = 0x8007;
inline constexpr GLenum GL_MAX = 0x8008;
inline constexpr GLenum GL_BLEND_EQUATION = 0x8009;
inline constexpr GLenum GL_BLEND_EQUATION_RGB = 0x8009;
inline constexpr GLenum GL_BLEND_EQUATION_ALPHA = 0x883D;
inline constexpr GLenum GL_BLEND_DST_RGB = 0x80C8;
inline constexpr GLenum GL_BLEND_SRC_RGB = 0x80C9;
inline constexpr GLenum GL_BLEND_DST_ALPHA = 0x80CA;
inline constexpr GLenum GL_BLEND_SRC_ALPHA = 0x80CB;
inline constexpr GLenum GL_BLEND_COLOR = 0x8005;
inline constexpr GLenum GL_CONSTANT_COLOR = 0x8001;
inline constexpr GLenum GL_ONE_MINUS_CONSTANT_COLOR = 0x8002;
inline constexpr GLenum GL_CONSTANT_ALPHA = 0x8003;
inline constexpr GLenum GL_ONE_MINUS_CONSTANT_ALPHA = 0x8004;
inline constexpr GLenum GL_CLEAR = 0x1500;
inline constexpr GLenum GL_SET = 0x150F;
inline constexpr GLenum GL_COPY = 0x1503;
inline constexpr GLenum GL_NOOP = 0x1505;
inline constexpr GLenum GL_AND = 0x1501;
inline constexpr GLenum GL_NAND = 0x150E;
inline constexpr GLenum GL_OR = 0x1507;
inline constexpr GLenum GL_NOR = 0x1508;
inline constexpr GLenum GL_XOR = 0x1506;
inline constexpr GLenum GL_EQUIV = 0x1509;
inline constexpr GLenum GL_INVERT = 0x150A;
inline constexpr GLenum GL_AND_REVERSE = 0x1502;
inline constexpr GLenum GL_AND_INVERTED = 0x1504;
inline constexpr GLenum GL_OR_REVERSE = 0x150B;
inline constexpr GLenum GL_OR_INVERTED = 0x150D;
inline constexpr GLenum GL_KEEP = 0x1E00;
inline constexpr GLenum GL_REPLACE = 0x1E01;
inline constexpr GLenum GL_INCR = 0x1E02;
inline constexpr GLenum GL_DECR = 0x1E03;
inline constexpr GLenum GL_INCR_WRAP = 0x8507;
inline constexpr GLenum GL_DECR_WRAP = 0x8508;

// --- rasterizer / viewport / scissor ---------------------------------------------------
inline constexpr GLenum GL_POINT = 0x1B00;
inline constexpr GLenum GL_LINE = 0x1B01;
inline constexpr GLenum GL_FILL = 0x1B02;
inline constexpr GLenum GL_POLYGON_MODE = 0x0B40;
inline constexpr GLenum GL_POLYGON_OFFSET_POINT = 0x2A01;
inline constexpr GLenum GL_POLYGON_OFFSET_LINE = 0x2A02;
inline constexpr GLenum GL_POLYGON_OFFSET_FILL = 0x8037;
inline constexpr GLenum GL_POINT_SIZE = 0x0B11;
inline constexpr GLenum GL_LINE_WIDTH = 0x0B21;
inline constexpr GLenum GL_LINE_SMOOTH = 0x0B20;
inline constexpr GLenum GL_POLYGON_SMOOTH = 0x0B41;
inline constexpr GLenum GL_CULL_FACE_MODE = 0x0B45;
inline constexpr GLenum GL_SAMPLE_COVERAGE = 0x80A0;
inline constexpr GLenum GL_SAMPLE_MASK = 0x8E51;
inline constexpr GLenum GL_MIN_SAMPLE_SHADING_VALUE = 0x8C37;
inline constexpr GLenum GL_PROVOKING_VERTEX = 0x8E4F;
inline constexpr GLenum GL_FIRST_VERTEX_CONVENTION = 0x8E4D;
inline constexpr GLenum GL_LAST_VERTEX_CONVENTION = 0x8E4E;
inline constexpr GLenum GL_UNDEFINED_VERTEX = 0x8260;
inline constexpr GLenum GL_PRIMITIVE_RESTART = 0x8F9D;
inline constexpr GLenum GL_PRIMITIVE_RESTART_INDEX = 0x8F9E;
inline constexpr GLenum GL_SCISSOR_BOX = 0x0C10;
inline constexpr GLenum GL_DONT_CARE = 0x1100;
inline constexpr GLenum GL_FASTEST = 0x1101;
inline constexpr GLenum GL_NICEST = 0x1102;
inline constexpr GLenum GL_LINE_SMOOTH_HINT = 0x0C52;
inline constexpr GLenum GL_POLYGON_SMOOTH_HINT = 0x0C53;
inline constexpr GLenum GL_TEXTURE_COMPRESSION_HINT = 0x84EF;
inline constexpr GLenum GL_FRAGMENT_SHADER_DERIVATIVE_HINT = 0x8B8B;

// --- patches / tessellation --------------------------------------------------------------
inline constexpr GLenum GL_PATCHES = 0x000E;
inline constexpr GLenum GL_PATCH_VERTICES = 0x8E72;
inline constexpr GLenum GL_MAX_PATCH_VERTICES = 0x8E7D;

// --- queries / sync -----------------------------------------------------------------------
inline constexpr GLenum GL_QUERY_COUNTER_BITS = 0x8864;
inline constexpr GLenum GL_CURRENT_QUERY = 0x8865;
inline constexpr GLenum GL_SYNC_CONDITION = 0x9113;
inline constexpr GLenum GL_SYNC_STATUS = 0x9114;
inline constexpr GLenum GL_SYNC_FLAGS = 0x9115;
inline constexpr GLenum GL_SIGNALED = 0x9119;
inline constexpr GLenum GL_UNSIGNALED = 0x9118;

// --- object labels / debug ------------------------------------------------------------------
inline constexpr GLenum GL_BUFFER = 0x82E0;
inline constexpr GLenum GL_SHADER = 0x82E1;
inline constexpr GLenum GL_PROGRAM = 0x82E2;
inline constexpr GLenum GL_VERTEX_ARRAY = 0x8074;
inline constexpr GLenum GL_QUERY = 0x82E3;
inline constexpr GLenum GL_PROGRAM_PIPELINE = 0x82E4;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK = 0x8E22;
inline constexpr GLenum GL_SAMPLER = 0x82E6;
inline constexpr GLenum GL_TEXTURE = 0x1702;
inline constexpr GLenum GL_MAX_DEBUG_MESSAGE_LENGTH = 0x9143;
inline constexpr GLenum GL_MAX_DEBUG_LOGGED_MESSAGES = 0x9144;
inline constexpr GLenum GL_MAX_DEBUG_GROUP_STACK_DEPTH = 0x826D;
inline constexpr GLenum GL_DEBUG_GROUP_STACK_DEPTH = 0x826E;
inline constexpr GLenum GL_DEBUG_LOGGED_MESSAGES = 0x9145;
inline constexpr GLenum GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH = 0x8243;

// --- internal formats / images ----------------------------------------------------------------
inline constexpr GLenum GL_SAMPLES = 0x80A9;
inline constexpr GLenum GL_INTERNALFORMAT_SUPPORTED = 0x826F;
inline constexpr GLenum GL_INTERNALFORMAT_PREFERRED = 0x8270;
inline constexpr GLenum GL_INTERNALFORMAT_RED_SIZE = 0x8271;
inline constexpr GLenum GL_INTERNALFORMAT_GREEN_SIZE = 0x8272;
inline constexpr GLenum GL_INTERNALFORMAT_BLUE_SIZE = 0x8273;
inline constexpr GLenum GL_INTERNALFORMAT_ALPHA_SIZE = 0x8274;
inline constexpr GLenum GL_INTERNALFORMAT_DEPTH_SIZE = 0x8275;
inline constexpr GLenum GL_INTERNALFORMAT_STENCIL_SIZE = 0x8276;
inline constexpr GLenum GL_INTERNALFORMAT_SHARED_SIZE = 0x8277;
inline constexpr GLenum GL_INTERNALFORMAT_RED_TYPE = 0x8278;
inline constexpr GLenum GL_INTERNALFORMAT_GREEN_TYPE = 0x8279;
inline constexpr GLenum GL_INTERNALFORMAT_BLUE_TYPE = 0x827A;
inline constexpr GLenum GL_INTERNALFORMAT_ALPHA_TYPE = 0x827B;
inline constexpr GLenum GL_INTERNALFORMAT_DEPTH_TYPE = 0x827C;
inline constexpr GLenum GL_INTERNALFORMAT_STENCIL_TYPE = 0x827D;
inline constexpr GLenum GL_TEXTURE_IMMUTABLE_FORMAT = 0x912F;
inline constexpr GLenum GL_IMAGE_COMPATIBILITY_CLASS = 0x82A8;

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
inline constexpr GLenum GL_STENCIL = 0x1802;
inline constexpr GLenum GL_COLOR_ATTACHMENT1 = 0x8CE1;
inline constexpr GLenum GL_COLOR_ATTACHMENT2 = 0x8CE2;
inline constexpr GLenum GL_COLOR_ATTACHMENT3 = 0x8CE3;
inline constexpr GLenum GL_RGB16F = 0x881B;

// --- DSA (GL 4.5) / buffer storage / queries / formats ---------------------------
inline constexpr GLbitfield GL_DYNAMIC_STORAGE_BIT = 0x0100;
inline constexpr GLbitfield GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT = 0x00000200;
inline constexpr GLenum GL_BUFFER_STORAGE_FLAGS = 0x8220;
inline constexpr GLenum GL_TEXTURE_BUFFER_SIZE = 0x919C;
inline constexpr GLenum GL_TEXTURE_BUFFER_OFFSET = 0x919D;
inline constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY = 0x84FE;
inline constexpr GLenum GL_TEXTURE_BORDER_COLOR = 0x1004;
inline constexpr GLenum GL_R8 = 0x8229;
inline constexpr GLenum GL_RG = 0x8227;
inline constexpr GLenum GL_BGRA = 0x80E1;
inline constexpr GLenum GL_SRGB8 = 0x8C41;
inline constexpr GLenum GL_SRGB8_ALPHA8 = 0x8C43;
inline constexpr GLenum GL_DEPTH_COMPONENT16 = 0x81A5;
inline constexpr GLenum GL_DEPTH_COMPONENT32 = 0x81A7;
inline constexpr GLenum GL_UNSIGNED_INT_24_8_REV = 0x8367;
inline constexpr GLenum GL_RGBA16 = 0x805B;
inline constexpr GLenum GL_TEXTURE_MIN_LOD = 0x813A;
inline constexpr GLenum GL_TEXTURE_MAX_LOD = 0x813B;
inline constexpr GLenum GL_TEXTURE_LOD_BIAS = 0x8501;
inline constexpr GLenum GL_SAMPLES_PASSED = 0x8914;
inline constexpr GLenum GL_ANY_SAMPLES_PASSED = 0x8C2F;
inline constexpr GLenum GL_QUERY_RESULT_NO_WAIT = 0x9194;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BUFFER = 0x8C8E;
inline constexpr GLenum GL_VERTEX_ATTRIB_ARRAY_DIVISOR = 0x88FE;
inline constexpr GLenum GL_RENDERBUFFER_SAMPLES = 0x8CAB;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
inline constexpr GLenum GL_SAMPLER_BINDING = 0x8919;
inline constexpr GLenum GL_R32F = 0x822E;
inline constexpr GLenum GL_R32UI = 0x8236;
inline constexpr GLenum GL_RGBA32UI = 0x8D70;
inline constexpr GLenum GL_RED_INTEGER = 0x8D94;
inline constexpr GLenum GL_RGBA_INTEGER = 0x8D99;

// --- P3: shader stage bits / program pipelines (GL 4.1) ----------------------------
inline constexpr GLbitfield GL_VERTEX_SHADER_BIT = 0x00000001;
inline constexpr GLbitfield GL_FRAGMENT_SHADER_BIT = 0x00000002;
inline constexpr GLbitfield GL_GEOMETRY_SHADER_BIT = 0x00000004;
inline constexpr GLbitfield GL_TESS_CONTROL_SHADER_BIT = 0x00000008;
inline constexpr GLbitfield GL_TESS_EVALUATION_SHADER_BIT = 0x00000010;
inline constexpr GLbitfield GL_COMPUTE_SHADER_BIT = 0x00000020;
inline constexpr GLbitfield GL_ALL_SHADER_BITS = 0xFFFFFFFF;
inline constexpr GLenum GL_PROGRAM_SEPARABLE = 0x8258;
inline constexpr GLenum GL_ACTIVE_PROGRAM = 0x8259;
inline constexpr GLenum GL_PROGRAM_PIPELINE_BINDING = 0x825A;
inline constexpr GLenum GL_MAX_VERTEX_ATTRIB_STRIDE = 0x82E5;

// --- P3: transform feedback (GL 4.0) -------------------------------------------------
inline constexpr GLenum GL_RASTERIZER_DISCARD = 0x8C89;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BINDING = 0x8E25;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BUFFER_BINDING = 0x8C8F;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BUFFER_START = 0x8C84;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BUFFER_SIZE = 0x8C85;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_BUFFER_MODE = 0x8C7F;
inline constexpr GLenum GL_INTERLEAVED_ATTRIBS = 0x8C8C;
inline constexpr GLenum GL_SEPARATE_ATTRIBS = 0x8C8D;
inline constexpr GLenum GL_MAX_TRANSFORM_FEEDBACK_BUFFERS = 0x8E70;
inline constexpr GLenum GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS = 0x8C7A;
inline constexpr GLenum GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS = 0x8C80;
inline constexpr GLenum GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS = 0x8C81;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH = 0x8C76;
inline constexpr GLenum GL_TRANSFORM_FEEDBACK_VARYINGS = 0x8C83;

// --- P3: shader / program binaries (GL 4.1) -------------------------------------------
inline constexpr GLenum GL_NUM_PROGRAM_BINARY_FORMATS = 0x87FE;
inline constexpr GLenum GL_PROGRAM_BINARY_FORMATS = 0x87FF;
inline constexpr GLenum GL_PROGRAM_BINARY_LENGTH = 0x8741;
inline constexpr GLenum GL_PROGRAM_BINARY_RETRIEVABLE_HINT = 0x8257;
inline constexpr GLenum GL_SHADER_BINARY_FORMATS = 0x8DF8;
inline constexpr GLenum GL_NUM_SHADER_BINARY_FORMATS = 0x8DF9;
inline constexpr GLenum GL_SHADER_COMPILER = 0x8DFA;

// --- P3: subroutines (GL 4.0) ----------------------------------------------------------
inline constexpr GLenum GL_ACTIVE_SUBROUTINES = 0x8DE5;
inline constexpr GLenum GL_ACTIVE_SUBROUTINE_UNIFORMS = 0x8DE6;
inline constexpr GLenum GL_MAX_SUBROUTINES = 0x8DE7;
inline constexpr GLenum GL_ACTIVE_SUBROUTINE_MAX_LENGTH = 0x8E48;
inline constexpr GLenum GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH = 0x8E49;
inline constexpr GLenum GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS = 0x8E4A;

// --- P3: GL 4.6 ------------------------------------------------------------------------
inline constexpr GLenum GL_POLYGON_OFFSET_CLAMP = 0x8E1B;
inline constexpr GLenum GL_QUERY_BUFFER_BINDING = 0x9193;
inline constexpr GLbitfield GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT = 0x00000004;

/// @brief Signature of the GL debug message callback (GLDEBUGPROC).
using GLDEBUGPROC = void (*)(GLenum source, GLenum type, GLuint id, GLenum severity,
                             GLsizei length, const GLchar* message, const void* userParam);

} // namespace fuhgl::gl
