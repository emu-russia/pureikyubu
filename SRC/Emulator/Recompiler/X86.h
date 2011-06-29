#define RE(b)   { cpu.recbuf[cpu.recptr++] = (u8)b; }
#define RE2(w)  { *(u16 *)((u32)cpu.recbuf + (u32)cpu.recptr) = (u16)w; cpu.recptr += 2; }
#define RE4(d)  { *(u32 *)((u32)cpu.recbuf + (u32)cpu.recptr) = (u32)d; cpu.recptr += 4; }

#define MOV_DD_IMM(addr, imm)   {RE2(0x05c7); RE4(addr); RE4(imm);}
#define MOV_EAX_DD(dd)          {RE(0xa1); RE4(dd);}
#define MOV_EDX_DD(dd)          {RE2(0x158b); RE4(dd);}
#define ADD_EAX_IMM(imm){                                               \
 if((u32)imm == 1) {RE(0x40);}                                          \
 else {RE(0x05); RE4(imm);}}
#define MOV_DD_EAX(dd)          {RE(0xa3); RE4(dd);}
#define MOV_DD_EDX(dd)          {RE2(0x1589); RE4(dd);}
#define MOV_DD_ECX(dd)          {RE2(0x0d89); RE4(dd);}
#define MOV_ECX_DD(dd)          {RE2(0x0d8b); RE4(dd);}
#define MOV_ECX_IMM(imm)        {RE(0xb9); RE4(imm);}
#define MOV_EAX_ECX()           {RE2(0xc18b);}
#define MOV_EAX_EDX()           {RE2(0xc28b);}
#define MOV_ECX_EAX()           {RE2(0xc88b);}
#define MOV_EDX_ECX()           {RE2(0xd18b);}
#define MOV_CL_IMM(imm)         {RE(0xb1); RE(imm);}
#define MOV_DL_IMM(imm)         {RE(0xb2); RE(imm);}
#define MOV_CL_EAX_EDX_IMM(imm) {RE2(0x4c8a); RE(0x10); RE(imm);}
#define ADD_ECX_IMM(imm)        {RE2(0xc181); RE4(imm);}
#define PUSH_EAX()              {RE(0x50);}
#define PUSH_ECX()              {RE(0x51);}
#define POP_ECX()               {RE(0x59);}
#define CALL_EAX()              {RE2(0xd0ff);}
#define MOV_EAX_IMM(imm)        {RE(0xb8); RE4(imm);}
#define ADD_ESP(i8)             {RE2(0xc483); RE(i8);}
#define SHL_EAX_IMM(imm)        {RE2(0xe0c1); RE(imm);}
#define SHL_ECX_IMM(imm)        {RE2(0xe1c1); RE(imm);}
#define SHR_EAX_IMM(imm)        {RE2(0xe8c1); RE(imm);}
#define SHR_ECX_IMM(imm)        {RE2(0xe9c1); RE(imm);}
#define SAR_EAX_IMM(imm)        {RE2(0xf8c1); RE(imm);}
#define SHL_EAX_CL()            {RE2(0xe0d3);}
#define SHR_EAX_CL()            {RE(0xe8d3);}
#define SAR_EAX_CL()            {RE(0xf8d3);}
#define CALL_4EAX(dd)           {RE(0xff); RE(0x14); RE(0x85); RE4(dd);}
#define ADD_DD_IMM(dd, imm){                                            \
 if(imm == 1) {RE2(0x05ff); RE4(dd); }                                  \
 else {RE(0x81); RE(0x05); RE4(dd); RE4(imm);}}
