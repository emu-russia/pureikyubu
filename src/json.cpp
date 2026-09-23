// The Json engine depends on the C++ standard library alone (plus the dependency-free verify.h):
// the portable machines (src/gba) compile it next to their own sources, without the emulator's
// precompiled header, SDL, OpenGL or ImGui.

#include "json.h"
#include "verify.h"

#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace
{
	// The two directions of the UTF-8 <-> wide conversion. The project's narrow string is UTF-8
	// (see the note in utils.h); these are the same rules, kept here so that this file does not
	// have to link the emulator's platform layer.

	const uint32_t Utf8Replacement = 0xFFFD;

	std::wstring DecodeUtf8(const char* str)
	{
		std::wstring wstr;

		if (str == nullptr)
		{
			return wstr;
		}

		const uint8_t* ptr = (const uint8_t*)str;

		while (*ptr != 0)
		{
			uint32_t cp = *ptr;
			int length = 0;

			if (cp < 0x80)
			{
				length = 1;
			}
			else if ((cp & 0xE0) == 0xC0)
			{
				cp &= 0x1F; length = 2;
			}
			else if ((cp & 0xF0) == 0xE0)
			{
				cp &= 0x0F; length = 3;
			}
			else if ((cp & 0xF8) == 0xF0)
			{
				cp &= 0x07; length = 4;
			}

			// A byte that cannot start a sequence stands for itself, so nothing is lost on the
			// way and a name from an unknown source still reaches the file system in one piece.
			if (length == 0)
			{
				wstr.push_back((wchar_t)*ptr++);
				continue;
			}

			int got = 1;
			for (; got < length && (ptr[got] & 0xC0) == 0x80; got++)
			{
				cp = (cp << 6) | (ptr[got] & 0x3F);
			}

			if (got != length)
			{
				// A sequence that is cut short is not text; the lead byte stands for itself.
				wstr.push_back((wchar_t)*ptr++);
				continue;
			}

			ptr += length;

			// The shortest form and the surrogate block are not code points. A one-byte sequence
			// is already at its shortest form - the value it carries is the code point - so the
			// table must not send it to the replacement character: that turned every ASCII string
			// added with AddUtf8String into a run of U+FFFD.
			uint32_t shortest = (length == 1) ? 0x0 : (length == 2) ? 0x80 : (length == 3) ? 0x800 : 0x10000;
			if (cp < shortest || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			{
				cp = Utf8Replacement;
			}

#if defined(_WINDOWS)
			// wchar_t is one UTF-16 code unit wide here, so a code point outside the BMP takes two.
			if (cp > 0xFFFF)
			{
				cp -= 0x10000;
				wstr.push_back((wchar_t)(0xD800 + (cp >> 10)));
				wstr.push_back((wchar_t)(0xDC00 + (cp & 0x3FF)));
				continue;
			}
#endif

			wstr.push_back((wchar_t)cp);
		}

		return wstr;
	}

	void EncodeUtf8(std::string& str, uint32_t cp)
	{
		if (cp < 0x80)
		{
			str.push_back((char)cp);
		}
		else if (cp < 0x800)
		{
			str.push_back((char)(0xC0 | (cp >> 6)));
			str.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000)
		{
			str.push_back((char)(0xE0 | (cp >> 12)));
			str.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			str.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else
		{
			str.push_back((char)(0xF0 | (cp >> 18)));
			str.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
			str.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			str.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
}

std::string Json::WideToUtf8(const wchar_t* str)
{
	std::string result;

	if (str == nullptr)
	{
		return result;
	}

	for (size_t i = 0; str[i] != 0; i++)
	{
		uint32_t cp = (uint32_t)str[i];

#if defined(_WINDOWS)

		// A code point outside the BMP is a pair of 16-bit code units here, and the two have to be
		// put together again before they can become four UTF-8 bytes.
		if (cp >= 0xD800 && cp <= 0xDBFF && str[i + 1] >= 0xDC00 && str[i + 1] <= 0xDFFF)
		{
			cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint32_t)str[i + 1] - 0xDC00);
			i++;
		}

#endif

		// A code unit that is half of a pair on its own is not text; it becomes the replacement
		// character rather than bytes no decoder would accept.
		if (cp >= 0xD800 && cp <= 0xDFFF)
		{
			cp = Utf8Replacement;
		}

		EncodeUtf8(result, cp);
	}

	return result;
}

std::wstring Json::Utf8ToWide(const char* str)
{
	return DecodeUtf8(str);
}

void Json::DestroyValue(Value* value)
{
	while (!value->children.empty())
	{
		Value* child = value->children.back();
		value->children.pop_back();
		DestroyValue(child);
		delete child;
	}

	if (value->type == ValueType::String)
	{
		if (value->value.AsString)
		{
			delete[] value->value.AsString;
			value->value.AsString = nullptr;
		}
	}

	if (value->name != nullptr)
	{
		delete[] value->name;
		value->name = nullptr;
	}
}

wchar_t* Json::CloneStr(const wchar_t* str)
{
	size_t len = wcslen(str);
	wchar_t* clone = new wchar_t[len + 1];
	wcscpy(clone, str);
	return clone;
}

wchar_t* Json::CloneUtf8Str(const char* str)
{
	// The narrow string of the project is UTF-8, so the shared codec does the whole job (including
	// the code points that wchar_t cannot hold on Windows: they become a surrogate pair there).
	std::wstring wide = Utf8ToWide(str);

	wchar_t* clone = new wchar_t[wide.size() + 1];
	wcscpy(clone, wide.c_str());
	return clone;
}

#pragma region "Serialization Related"

void Json::EmitChar(SerializeContext* ctx, uint8_t val, bool sizeOnly)
{
	// A size-only pass is asking for the size, so it is allowed to run past any buffer; a real
	// pass must never write past maxSize. assert() is no bound at all in Release.
	if (!sizeOnly && *ctx->actualSize >= ctx->maxSize)
	{
		throw "Json buffer overflow";
	}

	if (!sizeOnly)
	{
		(*ctx->ptr)[0] = val;
		(*ctx->ptr)++;
	}
	(*ctx->actualSize)++;
}

void Json::EmitText(SerializeContext* ctx, const char* text, bool sizeOnly)
{
	uint8_t* ptr = (uint8_t*)text;
	while (*ptr)
		EmitChar(ctx, *ptr++, sizeOnly);
}

void Json::EmitCodePoint(SerializeContext* ctx, int cp, bool sizeOnly)
{
	uint8_t c[4] = { 0 };
	int utf8Size = 0;

	// http://www.zedwood.com/article/cpp-utf8-char-to-codepoint
	if (cp <= 0x7F) { c[0] = (uint8_t)cp; utf8Size = 1; }
	else if (cp <= 0x7FF) { c[0] = (cp >> 6) + 192; c[1] = (cp & 63) + 128; utf8Size = 2; }
	else if (0xd800 <= cp && cp <= 0xdfff) { throw "Invalid block of utf8"; }
	else if (cp <= 0xFFFF) { c[0] = (cp >> 12) + 224; c[1] = ((cp >> 6) & 63) + 128; c[2] = (cp & 63) + 128; utf8Size = 3; }
	else if (cp <= 0x10FFFF) { c[0] = (cp >> 18) + 240; c[1] = ((cp >> 12) & 63) + 128; c[2] = ((cp >> 6) & 63) + 128; c[3] = (cp & 63) + 128; utf8Size = 4; }
	else { throw "Unsupported codepoint range"; }

	for (int i = 0; i < utf8Size; i++)
	{
		EmitChar(ctx, c[i], sizeOnly);
	}
}

void Json::EmitWcharString(SerializeContext* ctx, wchar_t* str, bool sizeOnly)
{
	wchar_t* ptr = str;
	while (*ptr)
	{
		int cp = (int)*ptr;

#if defined(_WINDOWS)

		// UTF-8 carries code points, not UTF-16 code units: a pair that stands for a code point
		// outside the BMP has to be put together again before it can be encoded (the surrogate
		// block itself is not encodable, so EmitCodePoint would refuse it).
		if (cp >= 0xD800 && cp <= 0xDBFF && ptr[1] >= 0xDC00 && ptr[1] <= 0xDFFF)
		{
			cp = 0x10000 + ((cp - 0xD800) << 10) + ((int)ptr[1] - 0xDC00);
			ptr++;
		}

#endif

		// Escaping

		switch (cp)
		{
			case '\"':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, '\"', sizeOnly);
				break;
			case '\\':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, '\\', sizeOnly);
				break;
			case '/':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, '/', sizeOnly);
				break;
			case '\b':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, 'b', sizeOnly);
				break;
			case '\f':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, 'f', sizeOnly);
				break;
			case '\n':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, 'n', sizeOnly);
				break;
			case '\r':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, 'r', sizeOnly);
				break;
			case '\t':
				EmitCodePoint(ctx, '\\', sizeOnly);
				EmitCodePoint(ctx, 't', sizeOnly);
				break;
			default:
				EmitCodePoint(ctx, cp, sizeOnly);
		}

		ptr++;
	}
}

