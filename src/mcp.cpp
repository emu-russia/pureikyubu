// MCP server (issue #383)
//
// The emulator's whole debug interface (the JDI) is published as MCP tools, and the protocol is a
// JDI command of its own (`McpRequest`), so that the server can be driven, tested and debugged
// exactly like any other part of the debug interface. See the note at the top of mcp.h and
// wiki/mcp.md.
//
// The protocol is JSON-RPC 2.0, which is what the emulator's own Json module already speaks. The
// two things that need care are the ones the specification is strict about:
//
//   * the stdio transport frames **one message per line**, while the emulator's Json serializer is
//     pretty-printed for the settings files and the debugger; an answer is therefore folded into a
//     single line before it leaves the server (`FoldToSingleLine`);
//   * a notification (a message without an "id") is never answered, and an invalid request is
//     answered with the error the specification assigns to it instead of being dropped.

#include "pch.h"

#include <algorithm>
#include <set>
#include <thread>

using namespace Debug;

namespace Mcp
{
	// ---------------------------------------------------------------------------------------
	// The protocol revisions this server speaks
	// ---------------------------------------------------------------------------------------

	// The revisions of the MCP specification this server was written against. A client that asks
	// for another one is answered with the newest revision here, which is what the specification
	// asks a server to do (the client then decides whether it can work with the answer).
	static const char* ProtocolVersions[] =
	{
		"2024-11-05",
		"2025-03-26",
		"2025-06-18",
	};

	static const char* LatestProtocolVersion = "2025-06-18";

	// JSON-RPC 2.0 error codes.
	enum ErrorCode
	{
		ParseError = -32700,
		InvalidRequest = -32600,
		MethodNotFound = -32601,
		InvalidParams = -32602,
		InternalError = -32603,
	};

	// The largest answer of a tool call that is handed to a client. A command that answers with a
	// whole binary file (FileLoad turns every byte into a Json value) would otherwise push tens of
	// megabytes through the transport and into the context of the model.
	static const size_t MaxAnswerSize = 4 * 1024 * 1024;

	// What the model of the client is told about this server once, at the handshake.
	static const char* Instructions =
		"pureikyubu is a Nintendo GameCube emulator, and this is its debug interface. The tools are "
		"the emulator's own JDI commands: emulator control (load, reset, run, stop), the Gekko and "
		"DSP debugger (registers, breakpoints, memory, disassembly), the Flipper hardware (DVD, GFX, "
		"the profiler) and the user interface. The arguments of a tool are the arguments of the "
		"command line of that command, in the order its parameters are described. The emulator is "
		"idle until an image is loaded with the `load` tool. A command that answers nothing writes "
		"into the emulator's debug message queue instead - the `qd` tool reads it (and clears it).";

	// ---------------------------------------------------------------------------------------
	// Small helpers over the Json tree
	// ---------------------------------------------------------------------------------------

	static Json::Value* Member(Json::Value* object, const char* name)
	{
		if (object == nullptr || object->type != Json::ValueType::Object)
		{
			return nullptr;
		}

		return object->ByName(name);
	}

	// The text of a string member of a JDI specification. Member names and values are UTF-8 in the
	// documents, and wide in the parsed tree.
	static std::string SpecString(Json::Value* spec, const char* member)
	{
		Json::Value* value = Member(spec, member);
		if (value == nullptr || value->type != Json::ValueType::String)
		{
			return std::string();
		}

		return Util::WstringToString(value->value.AsString);
	}

	// A scalar argument as the token it becomes on the command line. The specification types every
	// parameter as a string, but a client that sends a number or a boolean is understood as well -
	// it means the same thing to a command.
	static bool ScalarToUtf8(Json::Value* value, std::string& text)
	{
		char buffer[64] = { 0, };

		switch (value->type)
		{
			case Json::ValueType::String:
				text = Util::WstringToString(value->value.AsString);
				return true;

			case Json::ValueType::Int:
				snprintf(buffer, sizeof(buffer), "%lld", (long long)value->value.AsInt);
				text = buffer;
				return true;

			case Json::ValueType::Float:
				snprintf(buffer, sizeof(buffer), "%g", value->value.AsFloat);
				text = buffer;
				return true;

			case Json::ValueType::Bool:
				text = value->value.AsBool ? "1" : "0";
				return true;

			case Json::ValueType::Null:
				text.clear();
				return true;
		}

		return false;
	}

