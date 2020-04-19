# Json Debug Interface (JDI)

JDI (pronounced _"Jay-Dai"_) is a Dolwin service and debugging interface that connects all the emulator components through a single hub.

The following entities are included in the JDI infrastructure:
- Json description of component debugging commands (_JDI Node_)
- Implementation of debugging commands (encapsulated inside the component)
- The common hub (_JDI Hub_) through which the execution of commands is performed

The main idea is to isolate the internal state of the emulator components, with the ability to access them using Json.

Each component implements a separate JDI Node. All JDI Nodes are accessible through a single hub (**Debug::Hub**).

## Json JDI Node format

Json consists of two main sections: **info** and **can**.

### Section info

Contains a description of the JDI Node (**description**) and the **helpGroup** header, which is shown in the help command when listing the commands for this JDI Node.

### Section can

The **can** section contains a description of what this JDI Node can do.

The section contains entries with descriptions of debug commands. Important properties:
- **help**: description of the command for help called by the help command
- **hints**: additional hints that are displayed opposite the command
- **args**: number of required arguments (not counting **args[0]**)
- **usage**: detailed help for the command if it was called with insufficient arguments
- **output**: an informative hint in case the command returns some data in the form of **Json::Value**
- **internal**: the command is not displayed in the `help` list. Internal use.

All properties except the **help** property are optional.

## Debug commands

The debug command callback (_delegate_) procedure is as follows:

```c++

typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);

```

The command accepts a list of arguments. The **args[0]** argument contains the name of the command itself.

The command may return some **Json::Value** or **nullptr**.

The return value must be deleted after use (delete).

Commands do not need to check the number of arguments. This task is performed by the JDI Hub (using the "args" property).

Command example:

```c++
	// Mount GC DVD image (GCM)
	static Json::Value* MountIso(std::vector<std::string>& args)
	{
		bool result = MountFile(args[1]);

		if (result)
		{
			DBReport2(DbgChannel::DVD, "Mounted disk image: %s\n", args[1].c_str());
		}
		else
		{
			DBReport2(DbgChannel::Error, "Failed to mount disk image: %s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = result;

		return output;
	}
```

## Debug output

Commands can output textual debugging information using the classic **DBReport** and **DBReport2** methods.

**DBReport** outputs information to a common channel. **DBReport2** outputs information to the specified channel. The list of available channels (DbgChannel) is in the Debugger.h header.

## Reflector

A special method that registers the association of a debug command with its text name.

Reflector is used only once when adding JDI Node.

Reflector example:

```c++
	void DvdCommandsReflector()
	{
		Debug::Hub.AddCmd("DvdInfo", DvdInfo);
		Debug::Hub.AddCmd("MountIso", MountIso);
		Debug::Hub.AddCmd("OpenLid", OpenLid);
		Debug::Hub.AddCmd("CloseLid", CloseLid);
		Debug::Hub.AddCmd("DvdStats", DvdStats);
		Debug::Hub.AddCmd("MountSDK", MountSDK);
		Debug::Hub.AddCmd("UnmountDvd", UnmountDvd);
		Debug::Hub.AddCmd("DvdSeek", DvdSeek);
		Debug::Hub.AddCmd("DvdRead", DvdRead);
		Debug::Hub.AddCmd("DvdOpenFile", DvdOpenFile);
	}
```

## Adding JDI Node

- Create Json Description
- Make a .cpp module with command implementation and reflector
- Call **Debug::Hub::AddNode**

## Execution of commands

The method **Debug::Hub::Execute** is used.
