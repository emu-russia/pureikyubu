# MCP server (issue #383)

The emulator has one control surface: the Json Debug Interface (JDI). Every front end (the SDL UI
and the headless one) is a thin layer over it - a command line in, a Json answer out - and
this module adds another consumer of the same interface: a local **MCP** server, so that an LLM
agent can drive the emulator the way a human drives the debugger console.

## What the server is

MCP (the Model Context Protocol) is JSON-RPC 2.0 with a handful of standard methods, and it is how
an agent application (Claude Desktop, Claude Code, an IDE, a script of your own) reaches the tools
of a program it started. This server answers three of the methods: `initialize` (the handshake),
`tools/list` (what the program can do) and `tools/call` (do it). The rest of the protocol -
resources, prompts, sampling, the logging notifications - is not implemented, and a client that asks
for one of those is answered with "method not found" rather than with silence.

The interesting part is *what* the tools are: the debug interface itself.

```
MCP client  --- stdio --->  mcp.cpp  --- McpRequest --->  JdiHub  --->  the emulator command
```

## Every command of the emulator is a tool

The tool table is built from the `can` records of the registered JDI nodes (`jdispecs.cpp`), so a
command is described by what its record already says:

| Record | What it becomes |
|---|---|
| the name of the record | the name of the tool |
| `help`, `info`, `usage`, `output` | the description the model reads |
| `hints` (`<file> [silent]`) | the parameters: `<name>` is required, `[name]` is optional |
| a hint that lists values (`[0\|1]`, `[text\|image\|osd\|reset]`) | an enumeration of the parameter |
| `args` (the minimum count) | what tells "the command refused the arguments" from "it answered nothing" |

The commands that only report into the debug message queue do not answer anything at all, and a hint
is what names their arguments. A command whose record has no hints (about half of the interface:
`load`, `r`, `syms`, `GetPc`, `dspdisa`, ...) gets a single `args` parameter instead - the whole
command line as a list of tokens. Every tool has that parameter, so the arguments the hints do not
name can always be sent:

```json
{ "name": "memdump",  "arguments": { "address": "0x80000000", "lines": "16" } }
{ "name": "memdump",  "arguments": { "args": ["0x80000000", "16"] } }
{ "name": "r",        "arguments": { "args": ["r3", "=", "12"] } }
```

The two forms are not mixed (a client that sends both is refused, with an explanation), an argument
the command does not have is refused as well, and an optional argument cannot be left out in the
middle of the list: the command line of the emulator is positional, and a hole in it would put every
argument after it onto the wrong parameter. For the same reason a hint that follows an optional one
has to be sent as `args`.

## What a tool call answers

`tools/call` answers with the Json the command returned, as text. Most of the debugger's commands
answer nothing and report into the **debug message queue** instead (`r`, `du`, `dst`, `threads`,
`syms`, `GetConfig`, `dspdisa`, ...) - the same queue the debugger UI reads. The answer of such a
call says so, and the `qd` tool reads that queue (and clears it, exactly as it does for the UI).

A command that fails - a register that does not exist, a machine that is not running yet - is an
error of the **tool** and not of the protocol: the client gets `isError` together with the usage of
the command, and the session survives it. Only a name that is not a tool at all is a protocol error
(`-32602`), because that is a client mistake rather than a command that said no.

## How it is started

| How | What happens |
|---|---|
| `pureikyubu --mcp` | the front end starts the transport as soon as the debug interface is up. A file on the command line is loaded and run first, so the client attaches to a running game. |
| `mcp 1`, `mcp 0` | the same switch at run time from the debugger console; `mcp` alone reports whether the server is running |
| the client closes the stream | the transport stops (the message in flight is answered first). In the headless build the emulator then shuts down by itself, in the windowed ones the window stays. |

The stdio transport is the "local server" of the MCP specification: the client starts the emulator
itself and speaks over its stdin/stdout, one message per line. That is why an answer is folded into
a single line before it leaves the server, and why nothing else may be written to stdout while it
runs - the headless front end keeps its own text on stderr in that mode, and the reports go to the
message queue and to the `EMU_LOG` file as usual.

A client that starts its servers as a child process is configured with the executable and `--mcp`:

```json
{
  "mcpServers": {
    "pureikyubu": {
      "command": "C:\\pureikyubu\\pureikyubu_headless.exe",
      "args": ["--mcp"]
    }
  }
}
```

