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
			if (value->value.AsTchar)
			{
				delete [] value->value.AsTchar;
			}
		}

		if (value->name != nullptr)
		{
			delete [] value->name;
		}
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

	static void EmitEscapedString(SerializeContext* ctx, std::string & str)
	{
	}

	static void EmitEscapedWString(SerializeContext* ctx, std::wstring & str)
	{
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

	// Simple UTF-8 to TCHAR Converter + Unescaping

	std::string & EscapedUtf8ToString(void* utf8String, size_t utf8StringSizeInBytes)
	{
		std::string str;

		return str;
	}

	std::wstring & EscapedUtf8ToWString(void* utf8String, size_t utf8StringSizeInBytes)
	{
		std::wstring str;

		return str;
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
		Null = 'Z',
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

		TCHAR* CloneStr(const TCHAR* str)
		{
			size_t len = _tcslen(str);
			TCHAR* clone = new TCHAR[len + 1];
			_tcscpy_s(clone, len+1, str);
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
			TCHAR* AsTchar = nullptr;	// Only the String value type requires the release of the stored value.
			bool AsBool;
		} value;
		std::list<Value*> children;

		void Serialize(SerializeContext * ctx, int depth)
		{
			std::string tempString;
			std::wstring tempWString;

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
					if (sizeof(TCHAR) == sizeof(wchar_t))
					{
						tempWString = std::to_wstring(value.AsInt);
						EmitEscapedWString(ctx, tempWString);
					}
					else
					{
						tempString = std::to_string(value.AsInt);
						EmitEscapedString(ctx, tempString);
					}
					break;
				case ValueType::Float:
					if (sizeof(TCHAR) == sizeof(wchar_t))
					{
						tempWString = std::to_wstring(value.AsFloat);
						EmitEscapedWString(ctx, tempWString);
					}
					else
					{
						tempString = std::to_string(value.AsFloat);
						EmitEscapedString(ctx, tempString);
					}
					break;
				case ValueType::String:
					Json::EmitChar(ctx, '\"');
					if (sizeof(TCHAR) == sizeof(wchar_t))
					{
						tempWString = std::wstring((wchar_t *)value.AsTchar, _tcslen(value.AsTchar));
						EmitEscapedWString(ctx, tempWString);
					}
					else
					{
						tempString = std::string((char*)value.AsTchar, _tcslen(value.AsTchar));
						EmitEscapedString(ctx, tempString);
					}
					Json::EmitChar(ctx, '\"');
					break;
				default:
					throw "Unknown ValueType";
			}
		}	// Serialize

		// Dynamic modification

		Value* AddInt(const char* name, int value)
		{
			Value* child = new Value;
			child->type = ValueType::Int;
			child->name = CloneName(name);
			child->value.AsInt = value;
			children.push_back(child);
			return child;
		}

		Value* AddFloat(const char* name, float value)
		{
			Value* child = new Value;
			child->type = ValueType::Float;
			child->name = CloneName(name);
			child->value.AsFloat = value;
			children.push_back(child);
			return child;
		}

		Value* AddNull(const char* name)
		{
			Value* child = new Value;
			child->type = ValueType::Null;
			child->name = CloneName(name);
			children.push_back(child);
			return child;
		}

		Value* AddBool(const char* name, bool value)
		{
			Value* child = new Value;
			child->type = ValueType::Bool;
			child->name = CloneName(name);
			child->value.AsBool = value;
			children.push_back(child);
			return child;
		}

		Value* AddString(const char* name, const TCHAR * str)
		{
			Value* child = new Value;
			child->type = ValueType::String;
			child->name = CloneName(name);
			child->value.AsTchar = CloneStr(str);
			children.push_back(child);
			return child;
		}

		Value* AddObject(const char* name)
		{
			Value* child = new Value;
			child->type = ValueType::Object;
			child->name = CloneName(name);
			children.push_back(child);
			return child;
		}

		Value* AddArray(const char* name)
		{
			Value* child = new Value;
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
	}

};
