/* $VER: disasm.cpp V1.5 (10 Aug 2004)
 *
 * Disassembler module for the PowerPC microprocessor family
 * Copyright (c) 1998-2002  Frank Wille (phx)
 *               2003-2004  Andrei Shestakov (org)
 *
 * disasm.cpp is freeware and may be freely redistributed as long as
 * no modifications are made and nothing is charged for it.
 * Non-commercial usage is allowed without any restrictions.
 * EVERY PRODUCT OR PROGRAM DERIVED DIRECTLY FROM MY SOURCE MAY NOT BE
 * SOLD COMMERCIALLY WITHOUT PERMISSION FROM THE AUTHOR.
 *
 *
 * v1.5  (06.10.2004) org
 *       added dcbz_l for paired-singles.
 * v1.4  (26.10.2003) org
 *       ps_merge10 was not recognized.
 *       frsqrte was fsqrte.
 * v1.3  (05.10.2003) org
 *       GEKKO paired-singles finished.
 *       code cleaned up, added spaces between commas, for best view.
 *       mtfsf fixed.
 * v1.2  (31.07.2003) org
 *       modified for IBM PowerPC Gekko.
 * v1.1  (19.02.2000) phx
 *       fabs wasn't recognized.
 * v1.0  (30.01.2000) phx
 *       stfsx, stfdx, lfsx, lfdx, stfsux, stfdux, lfsux, lfdux, etc.
 *       printed "rd,ra,rb" as operands instead "fd,ra,rb".
 * v0.4  (01.06.1999) phx
 *       'stwm' shoud have been 'stmw'.
 * v0.3  (17.11.1998) phx
 *       The OE-types (e.g. addo, subfeo, etc.) didn't work for all
 *       instructions.
 *       AA-form branches have an absolute destination.
 *       addze and subfze must not have a third operand.
 *       sc was not recognized.
 * v0.2  (29.05.1998) phx
 *       Sign error. SUBI got negative immediate values.
 * v0.1  (23.05.1998) phx
 *       First version, which implements all PowerPC instructions.
 * v0.0  (09.05.1998) phx
 *       File created.
 */

#include "dolphin.h"

#define GEKKO                   /* allow Gekko specific opcodes */

/* Disassembler structure, the interface to the application */

typedef unsigned long ppc_word;

struct PPCDisasm
{
  ppc_word *instr;              /* pointer to instruction to disassemble */
  ppc_word *iaddr;              /* instr.addr., usually the same as instr */
  char *opcode;                 /* buffer for opcode, min. 10 chars. */
  char *operands;               /* operand buffer, min. 24 chars. */
/* changed by disassembler: */
  unsigned char type;           /* type of instruction, see below */
  unsigned char flags;          /* additional flags */
  unsigned short sreg;          /* register in load/store instructions */
  ppc_word displacement;        /* branch- or load/store displacement */
};

/* endianess */
#define BIGENDIAN

/* general defines */
#define PPCIDXMASK      0xfc000000
#define PPCIDX2MASK     0x000007fe
#define PPCDMASK        0x03e00000
#define PPCAMASK        0x001f0000
#define PPCBMASK        0x0000f800
#define PPCCMASK        0x000007c0
#define PPCMMASK        0x0000003e
#define PPCCRDMASK      0x03800000
#define PPCCRAMASK      0x001c0000
#define PPCLMASK        0x00600000
#define PPCOE           0x00000400

#define PPCIDXSH        26
#define PPCDSH          21
#define PPCASH          16
#define PPCBSH          11
#define PPCCSH          6
#define PPCMSH          1
#define PPCCRDSH        23
#define PPCCRASH        18
#define PPCLSH          21
#define PPCIDX2SH       1

#define PPCGETIDX(x)    (((x)&PPCIDXMASK)>>PPCIDXSH)
#define PPCGETD(x)      (((x)&PPCDMASK)>>PPCDSH)
#define PPCGETA(x)      (((x)&PPCAMASK)>>PPCASH)
#define PPCGETB(x)      (((x)&PPCBMASK)>>PPCBSH)
#define PPCGETC(x)      (((x)&PPCCMASK)>>PPCCSH)
#define PPCGETM(x)      (((x)&PPCMMASK)>>PPCMSH)
#define PPCGETCRD(x)    (((x)&PPCCRDMASK)>>PPCCRDSH)
#define PPCGETCRA(x)    (((x)&PPCCRAMASK)>>PPCCRASH)
#define PPCGETL(x)      (((x)&PPCLMASK)>>PPCLSH)
#define PPCGETIDX2(x)   (((x)&PPCIDX2MASK)>>PPCIDX2SH)

#define PPCINSTR_OTHER      0   /* no additional info for other instr. */
#define PPCINSTR_BRANCH     1   /* branch dest. = PC+displacement */
#define PPCINSTR_LDST       2   /* load/store instruction: displ(sreg) */
#define PPCINSTR_IMM        3   /* 16-bit immediate val. in displacement */

#define PPCF_ILLEGAL   (1<<0)   /* illegal PowerPC instruction */
#define PPCF_UNSIGNED  (1<<1)   /* unsigned immediate instruction */
#define PPCF_SUPER     (1<<2)   /* supervisor level instruction */
#define PPCF_64        (1<<3)   /* 64-bit only instruction */

/* Local data */

