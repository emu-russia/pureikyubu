#include "pch.h"

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

wchar_t* Json::CloneAnsiStr(const char* str)
{
	size_t len = strlen(str);
	wchar_t* clone = new wchar_t[len + 1];
	wchar_t* wcharPtr = clone;
	char* charPtr = (char*)str;
	while (*charPtr)
	{
		*wcharPtr++ = *charPtr++;
	}
	*wcharPtr++ = 0;
	return clone;
}

#pragma region "Serialization Related"

void Json::EmitChar(SerializeContext* ctx, uint8_t val, bool sizeOnly)
{
	assert(*ctx->actualSize < ctx->maxSize);

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
	if (ctx->offset < (ctx->maxSize - 4))
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

	if (ctx->offset < (ctx->maxSize - 5))
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

	assert(ctx->offset < ctx->maxSize);
	unsigned char u0 = ctx->ptr[0]; if (u0 >= 0 && u0 <= 127)
	{
		ctx->offset++;
		ctx->ptr++;
		return u0;
	}
	ctx->offset++;
	ctx->ptr++;

	assert(ctx->offset < ctx->maxSize);
	unsigned char u1 = ctx->ptr[0]; if (u0 >= 192 && u0 <= 223)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 192) * 64 + (u1 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	if (u0 == 0xed && (u1 & 0xa0) == 0xa0) throw "code points, 0xd800 to 0xdfff";

	assert(ctx->offset < ctx->maxSize);
	unsigned char u2 = ctx->ptr[0]; if (u0 >= 224 && u0 <= 239)
	{
		ctx->offset++;
		ctx->ptr++;
		return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
	}
	ctx->offset++;
	ctx->ptr++;

	assert(ctx->offset < ctx->maxSize);
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
		assert(strSize < MaxStringSize);

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
	char allowedChars[] = "0123456789.eE-";
	char number[0x100] = { 0, };
	int numberLen = 0;

	size_t offset = 0;

	while (offset < (ctx->maxSize - ctx->offset))
	{
		assert(numberLen < (sizeof(number) - 1));

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
		token.type = TokenType::Float;
		token.value.AsFloat = (float)atof(number);
		ctx->offset += numberLen;
		ctx->ptr += numberLen;
		return true;
	}

	return false;
}

bool Json::GetInt(DeserializeContext* ctx, Token& token)
{
	char allowedChars[] = "0123456789";
	char number[0x100] = { 0, };
	int numberLen = 0;

	size_t offset = 0;

	while (offset < (ctx->maxSize - ctx->offset))
	{
		assert(numberLen < (sizeof(number) - 1));

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
		token.type = TokenType::Int;
		token.value.AsInt = strtoull(number, nullptr, 10);
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

	size_t len = wcslen(otherName);
	char* clone = new char[len + 1];

	for (size_t i = 0; i < len; i++)
	{
		clone[i] = (char)otherName[i];
	}
	clone[len] = 0;

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

		assert(counter < MaxElements);

		Json::GetToken(token, ctx);

		switch (token.type)
		{
			case TokenType::String:

				Json::GetToken(colon, ctx);
				assert(colon.type == TokenType::Colon);

				child = new Value(this);
				children.push_back(child);
				child->Deserialize(ctx, token.value.AsString);

				if (token.value.AsString != nullptr)
				{
					delete[] token.value.AsString;
				}

				counter++;

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
		assert(counter < MaxElements);

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

		counter++;

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

	assert(depth < MaxDepth);

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
				Json::EmitText(ctx, child->name, sizeOnly);
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

Json::Value* Json::Value::AddAnsiString(const char* keyName, const char* str)
{
	Value* child = new Value(this);
	child->type = ValueType::String;
	child->name = CloneName(keyName);
	child->value.AsString = CloneAnsiStr(str);
	children.push_back(child);
	return child;
}

Json::Value* Json::Value::ReplaceString(const wchar_t* str)
{
	assert(type == ValueType::String);
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

	assert(text);

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
