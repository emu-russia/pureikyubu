#include "pch.h"

#include <cerrno>
#include <cfloat>

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
	std::wstring wide = Util::StringToWstring(str);

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
		throw "Invalid utf8 codepoint";

	unsigned char u0 = ctx->ptr[0]; if (u0 >= 0 && u0 <= 127)
	{
		ctx->offset++;
		ctx->ptr++;
		return u0;
	}
	ctx->offset++;
	ctx->ptr++;

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		throw "Invalid utf8 codepoint";

	unsigned char u1 = ctx->ptr[0]; if (u0 >= 192 && u0 <= 223)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 192) * 64 + (u1 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	if (u0 == 0xed && (u1 & 0xa0) == 0xa0) throw "code points, 0xd800 to 0xdfff";

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		throw "Invalid utf8 codepoint";

	unsigned char u2 = ctx->ptr[0]; if (u0 >= 224 && u0 <= 239)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	if (!Verify::Range(ctx->offset, 1, ctx->maxSize))
		throw "Invalid utf8 codepoint";

	unsigned char u3 = ctx->ptr[0]; if (u0 >= 240 && u0 <= 247)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 240) * 262144 + (u1 - 128) * 4096 + (u2 - 128) * 64 + (u3 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	throw "Invalid codepoint range";
}

bool Json::GetString(DeserializeContext* ctx, Token& token)
{
	wchar_t str[MaxStringSize] = { 0, };
	size_t strSize = 0;

	if (ctx->ptr[0] != '\"')
		return false;

	ctx->offset++;
	ctx->ptr++;

	while (ctx->offset < ctx->maxSize)
	{
		// One slot is reserved for the terminator written on the closing quote, so a string of
		// MaxStringSize - 1 characters no longer fits. In Release the assert() was not a bound.
		if (!Verify::Range(strSize, 1, MaxStringSize - 1))
		{
			throw "Json string too long";
		}

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
				throw "Invalid utf8 codepoint";
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
				case 'u': throw "uXXXX not supported";
				default: throw "Invalid escape sequence";
			}
		}

#if defined(_WINDOWS)

		// On Windows wchar_t is one UTF-16 code unit wide, so a code point outside the BMP takes
		// two of them; the extra slot is checked for rather than assumed.
		if (cp > 0xFFFF)
		{
			if (!Verify::Range(strSize, 2, MaxStringSize - 1))
			{
				throw "Json string too long";
			}

			cp -= 0x10000;
			str[strSize++] = (wchar_t)(0xD800 + (cp >> 10));
			str[strSize++] = (wchar_t)(0xDC00 + (cp & 0x3FF));
			continue;
		}

#endif

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
			throw "Json number too long";
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

		char* end = nullptr;
		double parsed = strtod(number, &end);

		// A malformed token must not be silently turned into half a number, and a value that does
		// not fit the stored float must not turn into infinity.
		if (end == number || *end != 0 || parsed > FLT_MAX || parsed < -FLT_MAX)
		{
			throw "Float out of range";
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
	static char allowedChars[] = "+-0123456789";
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
			throw "Json number too long";
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

		errno = 0;

		char* end = nullptr;
		unsigned long long parsed = strtoull(number, &end, 10);

		// strtoull folds a negative value onto ULLONG_MAX and saturates an out-of-range one, so
		// both are rejected here; the whole token also has to be consumed, otherwise "12+3" would
		// quietly become 12.
		if (number[0] == '-' || end == number || *end != 0 || errno == ERANGE)
		{
			throw "Integer out of range";
		}

		token.type = TokenType::Int;
		token.value.AsInt = parsed;
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

	throw "Unknown Token!";
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
	std::string name = Util::WstringToString(otherName);

	char* clone = new char[name.size() + 1];
	memcpy(clone, name.c_str(), name.size() + 1);
	return clone;
}

void Json::Value::DeserializeObject(DeserializeContext* ctx)
{
	Token token, colon, comma;

	type = ValueType::Object;

	int counter = 0;

	while (true)
	{
		Value* child = nullptr;

		// A runaway guard, not the format's limit: without it a document with millions of members
		// exhausts memory before anything else notices (assert() did nothing in Release).
		if (++counter > MaxElements)
		{
			throw "Too many Json elements";
		}

		Json::GetToken(token, ctx);

		switch (token.type)
		{
			case TokenType::String:

				Json::GetToken(colon, ctx);

				// This has to be a real check rather than an assert(): a missing colon is a
				// malformed document, and in a Debug build the assert would pop a blocking
				// "abort()" dialog instead of rejecting the file.
				if (colon.type != TokenType::Colon)
				{
					throw "Json Object Syntax Error";
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
					break;
				}
				else if (comma.type == TokenType::ObjectEnd)
				{
					return;
				}
				else
				{
					throw "Json Object Syntax Error";
				}

				break;

			case TokenType::ObjectEnd:
				return;

			case TokenType::EndOfStream:
			default:
				// GetToken does not advance on end of stream, so without this the loop spun at
				// 100% CPU forever on a truncated document such as "{".
				throw "Json Object Syntax Error";
		}
	}
}

void Json::Value::DeserializeArray(DeserializeContext* ctx)
{
	Token token;

	type = ValueType::Array;

	int counter = 0;

	while (true)
	{
		// A runaway guard, not the format's limit: without it a document with millions of elements
		// exhausts memory before anything else notices (assert() did nothing in Release).
		if (++counter > MaxElements)
		{
			throw "Too many Json elements";
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
			throw "Json Array Syntax Error";
		}

		if (ctx->ptr[0] == ']')
		{
			Json::GetToken(token, ctx);		// Eat end array token
			break;
		}

		// Array element

		Value* child = new Value(this);
		children.push_back(child);
		child->Deserialize(ctx, nullptr);

		Json::GetToken(token, ctx);

		switch (token.type)
		{
			case TokenType::Comma:
				break;

			case TokenType::ArrayEnd:
				return;

			default:
				throw "Json Array Syntax Error";
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
			Indent(ctx, depth, sizeOnly);
			Json::EmitText(ctx, "{\r\n", sizeOnly);

			for (auto it = children.begin(); it != children.end(); ++it)
			{
				if (it != children.begin())
				{
					Json::EmitText(ctx, ",\r\n", sizeOnly);
				}

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
			}

			Indent(ctx, depth, sizeOnly);
			Json::EmitText(ctx, "}\r\n", sizeOnly);
			break;
		case ValueType::Array:
			Indent(ctx, depth, sizeOnly);
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

			Indent(ctx, depth, sizeOnly);
			Json::EmitChar(ctx, ']', sizeOnly);
			break;
		case ValueType::Null:
			Json::EmitText(ctx, "null", sizeOnly);
			break;
		case ValueType::Bool:
			Json::EmitText(ctx, value.AsBool ? "true" : "false", sizeOnly);
			break;
		case ValueType::Int:
			swprintf(temp, sizeof(temp) / sizeof(temp[0]) - 1, L"%I64u", value.AsInt);
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
		throw "Json max depth exceeded";
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
			throw "Json Syntax Error";
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
	}
}

void Json::Deserialize(void* text, size_t textSize)
{
	DeserializeContext ctx = { 0 };

	if (text == nullptr || textSize == 0)
	{
		throw "Json input is empty";
	}

	ctx.ptr = (uint8_t*)text;
	ctx.offset = 0;
	ctx.maxSize = textSize;

	root.AddObject(nullptr)->Deserialize(&ctx, nullptr);
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
