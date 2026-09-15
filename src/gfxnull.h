/*

# Null GFX backend (headless build)

The GFX pipeline (gfx.cpp and the XF/SU/RAS/PE/TX/TEV blocks) is written against OpenGL: the XF is
a vertex shader, the TEV is a generated fragment shader and the EFB is the default framebuffer.
A headless build has no window, no GL context and often no driver at all, yet the emulation still
has to run: the CP FIFO, the XF register file, the PE token/finish interrupts and the whole
register state machine do not depend on the picture being drawn anywhere.

This header is the null renderer underneath that pipeline. Instead of an OpenGL context the
pipeline runs against the entry points in `GFX::Null`, so every draw and every state load becomes
a no-op while the emulation itself is unchanged. The result is the same emulated machine with
nothing presented to a display - which is exactly what an unattended run (a benchmark, a scripted
test, a DolphinSDK demo sweep) needs.

It is selected by the `GFX_NULL` macro, defined by the Headless build configurations; the OpenGL
backend is untouched without it (see gfx.cpp). The handful of entry points whose *return value*
the pipeline looks at (shader/program/buffer creation, the compile and link status, the uniform
locations, the viewport the hardware OSD draws into, and `glReadPixels`) are implemented in
gfxnull.cpp; everything else is a no-op. The header has to be included after the OpenGL headers
(it redefines the GLEW macros).

*/

#pragma once

#ifdef GFX_NULL

namespace GFX
{
	namespace Null
	{
		//! The null implementation of every GL entry point whose result the pipeline ignores.
		template <typename... Args> inline void Ignore(Args&&...) {}

		//! The null implementation of the entry points whose result the pipeline uses.
		GLuint CreateShader(GLenum type);
		GLuint CreateProgram();
		void   GenBuffers(GLsizei n, GLuint* ids);
		void   GenVertexArrays(GLsizei n, GLuint* ids);
		void   GenTextures(GLsizei n, GLuint* ids);
		void   GetFloatv(GLenum pname, GLfloat* params);
		void   GetIntegerv(GLenum pname, GLint* params);
		void   GetShaderiv(GLuint shader, GLenum pname, GLint* params);
		void   GetProgramiv(GLuint program, GLenum pname, GLint* params);
		void   GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
		void   GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
		const GLubyte* GetString(GLenum name);
		GLint  GetUniformLocation(GLuint program, const GLchar* name);
		void   ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
	}
}

// The no-ops. The argument list is forwarded so that a call site keeps compiling unchanged.

#undef glActiveTexture
#define glActiveTexture(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glAttachShader
#define glAttachShader(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBindBuffer
#define glBindBuffer(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBindTexture
#define glBindTexture(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBindVertexArray
#define glBindVertexArray(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBlendColor
#define glBlendColor(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBlendEquation
#define glBlendEquation(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBlendFunc
#define glBlendFunc(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBufferData
#define glBufferData(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glBufferSubData
#define glBufferSubData(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glClear
#define glClear(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glClearColor
#define glClearColor(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glClearDepth
#define glClearDepth(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glColorMask
#define glColorMask(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glCompileShader
#define glCompileShader(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glCullFace
#define glCullFace(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDeleteBuffers
#define glDeleteBuffers(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDeleteProgram
#define glDeleteProgram(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDeleteShader
#define glDeleteShader(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDeleteTextures
#define glDeleteTextures(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDeleteVertexArrays
#define glDeleteVertexArrays(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDepthFunc
#define glDepthFunc(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDepthMask
#define glDepthMask(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDepthRange
#define glDepthRange(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDisable
#define glDisable(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDrawArrays
#define glDrawArrays(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDrawBuffer
#define glDrawBuffer(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glDrawElements
#define glDrawElements(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glEnable
#define glEnable(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glEnableVertexAttribArray
#define glEnableVertexAttribArray(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glFinish
#define glFinish(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glFlush
#define glFlush(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glFrontFace
#define glFrontFace(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glGenerateMipmap
#define glGenerateMipmap(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glLineWidth
#define glLineWidth(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glLinkProgram
#define glLinkProgram(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glLogicOp
#define glLogicOp(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glPixelStorei
#define glPixelStorei(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glPointSize
#define glPointSize(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glPolygonMode
#define glPolygonMode(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glScissor
#define glScissor(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glShaderSource
#define glShaderSource(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glTexImage2D
#define glTexImage2D(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glTexParameterf
#define glTexParameterf(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glTexParameteri
#define glTexParameteri(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glTexSubImage2D
#define glTexSubImage2D(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform1f
#define glUniform1f(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform1fv
#define glUniform1fv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform1i
#define glUniform1i(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform1iv
#define glUniform1iv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform1uiv
#define glUniform1uiv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform2fv
#define glUniform2fv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform2uiv
#define glUniform2uiv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform4f
#define glUniform4f(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform4fv
#define glUniform4fv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUniform4uiv
#define glUniform4uiv(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glUseProgram
#define glUseProgram(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glVertexAttribIPointer
#define glVertexAttribIPointer(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glVertexAttribPointer
#define glVertexAttribPointer(...) GFX::Null::Ignore(__VA_ARGS__)
#undef glViewport
#define glViewport(...) GFX::Null::Ignore(__VA_ARGS__)

// The entry points whose result the pipeline uses.

#undef glCreateShader
#define glCreateShader(type) GFX::Null::CreateShader(type)
#undef glCreateProgram
#define glCreateProgram() GFX::Null::CreateProgram()
#undef glGenBuffers
#define glGenBuffers(n, ids) GFX::Null::GenBuffers(n, ids)
#undef glGenVertexArrays
#define glGenVertexArrays(n, ids) GFX::Null::GenVertexArrays(n, ids)
#undef glGenTextures
#define glGenTextures(n, ids) GFX::Null::GenTextures(n, ids)
#undef glGetFloatv
#define glGetFloatv(pname, params) GFX::Null::GetFloatv(pname, params)
#undef glGetIntegerv
#define glGetIntegerv(pname, params) GFX::Null::GetIntegerv(pname, params)
#undef glGetShaderiv
#define glGetShaderiv(shader, pname, params) GFX::Null::GetShaderiv(shader, pname, params)
#undef glGetProgramiv
#define glGetProgramiv(program, pname, params) GFX::Null::GetProgramiv(program, pname, params)
#undef glGetShaderInfoLog
#define glGetShaderInfoLog(shader, bufSize, length, infoLog) GFX::Null::GetShaderInfoLog(shader, bufSize, length, infoLog)
#undef glGetProgramInfoLog
#define glGetProgramInfoLog(program, bufSize, length, infoLog) GFX::Null::GetProgramInfoLog(program, bufSize, length, infoLog)
#undef glGetString
#define glGetString(name) GFX::Null::GetString(name)
#undef glGetUniformLocation
#define glGetUniformLocation(program, name) GFX::Null::GetUniformLocation(program, name)
#undef glReadPixels
#define glReadPixels(x, y, width, height, format, type, pixels) GFX::Null::ReadPixels(x, y, width, height, format, type, pixels)

#endif // GFX_NULL
