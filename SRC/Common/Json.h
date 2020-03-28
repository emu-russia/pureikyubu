// Json serialization engine. Json is used to store emulator settings, as well as for the JDI system (Json Debug Interface).

// All errors during deserialization are thrown by exceptions.
// During serializing, in theory, there shouldn't be any mistakes (if we are not enemies to ourselves and have not changed, for example, the Value type to some non-standard one).

#pragma once

#include <tchar.h>
#include <list>
#include <string>
#include <cassert>

class Json
{
	static const int MaxDepth = 255;			// Foolproof
	static const int MaxStringSize = 0x1000;	// Foolproof

public:
	class Value;

private:
	// Recursive destruction of value and all its descendants

	void DestroyValue(Value* value)
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
				delete [] value->value.AsString;
			}
		}

		if (value->name != nullptr)
		{
			delete [] value->name;
		}
	}

	static TCHAR* CloneStr(const TCHAR* str)
	{
		size_t len = _tcslen(str);
		TCHAR* clone = new TCHAR[len + 1];
		_tcscpy_s(clone, len + 1, str);
		return clone;
	}

	#pragma region "Serialization Related"

	// Serialize context

	typedef struct _SerializeContext
	{
		uint8_t** ptr;
		size_t* actualSize;
		size_t maxSize;
	} SerializeContext;

	// Base procedure for generating the text.

	static void EmitChar(SerializeContext* ctx, uint8_t val)
	{
		assert(*ctx->actualSize < ctx->maxSize);

		(*ctx->ptr)[0] = val;
		(*ctx->ptr)++;
		(*ctx->actualSize)++;
	}

	static void EmitText(SerializeContext* ctx, const char* text)
	{
		uint8_t* ptr = (uint8_t*)text;
		while (*ptr)
			EmitChar(ctx, *ptr++);
	}

	// Simple TCHAR to UTF-8 Converter + Escaping

	static void EmitCodePoint(SerializeContext* ctx, int cp)
	{
		uint8_t c[4] = { 0 };
		int utf8Size = 0;

		// http://www.zedwood.com/article/cpp-utf8-char-to-codepoint
		if (cp <= 0x7F) { c[0] = cp; utf8Size = 1; }
		else if (cp <= 0x7FF) { c[0] = (cp >> 6) + 192; c[1] = (cp & 63) + 128; utf8Size = 2; }
		else if (0xd800 <= cp && cp <= 0xdfff) { throw "Invalid block of utf8"; }
		else if (cp <= 0xFFFF) { c[0] = (cp >> 12) + 224; c[1] = ((cp >> 6) & 63) + 128; c[2] = (cp & 63) + 128; utf8Size = 3; }
		else if (cp <= 0x10FFFF) { c[0] = (cp >> 18) + 240; c[1] = ((cp >> 12) & 63) + 128; c[2] = ((cp >> 6) & 63) + 128; c[3] = (cp & 63) + 128; utf8Size = 4; }
		else { throw "Unsupported codepoint range"; }

		for (int i = 0; i < utf8Size; i++)
		{
			EmitChar(ctx, c[i]);
		}
	}

	static void EmitTcharString(SerializeContext* ctx, TCHAR * str)
	{
		TCHAR* ptr = str;
		while (*ptr)
		{
			int cp = (int)*ptr;

			// Escaping

			switch (cp)
			{
				case '\"':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, '\"');
					break;
				case '\\': 
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, '\\');
					break;
				case '/': 
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, '/');
					break;
				case '\b':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, 'b');
					break;
				case '\f':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, 'f');
					break;
				case '\n':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, 'n');
					break;
				case '\r':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, 'r');
					break;
				case '\t':
					EmitCodePoint(ctx, '\\');
					EmitCodePoint(ctx, 't');
					break;
				default:
					EmitCodePoint(ctx, cp);
			}

			ptr++;
		}
	}

	// Indentation

	static void Indent(SerializeContext* ctx, int depth)
	{
		while (depth--)
			Json::EmitText(ctx, "  ");
	}

	#pragma endregion "Serialization Related"

	#pragma region "De-Serialization Related"

	// Deserialize 

	typedef struct _DeserializeContext
	{
		uint8_t* ptr;
		size_t offset;
		size_t maxSize;
	} DeserializeContext;

	static bool IsWhiteSpace(uint8_t value)
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

	static bool IsControl(uint8_t value)
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

	// Json Token

	enum class TokenType : char
	{
		Unknown = '?',
		EndOfStream = 'E',
		ObjectStart = '{',
		ObjectEnd = '}',
		ArrayStart = '[',
		ArrayEnd = ']',
		Colon = ':',
		Comma = ',',
		Int = 'I',
		Float = 'F',
		True = 't',
		False = 'f',
		Null = 'N',
		String = 'S',
	};

	class Token
	{
	public:
		TokenType type = TokenType::Unknown;
		union
		{
			uint64_t AsInt;
			float AsFloat;
			bool AsBool;
			TCHAR* AsString = nullptr;
		} value;

		void Dump()
		{
			switch (type)
			{
				case TokenType::EndOfStream:
					std::cout << "EndOfStream" << std::endl;
					break;

				case TokenType::ObjectStart:
					std::cout << "ObjectStart" << std::endl;
					break;
				case TokenType::ObjectEnd:
					std::cout << "ObjectEnd" << std::endl;
					break;

				case TokenType::ArrayStart:
					std::cout << "ArrayStart" << std::endl;
					break;
				case TokenType::ArrayEnd:
					std::cout << "ArrayEnd" << std::endl;
					break;

				case TokenType::Colon:
					std::cout << "Colon" << std::endl;
					break;
				case TokenType::Comma:
					std::cout << "Comma" << std::endl;
					break;

				case TokenType::Int:
					std::cout << "Int: ";
					_tprintf (_T("%I64u"), value.AsInt);
					std::cout << std::endl;
					break;
				case TokenType::Float:
					std::cout << "Float: " << value.AsFloat << std::endl;
					break;

				case TokenType::True:
					std::cout << "True" << std::endl;
					break;
				case TokenType::False:
					std::cout << "False" << std::endl;
					break;
				case TokenType::Null:
					std::cout << "Null" << std::endl;
					break;

				case TokenType::String:
					std::cout << "String: ";
					_tprintf(_T("%s"), value.AsString);
					std::cout << std::endl;
					break;

				default:
					std::cout << "Unknown " << (int)type << std::endl;
					break;
			}
		}
	};

	static bool GetLiteral(DeserializeContext* ctx, Token& token)
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

	static int FetchCodepoint(DeserializeContext* ctx)
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

	static bool GetString(DeserializeContext* ctx, Token& token)
	{
		TCHAR str[MaxStringSize] = { 0, };
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

			str[strSize++] = (TCHAR)cp;
		}

		return false;
	}

	static bool IsAllowed(uint8_t val, char *allowed)
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

	static bool GetFloat(DeserializeContext* ctx, Token& token)
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

	static bool GetInt(DeserializeContext* ctx, Token& token)
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

	static void GetToken(Token & token, DeserializeContext* ctx)
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

