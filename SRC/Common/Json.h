// Json serialization engine. Json is used to store emulator settings, as well as for the JDI system (Json Debug Interface).

// All errors during deserialization are thrown by exceptions.
// During serializing, in theory, there shouldn't be any mistakes (if we are not enemies to ourselves and have not changed, for example, the Value type to some non-standard one).

#pragma once

class Json
{
	// Foolproof
	static const int MaxDepth = 255;
	static const int MaxStringSize = 0x1000;
	static const int MaxElements = 255;				// Deserialize only

public:
	class Value;

private:

	// Recursive destruction of value and all its descendants
	void DestroyValue(Value* value);

	static wchar_t* CloneStr(const wchar_t* str);
	static wchar_t* CloneAnsiStr(const char* str);

	#pragma region "Serialization Related"

	// Serialize context

	struct SerializeContext
	{
		uint8_t** ptr;
		size_t* actualSize;
		size_t maxSize;
	};

	// Base procedure for generating the text.
	static void EmitChar(SerializeContext* ctx, uint8_t val, bool sizeOnly);

	static void EmitText(SerializeContext* ctx, const char* text, bool sizeOnly);

	// Simple wchar_t to UTF-8 Converter + Escaping
	static void EmitCodePoint(SerializeContext* ctx, int cp, bool sizeOnly);
	static void EmitWcharString(SerializeContext* ctx, wchar_t* str, bool sizeOnly);

	// Indentation

	static void Indent(SerializeContext* ctx, int depth, bool sizeOnly);

	#pragma endregion "Serialization Related"

	#pragma region "De-Serialization Related"

	// Deserialize 

	struct DeserializeContext
	{
		uint8_t* ptr;
		size_t offset;
		size_t maxSize;
	};

	static bool IsWhiteSpace(uint8_t value);
	static bool IsControl(uint8_t value);

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
			wchar_t* AsString;
		} value;
		Token()
		{
			value.AsString = nullptr;
		}
	};

	static bool GetLiteral(DeserializeContext* ctx, Token& token);
	static int FetchCodepoint(DeserializeContext* ctx);
	static bool GetString(DeserializeContext* ctx, Token& token);
	static bool IsAllowed(uint8_t val, char* allowed);
	static bool GetFloat(DeserializeContext* ctx, Token& token);
	static bool GetInt(DeserializeContext* ctx, Token& token);

	static void GetToken(Token& token, DeserializeContext* ctx);

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
		char* CloneName(const char* otherName);
		char* CloneWcharName(const wchar_t* otherName);
		void DeserializeObject(DeserializeContext* ctx);
		void DeserializeArray(DeserializeContext* ctx);

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
			uint8_t AsUint8;
			uint16_t AsUint16;
			uint32_t AsUint32;
			wchar_t* AsString;
			bool AsBool;
		} value;
		std::list<Value*> children;

		Value()
		{
			// Only the String value type requires the release of the stored value.
			value.AsString = nullptr;
		}

		Value(Value* _parent)
		{
			// Only the String value type requires the release of the stored value.
			value.AsString = nullptr;
			this->parent = _parent;
		}

		~Value()
		{
			while (!children.empty())
			{
				Value *child = children.back();
				children.pop_back();
				delete child;
			}

			if (type == ValueType::String)
			{
				if (value.AsString)
				{
					delete[] value.AsString;
					value.AsString = nullptr;
				}
			}

			if (name != nullptr)
			{
				delete[] name;
				name = nullptr;
			}
		}

		void Serialize(SerializeContext* ctx, int depth, bool sizeOnly);
		void Deserialize(DeserializeContext* ctx, wchar_t* keyName);

		// Dynamic modification

		Value* AddInt(const char* keyName, int _value);
		Value* AddUInt16(const char* keyName, uint16_t _value);
		Value* AddUInt32(const char* keyName, uint32_t _value);
		Value* AddUInt64(const char* keyName, uint64_t _value);
		Value* AddFloat(const char* keyName, float _value);
		Value* AddNull(const char* keyName);
		Value* AddBool(const char* keyName, bool _value);
		Value* AddString(const char* keyName, const wchar_t* str);
		Value* AddAnsiString(const char* keyName, const char* str);
		Value* ReplaceString(const wchar_t* str);
		Value* AddObject(const char* keyName);
		Value* AddArray(const char* keyName);
		Value* AddValue(const char* keyName, Value* value);
		Value* Add(Value* _parent, Value* other);
		Value* Replace(Value* _parent, Value* other);

		// Access

		Value* ByName(const char* byName);
		Value* ByType(const ValueType byType);

		void SetName(const char* newName)
		{
			if (name != nullptr)
			{
				delete[] name;
			}
			name = CloneName(newName);
		}
	};

	/// <summary>
	/// Deserialized root.
	/// </summary>
	Value root;

	// Api

	~Json()
	{
		DestroyValue(&root);
	}

	// The process of serialization is that we recursively walk through all the values, passing in each "running" context 
	// which has the pointer to the generated text and the current text size.

	void Serialize(void* text, size_t maxTextSize, size_t& actualTextSize);
	void GetSerializedTextSize(void* text, size_t maxTextSize, size_t& actualTextSize);
	void Deserialize(void* text, size_t textSize);

	// Clone

	void Clone(Json* other);

	// Merge

	void Merge(Json* other);

};