static char *trap_condition[32] = {
  NULL,"lgt","llt",NULL,"eq","lge","lle",NULL,
  "gt",NULL,NULL,NULL,"ge",NULL,NULL,NULL,
  "lt",NULL,NULL,NULL,"le",NULL,NULL,NULL,
  "ne",NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

static char *cmpname[4] = {
  "cmpw","cmpd","cmplw","cmpld"
};

static char *b_ext[4] = {
  "","l","a","la"
};

static char *b_condition[8] = {
  "ge","le","ne","ns","lt","gt","eq","so"
};

static char *b_decr[16] = {
  "nzf","zf",NULL,NULL,"nzt","zt",NULL,NULL,
  "nz","z",NULL,NULL,"nz","z",NULL,NULL
};

static char *regsel[2] = {
  "","r"
};

static char *oesel[2] = {
  "","o"
};

static char *rcsel[2] = {
  "","."
};

static char *ldstnames[] = {
  "lwz","lwzu","lbz","lbzu","stw","stwu","stb","stbu","lhz","lhzu",
  "lha","lhau","sth","sthu","lmw","stmw","lfs","lfsu","lfd","lfdu",
  "stfs","stfsu","stfd","stfdu"
};

static char *regnames[] = {
 "r0" , "sp" , "sd2", "r3" , "r4" , "r5" , "r6" , "r7" , 
 "r8" , "r9" , "r10", "r11", "r12", "sd1", "r14", "r15", 
 "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
 "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

static char *spr_name(int n)
{
    static char def[8];

    switch(n)
    {
        case 1: return "XER";
        case 8: return "LR";
        case 9: return "CTR";
        case 18: return "DSISR";
        case 19: return "DAR";
        case 22: return "DEC";
        case 25: return "SDR1";
        case 26: return "SRR0";
        case 27: return "SRR1";
        case 272: return "SPRG0";
        case 273: return "SPRG1";
        case 274: return "SPRG2";
        case 275: return "SPRG3";
        case 282: return "EAR";
        case 287: return "PVR";
        case 528: return "IBAT0U";
        case 529: return "IBAT0L";
        case 530: return "IBAT1U";
        case 531: return "IBAT1L";
        case 532: return "IBAT2U";
        case 533: return "IBAT2L";
        case 534: return "IBAT3U";
        case 535: return "IBAT3L";
        case 536: return "DBAT0U";
        case 537: return "DBAT0L";
        case 538: return "DBAT1U";
        case 539: return "DBAT1L";
        case 540: return "DBAT2U";
        case 541: return "DBAT2L";
        case 542: return "DBAT3U";
        case 543: return "DBAT3L";
        case 936: return "UMMCR0";
        case 937: return "UPMC1";
        case 938: return "UPMC2";
        case 939: return "USIA";
        case 941: return "UPMC3";
        case 942: return "UPMC4";
        case 943: return "USDA";
        case 952: return "MMCR0";
        case 953: return "PMC1";
        case 954: return "PMC2";
        case 955: return "SIA";
        case 956: return "MMCR1";
        case 957: return "PMC3";
        case 958: return "PMC4";
        case 959: return "SDA";
        case 1008: return "HID0";
        case 1009: return "HID1";
        case 1010: return "IABR";
        case 1013: return "DABR";
        case 1017: return "L2CR";
        case 1019: return "ICTC";
        case 1020: return "THRM1";
        case 1021: return "THRM2";
        case 1022: return "THRM3";

        /* Gekko-specific SPRs */
#ifdef GEKKO
        case 912: return "GQR0";
        case 913: return "GQR1";
        case 914: return "GQR2";
        case 915: return "GQR3";
        case 916: return "GQR4";
        case 917: return "GQR5";
        case 918: return "GQR6";
        case 919: return "GQR7";
        case 920: return "HID2";
        case 921: return "WPAR";
        case 922: return "DMAU";
        case 923: return "DMAL";
        case 940: return "UMMCR1";
#endif
    }

    sprintf(def, "%u", n);
    return def;
}


static ppc_word swapda(ppc_word w)
{
  return ((w&0xfc00ffff)|((w&PPCAMASK)<<5)|((w&PPCDMASK)>>5));
}


static ppc_word swapab(ppc_word w)
{
  return ((w&0xffe007ff)|((w&PPCBMASK)<<5)|((w&PPCAMASK)>>5));
}


static void ill(struct PPCDisasm *dp,ppc_word in)
{
/*/
  strcpy(dp->opcode,".word");
  sprintf(dp->operands,"0x%08lx",(unsigned long)in);
/*/

  /* show empty */
  strcpy(dp->opcode,"");
  sprintf(dp->operands,"");

  dp->flags |= PPCF_ILLEGAL;
}


static void imm(struct PPCDisasm *dp,ppc_word in,int uimm,int type,int hex)
/* Generate immediate instruction operand. */
/* type 0: D-mode, D,A,imm */
/* type 1: S-mode, A,S,imm */
/* type 2: S/D register is ignored (trap,cmpi) */
/* type 3: A register is ignored (li) */
{
  int i = (int)(in & 0xffff);

  dp->type = PPCINSTR_IMM;
  if (!uimm) {
    if (i > 0x7fff)
      i -= 0x10000;
  }
  else
    dp->flags |= PPCF_UNSIGNED;
  dp->displacement = i;

  switch (type) {
    case 0:
      sprintf(dp->operands,"%s, %s, %d",regnames[(int)PPCGETD(in)],regnames[(int)PPCGETA(in)],i);
      break;
    case 1:
      if(hex)
         sprintf(dp->operands,"%s, %s, 0x%.4X",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETD(in)],i);
      else
         sprintf(dp->operands,"%s, %s, %d",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETD(in)],i);
      break;
    case 2:
      sprintf(dp->operands,"%s, %d",regnames[(int)PPCGETA(in)],i);
      break;
    case 3:
      if(hex)
         sprintf(dp->operands,"%s, 0x%.4X",regnames[(int)PPCGETD(in)],i);
      else
         sprintf(dp->operands,"%s, %d",regnames[(int)PPCGETD(in)],i);
      break;
  }
}


static void ra_rb(char *s,ppc_word in)
{
  sprintf(s,"%s, %s",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETB(in)]);
}


static char *rd_ra_rb(char *s,ppc_word in,int mask)
{
  static const char *fmt = "%s, ";

  if (mask) {
    if (mask & 4)
      s += sprintf(s,fmt,regnames[(int)PPCGETD(in)]);
    if (mask & 2)
      s += sprintf(s,fmt,regnames[(int)PPCGETA(in)]);
    if (mask & 1)
      s += sprintf(s,fmt,regnames[(int)PPCGETB(in)]);
    *--s = '\0';
    *--s = '\0';
  }
  else
    *s = '\0';
  return (s);
}


static char *fd_ra_rb(char *s,ppc_word in,int mask)
{
  static const char *ffmt = "f%d, ";
  static const char *rfmt = "%s,";

  if (mask) {
    if (mask & 4)
      s += sprintf(s,ffmt,(int)PPCGETD(in));
    if (mask & 2)
      s += sprintf(s,rfmt,regnames[(int)PPCGETA(in)]);
    if (mask & 1)
      s += sprintf(s,rfmt,regnames[(int)PPCGETB(in)]);
    s--;
    *--s = '\0';
  }
  else
    *s = '\0';
  return (s);
}


static void trapi(struct PPCDisasm *dp,ppc_word in,unsigned char dmode)
{
  char *cnd;

  if (cnd = trap_condition[PPCGETD(in)]) {
    dp->flags |= dmode;
    sprintf(dp->opcode,"t%c%s",dmode?'d':'w',cnd);
    imm(dp,in,0,2,0);
  }
  else
    ill(dp,in);
}


static void cmpi(struct PPCDisasm *dp,ppc_word in,int uimm)
{
  char *oper = dp->operands;
  int i = (int)PPCGETL(in);

  if (i < 2) {
    if (i)
      dp->flags |= PPCF_64;
    sprintf(dp->opcode,"%si",cmpname[uimm*2+i]);
    if (i = (int)PPCGETCRD(in)) {
      sprintf(oper,"cr%c, ",'0'+i);
      dp->operands += 5;
    }
    imm(dp,in,uimm,2,0);
    dp->operands = oper;
  }
  else
    ill(dp,in);
}


static void addi(struct PPCDisasm *dp,ppc_word in,char *ext)
{
  if ((in&0x08000000) && !PPCGETA(in)) {
    sprintf(dp->opcode,"l%s",ext);  /* li, lis */
    if(!strcmp(ext, "i"))
       imm(dp,in,0,3,0);
    else
       imm(dp,in,1,3,1);
  }
  else {
    sprintf(dp->opcode,"%s%s",(in&0x8000)?"sub":"add",ext);
    if (in & 0x8000)
      in = (in^0xffff) + 1;
    imm(dp,in,1,0,0);
  }
}