// Indentation

void Json::Indent(SerializeContext* ctx, int depth, bool sizeOnly)
{
	while (depth--)
		Json::EmitText(ctx, "  ", sizeOnly);
}

#pragma endregion "Serialization Related"

#pragma region "De-Serialization Related"

bool Json::IsWhiteSpace(uint8_t value)
{
	switch (value)
	{
		case ' ':
		case '\r':
		case '\n':
		case '\t':
			return true;
	}
	return false;
}

bool Json::IsControl(uint8_t value)
{
	switch (value)
	{
		case '{':
		case '}':
		case '[':
		case ']':
		case ':':
		case ',':
			return true;
	}
	return false;
}

void Json::Throw(DeserializeContext* ctx, const char* message)
{
	// The line of the current position: one plus the newlines the document has up to it. The
	// message is thrown as it is (the emulator's other readers catch it as a `const char*`), and
	// the line is recorded in the context so a caller that wants to say *where* the document went
	// wrong can ask for it after the catch.
	if (ctx->errorLine != nullptr)
	{
		int line = 1;
		if (ctx->base != nullptr)
		{
			for (size_t i = 0; i < ctx->offset && i < ctx->maxSize; i++)
			{
				if (ctx->base[i] == '\n')
				{
					line++;
				}
			}
		}
		*ctx->errorLine = line;
	}

	throw message;
}