	// The emulator's serializer writes CRLF line breaks and indentation, because its output is a
	// file or a debugger window. A message of the stdio transport has to be one line: what is
	// removed here is only the serializer's own formatting (a line break that is part of a value
	// is escaped as "\n" by the serializer, so it is not touched).
	static void FoldToSingleLine(std::string& text)
	{
		std::string folded;
		folded.reserve(text.size());

		for (size_t i = 0; i < text.size(); i++)
		{
			char c = text[i];

			if (c == '\r' || c == '\n')
			{
				while (i + 1 < text.size() && (text[i + 1] == ' ' || text[i + 1] == '\t'))
				{
					i++;
				}
				continue;
			}

			folded.push_back(c);
		}

		text.swap(folded);
	}

	// The size of the text a Json tree serializes into. The serializer writes the text with CRLF
	// line breaks, which is why the size and the text are two passes over the same tree.
	static size_t SerializedSize(Json& json)
	{
		size_t size = 0;
		json.GetSerializedTextSize(nullptr, (size_t)-1, size);
		return size;
	}

	// Serialize a Json tree as UTF-8 text, into room that was measured with `SerializedSize`.
	static void SerializeToText(Json& json, size_t size, std::string& text)
	{
		text.clear();

		if (size == 0)
		{
			return;
		}

		text.resize(size);

		size_t actualSize = 0;
		json.Serialize(&text[0], text.size(), actualSize);
	}

	static std::string FinishMessage(Json& json)
	{
		std::string text;
		SerializeToText(json, SerializedSize(json), text);
		FoldToSingleLine(text);
		return text;
	}

	// The "id" of a request is echoed exactly as it came: a number, a string or null. It is not
	// copied from the request tree (the request is destroyed together with its answer), it is
	// rebuilt from the value it holds.
	static void AddId(Json::Value* root, Json::Value* id)
	{
		if (id == nullptr)
		{
			root->AddNull("id");
			return;
		}

		switch (id->type)
		{
			case Json::ValueType::String:
				root->AddString("id", id->value.AsString);
				break;

			case Json::ValueType::Int:
				root->AddUInt64("id", id->value.AsInt);
				break;

			case Json::ValueType::Float:
				root->AddFloat("id", id->value.AsFloat);
				break;

			default:
				root->AddNull("id");
				break;
		}
	}

	// The error answer of the protocol. The emulator's Json module has no signed integers at all
	// (its documents never carry one, and the parser refuses a "-"), while every error code of
	// JSON-RPC 2.0 is negative. The code therefore goes into the tree as the number it is stored
	// as, and the digits the serializer wrote for it are folded back into their signed form - a
	// client that parses the answer gets the number the specification assigns to the error.
	static std::string ErrorMessage(Json::Value* id, int code, const std::string& message)
	{
		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		root->AddUtf8String("jsonrpc", "2.0");
		AddId(root, id);

		Json::Value* error = root->AddObject("error");
		error->AddUInt64("code", (uint64_t)(int64_t)code);
		error->AddUtf8String("message", message.c_str());

		std::string text = FinishMessage(json);

		// The member is written as `"code" : <digits>`; only the digits are replaced, so the
		// spelling of the number does not matter here (the serializer prints an integer with a
		// format of its own).
		size_t at = text.find("\"code\"");
		if (at != std::string::npos)
		{
			at = text.find(':', at);
			if (at != std::string::npos)
			{
				at++;
				while (at < text.size() && text[at] == ' ')
				{
					at++;
				}

				size_t end = at;
				while (end < text.size() && text[end] >= '0' && text[end] <= '9')
				{
					end++;
				}

				// Only a run of digits is replaced; a value that is not a number is left alone
				// rather than turned into something that is not an answer any more.
				if (end > at)
				{
					text.replace(at, end - at, std::to_string(code));
				}
			}
		}

		return text;
	}

	// The result of a tool call. `text` is what the client shows the model.
	static std::string ToolResult(Json::Value* id, const std::string& text, bool isError)
	{
		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		root->AddUtf8String("jsonrpc", "2.0");
		AddId(root, id);

		Json::Value* result = root->AddObject("result");
		Json::Value* content = result->AddArray("content");

		Json::Value* item = content->AddObject(nullptr);
		item->AddUtf8String("type", "text");
		item->AddUtf8String("text", text.c_str());

		result->AddBool("isError", isError);

		return FinishMessage(json);
	}

