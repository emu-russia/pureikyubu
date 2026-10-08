// GFX uniform upload cache (see gfxuniformcache.h for the whole note).

#include "pch.h"

namespace GFX
{
	//! The identity `NextProgramSerial` hands out. It is never reset: the point of the value is that
	//! no uniform storage ever carries the identity another one carried before.
	static uint64_t program_serial = 0;

	uint64_t NextProgramSerial()
	{
		return ++program_serial;
	}

	bool UniformCache::Holds(const GLProgram& program, const char* name, size_t bytes, const void* value) const
	{
		// A switched-off cache holds nothing: every upload of a draw reaches the GL context.
		if (!enabled)
			return false;

		auto it = values.find(name);
		if (it == values.end())
			return false;

		const Entry& entry = it->second;

		// A value of another uniform storage (the program was linked again) or of another size (the
		// upload carries a different number of items) is not the value this upload carries, whatever
		// the bytes of the two say.
		if (entry.serial != program.serial || entry.value.size() != bytes)
			return false;

		return memcmp(entry.value.data(), value, bytes) == 0;
	}

	void UniformCache::Remember(const GLProgram& program, const char* name, size_t bytes, const void* value)
	{
		// Nothing is kept while the cache is off (the values would be the memory of a context the
		// cache was not following); SetEnabled drops what was there before the switch.
		if (!enabled)
			return;

		Entry& entry = values[name];

		entry.serial = program.serial;
		entry.value.assign((const uint8_t*)value, (const uint8_t*)value + bytes);
	}

	void UniformCache::SetEnabled(bool value)
	{
		if (enabled == value)
			return;

		enabled = value;

		Invalidate();
	}

	void UniformCache::Invalidate()
	{
		values.clear();
	}

	// -------------------------------------------------------------------------------------------
	// The uploads
	//
	// Every one of them is the same three steps: the value the cache holds decides whether the GL
	// context is touched at all, a changed value is uploaded and remembered, and an unchanged one is
	// dropped (the program keeps the value it already has, see the note in the header).
	// -------------------------------------------------------------------------------------------

	void UniformCache::Set1f(GLProgram& program, const char* name, GLfloat v)
	{
		if (Holds(program, name, sizeof(v), &v))
			return;

		glUniform1f(program.Uniform(name), v);
		Remember(program, name, sizeof(v), &v);
	}

	void UniformCache::Set1i(GLProgram& program, const char* name, GLint v)
	{
		if (Holds(program, name, sizeof(v), &v))
			return;

		glUniform1i(program.Uniform(name), v);
		Remember(program, name, sizeof(v), &v);
	}

	void UniformCache::Set4f(GLProgram& program, const char* name, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
	{
		GLfloat v[4] = { x, y, z, w };

		if (Holds(program, name, sizeof(v), v))
			return;

		glUniform4f(program.Uniform(name), x, y, z, w);
		Remember(program, name, sizeof(v), v);
	}

	void UniformCache::Set1fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v)
	{
		size_t bytes = (size_t)count * sizeof(GLfloat);

		if (Holds(program, name, bytes, v))
			return;

		glUniform1fv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set2fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v)
	{
		size_t bytes = (size_t)count * sizeof(GLfloat) * 2;

		if (Holds(program, name, bytes, v))
			return;

		glUniform2fv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set4fv(GLProgram& program, const char* name, GLsizei count, const GLfloat* v)
	{
		size_t bytes = (size_t)count * sizeof(GLfloat) * 4;

		if (Holds(program, name, bytes, v))
			return;

		glUniform4fv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set1iv(GLProgram& program, const char* name, GLsizei count, const GLint* v)
	{
		size_t bytes = (size_t)count * sizeof(GLint);

		if (Holds(program, name, bytes, v))
			return;

		glUniform1iv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set1uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v)
	{
		size_t bytes = (size_t)count * sizeof(GLuint);

		if (Holds(program, name, bytes, v))
			return;

		glUniform1uiv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set2uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v)
	{
		size_t bytes = (size_t)count * sizeof(GLuint) * 2;

		if (Holds(program, name, bytes, v))
			return;

		glUniform2uiv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}

	void UniformCache::Set4uiv(GLProgram& program, const char* name, GLsizei count, const GLuint* v)
	{
		size_t bytes = (size_t)count * sizeof(GLuint) * 4;

		if (Holds(program, name, bytes, v))
			return;

		glUniform4uiv(program.Uniform(name), count, v);
		Remember(program, name, bytes, v);
	}
}