bool Json::GetLiteral(Json::DeserializeContext* ctx, Token& token)
{
	// :p
	// The window has to be tested without subtracting from maxSize: for a short input the
	// subtraction wrapped and the look-ahead below read past the end of the buffer. Note that a
	// literal ending exactly on the last byte is valid, hence the inclusive bound.
	if (Verify::Range(ctx->offset, 4, ctx->maxSize))
	{
		if (ctx->ptr[0] == 'n' && ctx->ptr[1] == 'u' && ctx->ptr[2] == 'l' && ctx->ptr[3] == 'l')
		{
			token.type = TokenType::Null;
			ctx->ptr += 4;
			ctx->offset += 4;
			return true;
		}
		else if (ctx->ptr[0] == 't' && ctx->ptr[1] == 'r' && ctx->ptr[2] == 'u' && ctx->ptr[3] == 'e')
		{
			token.type = TokenType::True;
			ctx->ptr += 4;
			ctx->offset += 4;
			return true;
		}
	}

	if (Verify::Range(ctx->offset, 5, ctx->maxSize))
	{
		if (ctx->ptr[0] == 'f' && ctx->ptr[1] == 'a' && ctx->ptr[2] == 'l' && ctx->ptr[3] == 's' && ctx->ptr[4] == 'e')
		{
			token.type = TokenType::False;
			ctx->ptr += 5;
			ctx->offset += 5;
			return true;
		}
	}

	return false;
}

int Json::FetchCodepoint(DeserializeContext* ctx)
{
	// http://www.zedwood.com/article/cpp-utf8-char-to-codepoint

	// Every continuation byte is read only after the offset has been checked for real: a truncated
	// UTF-8 sequence used to walk past the end of the buffer (the asserts are gone in Release).
	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		Throw(ctx, "Invalid utf8 codepoint");

	unsigned char u0 = ctx->ptr[0]; if (u0 >= 0 && u0 <= 127)
	{
		ctx->offset++;
		ctx->ptr++;
		return u0;
	}
	ctx->offset++;
	ctx->ptr++;

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		Throw(ctx, "Invalid utf8 codepoint");

	unsigned char u1 = ctx->ptr[0]; if (u0 >= 192 && u0 <= 223)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 192) * 64 + (u1 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	if (u0 == 0xed && (u1 & 0xa0) == 0xa0) Throw(ctx, "code points, 0xd800 to 0xdfff");

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		Throw(ctx, "Invalid utf8 codepoint");

	unsigned char u2 = ctx->ptr[0]; if (u0 >= 224 && u0 <= 239)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		Throw(ctx, "Invalid utf8 codepoint");

	unsigned char u3 = ctx->ptr[0]; if (u0 >= 240 && u0 <= 247)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 240) * 262144 + (u1 - 128) * 4096 + (u2 - 128) * 64 + (u3 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	Throw(ctx, "Invalid codepoint range");
}

bool Json::GetString(DeserializeContext* ctx, Token& token)
{
	// One slot past the limit is reserved for the terminator written on the closing quote, so a
	// string of exactly MaxStringSize code units fits.
	wchar_t str[MaxStringSize + 1] = { 0, };
	size_t strSize = 0;

	if (ctx->ptr[0] != '\"')
		return false;

	ctx->offset++;
	ctx->ptr++;

	while (ctx->offset < ctx->maxSize)
	{
		int cp = FetchCodepoint(ctx);

		// End of string?

		if (cp == '\"')
		{
			str[strSize] = 0;
			token.type = TokenType::String;
			token.value.AsString = CloneStr(str);
			return true;
		}

		// Unescaping

		if (cp == '\\')
		{
			// The escape needs a byte of its own; without this a trailing backslash walks off the
			// end of the buffer (FetchCodepoint checks too, this keeps the requirement local).
			if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
			{
				Throw(ctx, "Invalid utf8 codepoint");
			}

			cp = FetchCodepoint(ctx);
			switch (cp)
			{
				case '\"': cp = '\"'; break;
				case '\\': cp = '\\'; break;
				case '/': cp = '/'; break;
				case 'b': cp = '\b'; break;
				case 'f': cp = '\f'; break;
				case 'n': cp = '\n'; break;
				case 'r': cp = '\r'; break;
				case 't': cp = '\t'; break;
				case 'u':
				{
					// \uXXXX, and a UTF-16 surrogate pair (\uD800\uDC00) for the code points
					// outside the BMP. The pair is put together here, because the two halves on
					// their own are not text (and EmitCodePoint refuses them on the way out).
					uint32_t code = 0;
					for (int i = 0; i < 4; i++)
					{
						if (ctx->offset >= ctx->maxSize)
						{
							Throw(ctx, "a \\u escape without four hexadecimal digits");
						}

						uint8_t digit = ctx->ptr[0];
						uint32_t value;
						if (digit >= '0' && digit <= '9')
							value = digit - '0';
						else if (digit >= 'a' && digit <= 'f')
							value = digit - 'a' + 10;
						else if (digit >= 'A' && digit <= 'F')
							value = digit - 'A' + 10;
						else
							Throw(ctx, "a \\u escape without four hexadecimal digits");

						code = (code << 4) | value;
						ctx->offset++;
						ctx->ptr++;
					}

					if (code >= 0xD800 && code <= 0xDBFF)
					{
						if (ctx->offset + 1 < ctx->maxSize && ctx->ptr[0] == '\\' && ctx->ptr[1] == 'u')
						{
							ctx->offset += 2;
							ctx->ptr += 2;
							uint32_t low = 0;
							for (int i = 0; i < 4; i++)
							{
								if (ctx->offset >= ctx->maxSize)
									Throw(ctx, "a \\u escape without four hexadecimal digits");

								uint8_t digit = ctx->ptr[0];
								uint32_t value;
								if (digit >= '0' && digit <= '9')
									value = digit - '0';
								else if (digit >= 'a' && digit <= 'f')
									value = digit - 'a' + 10;
								else if (digit >= 'A' && digit <= 'F')
									value = digit - 'A' + 10;
								else
									Throw(ctx, "a \\u escape without four hexadecimal digits");

								low = (low << 4) | value;
								ctx->offset++;
								ctx->ptr++;
							}

							if (low < 0xDC00 || low > 0xDFFF)
								Throw(ctx, "a high surrogate that is not followed by a low surrogate");

							cp = (int)(0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00));
						}
						else
						{
							Throw(ctx, "an unpaired UTF-16 high surrogate");
						}
					}
					else if (code >= 0xDC00 && code <= 0xDFFF)
					{
						Throw(ctx, "an unpaired UTF-16 low surrogate");
					}
					else
					{
						cp = (int)code;
					}
				}
				break;
				default: Throw(ctx, "Invalid escape sequence");
			}
		}
		else if (cp < 0x20)
		{
			// A raw control character (the newline of a string broken across two lines, a tab) is
			// not JSON: it has to be escaped.
			Throw(ctx, "a control character in a string (escape it)");
		}

#if defined(_WINDOWS)

		// On Windows wchar_t is one UTF-16 code unit wide, so a code point outside the BMP takes
		// two of them; the extra slot is checked for rather than assumed.
		if (cp > 0xFFFF)
		{
			if (strSize + 2 > MaxStringSize)
			{
				Throw(ctx, "Json string too long");
			}

			cp -= 0x10000;
			str[strSize++] = (wchar_t)(0xD800 + (cp >> 10));
			str[strSize++] = (wchar_t)(0xDC00 + (cp & 0x3FF));
			continue;
		}

#endif

		if (strSize >= MaxStringSize)
		{
			Throw(ctx, "Json string too long");
		}

		str[strSize++] = (wchar_t)cp;
	}

	return false;
}

