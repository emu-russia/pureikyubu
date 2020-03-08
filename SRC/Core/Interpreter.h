#include "Interpreter/c_tables.h"
#include "Interpreter/c_integer.h"
#include "Interpreter/c_logical.h"
#include "Interpreter/c_compare.h"
#include "Interpreter/c_rotate.h"
#include "Interpreter/c_shift.h"
#include "Interpreter/c_loadstore.h"
#include "Interpreter/c_floatingpoint.h"
#include "Interpreter/c_fploadstore.h"
#include "Interpreter/c_pairedsingle.h"
#include "Interpreter/c_psloadstore.h"
#include "Interpreter/c_branch.h"
#include "Interpreter/c_condition.h"
#include "Interpreter/c_system.h"

// interpreter core API
void    IPTExecuteOpcode();
void    IPTException(uint32_t code);
