// MCP server unit tests (issue #383).
//
// The subject is the protocol half of src/mcp.cpp: the tool table it builds out of the JDI
// specifications and the JSON-RPC 2.0 answers it produces. The server is a pure function of one
// message (`Mcp::HandleRequest`), and the commands it publishes are the ones the registered JDI
// nodes declare, so the tests register a node of their own and drive the server from here: no
// emulator, no client and no streams.
//
// What the tests pin down:
//
//   * the handshake answers with the revision the client asked for, or with the newest one this
//     server speaks when it does not know that revision;
//   * a notification is never answered, and an invalid message is answered with the error JSON-RPC
//     2.0 assigns to it (a parse error, an invalid request, an unknown method, an unknown tool);
//   * the tools come from the specifications: the hints name the parameters, `[a|b|c]` becomes an
//     enumeration, a command without hints gets the whole command line as `args`, and
//     `"mcp": false` keeps a command off the list;
//   * a tool call runs the command through the JDI in both argument forms, a command that fails is
//     an error of the *tool* (the session survives it) and an argument the command does not have is
//     refused;
//   * an answer is a single line, because the stdio transport frames one message per line;
//   * the whole shipped debug interface survives the trip through the tool table (the second test
//     class registers every specification of the emulator and walks the tool list it produces).

#include "pch.h"

#include <algorithm>