/* build a branch instr. and return number of chars written to operand */
static int branch(struct PPCDisasm *dp,ppc_word in,char *bname,int aform,int bdisp)
{
  int bo = (int)PPCGETD(in);
  int bi = (int)PPCGETA(in);
  char y = (char)(bo & 1);
  int opercnt = 0;
  char *ext = b_ext[aform*2+(int)(in&1)];

  if (bdisp < 0)
    y ^= 1;
  y = y ? '+':'-';

  if (bo & 4) {
    /* standard case - no decrement */
    if (bo & 16) {
      /* branch always */
      if (PPCGETIDX(in) != 16) {
        sprintf(dp->opcode,"b%s%s",bname,ext);
      }
      else {
        sprintf(dp->opcode,"bc%s",ext);
        opercnt = sprintf(dp->operands,"%d, %d",bo,bi);
      }
    }
    else {
      /* branch conditional */
      sprintf(dp->opcode,"b%s%s%s%c",b_condition[((bo&8)>>1)+(bi&3)],
              bname,ext,y);
      if (bi >= 4)
        opercnt = sprintf(dp->operands,"cr%d",bi>>2);
    }
  }

  else {
    /* CTR is decremented and checked */
    sprintf(dp->opcode,"bd%s%s%s%c",b_decr[bo>>1],bname,ext,y);
    if (!(bo & 16))
      opercnt = sprintf(dp->operands,"%d",bi);
  }

  return (opercnt);
}


static void bc(struct PPCDisasm *dp,ppc_word in)
{
  unsigned long d = (int)(in & 0xfffc);
  int offs;
  char *oper = dp->operands;

  if(d & 0x8000) d |= 0xffff0000;

  if (offs = branch(dp,in,"",(in&2)?1:0,d)) {
    oper += offs;
    *oper++ = ',';
    *oper++ = ' ';
  }
  if (in & 2)  /* AA ? */
  {
    sprintf(dp->operands,"0x%08X",(unsigned long)d);
  }
  else
  {
    sprintf(oper,"0x%08X",(unsigned long)(*dp->iaddr) + d);
  }
  dp->type = PPCINSTR_BRANCH;
  dp->displacement = (ppc_word)d;
}


static void bli(struct PPCDisasm *dp,ppc_word in)
{
  unsigned long d = (unsigned long)(in & 0x3fffffc);

  if(d & 0x02000000) d |= 0xfc000000;

  sprintf(dp->opcode,"b%s",b_ext[in&3]);
  if (in & 2)  /* AA ? */
  {
    sprintf(dp->operands,"0x%08X",(unsigned long)d);
  }
  else
  {
    sprintf(dp->operands,"0x%08X",(unsigned long)(*dp->iaddr) + d);
  }
  dp->type = PPCINSTR_BRANCH;
  dp->displacement = (ppc_word)d;
}


static void mcrf(struct PPCDisasm *dp,ppc_word in,char c)
{
  if (!(in & 0x0063f801)) {
    sprintf(dp->opcode,"mcrf%c",c);
    sprintf(dp->operands,"cr%d, cr%d",(int)PPCGETCRD(in),(int)PPCGETCRA(in));
  }
  else
    ill(dp,in);
}


static void crop(struct PPCDisasm *dp,ppc_word in,char *n1,char *n2)
{
  int crd = (int)PPCGETD(in);
  int cra = (int)PPCGETA(in);
  int crb = (int)PPCGETB(in);

  if (!(in & 1)) {
    sprintf(dp->opcode,"cr%s",(cra==crb && n2)?n2:n1);
    if (cra == crb && n2)
      sprintf(dp->operands,"%d, %d",crd,cra);
    else
      sprintf(dp->operands,"%d, %d, %d",crd,cra,crb);
  }
  else
    ill(dp,in);
}


static void nooper(struct PPCDisasm *dp,ppc_word in,char *name,
                   unsigned char dmode)
{
  if (in & (PPCDMASK|PPCAMASK|PPCBMASK|1)) {
    ill(dp,in);
  }
  else {
    dp->flags |= dmode;
    strcpy(dp->opcode,name);
  }
}


static void rlw(struct PPCDisasm *dp,ppc_word in,char *name,int i)
{
  int s = (int)PPCGETD(in);
  int a = (int)PPCGETA(in);
  int bsh = (int)PPCGETB(in);
  int mb = (int)PPCGETC(in);
  int me = (int)PPCGETM(in);

  sprintf(dp->opcode,"rlw%s%c",name,in&1?'.':'\0');
  sprintf(dp->operands,"%s, %s, %s%d, %d, %d",regnames[a],regnames[s],regsel[i],bsh,mb,me);
}


static void ori(struct PPCDisasm *dp,ppc_word in,char *name)
{
  strcpy(dp->opcode,name);
  imm(dp,in,1,1,1);
}


static void rld(struct PPCDisasm *dp,ppc_word in,char *name,int i)
{
  int s = (int)PPCGETD(in);
  int a = (int)PPCGETA(in);
  int bsh = i ? (int)PPCGETB(in) : (int)(((in&2)<<4)+PPCGETB(in));
  int m = (int)(in&0x7e0)>>5;

  dp->flags |= PPCF_64;
  sprintf(dp->opcode,"rld%s%c",name,in&1?'.':'\0');
  sprintf(dp->operands,"%s, %s, %s%d, %d",regnames[a],regnames[s],regsel[i],bsh,m);
}


static void cmp(struct PPCDisasm *dp,ppc_word in)
{
  char *oper = dp->operands;
  int i = (int)PPCGETL(in);

  if (i < 2) {
    if (i)
      dp->flags |= PPCF_64;
    strcpy(dp->opcode,cmpname[((in&PPCIDX2MASK)?2:0)+i]);
    if (i = (int)PPCGETCRD(in))
      oper += sprintf(oper,"cr%c, ",'0'+i);
    ra_rb(oper,in);
  }
  else
    ill(dp,in);
}


static void trap(struct PPCDisasm *dp,ppc_word in,unsigned char dmode)
{
  char *cnd;
  int to = (int)PPCGETD(in);

  if (cnd = trap_condition[to]) {
    dp->flags |= dmode;
    sprintf(dp->opcode,"t%c%s",dmode?'d':'w',cnd);
    ra_rb(dp->operands,in);
  }
  else {
    if (to == 31) {
      if (dmode) {
        dp->flags |= dmode;
        strcpy(dp->opcode,"td");
        strcpy(dp->operands,"31,0,0");
      }
      else
        strcpy(dp->opcode,"trap");
    }
    else
      ill(dp,in);
  }
}


static void dab(struct PPCDisasm *dp,ppc_word in,char *name,int mask,
                int smode,int chkoe,int chkrc,unsigned char dmode)
/* standard instruction: xxxx rD,rA,rB */
{
  if (chkrc>=0 && ((in&1)!=(unsigned)chkrc)) {
    ill(dp,in);
  }
  else {
    dp->flags |= dmode;
    if (smode)
      in = swapda(in);  /* rA,rS,rB */
    sprintf(dp->opcode,"%s%s%s",name,
            oesel[chkoe&&(in&PPCOE)],rcsel[(chkrc<0)&&(in&1)]);
    rd_ra_rb(dp->operands,in,mask);
  }
}


static void rrn(struct PPCDisasm *dp,ppc_word in,char *name,
                int smode,int chkoe,int chkrc,unsigned char dmode)
/* Last operand is no register: xxxx rD,rA,NB */
{
  char *s;

  if (chkrc>=0 && ((in&1)!=(unsigned)chkrc)) {
    ill(dp,in);
  }
  else {
    dp->flags |= dmode;
    if (smode)
      in = swapda(in);  /* rA,rS,NB */
    sprintf(dp->opcode,"%s%s%s",name,
            oesel[chkoe&&(in&PPCOE)],rcsel[(chkrc<0)&&(in&1)]);
    s = rd_ra_rb(dp->operands,in,6);
    sprintf(s,", %d",(int)PPCGETB(in));
  }
}


