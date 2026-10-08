// GFX uniform upload cache
//
// The shader pipeline is driven by two static programs (the XF vertex shader and the TEV fragment
// shader, see gfx.h), so the whole register state of a block travels to the shaders as uniforms and
// is uploaded before every draw (Rasterizer::SetUpPipeline -> TransformUnit::UploadUniforms and
// TextureEnvironmentUnit::UploadUniforms). A title that draws a hundred primitives with the same
// material repeats those ~60 glUniform* calls a hundred times, and every one of them carries the
// value the uniform of the program already holds.
//
// This module keeps the value every uniform was last uploaded with and drops the upload when the
// new value is bit-for-bit the same. The values are compared as bytes, which is the granularity
// glUniform itself works at: the call copies the raw value into the uniform storage of the program,
// so bytes that are equal leave the storage exactly as it is.
//
// What a value is keyed on, and why it is not just the name:
//
//   * the *name* of the uniform, which is how the blocks address it (the GL location of a linked
//     program never changes, so the name identifies the uniform of that program);
//   * the *identity of the uniform storage* it was uploaded to, `GLProgram::serial`. Neither the
//     GLProgram object nor its GL name can identify a program: GL reuses the names of the programs
//     that were deleted, and a program that was linked again has no uniform set at all (the values
//     of a fresh program are the defaults). A value that belonged to the program an object carried
//     before must therefore not be mistaken for a value of the program it carries now - the TEV
//     program is linked again whenever `GEN_MODE.flat_en` flips (TextureEnvironmentUnit::
//     GetTevProgram), which changes nothing the register state of a draw would show.
//
// When the cache has to be dropped:
//
//   * the pipeline switch (`GFXCore::SetPipeline`): the software pipeline never uploads anything,
//     and the values the shader pipeline left behind describe the context the next draw may not
//     still hold (the GL backend is closed and opened again with the switch);
//   * the GL context going away (`GFXCore::GL_CloseSubsystem`): the values are the host's copy of
//     what the context holds, and the programs they belong to are destroyed with it;
//   * `Invalidate` itself, which is what both of the above call.
//
// The cache can be switched off with the `GFX_UNIFORM_CACHE` setting (the settings window, the
// `gxuniformcache` command): the uploads of a draw then reach the GL context like they did before
// the cache existed, which is what a picture that a stale uniform is suspected of is compared
// against. Switching it also drops the values (see SetEnabled).

#pragma once

namespace GFX
{
	class GLProgram;

	//! Hand out the identity of the uniform storage of a GL program object (see the note above).
	//! The values are never reused, so a program that was linked again is a program the cache has
	//! never seen, whatever object or GL name it has.
	uint64_t NextProgramSerial();

	//! The value every uniform of the shader program was last uploaded with. The blocks of the
	//! pipeline upload through this cache instead of calling glUniform* (see gfx.h).
	class UniformCache
	{
		//! One uniform: the storage it was uploaded to and the bytes of the upload.
		struct Entry
		{
			//! The uniform storage the value belongs to (GLProgram::serial).
			uint64_t serial = 0;

			//! The bytes the glUniform* call carried.
			std::vector<uint8_t> value;
		};

		//! The values, by the name the call sites address the uniform with.
		std::unordered_map<std::string, Entry> values;

		//! Whether an upload whose value has not changed is dropped (the GFX_UNIFORM_CACHE setting).
		bool enabled = true;

		//! True when the uniform already holds exactly these bytes in this program, i.e. when the
		//! upload can be dropped. A value of another program (or of another size) is not a match,
		//! and a switched-off cache never answers that it holds a value.
		bool Holds(const GLProgram& program, const char* name, size_t bytes, const void* value) const;

		//! Remember the value of an upload that has just been made.
		void Remember(const GLProgram& program, const char* name, size_t bytes, const void* value);

	public:

		//! Switch the cache. Switching it drops the values it holds: the context was free to move on
		//! while the cache was off (every upload went through), so what it remembers describes a
		//! context that may be long gone, and the first upload of every uniform has to go through
		//! after the switch - in both directions.
		void SetEnabled(bool value);

		//! Whether an upload whose value has not changed is dropped.
		bool Enabled() const { return enabled; }

		//! Drop every value: the next upload of every uniform goes to the GL context again.
		void Invalidate();

		// The uploads of the pipeline, one entry point per glUniform* form the blocks use. Each one
		// resolves the location of a changed value through `GLProgram::Uniform` (which caches the
		// locations) and leaves the GL context alone when the value has not changed.
		void Set1f(GLProgram& program, const char* name, GLfloat v);
		void Set1i(GLProgram& program, const char* name, GLint v);
		void Set4f(GLProgram& program, const char* name, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
		void Set1fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v);
		void Set2fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v);
		void Set4fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v);
		void Set1iv(GLProgram& program, const char* name, GLsizei count, const GLint* v);
		void Set1uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v);
		void Set2uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v);
		void Set4uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v);
	};
}
