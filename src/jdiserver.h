// JDI Host Interface
//
// Every request and every reply of this interface is UTF-8 (issue #372): `request` is a UTF-8
// command line, `CallJdiReturnString` fills the caller's buffer with the UTF-8 form of the answer,
// and the names passed to JdiAddNode/JdiRemoveNode/JdiAddCmd are UTF-8 as well.

#pragma once

Json::Value *
CallJdi(const char* request);

bool
CallJdiNoReturn(const char* request);

bool
CallJdiReturnInt(const char* request, int* valueOut);

// `valueSize` is the room the caller leaves for the text alone (the callers pass the size of their
// buffer minus one, which is where the terminator goes).
bool
CallJdiReturnString(const char* request, char* valueOut, size_t valueSize);

bool
CallJdiReturnBool(const char* request, bool* valueOut);

void
JdiAddNode(const char* filename, const char *jsonText, JDI::JdiReflector reflector);

void
JdiRemoveNode(const char* filename);

void
JdiAddCmd(const char* name, JDI::CmdDelegate command);

void
CallJdiReturnJson(const char* request, char* reply, size_t replySize);