static void mtcr(struct PPCDisasm *dp,ppc_word in)
{
  int s = (int)PPCGETD(in);
  int crm = (int)(in&0x000ff000)>>12;
  char *oper = dp->operands;

  if (in & 0x00100801) {
    ill(dp,in);
  }
  else {
    sprintf(dp->opcode,"mtcr%c",crm==0xff?'\0':'f');
    if (crm != 0xff)
      oper += sprintf(oper,"0x%02x,",crm);
    sprintf(oper,"%s",regnames[s]);
  }
}


static void msr(struct PPCDisasm *dp,ppc_word in,int smode)
{
  int s = (int)PPCGETD(in);
  int sr = (int)(in&0x000f0000)>>16;

  if (in & 0x0010f801) {
    ill(dp,in);
  }
  else {
    dp->flags |= PPCF_SUPER;
    sprintf(dp->opcode,"m%csr",smode?'t':'f');
    if (smode)
      sprintf(dp->operands,"%d, %s",sr,regnames[s]);
    else
      sprintf(dp->operands,"%s, %d",regnames[s],sr);
  }
}


static void mspr(struct PPCDisasm *dp,ppc_word in,int smode)
{
  int d = (int)PPCGETD(in);
  int spr = (int)((PPCGETB(in)<<5)+PPCGETA(in));
  int fmt = 0;
  char *x;

  if (in & 1) {
    ill(dp,in);
  }

  else {
    if (spr!=1 && spr!=8 && spr!=9)
      dp->flags |= PPCF_SUPER;
    switch (spr) {
      case 1:
        x = "xer";
        break;
      case 8:
        x = "lr";
        break;
      case 9:
        x = "ctr";
        break;
      default:
        x = "spr";
        fmt = 1;
        break;
    }

    sprintf(dp->opcode,"m%c%s",smode?'t':'f',x);
    if (fmt) {
      if (smode)
        sprintf(dp->operands,"%s, %s",spr_name(spr),regnames[d]);
      else
        sprintf(dp->operands,"%s, %s",regnames[d],spr_name(spr));
    }
    else
      sprintf(dp->operands,"%s",regnames[d]);
  }
}


static void mtb(struct PPCDisasm *dp,ppc_word in)
{
  int d = (int)PPCGETD(in);
  int tbr = (int)((PPCGETB(in)<<5)+PPCGETA(in));
  char *s = dp->operands;
  char x;

  if (in & 1) {
    ill(dp,in);
  }

  else {
    s += sprintf(s,"%s",regnames[d]);
    switch (tbr) {
      case 268:
        x = 'l';
        break;
      case 269:
        x = 'u';
        break;
      default:
        x = '\0';
        dp->flags |= PPCF_SUPER;
        sprintf(s,", %d",tbr);
        break;
    }
    sprintf(dp->opcode,"mftb%c",x);
  }
}


static void sradi(struct PPCDisasm *dp,ppc_word in)
{
  int s = (int)PPCGETD(in);
  int a = (int)PPCGETA(in);
  int bsh = (int)(((in&2)<<4)+PPCGETB(in));

  dp->flags |= PPCF_64;
  sprintf(dp->opcode,"sradi%c",in&1?'.':'\0');
  sprintf(dp->operands,"%s, %s, %d",regnames[a],regnames[s],bsh);
}

static char *ldst_offs(unsigned long val)
{
    static char buf[8];

    if(val == 0)
    {
        return "0";
    }
    else
    {
        if(val <= 128)
        {
            sprintf(buf, "%i", val);
            return buf;
        }

        if(val & 0x8000)
        {
            sprintf(buf, "-0x%.4X", ((~val) & 0xffff) + 1);
        }
        else
        {
            sprintf(buf, "0x%.4X", val);
        }

        return buf;
    }
}

static void ldst(struct PPCDisasm *dp,ppc_word in,char *name,char reg,unsigned char dmode)
{
  int s = (int)PPCGETD(in);
  int a = (int)PPCGETA(in);
  int d = (ppc_word)(in & 0xffff);

  dp->type = PPCINSTR_LDST;
  dp->flags |= dmode;
  dp->sreg = (short)a;
  dp->displacement = (ppc_word)d;
  strcpy(dp->opcode,name);
  if(reg == 'r')
  {
    sprintf(dp->operands,"%s, %s (%s)", regnames[s], ldst_offs(d), regnames[a]);
  }
  else
  {
    sprintf(dp->operands,"%c%d, %s (%s)",reg,s, ldst_offs(d), regnames[a]);
  }
}

/* standard floating point instruction: xxxx fD,fA,fB,fC */
static void fdabc(struct PPCDisasm *dp,ppc_word in,char *name,int mask,unsigned char dmode)
{
  static const char *fmt = "f%d, ";
  char *s = dp->operands;
  int err = 0;

  dp->flags |= dmode;
  sprintf(dp->opcode,"f%s%s",name,rcsel[in&1]);
  s += sprintf(s,fmt,(int)PPCGETD(in));
  if (mask & 4)
    s += sprintf(s,fmt,(int)PPCGETA(in));
  else
    err |= (int)PPCGETA(in);
  if (mask & 2)
    s += sprintf(s,fmt,(int)PPCGETB(in));
  else if (PPCGETB(in))
    err |= (int)PPCGETB(in);
  if (mask & 1)
    s += sprintf(s,fmt,(int)PPCGETC(in));
  else if (!(mask&8))
    err |= (int)PPCGETC(in);
  *(s-2) = '\0';
  if (err)
    ill(dp,in);
}

/* indexed float instruction: xxxx fD,rA,rB */
static void fdab(struct PPCDisasm *dp,ppc_word in,char *name,int mask)
{
  strcpy(dp->opcode,name);
  fd_ra_rb(dp->operands,in,mask);
}

static void fcmp(struct PPCDisasm *dp,ppc_word in,char c)
{
  if (in & 0x00600001) {
    ill(dp,in);
  }
  else {
    sprintf(dp->opcode,"fcmp%c",c);
    sprintf(dp->operands,"cr%d, f%d, f%d",(int)PPCGETCRD(in),
            (int)PPCGETA(in),(int)PPCGETB(in));
  }
}


static void mtfsb(struct PPCDisasm *dp,ppc_word in,int n)
{
  if (in & (PPCAMASK|PPCBMASK)) {
    ill(dp,in);
  }
  else {
    sprintf(dp->opcode,"mtfsb%d%s",n,rcsel[in&1]);
    sprintf(dp->operands,"%d",(int)PPCGETD(in));
  }
}


/* *** PAIRED SINGLE SET *** */

#ifdef GEKKO

static void ps_cmpx(struct PPCDisasm *dp, ppc_word in, int n)
{
    static char *fix[] = { "u0", "o0", "u1", "o1" };
    sprintf(dp->opcode, "ps_cmp%s", fix[n]);
    sprintf(dp->operands, "cr%d, f%d, f%d", (int)PPCGETCRD(in), (int)PPCGETA(in), (int)PPCGETB(in));
}