bool Json::IsAllowed(uint8_t val, char* allowed)
{
	char* ptr = allowed;
	while (*ptr)
	{
		if (*ptr == val)
			return true;
		ptr++;
	}
	return false;
}

bool Json::GetFloat(DeserializeContext* ctx, Token& token)
{
	static char allowedChars[] = "0123456789.eE+-";
	char number[0x100] = { 0, };
	int numberLen = 0;

	size_t offset = 0;

	// The bound is computed once: "offset < (ctx->maxSize - ctx->offset)" wrapped as soon as
	// ctx->offset had walked past the end of the buffer and then looped over the heap.
	if (ctx->offset >= ctx->maxSize)
	{
		return false;
	}

	const size_t remaining = ctx->maxSize - ctx->offset;

	while (offset < remaining)
	{
		// The token has to leave room for the terminator appended below.
		if (numberLen > (int)sizeof(number) - 2)
		{
			Throw(ctx, "Json number too long");
		}

		if (IsWhiteSpace(ctx->ptr[offset]) || IsControl(ctx->ptr[offset]))
		{
			break;
		}

		if (!IsAllowed(ctx->ptr[offset], allowedChars))
		{
			return false;
		}

		number[numberLen++] = ctx->ptr[offset++];
	}

	if (numberLen != 0)
	{
		number[numberLen] = 0;

		// The token has to be a JSON number exactly: an optional '-', an integer part without a
		// leading zero, an optional fraction (with at least one digit after '.') and an optional
		// exponent. Anything else (a bare '.', a trailing 'e', two signs) is a syntax error, not
		// half a number.
		const char* p = number;
		if (*p == '-')
			p++;
		if (*p == '0')
		{
			p++;
		}
		else if (*p >= '1' && *p <= '9')
		{
			while (*p >= '0' && *p <= '9')
				p++;
		}
		else
		{
			Throw(ctx, "a fraction without a digit before '.'");
		}
		if (*p == '.')
		{
			p++;
			if (*p < '0' || *p > '9')
				Throw(ctx, "a fraction without a digit after '.'");
			while (*p >= '0' && *p <= '9')
				p++;
		}
		if (*p == 'e' || *p == 'E')
		{
			p++;
			if (*p == '+' || *p == '-')
				p++;
			if (*p < '0' || *p > '9')
				Throw(ctx, "an exponent without a digit");
			while (*p >= '0' && *p <= '9')
				p++;
		}
		if (*p != 0)
		{
			Throw(ctx, "a malformed number");
		}

		char* end = nullptr;
		double parsed = strtod(number, &end);

		// A value that does not fit the stored float must not turn into infinity.
		if (end == number || *end != 0 || parsed > FLT_MAX || parsed < -FLT_MAX)
		{
			Throw(ctx, "Float out of range");
		}

		token.type = TokenType::Float;
		token.value.AsFloat = (float)parsed;
		ctx->offset += numberLen;
		ctx->ptr += numberLen;
		return true;
	}

	return false;
}