public:

	// Json Value

	enum class ValueType : char
	{
		Unknown = '?',
		Object = 'O',
		Array = 'A',
		Int = 'I',
		Float = 'F',
		String = 'S',
		Bool = 'B',
		Null = 'N',
	};

	class Value
	{
		char* CloneName(const char* name)
		{
			if (name == nullptr)		// Name can be null
				return nullptr;

			size_t len = strlen(name);
			char* clone = new char[len + 1];
			strcpy_s(clone, len+1, name);
			return clone;
		}

	public:
		Value* parent = nullptr;
		ValueType type = ValueType::Unknown;
		// Name of object members. 
		// For simplicity, we do not support Non-Ansi Json value names (although full use of Utf-8 is allowed in the values)
		char* name = nullptr;
		union
		{
			float AsFloat;
			uint64_t AsInt;
			TCHAR* AsString = nullptr;	// Only the String value type requires the release of the stored value.
			bool AsBool;
		} value;
		std::list<Value*> children;

		Value() { }

		Value(Value* _parent)
		{
			this->parent = _parent;
		}

		void Serialize(SerializeContext * ctx, int depth)
		{
			TCHAR temp[0x100] = { 0, };

			assert(depth < MaxDepth);

			switch (type)
			{
				case ValueType::Object:
					Indent(ctx, depth);
					Json::EmitText(ctx, "{\r\n");

					for (auto it = children.begin(); it != children.end(); ++it)
					{
						if (it != children.begin())
						{
							Json::EmitText(ctx, ",\r\n");
						}

						Value* child = *it;

						Indent(ctx, depth + 1);
						Json::EmitChar(ctx, '\"');
						Json::EmitText(ctx, child->name);
						Json::EmitChar(ctx, '\"');
						Json::EmitText(ctx, " : ");

						child->Serialize(ctx, depth + 1);
					}

					Indent(ctx, depth);
					Json::EmitText(ctx, "}\r\n");
					break;
				case ValueType::Array:
					Indent(ctx, depth);
					Json::EmitText(ctx, "[ ");

					for (auto it = children.begin(); it != children.end(); ++it)
					{
						if (it != children.begin())
						{
							Json::EmitText(ctx, ", ");
						}

						Value* child = *it;
						child->Serialize(ctx, depth + 1);
					}

					Indent(ctx, depth);
					Json::EmitChar(ctx, ']');
					break;
				case ValueType::Null:
					Json::EmitText(ctx, "null");
					break;
				case ValueType::Bool:
					Json::EmitText(ctx, value.AsBool ? "true" : "false");
					break;
				case ValueType::Int:
					_stprintf_s(temp, _countof(temp) - 1, _T("%I64u"), value.AsInt);
					EmitTcharString(ctx, temp);
					break;
				case ValueType::Float:
					_stprintf_s(temp, _countof(temp) - 1, _T("%.4f"), value.AsFloat);
					EmitTcharString(ctx, temp);
					break;
				case ValueType::String:
					Json::EmitChar(ctx, '\"');
					EmitTcharString(ctx, value.AsString);
					Json::EmitChar(ctx, '\"');
					break;
				default:
					throw "Unknown ValueType";
			}
		}	// Serialize

		void Deserialize(DeserializeContext* ctx)
		{
			Token token;

			switch (type)
			{


			}

			// DEBUG: Dump token stream

			//do
			//{
			//	Json::GetToken(token, ctx);
			//	token.Dump();

			//	if (token.type == TokenType::String && token.value.AsString != nullptr)
			//	{
			//		delete[] token.value.AsString;
			//	}

			//} while (token.type != TokenType::EndOfStream);

		}

		// Dynamic modification

		Value* AddInt(const char* name, int value)
		{
			Value* child = new Value(this);
			child->type = ValueType::Int;
			child->name = CloneName(name);
			child->value.AsInt = value;
			children.push_back(child);
			return child;
		}

		Value* AddFloat(const char* name, float value)
		{
			Value* child = new Value(this);
			child->type = ValueType::Float;
			child->name = CloneName(name);
			child->value.AsFloat = value;
			children.push_back(child);
			return child;
		}

		Value* AddNull(const char* name)
		{
			Value* child = new Value(this);
			child->type = ValueType::Null;
			child->name = CloneName(name);
			children.push_back(child);
			return child;
		}

		Value* AddBool(const char* name, bool value)
		{
			Value* child = new Value(this);
			child->type = ValueType::Bool;
			child->name = CloneName(name);
			child->value.AsBool = value;
			children.push_back(child);
			return child;
		}

		Value* AddString(const char* name, const TCHAR * str)
		{
			Value* child = new Value(this);
			child->type = ValueType::String;
			child->name = CloneName(name);
			child->value.AsString = CloneStr(str);
			children.push_back(child);
			return child;
		}

		Value* AddObject(const char* name)
		{
			Value* child = new Value(this);
			child->type = ValueType::Object;
			child->name = CloneName(name);
			children.push_back(child);
			return child;
		}

		Value* AddArray(const char* name)
		{
			Value* child = new Value(this);
			child->type = ValueType::Array;
			child->name = CloneName(name);
			children.push_back(child);
			return child;
		}

		// Access

		Value* ByName(const char* name)
		{
			for (auto it = children.begin(); it != children.end(); ++it)
			{
				Value* child = *it;

				if (!strcmp(child->name, name))
				{
					return child;
				}
			}
			return nullptr;
		}

		// Debug

		void Dump(int depth = 0)
		{
			for (int i = 0; i < depth; i++)
			{
				std::cout << "    ";
			}

			switch (type)
			{
				case ValueType::Object:
					std::cout << "Object: ";
					for (auto it = children.begin(); it != children.end(); ++it)
					{
						Value* child = *it;
						child->Dump(depth + 1);
					}
					break;
				case ValueType::Array:
					std::cout << "Array: ";
					for (auto it = children.begin(); it != children.end(); ++it)
					{
						Value* child = *it;
						child->Dump(depth + 1);
					}
					break;

				case ValueType::Bool:
					if (name) std::cout << name << ": ";
					std::cout << value.AsBool ? "True" : "False";
					break;
				case ValueType::Null:
					if (name) std::cout << name << ": ";
					std::cout << "Null";
					break;

				case ValueType::Int:
					if (name) std::cout << name << ": ";
					std::cout << "Int: ";
					_tprintf(_T("%I64u"), value.AsInt);
					break;
				case ValueType::Float:
					if (name) std::cout << name << ": ";
					std::cout << "Float: " << (int)value.AsFloat;
					break;

				case ValueType::String:
					if (name) std::cout << name << ": ";
					std::cout << "String: ";
					_tprintf(_T("%s"), value.AsString);
					break;
			}

			std::cout << std::endl;
		}

	};

	// Deserialized root

	Value root;

	// Api

	~Json()
	{
		DestroyValue(&root);
	}

	// The process of serialization is that we recursively walk through all the values, passing in each "running" context 
	// which has the pointer to the generated text and the current text size.

	void Serialize(void* text, size_t maxTextSize, size_t & actualTextSize)
	{
		actualTextSize = 0;

		SerializeContext ctx = { 0 };

		ctx.ptr = (uint8_t **)&text;
		ctx.maxSize = maxTextSize;
		ctx.actualSize = &actualTextSize;

		for (auto it = root.children.begin(); it != root.children.end(); ++it)
		{
			(*it)->Serialize(&ctx, 0);
		}
	}

	void Deserialize(void* text, size_t textSize)
	{
		DeserializeContext ctx = { 0 };
		
		assert(text);

		ctx.ptr = (uint8_t *)text;
		ctx.offset = 0;
		ctx.maxSize = textSize;

		root.AddObject(nullptr)->Deserialize(&ctx);
	}

};