static char *ps_ldst_offs(unsigned long val)
{
    static char buf[8];

    if(val == 0)
    {
        return "0";
    }
    else
    {
        if(val <= 128)
        {
            sprintf(buf, "%i", val);
            return buf;
        }

        if(val & 0x800)
        {
            sprintf(buf, "-0x%.4X", ((~val) & 0xfff) + 1);
        }
        else
        {
            sprintf(buf, "0x%.4X", val);
        }

        return buf;
    }
}

static void ps_ldst(struct PPCDisasm *dp, ppc_word in, char *fix)
{
  int s = (int)PPCGETD(in);
  int a = (int)PPCGETA(in);
  int d = (ppc_word)(in & 0xfff);
  sprintf(dp->opcode, "psq_%s", fix);  
  sprintf(dp->operands, "f%d, %s (%s), %d, %d", s, ps_ldst_offs(d), regnames[a], (in >> 15) & 1, (in >> 12) & 7);
}

static void ps_ldstx(struct PPCDisasm *dp, ppc_word in, char *fix)
{
    int a = (int)PPCGETA(in), b = (int)PPCGETB(in);
    sprintf(dp->opcode, "psq_%s", fix);
    sprintf(dp->operands, "f%d, %s, %s, %d, %d", (int)PPCGETD(in), regnames[a], regnames[b], (in >> 10) & 1, (in >> 7) & 7);
}

static void ps_dacb(struct PPCDisasm *dp, ppc_word in, char *fix)
{
    int a = (int)PPCGETA(in), b = (int)PPCGETB(in), c = (int)PPCGETC(in), d = (int)PPCGETD(in);
    if(in & 1)
      sprintf(dp->opcode, "ps_%s.", fix);
    else
      sprintf(dp->opcode, "ps_%s", fix);
    sprintf(dp->operands, "f%d, f%d, f%d, f%d", d, a, c, b);
}

static void ps_dac(struct PPCDisasm *dp, ppc_word in, char *fix)
{
    int a = (int)PPCGETA(in), c = (int)PPCGETC(in), d = (int)PPCGETD(in);
    if(in & 1)
      sprintf(dp->opcode, "ps_%s.", fix);
    else
      sprintf(dp->opcode, "ps_%s", fix);
    sprintf(dp->operands, "f%d, f%d, f%d", d, a, c);
}

static void ps_dab(struct PPCDisasm *dp, ppc_word in, char *fix)
{
    int d = (int)PPCGETD(in), a = (int)PPCGETA(in), b = (int)PPCGETB(in);
    if(in & 1)
      sprintf(dp->opcode, "ps_%s.", fix);
    else
      sprintf(dp->opcode, "ps_%s", fix);
    sprintf(dp->operands, "f%d, f%d, f%d", d, a, b);
}

static void ps_db(struct PPCDisasm *dp, ppc_word in, char *fix)
{
    int d = (int)PPCGETD(in), b = (int)PPCGETB(in);
    if(in & 1)
      sprintf(dp->opcode, "ps_%s.", fix);
    else
      sprintf(dp->opcode, "ps_%s", fix);
    sprintf(dp->operands, "f%d, f%d", d, b);
}

static void ps_dcbz_l(struct PPCDisasm *dp, ppc_word in)
{
    int a = (int)PPCGETA(in), b = (int)PPCGETB(in);
    sprintf(dp->opcode, "dcbz_l");
    sprintf(dp->operands, "r%d, r%d", a, b);
}

#endif /* GEKKO */