bool Json::GetInt(DeserializeContext* ctx, Token& token)
{
	char number[0x100] = { 0, };
	int numberLen = 0;

	size_t offset = 0;

	// The bound is computed once: "offset < (ctx->maxSize - ctx->offset)" wrapped as soon as
	// ctx->offset had walked past the end of the buffer and then looped over the heap.
	if (ctx->offset >= ctx->maxSize)
	{
		return false;
	}

	const size_t remaining = ctx->maxSize - ctx->offset;

	while (offset < remaining)
	{
		// The token has to leave room for the terminator appended below.
		if (numberLen > (int)sizeof(number) - 2)
		{
			Throw(ctx, "Json number too long");
		}

		uint8_t c = ctx->ptr[offset];

		if (IsWhiteSpace(c) || IsControl(c))
		{
			break;
		}

		// The JSON integer grammar: an optional '-', then '0' or a digit 1-9 followed by digits.
		// A leading '+' and a leading zero ("05") are not JSON, and this reader refuses them
		// instead of quietly turning them into a number. '.' and 'e' mean the token is a float,
		// which GetFloat reads; any other byte means it is not a number at all.
		if (c == '+')
		{
			Throw(ctx, "a '+' sign is not part of the JSON number grammar");
		}
		if (c == '-')
		{
			if (offset != 0)
			{
				Throw(ctx, "a '-' sign is only allowed at the start of a number");
			}
			number[numberLen++] = '-';
			offset++;
			continue;
		}
		if (c == '.' || c == 'e' || c == 'E')
		{
			return false;
		}
		if (c < '0' || c > '9')
		{
			return false;
		}
		if (c == '0')
		{
			size_t digits = (number[0] == '-') ? 1 : 0;
			if (offset == digits && offset + 1 < remaining &&
				ctx->ptr[offset + 1] >= '0' && ctx->ptr[offset + 1] <= '9')
			{
				Throw(ctx, "a leading zero in a number");
			}
		}

		number[numberLen++] = (char)c;
		offset++;
	}

	if (numberLen != 0)
	{
		number[numberLen] = 0;

		size_t i = 0;
		bool negative = false;
		if (number[0] == '-')
		{
			negative = true;
			i = 1;
		}
		if (i >= (size_t)numberLen)
		{
			Throw(ctx, "a '-' sign without a digit");
		}

		// The accumulator saturates: a value that does not fit a signed 64-bit integer lands on
		// the end of its range instead of wrapping around. The value is kept in the two's
		// complement form the rest of the emulator already uses for a negative Int member.
		const uint64_t limit = negative ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
		uint64_t value = 0;
		for (; i < (size_t)numberLen; i++)
		{
			uint64_t digit = (uint64_t)(number[i] - '0');
			if (value > (limit - digit) / 10)
			{
				value = limit;
				break;
			}
			value = value * 10 + digit;
		}

		token.type = TokenType::Int;
		if (negative && value != 0x8000000000000000ULL)
		{
			token.value.AsInt = (uint64_t)(-(int64_t)value);
		}
		else
		{
			token.value.AsInt = value;
		}

		ctx->offset += numberLen;
		ctx->ptr += numberLen;
		return true;
	}

	return false;
}

void Json::GetToken(Token& token, DeserializeContext* ctx)
{
	while (ctx->offset < ctx->maxSize)
	{
		if (IsWhiteSpace(ctx->ptr[0]))
		{
			ctx->ptr++;
			ctx->offset++;
		}
		else break;
	}

	if (ctx->offset >= ctx->maxSize)
	{
		token.type = TokenType::EndOfStream;
		return;
	}

	switch (ctx->ptr[0])
	{
		case '{':
			token.type = TokenType::ObjectStart;
			ctx->ptr++;
			ctx->offset++;
			return;
		case '}':
			token.type = TokenType::ObjectEnd;
			ctx->ptr++;
			ctx->offset++;
			return;
		case '[':
			token.type = TokenType::ArrayStart;
			ctx->ptr++;
			ctx->offset++;
			return;
		case ']':
			token.type = TokenType::ArrayEnd;
			ctx->ptr++;
			ctx->offset++;
			return;
		case ':':
			token.type = TokenType::Colon;
			ctx->ptr++;
			ctx->offset++;
			return;
		case ',':
			token.type = TokenType::Comma;
			ctx->ptr++;
			ctx->offset++;
			return;
	}

	// Try get literal
	if (GetLiteral(ctx, token))
		return;

	// Try get String
	if (GetString(ctx, token))
		return;

	// Try get Int
	if (GetInt(ctx, token))
		return;

	// Try get Float
	if (GetFloat(ctx, token))
		return;

	Throw(ctx, "Unknown Token!");
}

#pragma endregion "De-Serialization Related"

char* Json::Value::CloneName(const char* otherName)
{
	if (otherName == nullptr)		// Name can be null
		return nullptr;

	size_t len = strlen(otherName);
	char* clone = new char[len + 1];
	strcpy(clone, otherName);
	return clone;
}

char* Json::Value::CloneWcharName(const wchar_t* otherName)
{
	if (otherName == nullptr)		// Name can be null
		return nullptr;

	// The name of a member is kept as UTF-8, like every narrow string of the project.
	std::string name = WideToUtf8(otherName);

	char* clone = new char[name.size() + 1];
	memcpy(clone, name.c_str(), name.size() + 1);
	return clone;
}