A windowed build is started exactly the same way, with a file or without it
(`"args": ["--mcp", "D:\\Isos\\game.iso"]`): the window opens and the client drives the emulator
that is running in it, which is the interesting case for a live session (a screenshot with `gxshot`,
a memory dump with `memdump`, a breakpoint with `b`). The **headless** build is the one to use for an
unattended run (a build server, a WSL session without a display): there is no window, and the
emulator is idle until a tool loads something.

## The protocol, in one page

| Method | Answer |
|---|---|
| `initialize` | the protocol revision (the one the client asked for when this server knows it, otherwise the newest one it speaks: `2024-11-05`, `2025-03-26`, `2025-06-18`), the `tools` capability, the name and version of the server, and a short instruction text for the model |
| `ping` | an empty result |
| `tools/list` | every command of every registered node that is not marked `"mcp": false`, sorted by name |
| `tools/call` | the answer of the command, or the error of the tool |
| `notifications/initialized` and the other notifications | nothing at all (a message without an `id` is never answered) |
| anything else | `-32601` (method not found) |

The errors of the protocol are the ones JSON-RPC 2.0 defines: `-32700` (a message that is not Json),
`-32600` (no `jsonrpc`/`method`), `-32601` (an unknown method), `-32602` (an unknown tool),
`-32603` (the command threw something the server did not expect). A message that carries the UTF-8
byte order mark is accepted, because a tool on Windows may well have written it there.

## The debug interface side of it

The server's own node is `McpJdi` and it has two commands, which is all a front end and a test need:

| Command | Meaning |
|---|---|
| `mcp [0\|1]` | start (`1`), stop (`0`) or report (no argument) the local server. The same shape as the other switches of the interface, `jit` and `hwsod` |
| `McpRequest <json>` | hand one MCP message to the server and answer with the message it produced (an empty string for a notification). **The whole protocol is this command**: the transport is only a carrier of its answers |

Which is what makes the server easy to look at from the console the emulator already has:

```
McpRequest '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
McpRequest '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"GetVersion"}}'
```

Two commands are kept off the tool list with the `"mcp": false` attribute of their specification:
`exit` and its aliases (`quit`, `x`, `q`) would end the process the server itself runs in the middle
of its own answer, and the MCP commands would only call the server back into itself.

## What the server is not

* There is no HTTP or SSE transport and no authentication: the server is the child process of its
  client, and whoever started it is the only thing that can talk to it. The emulator has no network
  listener of any kind.
* One client at a time: the transport is one reader on one stream. A second client would have to
  wait for the first one to close it.
* No `resources`, no `prompts` and no sampling. The emulator's data (a register dump, a memory
  window, a screenshot) is reached through the commands that return it, and the artifacts a command
  writes (`gxshot`, `hwprofile image`, `dspdisa`) land in the working directory or in the debugger
  session folder, where the client can read them from the file system.
* An answer of a tool call is capped at 4 MB of text: a command that turns a whole binary file into a
  Json array (`FileLoad`) is refused instead of being pushed through the transport and into the
  context of a model.
* `--mcp` and `--bench` are not combined: the benchmark owns the run and prints its report to the
  console, which is the stream the protocol needs for itself.
* The commands that the emulated machine registers when it is built (the Flipper hardware commands,
  the GFX ones) only exist as tools once a file has been loaded - the tool list grows with the
  machine, which is why the emulator answers `initialize` with the note that it is idle until then.
* The debug interface has one driver. In a windowed build the client and a human at the same window
  (or at the debugger console) drive the same interface at the same time, and two commands that
  rebuild the machine at once (`load`, `reset`) do not mix; the MCP session is meant for a client
  that owns the run, with the window there to watch it.

## The files

| File | Contents |
|---|---|
| `src/mcp.cpp`, `src/mcp.h` | the protocol, the tool table and the stdio transport |
| `testing/mcp_test.cpp` | the tests: the handshake, the errors, the tool table over a node of its own, and the tool table over every specification the emulator ships |
| `src/jdispecs.cpp` | the `McpJdi` node, and the `"mcp": false` mark of the commands that are not tools |
| `src/jdi.cpp` | `JdiHub::EnumCommands`, which is how the tool table is enumerated |