/* Disassemble PPC instruction and return a pointer to the next */
/* instruction, or NULL if an error occured. */
static ppc_word *PPC_Disassemble(struct PPCDisasm *dp)
{
  char *tc;
  ppc_word in = *(dp->instr);

  if (dp->opcode==NULL || dp->operands==NULL)
    return (NULL);  /* no buffers */

#ifdef LITTLEENDIAN
  in = (in & 0xff)<<24 | (in & 0xff00)<<8 | (in & 0xff0000)>>8 |
       (in & 0xff000000)>>24;
#endif

  dp->type = PPCINSTR_OTHER;
  dp->flags = 0;
  *(dp->operands) = 0;

  switch (PPCGETIDX(in)) {
    case 2:
      trapi(dp,in,PPCF_64);  /* tdi */
      break;

    case 3:
      trapi(dp,in,0);  /* twi */
      break;

#ifdef GEKKO

    case 4:     /* ps */
      if(PPCGETIDX2(in) == 1014)                    /* dcbz_l */
      {
        ps_dcbz_l(dp, in);
      }
      else switch((in >> 1) & 0x1f)
      {
        case 0:
          ps_cmpx(dp, in, (in >> 6) & 3);           /* ps_cmpxx */
          break;
        case 6:
          if(in & 0x40) ps_ldstx(dp, in, "lux");    /* psq_lux */
          else ps_ldstx(dp, in, "lx");              /* psq_lx */
          break;
        case 7:
          if(in & 0x40) ps_ldstx(dp, in, "stux");   /* psq_stux */
          else ps_ldstx(dp, in, "stx");             /* psq_stx */
          break;
        case 8:
          switch((in >> 6) & 0x1f)
          {
            case 1:
              ps_db(dp, in, "neg");                 /* ps_neg */
              break;
            case 2:
              ps_db(dp, in, "mr");                  /* ps_mr */
              break;
            case 4:
              ps_db(dp, in, "nabs");                /* ps_nabs */
              break;
            case 8:
              ps_db(dp, in, "abs");                 /* ps_abs */
              break;
            default:
              ill(dp, in);
          }
          break;
        case 10:
          ps_dacb(dp, in, "sum0");                  /* ps_sum0 */
          break;
        case 11:
          ps_dacb(dp, in, "sum1");                  /* ps_sum1 */
          break;
        case 12:
          ps_dac(dp, in, "muls0");                  /* ps_muls0 */
          break;
        case 13:
          ps_dac(dp, in, "muls1");                  /* ps_muls1 */
          break;
        case 14:
          ps_dacb(dp, in, "madds0");                /* ps_madds0 */
          break;
        case 15:
          ps_dacb(dp, in, "madds1");                /* ps_madds1 */
          break;
        case 16:
          switch((in >> 6) & 0x1f)
          {
            case 16:
              ps_dab(dp, in, "merge00");            /* ps_merge00 */
              break;
            case 17:
              ps_dab(dp, in, "merge01");            /* ps_merge01 */
              break;
            case 18:
              ps_dab(dp, in, "merge10");            /* ps_merge10 */
              break;
            case 19:
              ps_dab(dp, in, "merge11");            /* ps_merge11 */
              break;
            default:
              ill(dp, in);
          }
          break;
        case 18:
          ps_dab(dp, in, "div");                    /* ps_div */
          break;
        case 20:
          ps_dab(dp, in, "sub");                    /* ps_sub */
          break;
        case 21:
          ps_dab(dp, in, "add");                    /* ps_add */
          break;
        case 23:
          ps_dacb(dp, in, "sel");                   /* ps_sel */
          break;
        case 24:
          ps_db(dp, in, "res");                     /* ps_res */
          break;
        case 25:
          ps_dac(dp, in, "mul");                    /* ps_mul */
          break;
        case 26:
          ps_db(dp, in, "rsqrte");                  /* ps_rsqrte */
          break;
        case 28:
          ps_dacb(dp, in, "msub");                  /* ps_msub */
          break;
        case 29:
          ps_dacb(dp, in, "madd");                  /* ps_madd */
          break;
        case 30:
          ps_dacb(dp, in, "nmsub");                 /* ps_nmsub */
          break;
        case 31:
          ps_dacb(dp, in, "nmadd");                 /* ps_nmadd */
          break;
        default:
          ill(dp, in);
      }
      break;

#endif /* GEKKO */

    case 7: 
      strcpy(dp->opcode,"mulli");
      imm(dp,in,0,0,0);
      break;

    case 8:
      strcpy(dp->opcode,"subfic");
      imm(dp,in,0,0,0);
      break;

    case 10:
      cmpi(dp,in,1);  /* cmpli */
      break;

    case 11:
      cmpi(dp,in,0);  /* cmpi */
      break;

    case 12:
      addi(dp,in,"ic");  /* addic */
      break;

    case 13:
      addi(dp,in,"ic.");  /* addic. */
      break;

    case 14:
      addi(dp,in,"i");  /* addi */
      break;

    case 15:
      addi(dp,in,"is");  /* addis */
      break;

    case 16:
      bc(dp,in);
      break;

    case 17:
      if ((in & ~PPCIDXMASK) == 2)
        strcpy(dp->opcode,"sc");
      else
        ill(dp,in);
      break;

    case 18:
      bli(dp,in);
      break;

    case 19:
      switch (PPCGETIDX2(in)) {
        case 0:
          mcrf(dp,in,'\0');  /* mcrf */
          break;

        case 16:
          branch(dp,in,"lr",0,0);  /* bclr */
          break;

        case 33:
          crop(dp,in,"nor","not");  /* crnor */
          break;

        case 50:
          nooper(dp,in,"rfi",PPCF_SUPER);
          break;

        case 129:
          crop(dp,in,"andc",NULL);  /* crandc */
          break;

        case 150:
          nooper(dp,in,"isync",0);
          break;

        case 193:
          crop(dp,in,"xor","clr");  /* crxor */
          break;

        case 225:
          crop(dp,in,"nand",NULL);  /* crnand */
          break;

        case 257:
          crop(dp,in,"and",NULL);  /* crand */
          break;

        case 289:
          crop(dp,in,"eqv","set");  /* creqv */
          break;

        case 417:
          crop(dp,in,"orc",NULL);  /* crorc */
          break;

        case 449:
          crop(dp,in,"or","move");  /* cror */
          break;

        case 528:
          branch(dp,in,"ctr",0,0);  /* bcctr */
          break;

        default:
          ill(dp,in);
          break;
      }
      break;

    case 20:
      rlw(dp,in,"imi",0);  /* rlwimi */
      break;

    case 21:
      rlw(dp,in,"inm",0);  /* rlwinm */
      break;

    case 23:
      rlw(dp,in,"nm",1);  /* rlwnm */
      break;

    case 24:
      if (in & ~PPCIDXMASK)
        ori(dp,in,"ori");
      else
        strcpy(dp->opcode,"nop");
      break;

    case 25:
      ori(dp,in,"oris");
      break;

    case 26:
      ori(dp,in,"xori");
      break;

    case 27:
      ori(dp,in,"xoris");
      break;

    case 28:
      ori(dp,in,"andi.");
      break;

    case 29:
      ori(dp,in,"andis.");
      break;

    case 30:
      switch (in & 0x1c) {
        case 0:
          rld(dp,in,"icl",0);  /* rldicl */
          break;
        case 1:
          rld(dp,in,"icr",0);  /* rldicr */
          break;
        case 2:
          rld(dp,in,"ic",0);   /* rldic */
          break;
        case 3:
          rld(dp,in,"imi",0);  /* rldimi */
          break;
        case 4:
          tc = (in&2)?((char *)("cl")):((char *)("cr"));
          rld(dp,in,tc,1);  /* rldcl, rldcr */
          break;
        default:
          ill(dp,in);
          break;
      }
      break;

    case 31:
      switch (PPCGETIDX2(in)) {
        case 0:
        case 32:
          if (in & 1)
            ill(dp,in);
          else
            cmp(dp,in);  /* cmp, cmpl */
          break;

        case 4:
          if (in & 1)
            ill(dp,in);
          else
            trap(dp,in,0);  /* tw */
          break;

        case 8:
        case (PPCOE>>1)+8:
          dab(dp,swapab(in),"subc",7,0,1,-1,0);
          break;

        case 9:
          dab(dp,in,"mulhdu",7,0,0,-1,PPCF_64);
          break;

        case 10:
        case (PPCOE>>1)+10:
          dab(dp,in,"addc",7,0,1,-1,0);
          break;

        case 11:
          dab(dp,in,"mulhwu",7,0,0,-1,0);
          break;

        case 19:
          if (in & (PPCAMASK|PPCBMASK))
            ill(dp,in);
          else
            dab(dp,in,"mfcr",4,0,0,0,0);
          break;

        case 20:
          dab(dp,in,"lwarx",7,0,0,0,0);
          break;

        case 21:
          dab(dp,in,"ldx",7,0,0,0,PPCF_64);
          break;

        case 23:
          dab(dp,in,"lwzx",7,0,0,0,0);
          break;

        case 24:
          dab(dp,in,"slw",7,1,0,-1,0);
          break;

        case 26:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"cntlzw",6,1,0,-1,0);
          break;

        case 27:
          dab(dp,in,"sld",7,1,0,-1,PPCF_64);
          break;

        case 28:
          dab(dp,in,"and",7,1,0,-1,0);
          break;

        case 40:
        case (PPCOE>>1)+40:
          dab(dp,swapab(in),"sub",7,0,1,-1,0);
          break;

        case 53:
          dab(dp,in,"ldux",7,0,0,0,PPCF_64);
          break;

        case 54:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbst",3,0,0,0,0);
          break;

        case 55:
          dab(dp,in,"lwzux",7,0,0,0,0);
          break;

        case 58:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"cntlzd",6,1,0,-1,PPCF_64);
          break;

        case 60:
          dab(dp,in,"andc",7,1,0,-1,0);
          break;

        case 68:
          trap(dp,in,PPCF_64);  /* td */
          break;

        case 73:
          dab(dp,in,"mulhd",7,0,0,-1,PPCF_64);
          break;

        case 75:
          dab(dp,in,"mulhw",7,0,0,-1,0);
          break;

        case 83:
          if (in & (PPCAMASK|PPCBMASK))
            ill(dp,in);
          else
            dab(dp,in,"mfmsr",4,0,0,0,PPCF_SUPER);
          break;

        case 84:
          dab(dp,in,"ldarx",7,0,0,0,PPCF_64);
          break;

        case 86:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbf",3,0,0,0,0);
          break;

        case 87:
          dab(dp,in,"lbzx",7,0,0,0,0);
          break;

        case 104:
        case (PPCOE>>1)+104:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"neg",6,0,1,-1,0);
          break;

        case 119:
          dab(dp,in,"lbzux",7,0,0,0,0);
          break;

        case 124:
          if (PPCGETD(in) == PPCGETB(in))
            dab(dp,in,"not",6,1,0,-1,0);
          else
            dab(dp,in,"nor",7,1,0,-1,0);
          break;

        case 136:
        case (PPCOE>>1)+136:
          dab(dp,in,"subfe",7,0,1,-1,0);
          break;

        case 138:
        case (PPCOE>>1)+138:
          dab(dp,in,"adde",7,0,1,-1,0);
          break;

        case 144:
          mtcr(dp,in);
          break;

        case 146:
          if (in & (PPCAMASK|PPCBMASK))
            ill(dp,in);
          else
            dab(dp,in,"mtmsr",4,0,0,0,PPCF_SUPER);
          break;

        case 149:
          dab(dp,in,"stdx",7,0,0,0,PPCF_64);
          break;

        case 150:
          dab(dp,in,"stwcx.",7,0,0,1,0);
          break;

        case 151:
          dab(dp,in,"stwx",7,0,0,0,0);
          break;

        case 181:
          dab(dp,in,"stdux",7,0,0,0,PPCF_64);
          break;

        case 183:
          dab(dp,in,"stwux",7,0,0,0,0);
          break;

        case 200:
        case (PPCOE>>1)+200:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"subfze",6,0,1,-1,0);
          break;

        case 202:
        case (PPCOE>>1)+202:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"addze",6,0,1,-1,0);
          break;

        case 210:
          msr(dp,in,1);  /* mfsr */
          break;

        case 214:
          dab(dp,in,"stdcx.",7,0,0,1,PPCF_64);
          break;

        case 215:
          dab(dp,in,"stbx",7,0,0,0,0);
          break;

        case 232:
        case (PPCOE>>1)+232:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"subfme",6,0,1,-1,0);
          break;

        case 233:
        case (PPCOE>>1)+233:
          dab(dp,in,"mulld",7,0,1,-1,PPCF_64);
          break;

        case 234:
        case (PPCOE>>1)+234:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"addme",6,0,1,-1,0);
          break;

        case 235:
        case (PPCOE>>1)+235:
          dab(dp,in,"mullw",7,0,1,-1,0);
          break;

        case 242:
          if (in & PPCAMASK)
            ill(dp,in);
          else
            dab(dp,in,"mtsrin",5,0,0,0,PPCF_SUPER);
          break;

        case 246:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbtst",3,0,0,0,0);
          break;

        case 247:
          dab(dp,in,"stbux",7,0,0,0,0);
          break;

        case 266:
        case (PPCOE>>1)+266:
          dab(dp,in,"add",7,0,1,-1,0);
          break;

        case 278:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbt",3,0,0,0,0);
          break;

        case 279:
          dab(dp,in,"lhzx",7,0,0,0,0);
          break;

        case 284:
          dab(dp,in,"eqv",7,1,0,-1,0);
          break;

        case 306:
          if (in & (PPCDMASK|PPCAMASK))
            ill(dp,in);
          else
            dab(dp,in,"tlbie",1,0,0,0,PPCF_SUPER);
          break;

        case 310:
          dab(dp,in,"eciwx",7,0,0,0,0);
          break;

        case 311:
          dab(dp,in,"lhzux",7,0,0,0,0);
          break;

        case 316:
          dab(dp,in,"xor",7,1,0,-1,0);
          break;

        case 339:
          mspr(dp,in,0);  /* mfspr */
          break;

        case 341:
          dab(dp,in,"lwax",7,0,0,0,PPCF_64);
          break;

        case 343:
          dab(dp,in,"lhax",7,0,0,0,0);
          break;

        case 370:
          nooper(dp,in,"tlbia",PPCF_SUPER);
          break;

        case 371:
          mtb(dp,in);  /* mftb */
          break;

        case 373:
          dab(dp,in,"lwaux",7,0,0,0,PPCF_64);
          break;

        case 375:
          dab(dp,in,"lhaux",7,0,0,0,0);
          break;

        case 407:
          dab(dp,in,"sthx",7,0,0,0,0);
          break;

        case 412:
          dab(dp,in,"orc",7,1,0,-1,0);
          break;

        case 413:
          sradi(dp,in);  /* sradi */
          break;

        case 434:
          if (in & (PPCDMASK|PPCAMASK))
            ill(dp,in);
          else
            dab(dp,in,"slbie",1,0,0,0,PPCF_SUPER|PPCF_64);
          break;

        case 438:
          dab(dp,in,"ecowx",7,0,0,0,0);
          break;

        case 439:
          dab(dp,in,"sthux",7,0,0,0,0);
          break;

        case 444:
          if (PPCGETD(in) == PPCGETB(in))
            dab(dp,in,"mr",6,1,0,-1,0);
          else
            dab(dp,in,"or",7,1,0,-1,0);
          break;

        case 457:
        case (PPCOE>>1)+457:
          dab(dp,in,"divdu",7,0,1,-1,PPCF_64);
          break;

        case 459:
        case (PPCOE>>1)+459:
          dab(dp,in,"divwu",7,0,1,-1,0);
          break;

        case 467:
          mspr(dp,in,1);  /* mtspr */
          break;

        case 470:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbi",3,0,0,0,0);
          break;

        case 476:
          dab(dp,in,"nand",7,1,0,-1,0);
          break;

        case 489:
        case (PPCOE>>1)+489:
          dab(dp,in,"divd",7,0,1,-1,PPCF_64);
          break;

        case 491:
        case (PPCOE>>1)+491:
          dab(dp,in,"divw",7,0,1,-1,0);
          break;

        case 498:
          nooper(dp,in,"slbia",PPCF_SUPER|PPCF_64);
          break;

        case 512:
          if (in & 0x007ff801)
            ill(dp,in);
          else {
            strcpy(dp->opcode,"mcrxr");
            sprintf(dp->operands,"cr%d",(int)PPCGETCRD(in));
          }
          break;

        case 533:
          dab(dp,in,"lswx",7,0,0,0,0);
          break;

        case 534:
          dab(dp,in,"lwbrx",7,0,0,0,0);
          break;

        case 535:
          fdab(dp,in,"lfsx",7);
          break;

        case 536:
          dab(dp,in,"srw",7,1,0,-1,0);
          break;

        case 539:
          dab(dp,in,"srd",7,1,0,-1,PPCF_64);
          break;

        case 566:
          nooper(dp,in,"tlbsync",PPCF_SUPER);
          break;

        case 567:
          fdab(dp,in,"lfsux",7);
          break;

        case 595:
          msr(dp,in,0);  /* mfsr */
          break;

        case 597:
          rrn(dp,in,"lswi",0,0,0,0);
          break;

        case 598:
          nooper(dp,in,"sync",PPCF_SUPER);
          break;

        case 599:
          fdab(dp,in,"lfdx",7);
          break;

        case 631:
          fdab(dp,in,"lfdux",7);
          break;

        case 659:
          if (in & PPCAMASK)
            ill(dp,in);
          else
            dab(dp,in,"mfsrin",5,0,0,0,PPCF_SUPER);
          break;

        case 661:
          dab(dp,in,"stswx",7,0,0,0,0);
          break;

        case 662:
          dab(dp,in,"stwbrx",7,0,0,0,0);
          break;

        case 663:
          fdab(dp,in,"stfsx",7);
          break;

        case 695:
          fdab(dp,in,"stfsux",7);
          break;

        case 725:
          rrn(dp,in,"stswi",0,0,0,0);
          break;

        case 727:
          fdab(dp,in,"stfdx",7);
          break;

        case 759:
          fdab(dp,in,"stfdux",7);
          break;

        case 790:
          dab(dp,in,"lhbrx",7,0,0,0,0);
          break;

        case 792:
          dab(dp,in,"sraw",7,1,0,-1,0);
          break;

        case 794:
          dab(dp,in,"srad",7,1,0,-1,PPCF_64);
          break;

        case 824:
          rrn(dp,in,"srawi",1,0,-1,0);
          break;

        case 854:
          nooper(dp,in,"eieio",PPCF_SUPER);
          break;

        case 918:
          dab(dp,in,"sthbrx",7,0,0,0,0);
          break;

        case 922:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"extsh",6,1,0,-1,0);
          break;

        case 954:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"extsb",6,1,0,-1,0);
          break;

        case 982:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"icbi",3,0,0,0,0);
          break;

        case 983:
          fdab(dp,in,"stfiwx",7);
          break;

        case 986:
          if (in & PPCBMASK)
            ill(dp,in);
          else
            dab(dp,in,"extsw",6,1,0,-1,PPCF_64);
          break;

        case 1014:
          if (in & PPCDMASK)
            ill(dp,in);
          else
            dab(dp,in,"dcbz",3,0,0,0,0);
          break;

        default:
          ill(dp,in);
          break;
      }
      break;

    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      ldst(dp,in,ldstnames[PPCGETIDX(in)-32],'r',0);
      break;

    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
      ldst(dp,in,ldstnames[PPCGETIDX(in)-32],'f',0);
      break;

