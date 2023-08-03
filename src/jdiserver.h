// JDI Host Interface

#pragma once

Json::Value *
CallJdi(const char* request);

bool
CallJdiNoReturn(const char* request);

bool
CallJdiReturnInt(const char* request, int* valueOut);

bool
CallJdiReturnString(const char* request, char* valueOut, size_t valueSize);

bool
CallJdiReturnBool(const char* request, bool* valueOut);

void
JdiAddNode(const char* filename, JDI::JdiReflector reflector);

void
JdiRemoveNode(const char* filename);

void
JdiAddCmd(const char* name, JDI::CmdDelegate command);

void
CallJdiReturnJson(const char* request, char* reply, size_t replySize);
