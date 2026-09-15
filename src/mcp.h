/*

# MCP server (issue #383)

The emulator has one universal control surface - the Json Debug Interface - and every front end
(the Win32 UI, the SDL one, the headless one) is a thin layer over it. This module adds another
consumer of the same interface: a local **MCP** (Model Context Protocol) server, so that an LLM
agent can drive the emulator the way a debugger console does.

## The protocol is a JDI command

The server does not implement emulator commands, it *publishes* them: every command of every
registered JDI node (`jdispecs.cpp`) becomes an MCP tool, with its description and its argument
list taken from the specification the emulator already ships.

The protocol itself is a JDI command too (`McpRequest`): one message in, one message out. A
transport is only the carrier of those answers, which is what makes the whole server testable
without a process and reachable from the debugger console:

```
Stdio transport (mcp.cpp)  ->  McpRequest  ->  JdiHub  ->  the emulator command
```

## The local transport

The MCP client (the agent application) starts the emulator itself and speaks JSON-RPC 2.0 over the
process's stdin/stdout - the "stdio" transport of the specification, one message per line. `--mcp`
makes the emulator take that role, and the `mcp 0` / `mcp 1` commands switch it at run time.

`StartTransport` is a front end's call: the headless build serves the client instead of running an
image until it is told to, the windowed ones keep their window and are driven through the same
interface. See `wiki/mcp.md` for the client side (how to plug the emulator into an MCP client).

## What the specification of a command has to say for a tool to be usable

The tool list is built from the "can" records of the JDI nodes, so a command is described by what
its record already says (`help`, `hints`, `args`, `usage`, `output`). Two additions were needed:

- `"hints"` is read as the parameter list of the tool: `<file>` is a required parameter, `[0|1]`
  an optional one, `[a|b|c]` a choice between the listed values. A command without hints gets a
  single `args` parameter (the whole command line as a list of tokens);
- `"mcp": false` keeps a command off the tool list. That is for the commands that would end the
  process the server itself runs in (`exit` and its aliases) and for the MCP commands themselves,
  which as tools would only call the server back into itself.

*/

#pragma once

namespace Mcp
{
	/// <summary>
	/// Hand one MCP message (a JSON-RPC 2.0 request or notification, as UTF-8 text) to the server
	/// and answer with the message it produced. A notification is answered with an empty string:
	/// the protocol forbids answering one, whatever happens to it.
	/// </summary>
	std::string HandleRequest(const char* text);

	/// <summary>
	/// Start the local transport: the messages are read from stdin, one per line, and the answers
	/// are written to stdout. Nothing else may be written to stdout while it runs. Does nothing
	/// when the transport is already running.
	/// </summary>
	void StartTransport();

	/// <summary>
	/// Ask the transport to stop. The message being handled is answered first: a read from stdin
	/// cannot be interrupted from here, so a client that is holding the stream open is the one
	/// that ends the session (by closing it).
	/// </summary>
	void StopTransport();

	/// <summary>
	/// Whether the local transport is running.
	/// </summary>
	bool TransportRunning();

	/// <summary>
	/// Register the handlers of the MCP JDI node (`McpJdi`): `mcp` and `McpRequest`.
	/// </summary>
	void Reflector();
}