#define CALLFN(fn)              {MOV_EAX_IMM(fn); CALL_EAX();}
#define CALL_DD(dd)             {RE(0xff); RE(0x15); RE4(dd);}
#define JMPFN(fn)               {MOV_EAX_IMM(fn); JMP_EAX();}
#define NOP()                   {RE(0x90);}
#define RET()                   {RE(0xc3);}
#define INT3()                  {RE(0xcc);}
#define CMP_EAX_DD(dd)          {RE2(0x053b); RE4(dd);}
#define CMP_ECX_DD(dd)          {RE2(0x0d3b); RE4(dd);}
#define CMP_ECX_IMM(imm)        {RE(0xf981); RE4(imm);}
#define CMP_DD_IMM(dd, imm)     {RE2(0x3d81); RE4(dd); RE4(imm);}
#define CMP_EAX_EDX()           {RE2(0xc23b);}
#define CMP_ECX_EDX()           {RE2(0xca3b);}
#define INC_DD(dd)              {RE2(0x05ff); RE4(dd);}
#define INC_EAX()               {RE(0x40);}
#define INC_ECX()               {RE(0x41);}
#define INC_EDX()               {RE(0x42);}
#define DEC_DD(dd)              {RE(0x0dff); RE4(dd);}
#define MOV_EDX_IMM(imm)        {RE(0xba); RE4(imm);}
#define CALL_EDX()              {RE2(0xd2ff);}
#define OR_EAX_DD(dd)           {RE2(0x050b); RE4(dd);}
#define OR_ECX_DD(dd)           {RE2(0x0d0b); RE4(dd);}
#define OR_EAX_IMM(imm)         {RE(0x0d); RE4(imm);}
#define OR_ECX_IMM(imm)         {RE2(0xc981); RE4(imm);}
#define OR_DD_IMM(dd, imm)      {RE2(0x0d81); RE4(dd); RE4(imm);}
#define OR_DD_EAX(dd)           {RE2(0x0509); RE4(dd);}
#define OR_DD_ECX(dd)           {RE2(0x0d09); RE4(dd);}
#define OR_CL_AL()              {RE2(0xc80a);}
#define OR_DL_AL()              {RE2(0xd00a);}
#define OR_CL_DL()              {RE2(0xca0a);}
#define OR_EAX_ECX()            {RE2(0xc10b);}
#define OR_ECX_EAX()            {RE2(0xc80b);}
#define OR_CL_EAX_EDX_IMM(imm)  {RE2(0x4c0a); RE(0x10); RE(imm);}
#define XOR_EAX_IMM(imm)        {RE(0x35); RE4(imm);}
#define XOR_DD_IMM(dd, imm)     {RE2(0x3581); RE4(dd); RE4(imm);}
#define XOR_EAX_DD(dd)          {RE2(0x0533); RE4(dd);}
#define XOR_AL_IMM(imm)         {RE(0x34); RE(imm);}
#define XOR_EAX_EAX()           {RE2(0xc033);}
#define XOR_ECX_ECX()           {RE2(0xc933);}
#define XOR_EDX_EDX()           {RE2(0xd233);}
#define XOR_CL_DL()             {RE2(0xca32);}
#define SUB_EAX_DD(dd)          {RE2(0x052b); RE4(dd);}
#define SUB_ECX_DD(dd)          {RE2(0x0d2b); RE4(dd);}
#define SUB_ECX_IMM(imm)        {RE2(0xe981); RE4(imm);}
#define SUB_ECX_EAX()           {RE2(0xc82b);}
#define SUB_EAX_ECX()           {RE2(0xc12b);}
#define SUB_DD_EAX(dd)          {RE2(0x0529); RE4(dd);}
#define SUB_DD_IMM(dd, imm)     {RE2(0x2d81); RE4(dd); RE4(imm);}
#define ADD_EAX_DD(dd)          {RE2(0x0503); RE4(dd);}
#define ADD_ECX_DD(dd)          {RE2(0x0d03); RE4(dd);}
#define ADD_DD_EAX(dd)          {RE2(0x0501); RE4(dd);}
#define SETB_AL()               {RE2(0x920f); RE(0xc0);}
#define SETB_CL()               {RE2(0x920f); RE(0xc1);}
#define SETB_DL()               {RE2(0x920f); RE(0xc2);}
#define SETNB_CL()              {RE2(0x930f); RE(0xc1);}
#define SETNB_DL()              {RE2(0x930f); RE(0xc2);}
#define SETE_AL()               {RE2(0x940f); RE(0xc0);}
#define SETNE_AL()              {RE2(0x950f); RE(0xc0);}
#define SETA_AL()               {RE2(0x970f); RE(0xc0);}
#define SETL_AL()               {RE2(0x9c0f); RE(0xc0);}
#define SETG_AL()               {RE2(0x9f0f); RE(0xc0);}
#define AND_EAX_IMM(imm)        {RE(0x25); RE4(imm);}
#define AND_ECX_IMM(imm)        {RE2(0xe181); RE4(imm);}
#define AND_EAX_ECX()           {RE2(0xc123);}
#define AND_EAX_DD(dd)          {RE2(0x0523); RE4(dd);}
#define AND_ECX_DD(dd)          {RE2(0x0d23); RE4(dd);}
#define AND_DD_IMM(dd, imm)     {RE2(0x2581); RE4(dd); RE4(imm);}
#define AND_CL_IMM(imm)         {RE2(0xe180); RE(imm);}
#define AND_CL_DL()             {RE2(0xca22);}
#define JMP_DD_EAX(dd)          {RE2(0xa0ff); RE4(dd);}
#define JMP_DD_ECX(dd)          {RE2(0xa1ff); RE4(dd);}
#define JMP_EAX()               {RE2(0xe0ff);}
#define JMP_DD(dd)              {RE2(0x25ff); RE4(dd);}
#define IMUL_EDX()              {RE2(0xeaf7);}
#define IMUL_EAX_DD(dd)         {RE2(0xaf0f); RE(0x05); RE4(dd);}
#define MUL_EDX()               {RE2(0xe2f7);}
#define TEST_EAX_EAX()          {RE2(0xc085);}
#define TEST_ECX_ECX()          {RE2(0xc985);}
#define TEST_EDX_EDX()          {RE2(0xd285);}
#define TEST_EAX_EDX()          {RE2(0xd085);}
#define TEST_EAX_IMM(imm)       {RE(0xa9); RE4(imm);}
#define TEST_CL_DL()            {RE2(0xd184);}
#define NOT_EAX()               {RE2(0xd0f7);}
#define ROL_ECX_IMM(imm)        {RE2(0xc1c1); RE(imm);}
#define MOVSX_ECX_AL()          {RE2(0xbe0f); RE(0xc8);}
#define MOVSX_ECX_AX()          {RE2(0xbf0f); RE(0xc8);}
#define CDQ()                   {RE(0x99);}
#define BT_EAX_IMM(imm)         {RE2(0xba0f); RE(0xe0); RE(imm);}
#define BT_EDX_IMM(imm)         {RE2(0xba0f); RE(0xe2); RE(imm);}
#define NEG_CL()                {RE2(0xd9f6);}

// relative jumps
#define JUMP_START(slot)        {cpu.jumprel[slot&3] = cpu.recptr;}
#define JUMP_END(slot)          {cpu.recbuf[cpu.jumprel[slot&3] + 1] = (u8)(cpu.recptr - cpu.jumprel[slot&3] - 2);}
#define JMP(n)                  {RE(0xeb); RE(n);}
#define JE(n)                   {RE(0x74); RE(n);}
#define JNE(n)                  {RE(0x75); RE(n);}
#define JO(n)                   {RE(0x70); RE(n);}
#define JNO(n)                  {RE(0x71); RE(n);}
#define JA(n)                   {RE(0x77); RE(n);}
#define JAE(n)                  {RE(0x73); RE(n);}
#define JB(n)                   {RE(0x72); RE(n);}
#define JBE(n)                  {RE(0x76); RE(n);}
#define JG(n)                   {RE(0x7f); RE(n);}
#define JGE(n)                  {RE(0x7d); RE(n);}
#define JL(n)                   {RE(0x7c); RE(n);}
#define JLE(n)                  {RE(0x7e); RE(n);}
#define JC(n)                   {RE(0x72); RE(n);}
#define JNC(n)                  {RE(0x73); RE(n);}