void Json::Value::DeserializeObject(DeserializeContext* ctx)
{
	Token token, colon, comma;

	type = ValueType::Object;

	bool haveToken = false;
	int counter = 0;

	while (true)
	{
		Value* child = nullptr;

		// A runaway guard, not the format's limit: without it a document with millions of members
		// exhausts memory before anything else notices (assert() did nothing in Release).
		if (++counter > MaxElements)
		{
			Throw(ctx, "Too many Json elements");
		}

		if (!haveToken)
		{
			Json::GetToken(token, ctx);
		}
		haveToken = false;

		switch (token.type)
		{
			case TokenType::String:

				Json::GetToken(colon, ctx);

				// This has to be a real check rather than an assert(): a missing colon is a
				// malformed document, and in a Debug build the assert would pop a blocking
				// "abort()" dialog instead of rejecting the file.
				if (colon.type != TokenType::Colon)
				{
					Throw(ctx, "Json Object Syntax Error");
				}

				child = new Value(this);
				children.push_back(child);
				child->Deserialize(ctx, token.value.AsString);

				if (token.value.AsString != nullptr)
				{
					delete[] token.value.AsString;
				}

				Json::GetToken(comma, ctx);
				if (comma.type == TokenType::Comma)
				{
					// A '}' right after a ',' is a trailing comma: nearly always a half deleted
					// member, which is why it is refused instead of ignored.
					Json::GetToken(token, ctx);
					if (token.type == TokenType::ObjectEnd)
					{
						Throw(ctx, "unexpected '}' after ',' (a trailing comma)");
					}
					haveToken = true;
					break;
				}
				else if (comma.type == TokenType::ObjectEnd)
				{
					return;
				}
				else
				{
					Throw(ctx, "Json Object Syntax Error");
				}

				break;

			case TokenType::ObjectEnd:
				return;

			case TokenType::EndOfStream:
			default:
				// GetToken does not advance on end of stream, so without this the loop spun at
				// 100% CPU forever on a truncated document such as "{".
				Throw(ctx, "Json Object Syntax Error");
		}
	}
}

void Json::Value::DeserializeArray(DeserializeContext* ctx)
{
	Token token;

	type = ValueType::Array;

	bool afterComma = false;
	int counter = 0;

	while (true)
	{
		// A runaway guard, not the format's limit: without it a document with millions of elements
		// exhausts memory before anything else notices (assert() did nothing in Release).
		if (++counter > MaxElements)
		{
			Throw(ctx, "Too many Json elements");
		}

		// Check empty arrays

		while (ctx->offset < ctx->maxSize)
		{
			if (IsWhiteSpace(ctx->ptr[0]))
			{
				ctx->ptr++;
				ctx->offset++;
			}
			else break;
		}

		if (ctx->offset >= ctx->maxSize)
		{
			Throw(ctx, "Json Array Syntax Error");
		}

		if (ctx->ptr[0] == ']')
		{
			if (afterComma)
			{
				Throw(ctx, "unexpected ']' after ',' (a trailing comma)");
			}
			Json::GetToken(token, ctx);		// Eat end array token
			break;
		}

		// Array element

		Value* child = new Value(this);
		children.push_back(child);
		child->Deserialize(ctx, nullptr);

		Json::GetToken(token, ctx);

		if (token.type == TokenType::Comma)
		{
			afterComma = true;
			continue;
		}
		else if (token.type == TokenType::ArrayEnd)
		{
			return;
		}
		else
		{
			Throw(ctx, "Json Array Syntax Error");
		}
	}
}

void Json::Value::Serialize(SerializeContext* ctx, int depth, bool sizeOnly)
{
	wchar_t temp[0x100] = { 0, };

	// The same bound the parser enforces on the way in: a tree deep enough to run this recursion
	// out of stack must be refused, not walked (assert() is compiled out in Release).
	if (depth >= MaxDepth)
	{
		throw "Json max depth exceeded";
	}

	switch (type)
	{
		case ValueType::Object:
			// The opening brace stays on the line of the member it belongs to (the caller has
			// already written the "name" : prefix), and the separator comma is written before the
			// line break, so it never lands on a line of its own.
			Json::EmitText(ctx, "{\r\n", sizeOnly);

			for (auto it = children.begin(); it != children.end(); ++it)
			{
				Value* child = *it;

				Indent(ctx, depth + 1, sizeOnly);
				Json::EmitChar(ctx, '\"', sizeOnly);

				// A member name is optional, so it may be null - EmitText(nullptr) used to crash.
				if (child->name != nullptr)
				{
					Json::EmitText(ctx, child->name, sizeOnly);
				}

				Json::EmitChar(ctx, '\"', sizeOnly);
				Json::EmitText(ctx, " : ", sizeOnly);

				child->Serialize(ctx, depth + 1, sizeOnly);

				auto next = it;
				++next;
				if (next != children.end())
				{
					Json::EmitChar(ctx, ',', sizeOnly);
				}
				Json::EmitText(ctx, "\r\n", sizeOnly);
			}

			Indent(ctx, depth, sizeOnly);
			Json::EmitChar(ctx, '}', sizeOnly);
			break;
		case ValueType::Array:
			Json::EmitText(ctx, "[ ", sizeOnly);

			for (auto it = children.begin(); it != children.end(); ++it)
			{
				if (it != children.begin())
				{
					Json::EmitText(ctx, ", ", sizeOnly);
				}

				Value* child = *it;
				child->Serialize(ctx, depth + 1, sizeOnly);
			}

			Json::EmitText(ctx, " ]", sizeOnly);
			break;
		case ValueType::Null:
			Json::EmitText(ctx, "null", sizeOnly);
			break;
		case ValueType::Bool:
			Json::EmitText(ctx, value.AsBool ? "true" : "false", sizeOnly);
			break;
		case ValueType::Int:
			// %llu, not the MSVC %I64u: glibc reads "%I64u" as the 'I' flag plus a field width of
			// 64 and pads every number to 64 columns.
			swprintf(temp, sizeof(temp) / sizeof(temp[0]) - 1, L"%llu", (unsigned long long)value.AsInt);
			EmitWcharString(ctx, temp, sizeOnly);
			break;
		case ValueType::Float:
			swprintf(temp, sizeof(temp) / sizeof(temp[0]) - 1, L"%.4f", value.AsFloat);
			EmitWcharString(ctx, temp, sizeOnly);
			break;
		case ValueType::String:
			Json::EmitChar(ctx, '\"', sizeOnly);
			EmitWcharString(ctx, value.AsString, sizeOnly);
			Json::EmitChar(ctx, '\"', sizeOnly);
			break;
		default:
			throw "Unknown ValueType";
	}
}

