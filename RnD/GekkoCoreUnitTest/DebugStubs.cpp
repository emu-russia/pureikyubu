#include "pch.h"
#include "../../SRC/Debugger/Debugger.h"

static void dummy(const char* text, ...) {}
static void dummy2(DbgChannel chan, const char* text, ...) {}
void (*DBHalt)(const char* text, ...) = dummy;
void (*DBReport)(const char* text, ...) = dummy;
void (*DBReport2)(DbgChannel chan, const char* text, ...) = dummy2;