	// ---------------------------------------------------------------------------------------
	// The commands of the debug interface as MCP tools
	// ---------------------------------------------------------------------------------------

	// A parameter of a tool, as the "hints" of the command's specification spell it out.
	struct ToolParam
	{
		std::string name;					// The member name the client sends
		bool required = false;				// `<addr>` is required, `[addr]` is not
		std::string hint;					// The hint itself, for the description
		std::vector<std::string> values;	// The alternatives of `[a|b|c]`
	};

	// The name of a parameter: the leading part of the hint that can be a Json member name. A hint
	// may carry a range or a file extension after the name (`map 0-7`, `filename.png`); the rest
	// stays in the description of the parameter.
	static std::string ParamName(const std::string& hint)
	{
		std::string name;

		for (char c : hint)
		{
			bool identifier = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_';

			if (!identifier)
			{
				break;
			}

			name.push_back(c);
		}

		// A name cannot be empty, cannot start with a digit and cannot be the name of the fallback
		// list of the whole command line.
		if (name.empty() || (name[0] >= '0' && name[0] <= '9') || name == "args")
		{
			name = "arg";
		}

		return name;
	}

	// Read the parameters of a command from its hints, in the order they appear on the command
	// line. The same spelling the debugger console prints: `<name>` is required, `[name]` is
	// optional, and `[a|b|c]` is a choice between the listed values.
	static std::vector<ToolParam> ParseParams(Json::Value* spec)
	{
		std::vector<ToolParam> params;

		std::string hints = SpecString(spec, "hints");

		size_t offset = 0;

		while (offset < hints.size())
		{
			char open = hints[offset];

			if (open != '<' && open != '[')
			{
				offset++;
				continue;
			}

			size_t end = hints.find(open == '<' ? '>' : ']', offset + 1);
			if (end == std::string::npos)
			{
				break;
			}

			ToolParam param;
			param.required = (open == '<');
			param.hint = hints.substr(offset + 1, end - offset - 1);

			if (param.hint.find('|') != std::string::npos)
			{
				// A list of alternatives describes a choice, not a name: the parameter is called
				// `value` and the alternatives become its enumeration.
				size_t start = 0;

				while (start <= param.hint.size())
				{
					size_t bar = param.hint.find('|', start);
					std::string value = param.hint.substr(start, (bar == std::string::npos) ?
						std::string::npos : bar - start);

					if (!value.empty())
					{
						param.values.push_back(value);
					}

					if (bar == std::string::npos)
					{
						break;
					}

					start = bar + 1;
				}

				param.name = "value";
			}
			else
			{
				param.name = ParamName(param.hint);
			}

			// A Json object cannot have two members with the same name, and a command may name the
			// same parameter twice.
			std::string base = param.name;
			int counter = 2;
			for (;;)
			{
				bool taken = false;

				for (auto& existing : params)
				{
					if (existing.name == param.name)
					{
						taken = true;
						break;
					}
				}

				if (!taken)
				{
					break;
				}

				param.name = base + "_" + std::to_string(counter++);
			}

			params.push_back(param);
			offset = end + 1;
		}

		return params;
	}

	// Whether the specification publishes the command as a tool. The commands that end the process
	// the server itself runs in (`exit` and its aliases) and the MCP commands themselves carry
	// `"mcp": false`: as tools the first would kill the server in the middle of a call and the
	// second would only call the server back into itself.
	static bool IsTool(Json::Value* spec)
	{
		Json::Value* mcp = Member(spec, "mcp");

		if (mcp != nullptr && mcp->type == Json::ValueType::Bool && !mcp->value.AsBool)
		{
			return false;
		}

		return true;
	}

	// The text a tool call answers with when the command failed. The description of the command is
	// the usage the console would have printed.
	static std::string BuildDescription(Json::Value* spec);

	static std::string ToolFailure(Json::Value* id, Json::Value* spec, const std::string& reason)
	{
		std::string text = reason;

		if (spec != nullptr)
		{
			text += "\n\n";
			text += BuildDescription(spec);
		}

		return ToolResult(id, text, true);
	}

