/*

The null GFX backend (see gfxnull.h). This file implements the GL entry points whose result the
GFX pipeline actually looks at, so that the pipeline can run its state machine and build its
shaders without an OpenGL context:

  * shader/program/buffer/texture "handles" are small unique integers, so the pipeline's
    `if (handle != 0)` checks behave the same way they do with a real driver;
  * the compile and link status is reported as success and the info logs are empty, so no
    "SHADER COMPILE ERROR" is reported for a shader that was never compiled;
  * every uniform location is 0, which is a valid location (`glUniform*` is a no-op, so the
    value does not matter);
  * `glReadPixels` returns black pixels instead of leaving the caller's buffer untouched - an
    EFB readback (the XFB copy, `gxshot`/`gxpixel`) then returns a defined, all-black picture
    rather than reading uninitialised memory.

*/

#include "pch.h"

#ifdef GFX_NULL

namespace GFX
{
	namespace Null
	{

		// Handles only have to be unique and non-zero. The pipeline uses 0 as "not created yet"
		// (and `glDelete*` clears the handle), which is why the counter starts above zero.
		static GLuint nextHandle = 1;

		GLuint CreateShader(GLenum type)
		{
			return nextHandle++;
		}

		GLuint CreateProgram()
		{
			return nextHandle++;
		}

		static void Generate(GLsizei n, GLuint* ids)
		{
			if (ids == nullptr)
			{
				return;
			}

			for (GLsizei i = 0; i < n; i++)
			{
				ids[i] = nextHandle++;
			}
		}

		void GenBuffers(GLsizei n, GLuint* ids)
		{
			Generate(n, ids);
		}

		void GenVertexArrays(GLsizei n, GLuint* ids)
		{
			Generate(n, ids);
		}

		void GenTextures(GLsizei n, GLuint* ids)
		{
			Generate(n, ids);
		}

		// The pipeline asks for the line-width and point-size limits, so that it can clamp the value
		// programmed through SU_LPSIZE. [1, 1] is the range of the most restrictive driver: the
		// request is accepted and clamped to a single pixel, and it never divides by a range of zero.
		void GetFloatv(GLenum pname, GLfloat* params)
		{
			if (params == nullptr)
			{
				return;
			}

			params[0] = 1.0f;
			params[1] = 1.0f;
		}

		// The viewport query of the hardware OSD (gfxosd.cpp). There is no render target to draw the
		// overlay into, and an empty viewport is exactly what the caller reads as "there is nowhere
		// to draw": it returns before it touches the pipeline.
		void GetIntegerv(GLenum pname, GLint* params)
		{
			if (params == nullptr)
			{
				return;
			}

			if (pname == GL_VIEWPORT)
			{
				params[0] = 0;
				params[1] = 0;
				params[2] = 0;
				params[3] = 0;
			}
			else
			{
				*params = 0;
			}
		}

		void GetShaderiv(GLuint shader, GLenum pname, GLint* params)
		{
			if (params == nullptr)
			{
				return;
			}

			*params = (pname == GL_COMPILE_STATUS) ? GL_TRUE : 0;
		}

		void GetProgramiv(GLuint program, GLenum pname, GLint* params)
		{
			if (params == nullptr)
			{
				return;
			}

			*params = (pname == GL_LINK_STATUS) ? GL_TRUE : 0;
		}

		static void EmptyLog(GLsizei bufSize, GLsizei* length, GLchar* infoLog)
		{
			if (length != nullptr)
			{
				*length = 0;
			}
			if (infoLog != nullptr && bufSize > 0)
			{
				infoLog[0] = 0;
			}
		}

		void GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog)
		{
			EmptyLog(bufSize, length, infoLog);
		}

		void GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog)
		{
			EmptyLog(bufSize, length, infoLog);
		}

		// The three queries the OpenGL backend reports at start-up; they are only ever printed.
		const GLubyte* GetString(GLenum name)
		{
			return (const GLubyte*)"null (headless GFX backend)";
		}

		GLint GetUniformLocation(GLuint program, const GLchar* name)
		{
			return 0;
		}

		void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)
		{
			if (pixels == nullptr || width <= 0 || height <= 0)
			{
				return;
			}

			// The formats the pipeline reads back: 8-bit RGB (the EFB copy and the frame dump) and
			// the single RGBA/float value of `gxpixel`.
			size_t components = 4;
			switch (format)
			{
				case GL_RGB: components = 3; break;
				case GL_RGBA: components = 4; break;
				case GL_DEPTH_COMPONENT: components = 1; break;
				default: components = 4; break;
			}

			size_t componentSize = 1;
			switch (type)
			{
				case GL_UNSIGNED_BYTE: componentSize = 1; break;
				case GL_FLOAT: componentSize = 4; break;
				case GL_UNSIGNED_INT: componentSize = 4; break;
				default: componentSize = 1; break;
			}

			memset(pixels, 0, (size_t)width * (size_t)height * components * componentSize);
		}

	}
}

#endif // GFX_NULL
