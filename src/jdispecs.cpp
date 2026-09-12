// Json Debug Interface (JDI) specifications.
//
// Every JDI node of the emulator is described by a JSON text. All of them are collected
// here, so that the whole debug interface can be reviewed and edited in one place.
// The components only register their node with the text declared in jdispecs.h.

#include "pch.h"

namespace JdiSpecs
{

	const char* EmuJdi = R"json(
{
  "info": {
    "description": "Emulator Jey-Dai specs.",
    "helpGroup": "EMU Control Commands"
  },

  "can": {

    "FileLoad": {
      "help": "Load file",
      "args": 1,
      "hints": "<file>",
      "usage": [
        "Syntax: FileLoad <file>\n",
        "Commands outputs Array of bytes [], which can be used by other commands.\n",
        "Example: FileLoad \"Data\\AnsiFont.szp\"\n"
      ],
      "output": "Array bytes []"
    },

    "FileSave": {
      "help": "Save file",
      "args": 2,
      "hints": "<file> <cmd>",
      "usage": [
        "Syntax: FileSave <file> <cmd ...>\n",
        "Save data returned by another command (cmd) to specified file.\n",
        "Example: FileSave \"Data\\FST.bin\" DumpFst\n"
      ]
    },

    "sleep": {
      "help": "Sleep specified number of milliseconds",
      "args": 1,
      "hints": "<msec>",
      "usage": [
        "Syntax: sleep <milliseconds>\n",
        "Examples of use: sleep 1000\n"
      ]
    },

    "exit": {
      "help": "Exit (also: x, quit, q)"
    },
    "quit": {
      "internal": true,
      "help": "Exit"
    },
    "x": {
      "internal": true,
      "help": "Exit"
    },
    "q": {
      "internal": true,
      "help": "Exit"
    },

    "load": {
      "help": "load DVD/executable from file",
      "args": 1,
      "usage": [
        "Syntax: load <file>\n",
        "path can be relative. Use `load Bootrom` to load IPL.\n",
        "Examples of use: boot c:\\luigimansion.gcm\n",
        "                 boot PONG.dol\n"
      ]
    },

    "unload": {
      "help": "unload current file"
    },

    "reset": {
      "help": "Reset emulation"
    },

    "IsLoaded": {
      "internal": true,
      "help": "Return true if emulation state is `Loaded`",
      "output": "Bool"
    },

    "GetLoaded": {
      "internal": true,
      "help": "Get the full path of the loaded file",
      "info": "Used by other components to obtain information about the currently running game or DOL file.",
      "output": "{ loaded: PathString }"
    },

    "GetVersion": {
      "internal": true,
      "help": "Get emulator version",
      "output": "Array: [String]"
    },

    "GetConfig": {
      "help": "Dump config"
    },

    "GetConfigString": {
      "internal": true,
      "help": "Get configuration String parameter",
      "args": 2,
      "usage": [
        "Use: GetConfigString <section> <param>"
      ],
      "output": "Array: [String]"
    },

    "SetConfigString": {
      "internal": true,
      "help": "Set configuration String parameter",
      "args": 3,
      "usage": [
        "Use: SetConfigString <section> <param> <value>"
      ]
    },

    "GetConfigInt": {
      "internal": true,
      "help": "Get configuration Int parameter",
      "args": 2,
      "usage": [
        "Use: GetConfigInt <section> <param>"
      ],
      "output": "Array: [Int]"
    },

    "SetConfigInt": {
      "internal": true,
      "help": "Set configuration Int parameter",
      "args": 3,
      "usage": [
        "Use: SetConfigInt <section> <param> <value>"
      ]
    },

    "GetConfigBool": {
      "internal": true,
      "help": "Get configuration Bool parameter",
      "args": 2,
      "usage": [
        "Use: GetConfigBool <section> <param>"
      ],
      "output": "Array: [Bool]"
    },

    "SetConfigBool": {
      "internal": true,
      "help": "Set configuration Bool parameter",
      "args": 3,
      "usage": [
        "Use: SetConfigBool <section> <param> <value>"
      ]
    },

    "threads": {
      "help": "Show emulator threads (Util::Thread)"
    }

  }

}
)json";

	const char* DebuggerJdi = R"json(
{
  "info": {
    "description": "Debug Interface.",
    "helpGroup": "Debugger Interface"
  },

  "can": {

    "script": {
      "help": "execute batch script",
      "args": 1,
      "usage": [
        "Syntax: script <file>\n",
        "path can be relative\n",
        "Examples of use: script data\\zelda.cmd\n",
        "                 script c:\\luigi.cmd\n"
      ]
    },

    "echo": {
      "help": "Echo",
      "args": 1,
      "hints": "<text>",
      "usage": [
        "Syntax: echo <text>\n",
        "Example: echo \"Hello, world!\"\n"
      ]
    },

    "StartProfiler": {
      "help": "Start Gekko profiling",
      "args": 1,
      "usage": [
        "Syntax: StartProfiler <json> [ms]",
        "Specify the Json file name where the collected information will be saved, after calling the StopProfiler command.",
        "The interval is specified in emulated Gekko milliseconds. Possible values are 2-50. The default is 5.",
        "Example: StartProfiler Data/sampleData.json 10"
      ]
    },

    "StopProfiler": {
      "help": "Stop Gekko profiling"
    },

    "GetChannelName": {
      "internal": true,
      "help": "Get the human-readable name of a debug channel",
      "args": 1,
      "hints": "<channel>",
      "usage": [
        "Syntax: GetChannelName <channel>\n",
        "Example: GetChannelName 5\n"
      ],
      "output": "Array of single string: [ \"ChannelName\" ]"
    },

    "qd": {
      "internal": true,
      "help": "Get history of debug messages. Clear queue in progress.",
      "usage": [
        "Syntax: qd\n"
      ],
      "output": "Array of Pair<Debug::Channel, string>: [ 4, \"Message1\", 7, \"Message2\" ] or empty Array []"
    },

    "help": {
      "internal": true,
      "help": "Show help :-)"
    },

    "IsCommandExists": {
      "internal": true,
      "args": 1,
      "help": "Check whenever command exists",
      "output": "Bool"
    },

    "GetPerformanceCounter": {
      "internal": true,
      "args": 1,
      "help": "Get the value of the performance counter",
      "output": "UInt64"
    },

    "ResetPerformanceCounter": {
      "internal": true,
      "args": 1,
      "help": "Reset the value of the performance counter"
    }

  }

}
)json";

	const char* DebugUiJdi = R"json(
{
  "info": {
    "description": "Debug UI Jey-Dai specs.",
    "helpGroup": "Debug UI Commands"
  },

  "can": {

    "d": {
      "help": "Set memory address to view in debugger.",
      "args": 1,
      "hints": "<address>",
      "usage": [
        "Syntax: d <address>\n",
        "Example: d 0x80003000\n"
      ]
    },

    "u": {
      "help": "Set memory address for viewing disassembled Gekko/DSP code.",
      "args": 1,
      "hints": "<address>",
      "usage": [
        "Syntax: u <address>\n",
        "Example: u 0x80003000\n"
      ]
    }

  }

}
)json";

	const char* GekkoCoreJdi = R"json(
{
  "info": {
    "description": "Processor debug commands. Some available only after emulation has been started.",
    "helpGroup": "Gekko Debug Commands"
  },

  "can": {

    "run": {
      "help": "Run processor until break or stop"
    },

    "stop": {
      "help": "Stop processor execution"
    },

    "r": {
      "help": "show / change CPU register",
      "args": 1,
      "usage": [
        "Syntax: r <reg> OR r <reg> <op> <val> OR r <reg> <op> <reg>\n",
        "sp, sd1, sd2 semantics are supported for reg name.\n",
        "Value can be decimal, or hex with '0x' prefix.\n",
        "Possible operations are: = + - * / | & ^ << >>\n",
        "Examples of use: r sp\n",
        "                 r r3 = 12\n",
        "                 r r7 | 0x8020\n",
        "                 r msr\n",
        "                 r hid2 | 2\n",
        "                 r r7 = sd1\n"
      ]
    },

    "b": {
      "help": "Add Gekko breakpoint",
      "args": 1,
      "usage": [
        "Syntax: b <addr>",
        "Example: b 0x8003100"
      ]
    },

    "br": {
      "help": "Add Gekko read memory watch",
      "args": 1,
      "usage": [
        "Syntax: br <addr>",
        "Example: br 0x8000000"
      ]
    },

    "bw": {
      "help": "Add Gekko write memory watch",
      "args": 1,
      "usage": [
        "Syntax: bw <addr>",
        "Example: bw 0x8000000"
      ]
    },

    "bc": {
      "help": "Clear Gekko breakpoints"
    },

    "CacheLog": {
      "help": "Set cache operations log mode (0: none, 1: cache commands, 2: all)",
      "args": 1,
      "usage": [
        "Syntax: CacheLog <0|1|2>",
        "0: None (disabled)",
        "1: Show commands (cache control instructions activity)",
        "2: Show all operations (load/store, cast-in/cast-out, etc.)"
      ]
    },

    "IsRunning": {
      "internal": true,
      "output": "Bool"
    },

    "GekkoRun": {
      "internal": true
    },

    "GekkoSuspend": {
      "internal": true
    },

    "GekkoStep": {
      "internal": true
    },

    "GekkoSkipInstruction": {
      "internal": true
    },

    "GetGpr": {
      "internal": true,
      "args": 1,
      "output": "UInt32"
    },

    "GetPs0": {
      "internal": true,
      "args": 1,
      "output": "UInt64 (raw PS0/FPR value)"
    },

    "GetPs1": {
      "internal": true,
      "args": 1,
      "output": "UInt64 (raw PS1 value)"
    },

    "GetPc": {
      "internal": true,
      "output": "UInt32"
    },

    "GetMsr": {
      "internal": true,
      "output": "UInt32"
    },

    "GetCr": {
      "internal": true,
      "output": "UInt32"
    },

    "GetFpscr": {
      "internal": true,
      "output": "UInt32"
    },

    "GetSpr": {
      "internal": true,
      "args": 1,
      "output": "UInt32"
    },

    "GetSr": {
      "internal": true,
      "args": 1,
      "output": "UInt32"
    },

    "GetTbu": {
      "internal": true,
      "output": "UInt32"
    },

    "GetTbl": {
      "internal": true,
      "output": "UInt32"
    },

    "TranslateDMmu": {
      "internal": true,
      "args": 1,
      "output": "UInt64 (pointer). nullptr if cannot be translated."
    },

    "TranslateIMmu": {
      "internal": true,
      "args": 1,
      "output": "UInt64 (pointer). nullptr if cannot be translated."
    },

    "VirtualToPhysicalDMmu": {
      "internal": true,
      "args": 1,
      "output": "UInt32 (physical address). -1 if cannot be translated."
    },

    "VirtualToPhysicalIMmu": {
      "internal": true,
      "args": 1,
      "output": "UInt32 (physical address). -1 if cannot be translated."
    },

    "GekkoTestBreakpoint": {
      "internal": true,
      "args": 1,
      "output": "Bool"
    },

    "GekkoToggleBreakpoint": {
      "internal": true,
      "args": 1
    },

    "GekkoAddOneShotBreakpoint": {
      "internal": true,
      "args": 1
    },

    "GekkoDisasm": {
      "internal": true,
      "help": "Disassemble instruction at Gekko virtual memory address",
      "hints": "<vaddr>",
      "args": 1,
      "output": "Array: [String] (disassembled instruction with parameters)"
    },

    "GekkoDisasmNoMemAccess": {
      "internal": true,
      "help": "Disassemble the instruction without accessing memory (all necessary information is passed through parameters)",
      "args": 4,
      "hints": "<pc> <opcode> <showAddress> <showBytes>",
      "output": "Array: [String] (disassembled instruction with parameters)"
    },

    "GekkoIsBranch": {
      "internal": true,
      "args": 1,
      "output": "Array: [Bool, UInt32 targetAddress]"
    },

    "nop": {
      "help": "Insert `nop` at virtual address",
      "hint": "<vaddr>",
      "args": 1,
      "usage": [
        "Syntax: nop <virtual_address>",
        "Example: nop 0x80003100"
      ]
    },

    "jit": {
      "help": "Enables or disables the Gekko basic block recompiler (0: interpreter only, 1: recompiler). Reports the current state when called without an argument",
      "args": 1,
      "output": "Bool",
      "usage": [
        "Syntax: jit <0|1>"
      ]
    },

    "EnableOpcodeStats": {
      "help": "Enables or disables the maintenance of opcode usage statistics",
      "args": 1,
      "usage": [
        "Syntax: EnableOpcodeStats <0|1>"
      ]
    },

    "PrintOpcodeStats": {
      "help": "Displays the most commonly used Gekko opcodes",
      "args": 1,
      "usage": [
        "Syntax: PrintOpcodeStats <maxCount>"
      ]
    },

    "ResetOpcodeStats": {
      "help": "Clears statistics of opcode usage"
    },

    "RunOpcodeStats": {
      "help": "Runs a low priority thread that prints opcode statistics once a second"
    },

    "StopOpcodeStats": {
      "help": "Stop the thread that outputs the opcode statistics"
    },

    "GekkoAnalyze": {
      "internal": true,
      "help": "Parse Gekko instruction",
      "hints": "<pc> <opcode>",
      "args": 2,
      "output": "Array: [Int instr, Int numParams, Int param0, Int paramBits0, Int param1, Int paramBits1, Int param2, Int paramBits2, Int param3, Int paramBits3, Int param4, Int paramBits4, UInt32 immedValue, UInt32 newPc, Bool flow]"
    },

    "GekkoInstrToString": {
      "internal": true,
      "help": "Return the name of the Gekko instruction (Gekko::Instruction)",
      "hints": "<instr>",
      "args": 1,
      "output": "Array: [String]"
    },

    "GekkoInstrParamToString": {
      "internal": true,
      "help": "Return the parameter name of a Gekko instruction (Gekko::Param)",
      "hints": "<param> <paramBits> <immedValue>",
      "args": 3,
      "output": "Array: [String paramName, String paramDecoded]"
    },

    "dtlb": {
      "help": "Display DTLB state"
    },

    "itlb": {
      "help": "Display ITLB state"
    },

    "tlbinv": {
      "help": "Invalidate both DTLB/ITLB"
    }

  }

}
)json";

	const char* DspJdi = R"json(
{
	"info":
	{
		"description": "DSP JDI",
		"helpGroup": "DSP Debug Commands"
	},

	"can": {
		"dspdisa": {
			"help": "Disassemble DSP code into text file",
			"args": 1,
			"usage": [
				"Syntax: dspdisa <dsp_ucode.bin> [start_addr]\n",
				"disassemble dsp ucode from binary file and dump it into dspdisa.txt\n",
				"start_addr in DSP slots;\n",
				"Example of use: dspdisa Data/dsp_irom.bin 0x8000\n"
			]
		},

		"dregs": {
			"help": "Show DSP registers",
			"output": "Array [] of all DSP registers contents"
		},

		"dreg": {
			"help": "Modify DSP register",
			"hints": "<reg> <value>",
			"args": 2,
			"usage": [
				"Syntax: dreg <register> <value>\n",
				"Register names: ar0 ar1 ar2 ar3 ix0 ix1 ix2 ix3",
				"r8 r9 r10 r11 st0 st1 st2 st3",
				"ac0h ac1h config sr prodl prodm1 prodh prodm2",
				"ax0l ax0h ax1l ax1h ac0l ac1l ac0m ac1m",
				"Example of use: dreg ar0 0x300\n"
			]
		},

		"dmem": {
			"help": "Dump DSP DMEM",
			"args": 1,
			"usage": [
				"Syntax: dmem <dsp_addr>, dmem .\n",
				"Dump 32 bytes of DMEM at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"dmem . will dump 0x800 bytes at dmem address 0\n",
				"Example of use: dmem 0x8000\n"
			],
			"output": "Array [] of memory data"
		},

		"imem": {
			"help": "Dump DSP IMEM",
			"args": 1,
			"usage": [
				"Syntax: imem <dsp_addr>, imem .\n",
				"Dump 32 bytes of IMEM at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"imem . will dump 32 bytes of imem at program counter address.\n",
				"Example of use: imem 0\n"
			],
			"output": "Array [] of memory data"
		},

		"drun": {
			"help": "Run DSP thread until break, halt or dstop"
		},

		"dstop": {
			"help": "Stop DSP thread"
		},

		"dstep": {
			"help": "Step DSP instruction(s)",
			"hints": "[n]"
		},

		"dbrk": {
			"help": "Add IMEM breakpoint",
			"args": 1,
			"usage": [
				"Syntax: dbrk <dsp_addr>\n",
				"Add breakpoint at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"Example of use: dbrk 0x8020\n"
			]
		},

		"dcan": {
			"help": "Add IMEM canary",
			"args": 2,
			"usage": [
				"Syntax: dcan <dsp_addr> <message>\n",
				"Add canary at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"When the PC is equal to the canary address, a debug message is displayed\n",
				"Example of use: dcan 0x10 \"Ucode entrypoint\"\n"
			]
		},

		"dlist": {
			"help": "List IMEM breakpoints / canaries"
		},

		"dbrkclr": {
			"help": "Clear all IMEM breakpoints"
		},

		"dcanclr": {
			"help": "Clear all IMEM canaries"
		},

		"dpc": {
			"help": "Set DSP program counter",
			"args": 1,
			"usage": [
				"Syntax: dpc <dsp_addr>\n",
				"Set DSP program counter to dsp_addr. dsp_addr in halfword DSP slots.\n",
				"Example of use: dpc 0x8000\n"
			]
		},

		"dreset": {
			"help": "Issue DSP reset"
		},

		"du": {
			"help": "Disassemble some DSP instructions at pc / address",
			"hints": "[addr] [count]"
		},

		"dst": {
			"help": "Dump DSP call stack"
		},

		"difx": {
			"help": "Dump DSP IFX"
		},

		"cpumbox": {
			"help": "Write message to CPU Mailbox",
			"args": 1,
			"usage": [
				"Syntax: cpumbox <value>\n",
				"Example of use: cpumbox 0x8001FEED\n"
			]
		},

		"dspmbox": {
			"help": "Read message from DSP Mailbox"
		},

		"cpudspint": {
			"help": "Send CPU->DSP interrupt"
		},

		"dspcpuint": {
			"help": "Send DSP->CPU interrupt"
		},

		"DspIsRunning": {
			"internal": true,
			"output": "Bool"
		},

		"DspRun": {
			"internal": true
		},

		"DspSuspend": {
			"internal": true
		},

		"DspStep": {
			"internal": true
		},

		"DspGetReg": {
			"internal": true,
			"args": 1,
			"output": "UInt16"
		},

		"DspGetPsr": {
			"internal": true,
			"output": "UInt16"
		},

		"DspGetPc": {
			"internal": true,
			"output": "UInt16"
		},

		"DspPackProd": {
			"internal": true,
			"output": "UInt64 (crazy packed multiply product)"
		},

		"DspTranslateDMem": {
			"internal": true,
			"args": 1,
			"output": "UInt64 (pointer). nullptr if cannot be translated."
		},

		"DspTranslateIMem": {
			"internal": true,
			"args": 1,
			"output": "UInt64 (pointer). nullptr if cannot be translated."
		},

		"DspTestBreakpoint": {
			"internal": true,
			"args": 1,
			"output": "Bool"
		},

		"DspToggleBreakpoint": {
			"internal": true,
			"args": 1
		},

		"DspAddOneShotBreakpoint": {
			"internal": true,
			"args": 1
		},

		"DspIsCall": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool, UInt32 targetAddress]"
		},

		"DspIsCallOrJump": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool, UInt32 targetAddress]"
		},

		"DspDisasm": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool flowControl, Int instrSizeWords, String]. Empty string (\"\") mean disasm error"
		},

		"DspWatch": {
			"help": "Adds DSP DMEM address for tracking",
			"args": 1,
			"hint": "<addr>",
			"usage": [
				"Syntax: DspUnwatch <dsp_addr>\n",
				"Adds DSP DMEM address for tracking\n",
				"Example of use: DspUnwatch 0x0BE5\n"
			]
		},

		"DspUnwatch": {
			"help": "Removes DSP DMEM address tracking",
			"args": 1,
			"hint": "<addr>",
			"usage": [
				"Syntax: DspUnwatch <dsp_addr>\n",
				"Removes DSP DMEM address tracking\n",
				"Example of use: DspUnwatch 0x0BE5\n"
			]
		},

		"DspUnwatchAll": {
			"help": "Remove all DSP DMEM tracking addresses"
		},

		"DspWatchList": {
			"help": "List DSP DMEM addresses for tracking",
			"hint": "[hide]",
			"output": "Array: [address1, address2, ...]"
		}

	}
}
)json";

	const char* HwJdi = R"json(
{
  "info": {
	"description": "Various commands for debugging hardware (Flipper). Available only after emulation has been started.",
	"helpGroup": "HW Debug Commands"
  },

  "can": {

	"ramload": {
	  "help": "Load binary file to main memory",
	  "args": 2,
	  "usage": [
		"Syntax: ramload <file> <address>\n",
		"Example of use: ramload lomem.bin 0x80000000\n"
	  ]
	},

	"ramsave": {
	  "help": "Save main memory content to file",
	  "args": 3,
	  "usage": [
		"Syntax: ramsave <file> <address> <size>\n",
		"Example of use: ramsave lomem.bin 0x80000000 0x3300\n"
	  ]
	},

	"aramload": {
	  "help": "Load binary file to ARAM",
	  "args": 2,
	  "usage": [
		"Syntax: aramload <file> <address>\n",
		"Example of use: aramload samples.bin 0x10000\n"
	  ]
	},

	"aramsave": {
	  "help": "Save ARAM content to file",
	  "args": 3,
	  "usage": [
		"Syntax: aramsave <file> <address> <size>\n",
		"Example of use: aramsave samples.bin 0x10000 0x200\n"
	  ]
	},

	"nvi": {
	  "help": "Run emulation until the next VI interrupt. You can perform frame-by-frame emulation from a VI perspective"
	},

	"npe": {
	  "help": "Run emulation until the next PE interrupt (Done/Token). You can perform frame-by-frame emulation from the GFX Engine perspective"
	}

  },

  "todo": [ "Parametrize input/output Arrays"]

}
)json";

	const char* DduJdi = R"json(
{
	"info":
	{
		"description": "GAMECUBE Disk Drive Unit Jey-Dai specs.",
		"helpGroup": "DVD Debug Commands"
	},

	"can": {
		"DvdInfo": {
			"help": "Get DDU Status Information",
			"hints": "[silent]",
			"output": "Array [String File/Dir or \"\" if not mounted, Int Seek, Bool LidStatus (true: Open, false: Close)]"
		},

		"MountIso": {
			"help": "Mount GC DVD image (GCM/ISO/RVZ)",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: MountIso <file>\n",
				"Examples of use: MountIso C:\\Isos\\mygame.iso\n",
				"                 MountIso C:\\Isos\\mygame.rvz\n"
			],
			"output": "Bool Success/Fail"
		},

		"OpenLid": {
			"help": "Simulate opening of the drive cover"
		},

		"CloseLid": {
			"help": "Simulate closing of the drive cover"
		},

		"DvdStats": {
			"help": "Show some stats"
		},

		"DvdResetStats": {
			"help": "Reset stats"
		},

		"MountSDK": {
			"help": "Mount Dolphin SDK folder as virtual disk",
			"hints": "<dir>",
			"args": 1,
			"usage": [
				"Syntax: MountSDK <dir>\n",
				"Examples of use: MountSDK C:\\DolphinSDK\n"
			],
			"output": "Bool Success/Fail"
		},

		"UnmountDvd": {
			"help": "Unmount DVD (extract virtual disk)"
		},

		"DvdSeek": {
			"help": "Seek at DVD offset",
			"hints": "<pos>",
			"args": 1,
			"usage": [
				"Syntax: DvdSeek <offset>\n",
				"Offset (in bytes) must not be greater current DVD size.\n",
				"Examples of use: DvdSeek 0\n",
				"                 DvdSeek 0x2440\n"
			]
		},

		"DvdRead": {
			"help": "Read DVD data",
			"hints": "<len> [silent]",
			"args": 1,
			"usage": [
				"Syntax: DvdRead <length> [silent]\n",
				"First 32 Bytes will be dumped on screen. Length must not greater 1 MByte.\n",
				"If silent == 1, then the command works in silent mode (nothing is output to the log)\n",
				"Examples of use: DvdRead 32\n",
				"                 DvdRead 0x1000 1\n"
			],
			"output": "Array [] of bytes"
		},

		"DvdOpenFile": {
			"help": "Open file on DVD filesystem",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: DvdOpenFile <file>\n",
				"path must be absolute, including root prefix '/'\n",
				"Examples of use: DvdOpenFile \"/opening.bnr\"\n",
				"                 DvdOpenFile \"/gxTests/tex-02/ia8_odd.tpl\"\n"
			],
			"output": "File offset in bytes. 0 if file not found."
		},

		"DumpBb2": {
			"help": "Dump mounted DVDBB2 struct",
			"output": "Array [] of bytes"
		},

		"DumpFst": {
			"help": "Dump mounted DVD filesystem",
			"hints": "[0|1]",
			"output": "Array [] of bytes"
		},

		"MnDisa": {
			"help": "Disassemble DVD Firmware",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: MnDisa <file>",
				"Disassembles specified firmware dump in disa.txt",
				"Example: MnDisa gc-dvd-20010608.bin"
			]
		},

		"DvdRegionById": {
			"internal": true,
			"args": 1,
			"output": "Array [String regionName=Unknown|EUR|NOE|FRA|ESP|ITA|FAH|HOL|AUS|JPN|USA|KOR]"
		}

	}
}
)json";

	const char* GfxJdi = R"json(
{
	"info":
	{
		"description": "Flipper GFX (GX) JDI",
		"helpGroup": "GFX Debug Commands"
	},

	"can": {
		"gx":
		{
			"help": "Show the state of the whole GFX pipeline",
			"output": "Object with one member per pipeline block and the frame counters"
		},

		"gxframes":
		{
			"help": "Show the GFX frame statistics",
			"output": "Object with the frame counters and the draw command counters of the CP"
		},

		"gxregs":
		{
			"help": "Dump the register state of one GFX block",
			"args": 1,
			"hints": "<block>",
			"usage": [
				"Syntax: gxregs <block>\n",
				"Blocks: xf, su, ras, tx, tev, pe, bump, cp, all\n",
				"Example of use: gxregs tev\n"
			],
			"output": "Object with the raw register fields of the block"
		},

		"gxshader":
		{
			"help": "Write the GLSL shaders the pipeline uses into files",
			"args": 1,
			"hints": "<basename>",
			"usage": [
				"Syntax: gxshader <basename>\n",
				"Writes <basename>.vert.glsl and <basename>.frag.glsl\n",
				"Example of use: gxshader gfx_shader\n"
			],
			"output": "Object with the file names and their sizes"
		},

		"gxtex":
		{
			"help": "Show the texture cache: what every one of the eight texture maps holds",
			"output": "Array of 8 objects (one per texture map) with the decoded image and its registers"
		},

		"gxshot":
		{
			"help": "Save a screenshot of the emulated EFB as a PNG file",
			"args": 1,
			"hints": "<filename.png> [x y width height]",
			"usage": [
				"Syntax: gxshot <filename.png> [x y width height]\n",
				"Without the optional rectangle the whole render target is saved.\n",
				"Example of use: gxshot frame.png\n"
			],
			"output": "Object with the file name and the size of the saved image"
		},

		"gxpixel":
		{
			"help": "Read one EFB pixel",
			"args": 2,
			"hints": "<x> <y>",
			"usage": [
				"Syntax: gxpixel <x> <y>\n",
				"Reads the colour and the depth of one EFB pixel (origin: top left).\n",
				"Example of use: gxpixel 320 240\n"
			],
			"output": "Object with r, g, b, a and z"
		},

		"gxreset":
		{
			"help": "Reset the GFX pipeline register state (the software equivalent of a GX reset)"
		},

		"gxtexdump":
		{
			"help": "Save the image of one texture map as a PNG file",
			"args": 2,
			"hints": "<map 0-7> <filename.png>",
			"usage": [
				"Syntax: gxtexdump <map> <filename.png>\n",
				"Example of use: gxtexdump 0 map0.png\n"
			],
			"output": "Object with the file name and the size of the saved image"
		}
	}
}
)json";

	const char* HleJdi = R"json(
{
  "info": {
    "description": "HLE debug commands. Available only after emulation has been started.",
    "helpGroup": "High-level commands"
  },

  "can": {

    "syms": {
      "help": "list symbolic information",
      "args": 1,
      "usage": [
        "Syntax: syms <string> OR syms *\n",
        "<string> is the first occurance of symbol to find.\n",
        "* - list all symbols (possible overflow of message buffer).\n",
        "Examples of use: syms ma\n",
        "                 syms __z\n",
        "                 syms *\n",
        "See also: name savemap loadmap addmap\n"
      ]
    },

    "name": {
      "help": "name function (add symbol)",
      "args": 2,
      "usage": [
        "Syntax: name <addr> <symbol>\n",
        "give name to function or memory variable (add symbol).\n",
        "Example: name 0x80003100 __start\n",
        "See also: syms savemap loadmap addmap\n"
      ]
    },

    "savemap": {
      "help": "save symbolic map into file",
      "args": 1,
      "usage": [
        "Syntax: savemap <file> OR savemap .\n",
        ". is used to update current loaded map.\n",
        "path can be relative\n",
        "Examples of use: savemap .\n",
        "                 savemap Data/my.map\n",
        "See also: name loadmap addmap\n"
      ]
    },

    "DumpThreads": {
      "help": "Dump DolphinOS threads",
      "info": "Some games freeze in one of the threads, polling something. If you look from the debugger, the context switches back and forth and it is difficult to catch such moments. Dumping current Dolphin OS threads will simplify the task.",
      "output": "Array [Active Threads]"
    },

    "DumpContext": {
      "help": "Dump thread context",
      "args": 1,
      "usage": [
        "Syntax: DumpContext <effective_addr> [display]",
        "You must be sure that the address you provide is a pointer to the context, otherwise you will get garbage.",
        "Address must be effective address (DolphinOS)",
        "display: 1 - show on screen, 0 - return only serialized context (do not show). Default value is 1.",
        "Example: DumpContext 0x80331200 1"
      ],
      "output": "Array [ UInt32 gpr[32], Float fpr[32], Float psr[32], UInt32 gqr[32], UInt32 cr, UInt32 lr, UInt32 ctr, UInt32 xer, UInt32 fpscr, UInt32 srr0, UInt32 srr1, UInt16 mode, UInt16 state]  (see OSContext struct)"
    },

    "UnloadMap": {
      "help": "Unload map and clear all symbols"
    },

    "LoadMap": {
      "help": "Unload map and load new one",
      "args": 1,
      "usage": [
        "Syntax: LoadMap <file>",
        "Example: LoadMap pong.map"
      ]
    },

    "AddMap": {
      "help": "Add symbols from another map",
      "args": 1,
      "usage": [
        "Syntax: AddMap <file>",
        "If the Map file is not loaded, it loads the specifed Map and makes it current, instead of adding.",
        "Example: AddMap pong.map"
      ]
    },

    "AddressByName": {
      "help": "Get address by symbol name",
      "args": 1,
      "usage": [
        "Syntax: AddressByName <name>",
        "Example: AddressByName OSInit"
      ],
      "output": "UInt32 / 0 (not found)"
    },

    "NameByAddress": {
      "help": "Get symbol name by address",
      "args": 1,
      "usage": [
        "Syntax: NameByAddress <address>",
        "Example: NameByAddress 0x80031000"
      ],
      "output": "Array: [String] / [empty string] (not found)"
    },

    "OSDateTime": {
      "internal": true,
      "help": "Convert Gekko ticks to human-readable time (including date)",
      "hint": "[value]",
      "usage": [
        "Syntax: OSDateTime [value]"
      ],
      "output": "Array: [String]"
    },

    "OSTime": {
      "internal": true,
      "help": "Convert Gekko ticks to human-readable time (no date)",
      "hint": "[value]",
      "usage": [
        "Syntax: OSTime [value]"
      ],
      "output": "Array: [String]"
    },

    "GetNearestName": {
      "internal": true,
      "help": "Get the symbol closest to the specified address and offset relative to the start of the symbol.",
      "args": 1,
      "usage": [
        "Syntax: GetNearestName <address>"
      ],
      "output": "Object: { String name, Int offset } / nullptr (not found)"
    }

  }

}
)json";

	const char* UiJdi = R"json(
{
  "info": {
    "description": "UI Jey-Dai specs.",
    "helpGroup": "UI Commands"
  },

  "can": {

    "UIError": {
      "help": "Display UI error message",
      "args": 1,
      "hints": "<text>",
      "usage": [
        "Syntax: UIError <text>\n",
        "Example: UIError \"Error message!\"\n"
      ]
    },

    "UIReport": {
      "help": "Display UI message",
      "args": 1,
      "hints": "<text>",
      "usage": [
        "Syntax: UIReport <text>\n",
        "Example: UIReport \"Hello, world!\"\n"
      ]
    },

    "GetRenderTarget": {
      "internal": true,
      "help": "Return UI Render Target object (example: HWND). Flipper GFX will use to output graphics",
      "output": "Int"
    }

  }

}
)json";
}