	static std::string BuildDescription(Json::Value* spec)
	{
		std::string description;

		std::string help = SpecString(spec, "help");
		if (!help.empty())
		{
			description = help;
		}

		std::string info = SpecString(spec, "info");
		if (!info.empty())
		{
			if (!description.empty())
			{
				description += "\n\n";
			}
			description += info;
		}

		Json::Value* usage = Member(spec, "usage");
		if (usage != nullptr && usage->type == Json::ValueType::Array)
		{
			std::string syntax;

			for (auto it = usage->children.begin(); it != usage->children.end(); ++it)
			{
				Json::Value* line = *it;
				if (line->type == Json::ValueType::String)
				{
					syntax += Util::WstringToString(line->value.AsString);
					if (!syntax.empty() && syntax.back() != '\n')
					{
						syntax += "\n";
					}
				}
			}

			if (!syntax.empty())
			{
				if (!description.empty())
				{
					description += "\n\n";
				}
				description += syntax;
			}
		}

		std::string output = SpecString(spec, "output");
		if (output.empty())
		{
			output = "none (the command reports into the debug message queue; the qd tool reads it)";
		}

		if (!description.empty())
		{
			description += "\n\n";
		}
		description += "Output: " + output;

		return description;
	}

	static void AddTool(Json::Value* tools, const char* name, Json::Value* spec)
	{
		Json::Value* tool = tools->AddObject(nullptr);
		tool->AddUtf8String("name", name);

		std::string description = BuildDescription(spec);
		tool->AddUtf8String("description", description.c_str());

		Json::Value* schema = tool->AddObject("inputSchema");
		schema->AddUtf8String("type", "object");

		Json::Value* properties = schema->AddObject("properties");
		Json::Value* required = schema->AddArray("required");

		for (auto& param : ParseParams(spec))
		{
			Json::Value* property = properties->AddObject(param.name.c_str());
			property->AddUtf8String("type", "string");

			if (!param.values.empty())
			{
				Json::Value* values = property->AddArray("enum");
				for (auto& value : param.values)
				{
					values->AddUtf8String(nullptr, value.c_str());
				}
			}

			property->AddUtf8String("description", param.hint.c_str());

			if (param.required)
			{
				required->AddUtf8String(nullptr, param.name.c_str());
			}
		}

		// The whole argument list as one value: it is what a command without hints takes (the
		// parameters of those are not named anywhere), and it is the way a client sends the
		// arguments the hints do not cover.
		Json::Value* args = properties->AddObject("args");
		args->AddUtf8String("type", "array");
		Json::Value* items = args->AddObject("items");
		items->AddUtf8String("type", "string");
		args->AddUtf8String("description",
			"The arguments of the command line as a list of tokens, without the command name. "
			"Use it instead of the named parameters (sending both is refused).");

		schema->AddBool("additionalProperties", false);
	}

	// The minimum number of arguments the specification of a command declares. It is the check
	// `JdiHub::Execute` makes before it calls the command, which is what tells "the command refused
	// the arguments" from "the command answered nothing" (a command that only prints is one that
	// answered nothing, and there are many of those).
	static size_t MinimumArgs(Json::Value* spec)
	{
		Json::Value* args = Member(spec, "args");
		if (args == nullptr || args->type != Json::ValueType::Int)
		{
			return 0;
		}

		return (size_t)args->value.AsInt;
	}

	// The specification of a command that is published as a tool, or nullptr.
	static Json::Value* FindTool(const std::string& name)
	{
		std::vector<JDI::JdiHub::CommandInfo> commands;
		JDI::Hub.EnumCommands(commands);

		for (auto& command : commands)
		{
			if (name != command.name)
			{
				continue;
			}

			return IsTool(command.spec) ? command.spec : nullptr;
		}

		return nullptr;
	}

