// Local JDI Host
//
// A JDI request is a UTF-8 command line: the command name and every argument are UTF-8, and only
// the ASCII characters ' ', '\'' and '\"' have a meaning to the tokenizer, which is what makes the
// byte-wise scan below safe for it (a UTF-8 continuation byte is never an ASCII character).

#include "pch.h"

#define endl    ( line[p] == 0 )
#define space   ( line[p] == 0x20 )
#define quot    ( line[p] == '\'' )
#define dquot   ( line[p] == '\"' )

// Splits a console command line into arguments. Returns false (with the argument list cleared)
// when the line has an unterminated quotation: this used to throw, and the throw travelled all
// the way out of the emulator loop (nothing between here and the caller catches it), so a single
// mistyped quotation in autoexec.cmd or in a console command killed the process.
static bool Tokenize(const char* line, std::vector<std::string>& args)
{
	int p, start, end;
	p = start = end = 0;

	args.clear();

	// A command line that was read from a file may still carry the UTF-8 byte order mark; without
	// this the mark becomes part of the first argument and the command it names is not found.
	if ((uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF)
	{
		p = 3;
	}

	// while not end line
	while (!endl)
	{
		// skip space first, if any
		while (space) p++;
		if (!endl && quot)
		{   // single quotation, need special case
			p++;
			start = p;
			while (1)
			{
				if (endl)
				{
					args.clear();
					return false;
				}

				if (quot)
				{
					end = p;
					p++;
					break;
				}
				else p++;
			}

			args.push_back(std::string(line + start, end - start));
		}
		else if (!endl && dquot)
		{   // double quotation, need special case
			p++;
			start = p;
			while (1)
			{
				if (endl)
				{
					args.clear();
					return false;
				}

				if (dquot)
				{
					end = p;
					p++;
					break;
				}
				else p++;
			}

			args.push_back(std::string(line + start, end - start));
		}
		else if (!endl)
		{
			start = p;
			while (1)
			{
				if (endl || space || quot || dquot)
				{
					end = p;
					break;
				}

				p++;
			}

			args.push_back(std::string(line + start, end - start));
		}
	}

	return true;
}

#undef space
#undef quot
#undef dquot
#undef endl

Json::Value* 
CallJdi(const char* request)
{
	std::vector<std::string> args;

	if (!Tokenize(request, args))
	{
		Debug::Report(Debug::Channel::Error, "Open quotation in command: %s\n", request);
		return nullptr;
	}

	return JDI::Hub.Execute(args);
}

bool 
CallJdiNoReturn(const char* request)
{
	CallJdi(request);
	return true;
}

bool 
CallJdiReturnInt(const char* request, int* valueOut)
{
	if (!valueOut)
	{
		return false;
	}

	Json::Value* value = CallJdi(request);
	if (!value)
	{
		return false;
	}
	if (value->type != Json::ValueType::Int)
	{
		delete value;
		return false;
	}

	*valueOut = (int)value->value.AsInt;
	delete value;

	return true;
}

bool 
CallJdiReturnString(const char* request, char* valueOut, size_t valueSize)
{
	if (!valueOut)
	{
		return false;
	}

	Json::Value* value = CallJdi(request);
	if (!value)
	{
		return false;
	}

	// String are returned in form of: [ "string" ]

	if (value->type != Json::ValueType::Array)
	{
		delete value;
		return false;
	}

	if (value->children.size() < 1)
	{
		delete value;
		return false;
	}

	Json::Value* child = value->children.front();

	if (child->type != Json::ValueType::String )
	{
		delete value;
		return false;
	}

	// The answer is handed over as UTF-8, so a code point outside the ASCII range keeps its
	// meaning (a wide character used to be truncated to its low byte here).

	std::string utf8 = Util::WstringToString(child->value.AsString);
	size_t sizeInBytes = utf8.size() + 1;

	// The callers hand over `sizeof(buffer) - 1`, so `valueSize` is the room left for the text
	// itself and the terminator still has to fit next to it.
	if (sizeInBytes > (valueSize + 1))
	{
		delete value;
		return false;
	}

	memcpy(valueOut, utf8.c_str(), sizeInBytes);

	delete value;

	return true;
}

bool 
CallJdiReturnBool(const char* request, bool* valueOut)
{
	if (!valueOut)
	{
		return false;
	}

	Json::Value* value = CallJdi(request);
	if (!value)
	{
		return false;
	}
	if ( ! (value->type == Json::ValueType::Int || value->type == Json::ValueType::Bool) )
	{
		delete value;
		return false;
	}

	if (value->type == Json::ValueType::Bool)
	{
		*valueOut = value->value.AsBool;
	}
	else
	{
		*valueOut = value->value.AsInt != 0;
	}

	delete value;

	return true;
}

void 
JdiAddNode(const char* filename, const char* jsonText, JDI::JdiReflector reflector)
{
	JDI::Hub.AddNode(Util::StringToWstring(filename), jsonText, reflector);
}

void 
JdiRemoveNode(const char* filename)
{
	JDI::Hub.RemoveNode(Util::StringToWstring(filename));
}

void 
JdiAddCmd(const char* name, JDI::CmdDelegate command)
{
	JDI::Hub.AddCmd(name, command);
}

void
CallJdiReturnJson(const char* request, char * reply, size_t replySize)
{
	std::vector<std::string> args;

	if (!Tokenize(request, args))
	{
		Debug::Report(Debug::Channel::Error, "Open quotation in command: %s\n", request);
		reply[0] = '{';
		reply[1] = '}';
		reply[2] = 0;
		return;
	}

	Json::Value *value = JDI::Hub.Execute(args);

	// The command may not exist or may have rejected its arguments; the caller expects a reply.
	if (value == nullptr)
	{
		reply[0] = '{';
		reply[1] = '}';
		reply[2] = 0;
		return;
	}

	Json json;

	Json::Value * rootObj = json.root.AddObject(nullptr);
	value->SetName("result");
	rootObj->children.push_back(value);
	value->parent = rootObj;

	size_t actualSize = 0;
	json.GetSerializedTextSize(reply, -1, actualSize);

	if (actualSize >= (replySize - 1))
	{
		Debug::Report(Debug::Channel::Error, "Not enough space to serialize result! (actualSize: %i)\n", actualSize);
		reply[0] = '{';
		reply[1] = '}';
		reply[2] = 0;
		return;
	}

	json.Serialize(reply, actualSize + 1, actualSize);
	reply[actualSize] = 0;
}