void Json::Value::Deserialize(DeserializeContext* ctx, wchar_t* keyName)
{
	Token token, key;

	// The containers below come back through here for every nested value, so the depth belongs to
	// the context. Without this a document of 20000 '[' ran the stack out before anything noticed.
	if (++ctx->depth > MaxDepth)
	{
		Throw(ctx, "Json max nested depth exceeded");
	}

	this->name = CloneWcharName(keyName);

	Json::GetToken(token, ctx);

	switch (token.type)
	{
		case TokenType::ObjectStart:
			DeserializeObject(ctx);
			break;
		case TokenType::ArrayStart:
			DeserializeArray(ctx);
			break;

		case TokenType::String:
			type = ValueType::String;
			if (token.value.AsString != nullptr)
			{
				value.AsString = CloneStr(token.value.AsString);
				delete[] token.value.AsString;
			}
			else
			{
				value.AsString = nullptr;
			}
			break;
		case TokenType::Int:
			type = ValueType::Int;
			value.AsInt = token.value.AsInt;
			break;
		case TokenType::Float:
			type = ValueType::Float;
			value.AsFloat = token.value.AsFloat;
			break;
		case TokenType::True:
			type = ValueType::Bool;
			value.AsBool = true;
			break;
		case TokenType::False:
			type = ValueType::Bool;
			value.AsBool = false;
			break;
		case TokenType::Null:
			type = ValueType::Null;
			break;

		default:
			Throw(ctx, "Json Syntax Error");
			break;
	}

	ctx->depth--;
}

// Dynamic modification