	// Turn the arguments of a tool call into the command line of the command. The client either
	// names the parameters the hints describe, or hands the whole list of tokens over as `args`;
	// both end up as the same positional list, because that is what a JDI command line is.
	static bool BuildArgs(const std::string& toolName, Json::Value* spec, Json::Value* arguments,
		std::vector<std::string>& args, std::string& problem)
	{
		args.clear();
		args.push_back(toolName);

		if (arguments != nullptr && arguments->type != Json::ValueType::Object)
		{
			problem = "\"arguments\" must be an object";
			return false;
		}

		Json::Value* list = Member(arguments, "args");

		if (list != nullptr)
		{
			if (list->type != Json::ValueType::Array)
			{
				problem = "\"args\" must be an array of strings";
				return false;
			}

			if (arguments->children.size() > 1)
			{
				problem = "send either \"args\" or the named parameters, not both";
				return false;
			}

			for (auto it = list->children.begin(); it != list->children.end(); ++it)
			{
				std::string token;
				if (!ScalarToUtf8(*it, token))
				{
					problem = "every element of \"args\" must be a string";
					return false;
				}
				args.push_back(token);
			}

			return true;
		}

		std::vector<ToolParam> params = ParseParams(spec);

		// A command line is positional, so an optional argument can only be left out at the end:
		// a hole in the middle would shift every argument after it onto the wrong parameter.
		bool hole = false;
		std::string holeName;

		for (auto& param : params)
		{
			Json::Value* value = Member(arguments, param.name.c_str());

			if (value != nullptr)
			{
				if (hole)
				{
					problem = "the argument \"" + param.name + "\" is given while \"" + holeName +
						"\" is not: send the whole command line as \"args\" instead";
					return false;
				}

				std::string token;
				if (!ScalarToUtf8(value, token))
				{
					problem = "the argument \"" + param.name + "\" must be a string";
					return false;
				}

				args.push_back(token);
				continue;
			}

			if (param.required)
			{
				problem = "the required argument \"" + param.name + "\" is missing";
				return false;
			}

			if (!hole)
			{
				hole = true;
				holeName = param.name;
			}
		}

		if (arguments != nullptr)
		{
			for (auto it = arguments->children.begin(); it != arguments->children.end(); ++it)
			{
				Json::Value* member = *it;
				if (member->name == nullptr || member->name == std::string("args"))
				{
					continue;
				}

				bool known = false;
				for (auto& param : params)
				{
					if (member->name == param.name)
					{
						known = true;
						break;
					}
				}

				if (!known)
				{
					problem = std::string("the command has no argument \"") + member->name + "\"";
					return false;
				}
			}
		}

		return true;
	}