#ifdef GEKKO

    case 56:                    /* psq_l */
      ps_ldst(dp, in, "l");
      break;

    case 57:                    /* psq_lu */
      ps_ldst(dp, in, "lu");
      break;

#endif /* GEKKO */

    case 58:
      switch (in & 3) {
        case 0:
          ldst(dp,in&~3,"ld",'r',PPCF_64);
          break;
        case 1:
          ldst(dp,in&~3,"ldu",'r',PPCF_64);
          break;
        case 2:
          ldst(dp,in&~3,"lwa",'r',PPCF_64);
          break;
        default:
          ill(dp,in);
          break;
      }
      break;

    case 59:
      switch (in & 0x3e) {
        case 36:
          fdabc(dp,in,"divs",6,0);
          break;

        case 40:
          fdabc(dp,in,"subs",6,0);
          break;

        case 42:
          fdabc(dp,in,"adds",6,0);
          break;

        case 44:
          fdabc(dp,in,"sqrts",2,0);
          break;

        case 48:
          fdabc(dp,in,"res",2,0);
          break;

        case 50:
          fdabc(dp,in,"muls",5,0);
          break;

        case 56:
          fdabc(dp,in,"msubs",7,0);
          break;

        case 58:
          fdabc(dp,in,"madds",7,0);
          break;

        case 60:
          fdabc(dp,in,"nmsubs",7,0);
          break;

        case 62:
          fdabc(dp,in,"nmadds",7,0);
          break;

        default:
          ill(dp,in);
          break;
      }
      break;