Json::Value* Json::Value::AddInt(const char* keyName, int _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Int;
	child->name = CloneName(keyName);
	child->value.AsInt = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddUInt16(const char* keyName, uint16_t _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Int;
	child->name = CloneName(keyName);
	child->value.AsInt = 0;
	child->value.AsUint16 = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddUInt32(const char* keyName, uint32_t _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Int;
	child->name = CloneName(keyName);
	child->value.AsInt = 0;
	child->value.AsUint32 = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddUInt64(const char* keyName, uint64_t _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Int;
	child->name = CloneName(keyName);
	child->value.AsInt = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddFloat(const char* keyName, float _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Float;
	child->name = CloneName(keyName);
	child->value.AsFloat = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddNull(const char* keyName)
{
	Value* child = new Value(this);
	child->type = ValueType::Null;
	child->name = CloneName(keyName);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddBool(const char* keyName, bool _value)
{
	Value* child = new Value(this);
	child->type = ValueType::Bool;
	child->name = CloneName(keyName);
	child->value.AsBool = _value;
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddString(const char* keyName, const wchar_t* str)
{
	Value* child = new Value(this);
	child->type = ValueType::String;
	child->name = CloneName(keyName);
	child->value.AsString = CloneStr(str);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddUtf8String(const char* keyName, const char* str)
{
	Value* child = new Value(this);
	child->type = ValueType::String;
	child->name = CloneName(keyName);
	child->value.AsString = CloneUtf8Str(str);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::ReplaceString(const wchar_t* str)
{
	if (type != ValueType::String)
	{
		throw "Json type mismatch";
	}
	if (value.AsString)
	{
		delete[] value.AsString;
	}
	value.AsString = CloneStr(str);
	return this;
}

Json::Value* Json::Value::AddObject(const char* keyName)
{
	Value* child = new Value(this);
	child->type = ValueType::Object;
	child->name = CloneName(keyName);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddArray(const char* keyName)
{
	Value* child = new Value(this);
	child->type = ValueType::Array;
	child->name = CloneName(keyName);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::AddValue(const char* keyName, Json::Value* value)
{
	if (value->name)
	{
		delete[] value->name;
	}
	value->name = CloneName(keyName);
	children.push_back(value);
	return value;
}

Json::Value* Json::Value::Add(Value* _parent, Value* other)
{
	Value* child = new Value(_parent);
	child->type = other->type;
	child->name = CloneName(other->name);
	switch (other->type)
	{
		case ValueType::Array:
		case ValueType::Object:
			for (auto it = other->children.begin(); it != other->children.end(); ++it)
			{
				child->children.push_back(Add(child, *it));
			}
			break;
		case ValueType::Bool:
			child->value.AsBool = other->value.AsBool;
			break;
		case ValueType::Int:
			child->value.AsInt = other->value.AsInt;
			break;
		case ValueType::Float:
			child->value.AsFloat = other->value.AsFloat;
			break;
		case ValueType::String:
			child->value.AsString = CloneStr(other->value.AsString);
			break;
	}
	return child;
}

Json::Value* Json::Value::Replace(Value* _parent, Value* other)
{
	Value* child = nullptr;
	bool newChild = false;

	if (other->name != nullptr)
	{
		child = _parent->ByName(other->name);
	}
	else
	{
		child = _parent->ByType(other->type);
	}

	if (child != nullptr)
	{
		if (child->type != other->type)
		{
			child = nullptr;
		}
	}

	if (child == nullptr)
	{
		child = new Value(parent);
		newChild = true;
	}

	child->type = other->type;
	if (child->name)
	{
		delete[] child->name;
	}
	child->name = CloneName(other->name);

	switch (other->type)
	{
		case ValueType::Array:
			// The list of the file replaces the list that was there (see the comment above).
			while (!child->children.empty())
			{
				Value* entry = child->children.front();
				child->children.pop_front();
				delete entry;
			}

			for (auto it = other->children.begin(); it != other->children.end(); ++it)
			{
				child->children.push_back(Add(child, *it));
			}
			break;

		case ValueType::Object:
			for (auto it = other->children.begin(); it != other->children.end(); ++it)
			{
				Value* sibling = Replace(child, *it);
				if (sibling != nullptr)
				{
					child->children.push_back(sibling);
				}
			}
			break;
		case ValueType::Bool:
			child->value.AsBool = other->value.AsBool;
			break;
		case ValueType::Int:
			child->value.AsInt = other->value.AsInt;
			break;
		case ValueType::Float:
			child->value.AsFloat = other->value.AsFloat;
			break;
		case ValueType::String:
			if (child->value.AsString)
			{
				delete[] child->value.AsString;
			}
			child->value.AsString = CloneStr(other->value.AsString);
			break;
	}

	return newChild ? child : nullptr;
}

// Access

Json::Value* Json::Value::ByName(const char* byName)
{
	for (auto it = children.begin(); it != children.end(); ++it)
	{
		Value* child = *it;

		if (child->name == nullptr)
			continue;

		if (!strcmp(child->name, byName))
		{
			return child;
		}
	}
	return nullptr;
}

Json::Value* Json::Value::ByType(const ValueType byType)
{
	for (auto it = children.begin(); it != children.end(); ++it)
	{
		Value* child = *it;

		if (child->type == byType)
		{
			return child;
		}
	}
	return nullptr;
}

bool Json::Value::Remove(Value* child)
{
	for (auto it = children.begin(); it != children.end(); ++it)
	{
		if (*it == child)
		{
			children.erase(it);
			delete child;
			return true;
		}
	}

	return false;
}

void Json::Serialize(void* text, size_t maxTextSize, size_t& actualTextSize)
{
	actualTextSize = 0;

	SerializeContext ctx = { 0 };

	ctx.ptr = (uint8_t**)&text;
	ctx.maxSize = maxTextSize;
	ctx.actualSize = &actualTextSize;

	for (auto it = root.children.begin(); it != root.children.end(); ++it)
	{
		(*it)->Serialize(&ctx, 0, false);
		Json::EmitText(&ctx, "\r\n", false);
	}
}

void Json::GetSerializedTextSize(void* text, size_t maxTextSize, size_t& actualTextSize)
{
	actualTextSize = 0;

	SerializeContext ctx = { 0 };

	ctx.ptr = (uint8_t**)&text;
	ctx.maxSize = maxTextSize;
	ctx.actualSize = &actualTextSize;

	for (auto it = root.children.begin(); it != root.children.end(); ++it)
	{
		(*it)->Serialize(&ctx, 0, true);
		Json::EmitText(&ctx, "\r\n", true);
	}
}

void Json::Deserialize(void* text, size_t textSize)
{
	DeserializeContext ctx = { 0 };

	errorLine = 0;

	if (text == nullptr || textSize == 0)
	{
		errorLine = 1;
		throw "Json input is empty";
	}

	ctx.ptr = (uint8_t*)text;
	ctx.offset = 0;
	ctx.maxSize = textSize;
	ctx.base = (uint8_t*)text;
	ctx.errorLine = &errorLine;

	// A UTF-8 BOM is not a JSON token, but Notepad and some editors write one, so it is skipped at
	// the very start of the document (and only there: a BOM anywhere else stays a syntax error).
	if (textSize >= 3 &&
		ctx.ptr[0] == 0xEF && ctx.ptr[1] == 0xBB && ctx.ptr[2] == 0xBF)
	{
		ctx.ptr += 3;
		ctx.offset += 3;
	}

	root.AddObject(nullptr)->Deserialize(&ctx, nullptr);

	// Nothing but whitespace may follow the document: a second value (or a stray byte) is a
	// malformed file, not something to be silently ignored.
	while (ctx.offset < ctx.maxSize && IsWhiteSpace(ctx.ptr[0]))
	{
		ctx.ptr++;
		ctx.offset++;
	}

	if (ctx.offset < ctx.maxSize)
	{
		Throw(&ctx, "unexpected text after the document");
	}
}

// Clone

void Json::Clone(Json* other)
{
	DestroyValue(&root);

	for (auto it = other->root.children.begin(); it != other->root.children.end(); ++it)
	{
		Value* child = root.Add(&root, *it);
		root.children.push_back(child);
	}
}

// Merge

void Json::Merge(Json* other)
{
	for (auto it = other->root.children.begin(); it != other->root.children.end(); ++it)
	{
		Value* child = root.Replace(&root, *it);
		if (child != nullptr)
		{
			root.children.push_back(child);
		}
	}
}