namespace McpUnitTest
{
	// The commands the tests publish. They are a small stand-in for the emulator's own nodes: what
	// matters is the shape of the specifications the tool table is built from.
	static const char* TestJdi = R"json(
{
  "info": {
    "description": "The commands the MCP tests publish.",
    "helpGroup": "MCP Test Commands"
  },

  "can": {

    "McpTestEcho": {
      "help": "Answer with the arguments it was given",
      "args": 1,
      "hints": "<text> [count]",
      "usage": [
        "Syntax: McpTestEcho <text> [count]\n"
      ],
      "output": "Array: [String]"
    },

    "McpTestSilent": {
      "help": "Answer nothing",
      "usage": [
        "Syntax: McpTestSilent\n"
      ]
    },

    "McpTestPrivate": {
      "mcp": false,
      "help": "A command that is not a tool"
    },

    "McpTestBroken": {
      "help": "Fail the way a command that cannot do its job fails",
      "args": 1,
      "hints": "<why>"
    },

    "McpTestChoice": {
      "help": "A command whose hint lists the values it takes",
      "hints": "[text|image|osd|reset]"
    },

    "McpTestNoHints": {
      "help": "A command whose arguments are not named anywhere",
      "args": 2
    }

  }

}
)json";

	static bool testNodeRegistered = false;

	static Json::Value* CmdTestEcho(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		for (size_t i = 1; i < args.size(); i++)
		{
			output->AddUtf8String(nullptr, args[i].c_str());
		}

		return output;
	}

	static Json::Value* CmdTestSilent(std::vector<std::string>& args)
	{
		return nullptr;
	}

	static Json::Value* CmdTestBroken(std::vector<std::string>& args)
	{
		throw "the emulator said no";
	}

	static void TestReflector()
	{
		JdiAddCmd("McpTestEcho", CmdTestEcho);
		JdiAddCmd("McpTestSilent", CmdTestSilent);
		JdiAddCmd("McpTestBroken", CmdTestBroken);
		JdiAddCmd("McpTestPrivate", CmdTestSilent);
		JdiAddCmd("McpTestChoice", CmdTestSilent);
		JdiAddCmd("McpTestNoHints", CmdTestSilent);
	}

	// -------------------------------------------------------------------------------------------
	// Reading an answer
	// -------------------------------------------------------------------------------------------

	// A parsed answer of the server. The tree is owned here, so the values the tests look at stay
	// valid for as long as the reader is alive.
	class Answer
	{
		Json json;
		Json::Value* root = nullptr;

	public:

		Answer(const std::string& text)
		{
			Assert::IsFalse(text.empty(), L"the server answered nothing");

			std::string buffer = text;
			json.Deserialize(buffer.data(), buffer.size());

			root = json.root.children.back();
			Assert::IsNotNull(root);
		}

		// The message itself (the "result" or the "error" member lives here).
		Json::Value* Message() { return root; }

		// The "result" member (the messages these tests build are successful ones).
		Json::Value* Result()
		{
			Json::Value* result = root->ByName("result");
			Assert::IsNotNull(result, L"the answer is not an error");
			return result;
		}

		// The tool list of a "tools/list" answer.
		Json::Value* Tools()
		{
			Json::Value* tools = Result()->ByName("tools");
			Assert::IsNotNull(tools, L"the answer has the tool list");
			return tools;
		}

		// The tool of that name, or nullptr.
		static Json::Value* Tool(Json::Value* tools, const wchar_t* name)
		{
			for (auto it = tools->children.begin(); it != tools->children.end(); ++it)
			{
				Json::Value* tool = *it;
				Json::Value* toolName = tool->ByName("name");

				if (toolName != nullptr && toolName->type == Json::ValueType::String &&
					wcscmp(toolName->value.AsString, name) == 0)
				{
					return tool;
				}
			}

			return nullptr;
		}

		// The text of the first content item of a "tools/call" answer.
		std::wstring Text()
		{
			Json::Value* content = Result()->ByName("content");
			Assert::IsNotNull(content, L"a tool result has content");

			Json::Value* item = content->children.front();
			Json::Value* text = item->ByName("text");
			Assert::IsNotNull(text, L"the content item is text");

			return text->value.AsString;
		}

		bool IsError()
		{
			Json::Value* isError = Result()->ByName("isError");
			Assert::IsNotNull(isError, L"a tool result says whether it failed");
			return isError->value.AsBool;
		}
	};

	static bool Contains(const std::wstring& text, const wchar_t* fragment)
	{
		return text.find(fragment) != std::wstring::npos;
	}

	TEST_CLASS(McpTest)
	{
	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			// The tool table is built from the nodes that are registered, so the tests bring their
			// own node with them (the emulator's are not linked into the test DLL).
			if (!testNodeRegistered)
			{
				JdiAddNode("MCP_TEST_JDI_JSON", TestJdi, TestReflector);
				testNodeRegistered = true;
			}
		}

		TEST_CLASS_CLEANUP(Cleanup)
		{
			JdiRemoveNode("MCP_TEST_JDI_JSON");
			testNodeRegistered = false;
		}

		// ------------------------------------------------------------------
		// The handshake
		// ------------------------------------------------------------------

		TEST_METHOD(Initialize_AnswersWithTheRevisionTheClientAskedFor)
		{
			Answer answer(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})json"));

			Json::Value* result = answer.Result();

			Json::Value* version = result->ByName("protocolVersion");
			Assert::IsNotNull(version);
			Assert::AreEqual(std::wstring(L"2024-11-05"), std::wstring(version->value.AsString));

			Json::Value* serverInfo = result->ByName("serverInfo");
			Assert::IsNotNull(serverInfo);
			Assert::AreEqual(std::wstring(L"pureikyubu"), std::wstring(serverInfo->ByName("name")->value.AsString));
			Assert::IsNotNull(serverInfo->ByName("version"));

			Json::Value* capabilities = result->ByName("capabilities");
			Assert::IsNotNull(capabilities);
			Assert::IsNotNull(capabilities->ByName("tools"), L"the server offers tools and nothing else");

			Json::Value* id = answer.Message()->ByName("id");
			Assert::IsNotNull(id);
			Assert::AreEqual((uint64_t)1, id->value.AsInt, L"the id of the request comes back");
		}

		TEST_METHOD(Initialize_AnswersWithItsOwnRevisionForAnUnknownOne)
		{
			Answer answer(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":"abc","method":"initialize","params":{"protocolVersion":"1999-01-01"}})json"));

			Json::Value* version = answer.Result()->ByName("protocolVersion");
			Assert::AreEqual(std::wstring(L"2025-06-18"), std::wstring(version->value.AsString),
				L"a revision the server does not know is answered with the newest one it speaks");

			Json::Value* id = answer.Message()->ByName("id");
			Assert::AreEqual(std::wstring(L"abc"), std::wstring(id->value.AsString),
				L"an id may be a string, and it is echoed as it came");
		}

		TEST_METHOD(Ping_AnswersWithAnEmptyResult)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":2,"method":"ping"})json"));

			Assert::AreEqual((size_t)0, answer.Result()->children.size());
		}

		TEST_METHOD(Notification_IsNotAnswered)
		{
			// The protocol forbids answering a notification, whatever happens to it.
			std::string text = Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","method":"notifications/initialized"})json");

			Assert::IsTrue(text.empty(), L"a message without an id is not answered");

			text = Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","method":"tools/call","params":{"name":"McpTestEcho"}})json");

			Assert::IsTrue(text.empty(), L"not even a tool call without an id is answered");
		}

		// ------------------------------------------------------------------
		// The tools are the commands of the debug interface
		// ------------------------------------------------------------------

		TEST_METHOD(ToolsList_PublishesTheCommandsOfTheRegisteredNodes)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":3,"method":"tools/list"})json"));
			Json::Value* tools = answer.Tools();

			Json::Value* echo = Answer::Tool(tools, L"McpTestEcho");
			Assert::IsNotNull(echo, L"the command of the test node is a tool");

			// The description is what the debugger console would print for the command.
			Json::Value* description = echo->ByName("description");
			Assert::IsTrue(Contains(description->value.AsString, L"Answer with the arguments it was given"));
			Assert::IsTrue(Contains(description->value.AsString, L"Syntax: McpTestEcho <text> [count]"));
			Assert::IsTrue(Contains(description->value.AsString, L"Output: Array: [String]"));

			// The parameters are the hints: `<text>` is required, `[count]` is not.
			Json::Value* schema = echo->ByName("inputSchema");
			Assert::IsNotNull(schema);
			Assert::AreEqual(std::wstring(L"object"), std::wstring(schema->ByName("type")->value.AsString));

			Json::Value* properties = schema->ByName("properties");
			Assert::IsNotNull(properties->ByName("text"));
			Assert::IsNotNull(properties->ByName("count"));
			Assert::IsNotNull(properties->ByName("args"), L"the whole command line can always be sent");
			Assert::AreEqual(std::wstring(L"string"),
				std::wstring(properties->ByName("text")->ByName("type")->value.AsString));

			Json::Value* required = schema->ByName("required");
			Assert::AreEqual((size_t)1, required->children.size());
			Assert::AreEqual(std::wstring(L"text"), std::wstring(required->children.front()->value.AsString));

			Assert::IsFalse(schema->ByName("additionalProperties")->value.AsBool,
				L"an argument the command does not have is not accepted");
		}

		TEST_METHOD(ToolsList_KeepsTheCommandsThatAreNotToolsOut)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":4,"method":"tools/list"})json"));
			Json::Value* tools = answer.Tools();

			Assert::IsNull(Answer::Tool(tools, L"McpTestPrivate"), L"\"mcp\": false keeps a command off the list");
			Assert::IsNull(Answer::Tool(tools, L"McpRequest"), L"the MCP commands themselves are not tools");
			Assert::IsNull(Answer::Tool(tools, L"mcp"));
			Assert::IsNull(Answer::Tool(tools, L"exit"), L"a command that would end the process is not a tool");
		}

		TEST_METHOD(ToolsList_DescribesAChoiceAsAnEnumeration)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":5,"method":"tools/list"})json"));
			Json::Value* tools = answer.Tools();

			Json::Value* choice = Answer::Tool(tools, L"McpTestChoice");
			Assert::IsNotNull(choice);

			Json::Value* values = choice->ByName("inputSchema")->ByName("properties")
				->ByName("value")->ByName("enum");
			Assert::IsNotNull(values, L"[a|b|c] becomes an enumeration of the parameter");

			const wchar_t* expected[] = { L"text", L"image", L"osd", L"reset" };
			Assert::AreEqual((size_t)4, values->children.size());

			size_t i = 0;
			for (auto it = values->children.begin(); it != values->children.end(); ++it, i++)
			{
				Assert::AreEqual(std::wstring(expected[i]), std::wstring((*it)->value.AsString));
			}

			Assert::IsTrue(choice->ByName("inputSchema")->ByName("required")->children.empty(),
				L"an optional parameter is not required");
		}

		TEST_METHOD(ToolsList_GivesACommandWithoutHintsTheWholeCommandLine)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":6,"method":"tools/list"})json"));
			Json::Value* tools = answer.Tools();

			Json::Value* unnamed = Answer::Tool(tools, L"McpTestNoHints");
			Assert::IsNotNull(unnamed);

			Json::Value* properties = unnamed->ByName("inputSchema")->ByName("properties");
			Assert::AreEqual((size_t)1, properties->children.size(), L"only the fallback list is there");
			Assert::IsNotNull(properties->ByName("args"));
		}

		// ------------------------------------------------------------------
		// Calling a tool
		// ------------------------------------------------------------------

		TEST_METHOD(ToolsCall_RunsTheCommandWithTheNamedArguments)
		{
			Answer answer(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"text":"hello","count":"2"}}})json"));

			Assert::IsFalse(answer.IsError(), L"the call succeeded");

			std::wstring text = answer.Text();
			Assert::IsTrue(Contains(text, L"hello"));
			Assert::IsTrue(Contains(text, L"2"), L"the arguments reach the command in the order of the hints");
		}

		TEST_METHOD(ToolsCall_RunsTheCommandWithTheWholeArgumentList)
		{
			Answer answer(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"args":["one","two three"]}}})json"));

			Assert::IsFalse(answer.IsError());

			std::wstring text = answer.Text();
			Assert::IsTrue(Contains(text, L"one"));
			Assert::IsTrue(Contains(text, L"two three"), L"an argument with a space is carried as one token");
		}

		TEST_METHOD(ToolsCall_RefusesAnArgumentHoleAndAnUnknownArgument)
		{
			// A command line is positional: `count` without `text` would put the count where the text
			// belongs.
			Answer answer(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"count":"2"}}})json"));

			Assert::IsTrue(answer.IsError());
			Assert::IsTrue(Contains(answer.Text(), L"text"), L"the client is told which argument is missing");

			Answer unknown(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"text":"a","nonsense":"b"}}})json"));

			Assert::IsTrue(unknown.IsError());
			Assert::IsTrue(Contains(unknown.Text(), L"nonsense"));

			Answer mixed(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"args":["a"],"text":"b"}}})json"));

			Assert::IsTrue(mixed.IsError(), L"the two argument forms are not mixed");
		}

		TEST_METHOD(ToolsCall_ReportsAFailingCommandAsAToolError)
		{
			// A command that only reports into the debug message queue answers nothing at all, and
			// that is not a failure: the client reads the report with the `qd` tool.
			Answer silent(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"McpTestSilent"}})json"));

			Assert::IsFalse(silent.IsError(), L"an empty answer is not an error");
			Assert::IsTrue(Contains(silent.Text(), L"qd"), L"the client is told where the report went");

			// A command that was given fewer arguments than its specification requires is a failure,
			// and the answer carries the description of the command (McpTestNoHints declares two).
			Answer few(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":13,"method":"tools/call","params":{"name":"McpTestNoHints","arguments":{"args":["one"]}}})json"));

			Assert::IsTrue(few.IsError());
			Assert::IsTrue(Contains(few.Text(), L"needs more arguments"));
			Assert::IsTrue(Contains(few.Text(), L"A command whose arguments are not named anywhere"),
				L"the answer carries the description of the command");

			// A command that throws is a failure of the tool as well, and the session survives it.
			Answer broken(Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":14,"method":"tools/call","params":{"name":"McpTestBroken","arguments":{"why":"test"}}})json"));

			Assert::IsTrue(broken.IsError());
			Assert::IsTrue(Contains(broken.Text(), L"the emulator said no"),
				L"the exception of the command becomes the message of the tool error");
		}

		TEST_METHOD(ToolsCall_ReportsAnUnknownToolAsInvalidParams)
		{
			std::string text = Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":15,"method":"tools/call","params":{"name":"NoSuchTool"}})json");

			Assert::IsTrue(text.find("-32602") != std::string::npos,
				L"an unknown tool is an invalid-params error of the protocol");

			text = Mcp::HandleRequest(
				R"json({"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"McpTestPrivate"}})json");

			Assert::IsTrue(text.find("-32602") != std::string::npos,
				L"a command that is not a tool cannot be called as one");
		}

		// ------------------------------------------------------------------
		// The errors of the protocol
		// ------------------------------------------------------------------

		TEST_METHOD(InvalidMessages_AreAnsweredWithTheErrorsOfTheProtocol)
		{
			// A document that is not Json at all.
			std::string text = Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":17,)json");
			Assert::IsTrue(text.find("-32700") != std::string::npos, L"a parse error");
			Assert::IsTrue(text.find("\"id\" : null") != std::string::npos || text.find("\"id\":null") != std::string::npos,
				L"the id of a message that could not be parsed is null");

			// A message of another protocol.
			text = Mcp::HandleRequest(R"json({"jsonrpc":"1.0","id":18,"method":"ping"})json");
			Assert::IsTrue(text.find("-32600") != std::string::npos, L"an invalid request");

			// A method the server does not have.
			text = Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":19,"method":"resources/list"})json");
			Assert::IsTrue(text.find("-32601") != std::string::npos, L"a method that is not found");
		}

		TEST_METHOD(Answers_AreASingleLine)
		{
			// The stdio transport frames one message per line, and the emulator's Json serializer
			// writes its documents with CRLF line breaks: the answer has to be folded.
			std::string text = Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":20,"method":"tools/list"})json");

			Assert::IsTrue(text.find('\n') == std::string::npos, L"no line break is left in an answer");
			Assert::IsTrue(text.find('\r') == std::string::npos);

			text = Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"McpTestEcho","arguments":{"text":"a\nb"}}})json");
			Assert::IsTrue(text.find('\n') == std::string::npos,
				L"a line break inside a value stays escaped, it is not folded into the framing");
			Assert::IsTrue(text.find("\\n") != std::string::npos, L"the value keeps the escape of its line break");
		}

		// ------------------------------------------------------------------
		// The debug interface side of the server
		// ------------------------------------------------------------------

		TEST_METHOD(Jdi_McpRequestIsTheWholeProtocolAndTheSwitchReportsTheState)
		{
			// The protocol is reachable as a command of the debug interface, which is what makes a
			// transport a carrier of answers and nothing more.
			JdiAddNode("MCP_JDI_JSON", JdiSpecs::McpJdi, Mcp::Reflector);

			Json::Value* message = CallJdi("McpRequest '{\"jsonrpc\":\"2.0\",\"id\":22,\"method\":\"ping\"}'");
			Assert::IsNotNull(message, L"the McpRequest command is registered");
			Assert::AreEqual((size_t)1, message->children.size());

			std::wstring answer = message->children.front()->value.AsString;
			Assert::IsTrue(answer.find(L"\"result\"") != std::wstring::npos,
				L"the answer of the server is what the command returns");
			Assert::IsTrue(answer.find(L"\"id\"") != std::wstring::npos);
			delete message;

			// The server is not running unless a front end started it.
			bool running = true;
			Assert::IsTrue(CallJdiReturnBool("mcp", &running));
			Assert::IsFalse(running, L"the transport is off until it is asked for");

			JdiRemoveNode("MCP_JDI_JSON");
		}
	};

	// -------------------------------------------------------------------------------------------
	// The tool table over the specifications the emulator ships
	// -------------------------------------------------------------------------------------------

	// The nodes are registered with a reflector that does nothing: the tool table is built from the
	// specifications alone (the handlers behind them live in the modules of the emulator, which the
	// test DLL does not link).
	static void NoReflector()
	{
	}

	struct SpecNode
	{
		const char* name;
		const char* text;
	};

	static const SpecNode SpecNodes[] =
	{
		{ "MCP_SPEC_EMU_JDI_JSON", JdiSpecs::EmuJdi },
		{ "MCP_SPEC_DEBUGGER_JDI_JSON", JdiSpecs::DebuggerJdi },
		{ "MCP_SPEC_DEBUGUI2_JDI_JSON", JdiSpecs::DebugUi2Jdi },
		{ "MCP_SPEC_GEKKO_JDI_JSON", JdiSpecs::GekkoCoreJdi },
		{ "MCP_SPEC_DSP_JDI_JSON", JdiSpecs::DspJdi },
		{ "MCP_SPEC_HW_JDI_JSON", JdiSpecs::HwJdi },
		{ "MCP_SPEC_DDU_JDI_JSON", JdiSpecs::DduJdi },
		{ "MCP_SPEC_GFX_JDI_JSON", JdiSpecs::GfxJdi },
		{ "MCP_SPEC_HLE_JDI_JSON", JdiSpecs::HleJdi },
		{ "MCP_SPEC_UI_JDI_JSON", JdiSpecs::UiJdi },
		{ "MCP_SPEC_MCP_JDI_JSON", JdiSpecs::McpJdi },
	};

	TEST_CLASS(McpSpecsTest)
	{
	public:

		TEST_CLASS_INITIALIZE(RegisterTheSpecifications)
		{
			for (const SpecNode& node : SpecNodes)
			{
				JdiAddNode(node.name, node.text, NoReflector);
			}
		}

		TEST_CLASS_CLEANUP(RemoveTheSpecifications)
		{
			for (const SpecNode& node : SpecNodes)
			{
				JdiRemoveNode(node.name);
			}
		}

		// Every command of the emulator becomes a tool, and every tool is a command the client can
		// actually call: the point of this test is that the whole shipped interface survives the trip
		// through the tool table (a name a client may not use, a missing schema or a duplicate would
		// leave a part of the debug interface unreachable).
		TEST_METHOD(ToolsList_CoversTheWholeDebugInterface)
		{
			Answer answer(Mcp::HandleRequest(R"json({"jsonrpc":"2.0","id":50,"method":"tools/list"})json"));
			Json::Value* tools = answer.Tools();

			Assert::IsTrue(tools->children.size() > 100, L"the debug interface is a large one");

			std::vector<std::wstring> names;

			for (auto it = tools->children.begin(); it != tools->children.end(); ++it)
			{
				Json::Value* tool = *it;

				Json::Value* name = tool->ByName("name");
				Assert::IsNotNull(name);
				Assert::IsTrue(name->type == Json::ValueType::String);

				std::wstring text = name->value.AsString;

				// The name of a tool has to be usable by a client (the specification allows the
				// letters, the digits, '_' and '-', up to 64 characters).
				Assert::IsTrue(text.size() >= 1 && text.size() <= 64, L"the name has a usable length");

				for (wchar_t c : text)
				{
					bool usable = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
						(c >= L'0' && c <= L'9') || c == L'_' || c == L'-';

					Assert::IsTrue(usable, L"the name is made of characters a client accepts");
				}

				Assert::IsTrue(std::find(names.begin(), names.end(), text) == names.end(),
					L"a tool name is published once");
				names.push_back(text);

				// Every tool describes its arguments, and always has the fallback command line.
				Json::Value* schema = tool->ByName("inputSchema");
				Assert::IsNotNull(schema, L"a tool has an input schema");
				Assert::AreEqual(std::wstring(L"object"),
					std::wstring(schema->ByName("type")->value.AsString));

				Json::Value* properties = schema->ByName("properties");
				Assert::IsNotNull(properties, L"a tool describes its arguments");
				Assert::IsNotNull(properties->ByName("args"), L"the whole command line is always accepted");

				Assert::IsNotNull(tool->ByName("description"), L"a tool says what it does");
			}

			// One command per node of the debug interface is known to be a tool.
			Assert::IsNotNull(Answer::Tool(tools, L"load"), L"emulator control");
			Assert::IsNotNull(Answer::Tool(tools, L"hwprofile"), L"the debugger");
			Assert::IsNotNull(Answer::Tool(tools, L"regs"), L"the Gekko core");
			Assert::IsNotNull(Answer::Tool(tools, L"dmem"), L"the DSP");
			Assert::IsNotNull(Answer::Tool(tools, L"memdump"), L"the Flipper hardware");
			Assert::IsNotNull(Answer::Tool(tools, L"MountIso"), L"the disk drive unit");
			Assert::IsNotNull(Answer::Tool(tools, L"gxshot"), L"the GFX pipeline");
			Assert::IsNotNull(Answer::Tool(tools, L"syms"), L"the high-level debugger");
			Assert::IsNotNull(Answer::Tool(tools, L"UIReport"), L"the user interface");

			// ... and the commands that may not be one.
			Assert::IsNull(Answer::Tool(tools, L"exit"));
			Assert::IsNull(Answer::Tool(tools, L"q"));
			Assert::IsNull(Answer::Tool(tools, L"McpRequest"));
			Assert::IsNull(Answer::Tool(tools, L"mcp"));
		}
	};
}