#ifdef GEKKO

    case 60:                        /* psq_st */
      ps_ldst(dp, in, "st");
      break;

    case 61:                        /* psq_stu */
      ps_ldst(dp, in, "stu");
      break;

#endif /* GEKKO */

    case 62:
      switch (in & 3) {
        case 0:
          ldst(dp,in&~3,"std",'r',PPCF_64);
          break;
        case 1:
          ldst(dp,in&~3,"stdu",'r',PPCF_64);
          break;
        default:
          ill(dp,in);
          break;
      }
      break;

    case 63:
      if (in & 32) {
        switch (in & 0x1e) {
          case 4:
            fdabc(dp,in,"div",6,0);
            break;

          case 8:
            fdabc(dp,in,"sub",6,0);
            break;

          case 10:
            fdabc(dp,in,"add",6,0);
            break;

          case 12:
            fdabc(dp,in,"sqrt",2,0);
            break;

          case 14:
            fdabc(dp,in,"sel",7,0);
            break;

          case 18:
            fdabc(dp,in,"mul",5,0);
            break;

          case 20:
            fdabc(dp,in,"rsqrte",2,0);
            break;

          case 24:
            fdabc(dp,in,"msub",7,0);
            break;

          case 26:
            fdabc(dp,in,"madd",7,0);
            break;

          case 28:
            fdabc(dp,in,"nmsub",7,0);
            break;

          case 30:
            fdabc(dp,in,"nmadd",7,0);
            break;

          default:
            ill(dp,in);
            break;
        }
      }

      else {
        switch (PPCGETIDX2(in)) {
          case 0:
            fcmp(dp,in,'u');
            break;

          case 12:
            fdabc(dp,in,"rsp",10,0);
            break;

          case 14:
            fdabc(dp,in,"ctiw",10,0);
            break;

          case 15:
            fdabc(dp,in,"ctiwz",10,0);
            break;

          case 32:
            fcmp(dp,in,'o');
            break;

          case 38:
            mtfsb(dp,in,1);
            break;

          case 40:
            fdabc(dp,in,"neg",10,0);
            break;

          case 64:
            mcrf(dp,in,'s');  /* mcrfs */
            break;

          case 70:
            mtfsb(dp,in,0);
            break;

          case 72:
            fdabc(dp,in,"mr",10,0);
            break;

          case 134:
            if (!(in & 0x006f0800)) {
              sprintf(dp->opcode,"mtfsfi%s",rcsel[in&1]);
              sprintf(dp->operands,"cr%d, %d",(int)PPCGETCRD(in),
                      (int)(in & 0xf000)>>12);
            }
            else
              ill(dp,in);
            break;

          case 136:
            fdabc(dp,in,"nabs",10,0);
            break;

          case 264:
            fdabc(dp,in,"abs",10,0);
            break;

          case 583:
            if (in & (PPCAMASK|PPCBMASK))
              ill(dp,in);
            else
              dab(dp,in,"mffs",4,0,0,-1,0);
            break;

          case 711:
            if (!(in & 0x02010000)) {
              sprintf(dp->opcode,"mtfsf%s",rcsel[in&1]);
              sprintf(dp->operands,"0x%.2x, f%d",
                      (unsigned)((in >> 17) & 0xff), (int)PPCGETB(in));
            }
            else
              ill(dp,in);
            break;

          case 814:
            fdabc(dp,in,"fctid",10,PPCF_64);
            break;

          case 815:
            fdabc(dp,in,"fctidz",10,PPCF_64);
            break;

          case 846:
            fdabc(dp,in,"fcfid",10,PPCF_64);
            break;

          default:
            ill(dp,in);
            break;
        }
      }
      break;

    default:
      ill(dp,in);
      break;
  }
  return (dp->instr + 1);
}

/* --------------------------------------------------------------------------- */

/* org : simplified interface for emu */

void dasm(DISA *d)
{
    struct PPCDisasm dp;

    dp.opcode = d->opcode;
    dp.operands = d->operands;
    dp.instr = (ppc_word *)&d->op;
    dp.iaddr = (ppc_word *)&d->pc;

    PPC_Disassemble(&dp);

    d->type = dp.type;
    if(dp.type == PPCINSTR_BRANCH)
    {
        if(d->op & 2)   // AA ?
        {
            d->disp = dp.displacement;        
        }
        else 
        {
            d->disp = d->pc + dp.displacement;        
        }
    }
    else d->disp = d->pc + 4;
}