	// The answer of a command as the text of a tool result. The value is released here (the caller
	// hands over its ownership). A runaway answer is refused instead of being pushed through the
	// transport.
	static bool AnswerToText(Json::Value* answer, std::string& text)
	{
		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		// The answer is wrapped the way `CallJdiReturnJson` wraps it (jdiserver.cpp): the member
		// name is what a client of the debug interface sees around the value.
		answer->SetName("result");
		root->children.push_back(answer);
		answer->parent = root;

		size_t size = SerializedSize(json);

		if (size > MaxAnswerSize)
		{
			return false;	// `json` owns the answer and releases it
		}

		SerializeToText(json, size, text);

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The protocol
	// ---------------------------------------------------------------------------------------

	static std::string Initialize(Json::Value* id, Json::Value* params)
	{
		// The revision the client asks for, when this server knows it; otherwise the newest one it
		// speaks (the client decides what to do with that).
		std::string requested;

		Json::Value* protocolVersion = Member(params, "protocolVersion");
		if (protocolVersion != nullptr && protocolVersion->type == Json::ValueType::String)
		{
			requested = Util::WstringToString(protocolVersion->value.AsString);
		}

		const char* version = LatestProtocolVersion;

		for (const char* known : ProtocolVersions)
		{
			if (requested == known)
			{
				version = known;
				break;
			}
		}

		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		root->AddUtf8String("jsonrpc", "2.0");
		AddId(root, id);

		Json::Value* result = root->AddObject("result");
		result->AddUtf8String("protocolVersion", version);

		Json::Value* capabilities = result->AddObject("capabilities");
		Json::Value* tools = capabilities->AddObject("tools");
		tools->AddBool("listChanged", false);

		Json::Value* serverInfo = result->AddObject("serverInfo");
		serverInfo->AddUtf8String("name", "pureikyubu");
		serverInfo->AddUtf8String("title", "pureikyubu, the Nintendo GameCube emulator");
		serverInfo->AddUtf8String("version", Util::WstringToString(EMU_VERSION).c_str());

		result->AddUtf8String("instructions", Instructions);

		return FinishMessage(json);
	}

	static std::string EmptyResult(Json::Value* id)
	{
		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		root->AddUtf8String("jsonrpc", "2.0");
		AddId(root, id);
		root->AddObject("result");

		return FinishMessage(json);
	}

	static std::string ToolsList(Json::Value* id)
	{
		std::vector<JDI::JdiHub::CommandInfo> commands;
		JDI::Hub.EnumCommands(commands);

		// The nodes are registered in a hash map, so the order they are enumerated in is arbitrary;
		// the tool list is sorted to make it stable from one run to the next.
		std::sort(commands.begin(), commands.end(),
			[](const JDI::JdiHub::CommandInfo& left, const JDI::JdiHub::CommandInfo& right)
			{
				return strcmp(left.name, right.name) < 0;
			});

		Json json;
		Json::Value* root = json.root.AddObject(nullptr);

		root->AddUtf8String("jsonrpc", "2.0");
		AddId(root, id);

		Json::Value* result = root->AddObject("result");
		Json::Value* tools = result->AddArray("tools");

		std::set<std::string> published;

		for (auto& command : commands)
		{
			if (!IsTool(command.spec))
			{
				continue;
			}

			// One name, one tool: a client calls a tool by name, and two commands of the same name
			// (from two nodes) would be indistinguishable.
			if (!published.insert(command.name).second)
			{
				continue;
			}

			AddTool(tools, command.name, command.spec);
		}

		return FinishMessage(json);
	}

	static std::string ToolsCall(Json::Value* id, Json::Value* params)
	{
		Json::Value* name = Member(params, "name");

		if (name == nullptr || name->type != Json::ValueType::String)
		{
			return ErrorMessage(id, InvalidParams, "the tool call has no \"name\"");
		}

		std::string toolName = Util::WstringToString(name->value.AsString);

		Json::Value* spec = FindTool(toolName);
		if (spec == nullptr)
		{
			return ErrorMessage(id, InvalidParams, "unknown tool: " + toolName);
		}

		std::vector<std::string> args;
		std::string problem;

		if (!BuildArgs(toolName, spec, Member(params, "arguments"), args, problem))
		{
			return ToolFailure(id, spec, problem);
		}

		Json::Value* answer = nullptr;

		// A command that fails (and the emulator has many of those - a register that does not
		// exist, a machine that is not running yet) is an error of the *tool*, not of the protocol:
		// the client is told what went wrong and keeps the session.
		try
		{
			answer = JDI::Hub.Execute(args);
		}
		catch (const char* text)
		{
			return ToolFailure(id, spec, std::string("the command failed: ") + ((text != nullptr) ? text : "(no message)"));
		}
		catch (const std::exception& e)
		{
			return ToolFailure(id, spec, std::string("the command failed: ") + e.what());
		}
		catch (...)
		{
			return ToolFailure(id, spec, "the command failed with an unknown error");
		}

		if (answer == nullptr)
		{
			// Every command of the debug interface that only reports into the debug message queue
			// (the console's "output", which is what most of the debug commands have) answers
			// nothing at all, so an empty answer is not a failure by itself.
			if ((args.size() - 1) >= MinimumArgs(spec))
			{
				return ToolResult(id,
					"The command answered nothing. Its report, if it wrote one, is in the debug message "
					"queue - the `qd` tool reads it (and clears it).",
					false);
			}

			return ToolFailure(id, spec, "the command needs more arguments");
		}

		std::string text;

		if (!AnswerToText(answer, text))
		{
			return ToolFailure(id, spec, "the answer of the command is too large to be returned to a client");
		}

		return ToolResult(id, text, false);
	}

	std::string HandleRequest(const char* text)
	{
		if (text == nullptr)
		{
			return ErrorMessage(nullptr, ParseError, "the message is empty");
		}

		// A message that was written to a file by a tool on Windows may still carry the UTF-8 byte
		// order mark, which is not part of the document (the JDI tokenizer skips it for the same
		// reason).
		if (strlen(text) >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB && (uint8_t)text[2] == 0xBF)
		{
			text += 3;
		}

		Json request;

		try
		{
			request.Deserialize((void*)text, strlen(text));
		}
		catch (...)
		{
			// The id of the message is unknown then, and the protocol asks for a null one.
			return ErrorMessage(nullptr, ParseError, "the message is not a Json object");
		}

		Json::Value* root = request.root.children.back();
		Json::Value* id = Member(root, "id");

		// A message without an "id" is a notification: the protocol forbids answering one, whatever
		// it says and whatever happens while it is handled.
		if (id == nullptr)
		{
			return std::string();
		}

		std::string version;
		Json::Value* jsonrpc = Member(root, "jsonrpc");
		if (jsonrpc != nullptr && jsonrpc->type == Json::ValueType::String)
		{
			version = Util::WstringToString(jsonrpc->value.AsString);
		}

		if (version != "2.0")
		{
			return ErrorMessage(id, InvalidRequest, "\"jsonrpc\" must be \"2.0\"");
		}

		Json::Value* method = Member(root, "method");
		if (method == nullptr || method->type != Json::ValueType::String)
		{
			return ErrorMessage(id, InvalidRequest, "the message has no \"method\"");
		}

		std::string methodName = Util::WstringToString(method->value.AsString);
		Json::Value* params = Member(root, "params");

		try
		{
			if (methodName == "initialize")
			{
				return Initialize(id, params);
			}

			if (methodName == "ping")
			{
				return EmptyResult(id);
			}

			if (methodName == "tools/list")
			{
				return ToolsList(id);
			}

			if (methodName == "tools/call")
			{
				return ToolsCall(id, params);
			}

			// The notifications of the protocol ("notifications/initialized" and the rest) carry
			// no answer; the server has nothing to do with them either.
			if (methodName.compare(0, 14, "notifications/") == 0)
			{
				return EmptyResult(id);
			}

			return ErrorMessage(id, MethodNotFound, "the method is not supported: " + methodName);
		}
		catch (const char* problem)
		{
			return ErrorMessage(id, InternalError, (problem != nullptr) ? problem : "the command failed");
		}
		catch (const std::exception& e)
		{
			return ErrorMessage(id, InternalError, e.what());
		}
		catch (...)
		{
			return ErrorMessage(id, InternalError, "the command failed");
		}
	}

	// ---------------------------------------------------------------------------------------
	// The local transport
	// ---------------------------------------------------------------------------------------

	// Every front end may own the streams and start the server (`--mcp`), so the state of the
	// reader lives here rather than in a front end.
	static std::atomic<bool> transportRunning(false);
	static std::atomic<bool> transportStop(false);

	// One message per line. A message may be as long as it needs to be: a tool call that carries a
	// whole command line is not a short one, and the line break is the framing, so it never becomes
	// part of the message.
	static bool ReadMessage(std::string& message)
	{
		message.clear();

		int c;
		while ((c = fgetc(stdin)) != EOF)
		{
			if (c == '\n')
			{
				return true;
			}

			if (c != '\r')
			{
				message.push_back((char)c);
			}
		}

		// The end of the stream is how a client says it is done with the server; a last line that
		// has no line break yet is still a message.
		return !message.empty();
	}

	static void TransportThreadProc()
	{
		std::string message;

		while (!transportStop && ReadMessage(message))
		{
			if (message.empty())
			{
				continue;
			}

			std::string answer = HandleRequest(message.c_str());

			if (!answer.empty())
			{
				fwrite(answer.c_str(), 1, answer.size(), stdout);
				fputc('\n', stdout);
				fflush(stdout);
			}
		}

		transportRunning = false;
	}

	void StartTransport()
	{
		if (transportRunning)
		{
			return;
		}

		transportStop = false;
		transportRunning = true;

		// The reader is detached: it blocks on stdin, and the process exit (which is how the
		// emulator ends when its client has left) is what ends it. Waiting for it at shutdown would
		// mean waiting for the client to send one more message.
		std::thread reader(TransportThreadProc);
		reader.detach();
	}

	void StopTransport()
	{
		transportStop = true;
	}

	bool TransportRunning()
	{
		return transportRunning;
	}

	// ---------------------------------------------------------------------------------------
	// The JDI node
	// ---------------------------------------------------------------------------------------

	// `mcp [0|1]`: the state of the local server with no argument, 1 starts it and 0 stops it (the
	// same shape as the other switches of the debug interface, `jit` and `hwsod`).
	static Json::Value* CmdMcp(std::vector<std::string>& args)
	{
		if (args.size() >= 2)
		{
			if (args[1] == "1")
			{
				StartTransport();
			}
			else if (args[1] == "0")
			{
				StopTransport();
			}
			else
			{
				Report(Channel::Error, "mcp: 1 starts the local server, 0 stops it\n");
				return nullptr;
			}
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = TransportRunning();

		return output;
	}

	// `McpRequest '<json>'`: the whole protocol as one command. The stdio transport above is only a
	// carrier of this call, and so would be a socket, the debugger console or a test.
	static Json::Value* CmdMcpRequest(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			return nullptr;
		}

		std::string answer = HandleRequest(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		output->AddUtf8String(nullptr, answer.c_str());

		return output;
	}

	void Reflector()
	{
		JdiAddCmd("mcp", CmdMcp);
		JdiAddCmd("McpRequest", CmdMcpRequest);
	}
}
