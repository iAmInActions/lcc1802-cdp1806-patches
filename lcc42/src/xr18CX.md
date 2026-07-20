%{
//stmt: ASGNP2(addr,ADDP2(reg,consm))  "\tincm R%c,%1\n\tst2 R%c,%0; ASGNP2(addr,ADDP2(reg,consm))**\n"  1
/* This is the XR1CX machine description file for the 1802 COMX-35.
 * adapted from xr16.md to target the 1802 microprocessor
 * jan 28 beginning work on the Birthday Compiler
 * jan 31 trying out inline assembly patch
 * feb 7 2013, trying a new option -volatile to run without integer variables 
 * feb 8 refining -volatile to save reg  and use nointerrupts/interrupts
 * feb 12 work in progress after fixing inc bug
 * feb 13 archived before trying to change address mode macros
 * feb 14 version 2.3, changing to reserve/release for stack frame
 * feb 17 changing from putc.inc to IO1802.inc - This is the Birthday Compiler
 * feb 26 putting R prefix on registers in zext sext, initializing unused storage (change ds to db )
 * mar 3 archiving before optimization push
 * mar 3 first optimization push xr18no.md
 * mar 4 improved register saves in function()
 * mar 5 inproved "IF" instruction code by incorporating jzi2 logic.
 * mar 15 archiving before second optimzation push
 * mar 15 trying some instruction combination optimizations but it started blowing up
 * mar 17 add decm for small subtractions, removed unused inc(replaced by incm)
 * mar 20 archiving before floating point push.
 * mar 20 xr18fl.md is the first try at floating point
 * mar 28 basic fp operations work and conversion from float->long & vice versa
 * mar 28 freg2 now based on IREG to clear the register assignment problem
 * april 5 experimenting with operation chaining to eliminate excess instructions
 * april 10 trying to fix int to float conversion problem
 * april 19 putting missing 'r' in zext & sext templates
 * april 22-25 experimenting optimization, add EQU2(reg,con0), avoid release/reserve 0 bytes
 * archived before monkeying with register definitions because of allocation overlaps.
 * may 8 putting move(a) cost function back on register copy rules, remove extra asgni2 rule
 * may 15 setting intvar and inttmp back
 * may 16 trying r6 in intvar 
 * may 24 packaging for uploading.
 * June 27 correction to defconst so that signed numbers are printed with %d instead of %u 
 * June 27 trying %A instead of %a+%F to get offset as a single figure
 * Oct 2 xr18DH.md is dhrystone optimization
 * Oct 2 break up global inits into 1000 byte chunks to prevent assembler error
 * Oct 13 wholesale change of register names in rules from r* to R* also typo in a 4 byte compare
 * Oct 19 experimenting with stmt: EQI2(CVUI2(INDIRU1(indaddr)),CVUI2(INDIRU1(indaddr))) "surely not %0,%1,%a\n" 0
 * Oct 23 redecorating ASGNI2(addr,reg)/U2/P2 so the same peephole rule picks them up
 * Dec 23 removing *s from loadx2(reg) so a common rule can pick them up
 * Nov 12 2016 fixing case of epilog& prolog.inc file names
 * Feb 22 2017 xr18NW.md begins adaptation for the 1804/5/66
 * Mar 5 more dicking with 1806 adaptation adding 1 to stack offsets as:
 addr: ADDRFP2  "'O',sp,(%A+1)"
 addr: ADDRLP2  "'O',sp,(%A+1)"
   also in open code
 * Mar 14 cleanup, get rid of cretn6
 * Mar 28 cleanup of working copy on dell laptop special case of EQUI2 with 8 bit constant. function begin/end markers
 * 17-10-20 SCRT routines in lcc1802proloNW now use clean stack so stack offsets are the same for 1802/1804/5/6
 * 17-10-21 adding option -cpu1805 to put regs 4&5 into variable pool, set CPU value for assembler
 *          changed how -volatile set the regs
 * 18-01-25 corrected how -volatile set the regs
 * 18-02-26 attempting some combinations 
 * 18-03-04 another combination rule CVIU4(CVUI2(INDIRU1(addr)))
 * 18-04-13 xr18NW is the default, commented out display of register mask
 * 20-04-28 xr18CX adapted for the COMX-35
 * 20-05-13 trying to avoid promotion of unsigned char comparisons
 * 20-05-23 adding include lcc1802finale.inc to the end of the module - mostly to accommodate wrapup comx code in include/comx
 * 20-05-23 removed GHI 15 before cretn.  now included in cretn
 * 20-06-02 updated segment() to support data relocation in combination with prolog changes.
            note uninitialized globals are now defined with the globss macro and zeroing is done by the macro when needed
 * 20-07-31 rule optimizations for cross-shoot(J2020), tweaking of function start/end markers.
 * 20-11-30 optimizations prompted by liveness analysis LV-x
 * 21-03-31 version for pixie.  accepts pixie,pixie2,pixie3,
 *          loads lcc1802prolopx for pixie, pixie2.  loads prolopx2 for pixie3.  loads pxbrcode for pixie only
 * 21-06-03 optmizes divi2(reg,2) to shr
 * 21-07-11 version for elfos accepts elfos, loads lcc1802epiloOS
 * 21-09-30 no real change - just synching for version control - calling it the September I remember the 4th wave.
 *          
 * Portions copyright (C) 1999, 2000, Gray Research LLC.  All rights reserved.
 * Portions of this file are subject to the XSOC License Agreement;
 * you may not use them except in compliance with this Agreement.
 * See the LICENSE file.
 *
 * This work is derived from the original src/mips.md file in the
 * lcc4.1 distribution.  See the CPYRIGHT file.
 *
 * Registers    Use
 * R0-R1	register variables, 2X16 bit, 1X32 bit
 * R2           stack pointer
 * R3         	main PC
 * R4		call register
 * R5		return register
 * R6		return address register - and integer variable !
 * R7		register variable, 1X16 bit
 * R8-R11	scratch registers
 * R12-R13      first function arguments, return value for longs
 * R14		memory address temp/macro work register	
 * R15		return value register for shorts
 * Operator terminals generated by ops c=1 s=2 i=2 l=4 h=4 f=4 d=4 x=4 p=2
 *
 * Floating point is implemented!
saving reg:  CVUI2(INDIRU1(addr))     "\tld1 R%c,%0\n\tzExt R%c ;CVUI2(INDIRU1(addr)): *widen unsigned char to signed int (zero extend)\n" 1
*/

#define INTTMP 0x0f00	//8-11 are temporaries
#define PX3TMP 0x0e00	//9-11 are temporaries
#define PX3VARS 0x0140	//8,6 for variables
#define INTVAR 0x0083	//0-1 7 can hold variables (NOT 6: R6 is REG_RETADDR, clobbered by every SCAL/SRET call on -cpu1805; see REG_RETADDR and the REGS6SAFE mask below)
#define REGS6SAFE 0xFFBF	//everything except bit 6 (R6/REG_RETADDR); ANDed into vmask after every mode-specific adjustment below, so no flag combination can put a variable in the call-linkage register 
#define REGSVOLATILE 0x0003	//registers 0 and 1 not available for variables in a volatile environment
#define REGSPIXIE2 0x0083	//registers 7, 0 and 1 not available for variables in a pixie2 environment
#define REGS1805 0x0030	//registers 4-5 are available for variables when -cpu1805 specified
#define INTRET 0x8000	//reg 15 is return value
//I'm going to try using the long integer registers for floats
//#define FLTTMP 0x0000007f //float regs 0-6 for temps
//#define FLTVAR 0x00000000 //no float register variables
//#define FLTRET 0x00000080 //float reg 7 is for return values

#define NUM_IREGS       16
#define REG_RETVAL      15       /* R15: return value */
#define REG_FIRST_ARG   12       /* R12: first argument register */
#define REG_LAST_ARG    13       /* R13: last argument register */
#define REG_FIRST_TEMP  8       /* R8: first temp register */
#define SZ_REG_FIRST_TEMP "8"   /* R8: first temp register */
#define REG_LAST_TEMP   11       /* R11: last temp register */
#define REG_SP          2      /* R2, sp: stack pointer */
#define REG_SP_VOLATILE 2      /* R2, is sp if we're accommodating interrupts */
#define REG_RETADDR     6      /* R6: return address */
#define REG_MEMADDR	14	/* used by macros */
#define NUM_ARG_REGS    (REG_LAST_ARG - REG_FIRST_ARG + 1)
	/* REG_FIRST_ARG/REG_LAST_ARG/rp1p2 remain in use as the fixed operand
	   registers for compiler-emitted runtime helper calls (_divi2, _mulu2,
	   long return values, etc.); that is a *different*, internal calling
	   convention and is intentionally left untouched below.  User-level C
	   function arguments no longer use hardware registers at all; they are
	   always passed through the fixed-address buffer ARGBUF (see argreg(),
	   emit2()'s ARG+x case, and function() below). */
#define ARGBUF_SIZE     16	/* bytes available in the fixed RAM argument area; this is
				   a hard ceiling checked by argreg()/function() below, not a
				   fixed cost: regular (non-vararg) parameter copying is always
				   sized to the real parameter list.  The one place this bound
				   directly drives code size is a va_alist function's vararg
				   tail copy, which conservatively copies everything ARGBUF
				   could still hold past the named parameters; keep this at
				   the smallest value that covers every call in the program. */

#define INT_CALLEE_SAVE INTVAR  //wjr jan 8 return address is saved in the call - save only the intvars
	/* NOT used directly by function() below anymore; see the comment at
	   its one use site (usedmask[IREG] &= vmask[IREG]) for why: this
	   static macro never reflected -cpu1805's REGS1805 addition (regs
	   4-5), so any function whose R4/R5 "register variable" needed to
	   survive a nested call was never actually saved/restored, silently
	   corrupting whatever the *caller* had stored there. Left defined
	   (unused) rather than deleted, in case something else still
	   references the name. */

#define readsreg(p) \
        (generic((p)->op)==INDIR && (p)->kids[0]->op==VREG+P)
#define setsrc(d) ((d) && (d)->x.regnode && \
        (d)->x.regnode->set == src->x.regnode->set && \
        (d)->x.regnode->mask&src->x.regnode->mask)

#define relink(a, b) ((b)->x.prev = (a), (a)->x.next = (b))

#include "c.h"
#include <time.h>

#define NODEPTR_TYPE Node
#define OP_LABEL(p) ((p)->op)
#define LEFT_CHILD(p) ((p)->kids[0])
#define RIGHT_CHILD(p) ((p)->kids[1])
#define STATE_LABEL(p) ((p)->x.state)
static void address(Symbol, Symbol, long);
static void blkfetch(int, int, int, int);
static void blkloop(int, int, int, int, int, int[]);
static void blkstore(int, int, int, int);
static void defaddress(Symbol);
static void defconst(int, int, Value);
static void defstring(int, char *);
static void defsymbol(Symbol);
static void doarg(Node);
static void emit2(Node);
static void export(Symbol);
static void clobber(Node);
static void function(Symbol, Symbol [], Symbol [], int);
static void global(Symbol);
static void import(Symbol);
static void local(Symbol);
static void progbeg(int, char **);
static void progend(void);
static void segment(int);
static void space(int);
static void target(Node);
static int fp();
static int      bitcount       (unsigned);
static Symbol   argreg         (int, int, int, int, int);

static Symbol ireg[32], lreg[32], freg2[32], rp1p2; //int regs, long regs, floats, borth parameter regs together
static Symbol iregw, lregw, freg2w;
static int tmpregs[] = {1, REG_FIRST_TEMP+1, REG_FIRST_TEMP+2};
static Symbol blkreg;

static int gnum = 8;
static int wjrelfos=0;		//controls elfos support
static int wjrpixie=0;		//controls whether 1861 is accommodated or not.  implies volatile
static int wjrvolatile=0;	//controls whether interrupts are supported or not
static int wjrcpu1805=0;	//controls whether static regs 4&5 are available for variables, sets cpu value for assembly
static char* wjrenv=0;		//controls whether an environment include precedes the prolog
static int wjrfloats=0;		//indicates whether floats have been used or not
static int wjrMulInlineWeight=5; //weight of 5 allows multiplies to be done inline
static int reg_sp_actual=REG_SP;		//stack pointer is reg 2 by default
static int argbufloc=0xEF00;	/* default base address of the fixed RAM argument-passing area,
				   override with -argbufloc=N. MUST be genuine writable RAM, not
				   ROM: 0x7F00 (this backend's original default) turned out to sit
				   inside the SBC1806's bank-switched ROM window (0x0000-0x7FFF).
				   BIOS_GPU.c's syscall() burns its syscall vector table there via
				   "ORG 0x7F00" at *compile* time, so writes to it at runtime would
				   be writes to ROM. 0xEF00 sits just above STACKLOC=0xEEEE, in the
				   region BIOS_GPU.c's own comments describe as reserved for BIOS
				   use ("stack went from 0xFFFF to EEEE to allow BIOS (reserved
				   memory) bytes"); verify this against your own memory map
				   before relying on it. */
static int cseg;

%}

%start stmt

%term CNSTF4=4113
%term CNSTI1=1045 CNSTI2=2069 CNSTI4=4117
%term CNSTP2=2071
%term CNSTU1=1046 CNSTU2=2070 CNSTU4=4118

%term ARGB=41
%term ARGF4=4129
%term ARGI2=2085 ARGI4=4133
%term ARGP2=2087
%term ARGU2=2086 ARGU4=4134

%term ASGNB=57
%term ASGNF4=4145
%term ASGNI1=1077 ASGNI2=2101 ASGNI4=4149
%term ASGNP2=2103
%term ASGNU1=1078 ASGNU2=2102 ASGNU4=4150

%term INDIRB=73
%term INDIRF4=4161
%term INDIRI1=1093 INDIRI2=2117 INDIRI4=4165
%term INDIRP2=2119
%term INDIRU1=1094 INDIRU2=2118 INDIRU4=4166

%term CVFF4=4209
%term CVFI2=2165 CVFI4=4213

%term CVIF4=4225
%term CVII1=1157 CVII2=2181 CVII4=4229
%term CVIU1=1158 CVIU2=2182 CVIU4=4230

%term CVPU2=2198

%term CVUI1=1205 CVUI2=2229 CVUI4=4277
%term CVUP2=2231
%term CVUU1=1206 CVUU2=2230 CVUU4=4278

%term NEGF4=4289
%term NEGI2=2245 NEGI4=4293

%term CALLB=217
%term CALLF4=4305
%term CALLI2=2261 CALLI4=4309
%term CALLP2=2263
%term CALLU2=2262 CALLU4=4310
%term CALLV=216

%term RETF4=4337
%term RETI2=2293 RETI4=4341
%term RETP2=2295
%term RETU2=2294 RETU4=4342
%term RETV=248

%term ADDRGP2=2311

%term ADDRFP2=2327

%term ADDRLP2=2343

%term ADDF4=4401
%term ADDI2=2357 ADDI4=4405
%term ADDP2=2359
%term ADDU2=2358 ADDU4=4406

%term SUBF4=4417
%term SUBI2=2373 SUBI4=4421
%term SUBP2=2375
%term SUBU2=2374 SUBU4=4422

%term LSHI2=2389 LSHI4=4437
%term LSHU2=2390 LSHU4=4438

%term MODI2=2405 MODI4=4453
%term MODU2=2406 MODU4=4454

%term RSHI2=2421 RSHI4=4469
%term RSHU2=2422 RSHU4=4470

%term BANDI2=2437 BANDI4=4485
%term BANDU2=2438 BANDU4=4486

%term BCOMI2=2453 BCOMI4=4501
%term BCOMU2=2454 BCOMU4=4502

%term BORI2=2469 BORI4=4517
%term BORU2=2470 BORU4=4518

%term BXORI2=2485 BXORI4=4533
%term BXORU2=2486 BXORU4=4534

%term DIVF4=4545
%term DIVI2=2501 DIVI4=4549
%term DIVU2=2502 DIVU4=4550

%term MULF4=4561
%term MULI2=2517 MULI4=4565
%term MULU2=2518 MULU4=4566

%term EQF4=4577
%term EQI2=2533 EQI4=4581
%term EQU2=2534 EQU4=4582

%term GEF4=4593
%term GEI2=2549 GEI4=4597
%term GEU2=2550 GEU4=4598

%term GTF4=4609
%term GTI2=2565 GTI4=4613
%term GTU2=2566 GTU4=4614

%term LEF4=4625
%term LEI2=2581 LEI4=4629
%term LEU2=2582 LEU4=4630

%term LTF4=4641
%term LTI2=2597 LTI4=4645
%term LTU2=2598 LTU4=4646

%term NEF4=4657
%term NEI2=2613 NEI4=4661
%term NEU2=2614 NEU4=4662

%term JUMPV=584

%term LABELV=600

%term LOADB=233
%term LOADF4=4321
%term LOADI1=1253 LOADI2=2277 LOADI4=4325
%term LOADP2=2279
%term LOADU1=1254 LOADU2=2278 LOADU4=4326

%term IASMV=88

%term VREGP=711
%%
reg:  INDIRI1(VREGP)     "# read register\n"
reg:  INDIRU1(VREGP)     "# read register\n"

reg:  INDIRI2(VREGP)     "# read register\n"
reg:  INDIRU2(VREGP)     "# read register\n"

reg:  INDIRF4(VREGP)     "# read register\n" fp()
reg:  INDIRI4(VREGP)     "# read register\n"
reg:  INDIRP2(VREGP)     "# read register\n"
reg:  INDIRU4(VREGP)     "# read register\n"

stmt: ASGNI1(VREGP,reg)  "# write register\n"
stmt: ASGNU1(VREGP,reg)  "# write register\n"

stmt: ASGNI2(VREGP,reg)  "# write register\n"
stmt: ASGNU2(VREGP,reg)  "# write register\n"
stmt: ASGNP2(VREGP,reg)  "# write register\n"

stmt: ASGNF4(VREGP,reg)  "# write register\n" fp()
stmt: ASGNI4(VREGP,reg)  "# write register\n"
stmt: ASGNU4(VREGP,reg)  "# write register\n"

con0: CNSTF4 "0"	range(a,0,0)
con0: CNSTU2  "0"	range(a,0,0)
con0: CNSTI2  "0"	range(a,0,0)
con2: CNSTU2  "2"	range(a,2,2)
con2: CNSTI2  "2"	range(a,2,2)
con2: CNSTU4  "2"	range(a,2,2)
con2: CNSTI4  "2"	range(a,2,2)
consm: CNSTU2 "%a" range (a,1,4)
consm: CNSTI2 "%a" range (a,1,4)
consm: CNSTP2 "%a" range (a,1,4)
con8bit: CNSTI2 "%a" range (a,1,255)
con8bit: CNSTU2 "%a" range (a,1,255)
con: CNSTI1  "%a"
con: CNSTU1  "%a"
con: CNSTI2  "%a"
con: CNSTU2  "%a"
con: CNSTP2  "%a"

reg: CNSTI4   "\tldI4 R%c,%a ;loading a long integer constant\n"  2
reg: CNSTU4   "\tldI4 R%c,%a ;loading a long unsigned constant\n"  2

stmt: reg  ""
acon: con     "%0"
acon: ADDRGP2 "%a"
addr: ADDI2(reg,acon)  "'O',R%0,(%1)"
addr: ADDU2(reg,acon)  "'O',R%0,(%1)"
addr: ADDP2(reg,acon)  "'O',R%0,(%1)"
addr: acon  "'D',(%0),0"
addr: reg   "'O',R%0,0"
indaddr: reg   "R%0"
addr: ADDRFP2  "'O',sp,(%A+1)"
addr: ADDRLP2  "'O',sp,(%A+1)"

reg: ADDI2(reg,consm) "?\tcpy2 R%c,R%0 ;reg:ADDI2(consm,reg)\n\tincm R%c,%1\n"  1
reg: ADDU2(reg,consm) "?\tcpy2 R%c,R%0\n\tincm R%c,%1\n"  1
reg: ADDP2(reg,consm) "?\tcpy2 R%c,R%0\n\tincm R%c,%1\n"  1

reg: SUBI2(reg,consm) "?\tcpy2 R%c,R%0	;SUBI2(reg,consm)\n\tdecm R%c,%1	;SUBI2(reg,consm)\n"  1
reg: SUBU2(reg,consm) "?\tcpy2 R%c,R%0	;SUBU2(reg,consm)\n\tdecm R%c,%1	;SUBU2(reg,consm)\n"  1
reg: SUBP2(reg,consm) "?\tcpy2 R%c,R%0\n\tdecm R%c,%1	; SUBP2(reg,consm)\n"  1

reg: con0  "\tld2z R%c; reg:con0\n" 1
reg: acon  "\tldaD R%c,%0; reg:acon\n" 1
reg: addr  "\tldA2 R%c,%0; reg:addr\n"  2

stmt: ASGNI1(addr,reg)  "\tst1 R%1,%0; ASGNI1\n"  10
stmt: ASGNI1(indaddr,reg)  "\tstr1 R%1,%0; ASGNI1(indaddr,reg)	DH\n"  5
stmt: ASGNU1(indaddr,acon)  "\tstr1I %1,%0; ASGNU1(indaddr,acon)	DH\n"  5
stmt: ASGNU1(addr,reg)  "\tst1 R%1,%0; ASGNU1\n"  10
stmt: ASGNU1(indaddr,INDIRU1(indaddr))  "\tldn %1\n\tstr %0; ASGNU1(indaddr,INDIRU1(indaddr))J2020-1\n"  3
stmt: ASGNU1(indaddr,reg)  "\tstr1 R%1,%0; ASGNU1(indaddr,reg)		DH*\n"  5
stmt: ASGNU1(indaddr,LOADU1(LOADU2(reg)))  "\tstr1 R%1,%0; ASGNU1(indaddr,LOADU1(LOADU2(reg))) 18-03-21\n"  1
stmt: ASGNU1(addr,acon)  "\tst1I %1,%0; ASGNU1(addr,acon) LV-1 \n"  5
stmt: ASGNI2(addr,acon)  "\tst2I %1,%0; ASGNI2(addr,acon)\n"  5
stmt: ASGNI2(addr,reg)  "\tst2 R%1,%0; ASGNI2(addr,reg)\n"  10
stmt: ASGNI2(addr,LOADI2(reg))  "\tst2 R%1,%0; ASGNI2(addr,LOADI2(reg)) 18-02-26\n"  10
stmt: ASGNU2(addr,reg)  "\tst2 R%1,%0; ASGNU2(addr,reg)\n"  10
stmt: ASGNU2(addr,LOADU2(reg))  "\tst2 R%1,%0; ASGNU2(addr,LOADU2(reg)) 18-02-26\n"  10
stmt: ASGNI2(addr,LOADI2(reg))  "\tst2 R%1,%0; ASGNI2(addr,LOADI2(reg)) 18-03-22\n"  10
stmt: ASGNP2(addr,reg)  "\tst2 R%1,%0; ASGNP2(addr,reg)\n"  1
stmt: ASGNP2(addr,acon)  "\tst2i %1,%0; ASGNP2(addr,acon) LV-3\n"  1
stmt: ASGNI4(addr,reg)  "\tst4 R%1,%0\n"  1
stmt: ASGNU4(addr,reg)  "\tst4 R%1,%0; ASGNU4\n"  1
reg:  INDIRI1(indaddr)     "\tldn1 R%c,%0;reg:  INDIRI1(indaddr)\n"  0
reg:  INDIRU1(indaddr)     "\tldn1 R%c,%0;reg:  INDIRU1(indaddr)\n"  0
reg:  INDIRI1(addr)     "\tld1 R%c,%0\n"  1
reg:  INDIRU1(addr)     "\tld1 R%c,%0\n"  1
reg:  INDIRI2(addr)     "\tld2 R%c,%0 ;reg:INDIRI2(addr)\n"  1
reg:  INDIRU2(addr)     "\tld2 R%c,%0 ;reg:INDIRU2(addr)\n"  1
reg:  INDIRP2(addr)     "\tld2 R%c,%0 ;reg:INDIRP2(addr)\n"  1
reg:  INDIRI4(addr)     "\tld4 R%c,%0;reg:  INDIRI4(addr)\n"  1
reg:  INDIRU4(addr)     "\tld4 R%c,%0;reg:  INDIRU4(addr)\n"  1

reg:  CVII2(INDIRI1(addr))     "\tld1 R%c,%0\n\tsExt R%c ;CVII2: widen signed char to signed int (sign extend)\n"  1
reg:  CVUU2(INDIRU1(addr))     "\tld1 R%c,%0\n\tzExt R%c ;CVUU2: widen unsigned char to unsigned int (zero extend)\n" 1
reg:  CVUI2(INDIRU1(addr))     "\tld1 R%c,%0\n\tzExt R%c ;CVUI2(INDIRU1(addr)): *widen unsigned char to signed int (zero extend)J2020-2\n" 1
reg:  CVII4(INDIRI1(addr))     "\tld1 R%c,%0\n\tsext R%c\n\tsext4 R%c ;CVII4: widen signed char to long int(sign extend)\n"  1
reg:  CVII4(INDIRI2(addr))     "\tld2 R%c,%0\n\tsext4 R%c ;CVII4: widen signed int to long int(sign extend)\n"  1
reg:  CVUU4(INDIRU1(addr))     "\tld1 R%c,%0\n\tzext R%c\n\tzext4 R%c ;CVUU4: widen unsigned char to unsigned long(zero extend)\n"   1
reg:  CVUU4(INDIRU2(addr))     "\tld2 R%c,%0\n\tzext4 R%c ;CVUU4: widen unsigned int to unsigned long (zero extend)\n"   1
reg:  CVUI4(INDIRU1(addr))     "\tld1 R%c,%0\n\tzext R%c\n\tzext4 R%c ;CVUI4: widen unsigned char to signed long(zero extend)\n" 1
reg:  CVUI4(INDIRU2(addr))     "\tld2 R%c,%0\n\tzext4 R%c ;CVUI4: widen unsigned int to signed long (zero extend)\n"  1
reg: DIVI2(reg,reg)  "\tCcall _divi2\n"   2
reg: DIVI2(reg,con2)  "?\tcpy2 R%c,R%0\n\tshri2I R%c,1; DIVI2(reg,2)\n"   1
reg: DIVI4(reg,con2)  "\tshri4I R%c,1; DIVI4(reg,2)\n"   1
reg: DIVI4(reg,reg)  "\tCcall _divi4; DIVI4(reg,reg)\n"   1
reg: DIVU2(reg,reg)  "\tCcall _divu2\n"  1
reg: DIVU4(reg,reg)  "\tCcall _divu4\n"  1
reg: MODI2(reg,reg)  "\tCcall _modi2\n"   1
reg: MODI4(reg,reg)  "\tCcall _modi4\n"   1
reg: MODU2(reg,reg)  "\tCcall _modu2\n"  1
reg: MODU4(reg,reg)  "\tCcall _modu4\n"  1
reg: MULI2(reg,reg)  "\tCcall _mulu2; MULI2(reg,reg)\n"   10
reg: MULI2(con8bit,reg)  "# MULI2(con8bit,reg) j2020-10\n"   wjrMulInlineWeight
reg: MULU2(con8bit,reg)  "# MULU2(con8bit,reg) j2020-10\n"   wjrMulInlineWeight
reg: MULI4(reg,reg)  "\tCcall _mulu4\n"   1
reg: MULU2(reg,reg)  "\tCcall _mulu2; MULU2(reg,reg)\n"   2
reg: MULU4(reg,reg)  "\tCcall _mulu4\n"   4

reg: ADDI2(reg,INDIRI2(addr))   "\talu2RRS R%c,R%0,%1,add,adc; ADDI2(r,INDIRI2(addr))	DH3\n"  5
reg: ADDI2(reg,reg)   "\talu2 R%c,R%0,R%1,add,adc; ADDI2(r,r)\n"  10
reg: ADDI4(reg,reg)   "\talu4 R%c,R%0,R%1,add,adc\n"  1
reg: ADDP2(reg,INDIRP2(addr))   "\talu2RRS R%c,R%0,%1,add,adc; ADDI2(r,INDIRP2(addr))	DH3.1\n"  5
reg: ADDP2(reg,reg)   "\talu2 R%c,R%0,R%1,add,adc	;ADDP2(reg,reg)\n"  10
reg: ADDU2(reg,reg)   "\talu2 R%c,R%0,R%1,add,adc; ADDU2(r,r)\n"  2
reg: ADDU4(reg,reg)   "\talu4 R%c,R%0,R%1,add,adc\n"  1
reg: BANDI2(reg,reg)  "\talu2 R%c,R%0,R%1,and,and\n"   1
reg: BANDI4(reg,reg)  "\talu4 R%c,R%0,R%1,and,and\n"   1
reg: BORI2(reg,reg)   "\talu2 R%c,R%0,R%1,or,or\n"    1
reg: BORI4(reg,reg)   "\talu4 R%c,R%0,R%1,or,or\n"    1
reg: BXORI2(reg,reg)  "\talu2 R%c,R%0,R%1,xor,xor\n"   1
reg: BXORI4(reg,reg)  "\talu4 R%c,R%0,R%1,xor,xor\n"   1
reg: BANDU2(reg,reg)  "\talu2 R%c,R%0,R%1,and,and; BANDU2(reg,reg)\n"   1
reg: BANDU4(reg,reg)  "\talu4 R%c,R%0,R%1,and,and\n"   1
reg: BORU2(reg,reg)   "\talu2 R%c,R%0,R%1,or,or\n"    1
reg: BORU4(reg,reg)   "\talu4 R%c,R%0,R%1,or,or\n"    1
reg: BXORU2(reg,reg)  "\talu2 R%c,R%0,R%1,xor,xor\n"  1
reg: BXORU4(reg,reg)  "\talu4 R%c,R%0,R%1,xor,xor\n"   1
reg: SUBI2(reg,reg)   "\talu2 R%c,R%0,R%1,sm,smb\n"  1
reg: SUBI4(reg,reg)   "\talu4 R%c,R%0,R%1,sm,smb\n"  1
reg: SUBP2(reg,reg)   "\talu2 R%c,R%0,R%1,sm,smb\n"  1
reg: SUBU2(reg,reg)   "\talu2 R%c,R%0,R%1,sm,smb\n"  1
reg: SUBU4(reg,reg)   "\talu4 R%c,R%0,R%1,sm,smb\n"  1

reg: ADDI2(reg,con)   "\talu2I R%c,R%0,%1,adi,adci; ADDI2(reg,con)\n"  2
reg: ADDI2(CVUI2(reg),CVUI2(reg))   "\talu1 R%c,R%0,R%1,add,adci; ADDI2(CVUI2(reg),CVUI2(reg))J2020-3\n"  5
reg: ADDI4(reg,con)   "\talu4I R%c,R%0,%1,adi,adci\n"  1
reg: ADDP2(reg,con)   "\talu2I R%c,R%0,%1,adi,adci; ADDP2(reg,con)\n"  2
reg: ADDU2(reg,con)   "\talu2I R%c,R%0,%1,adi,adci; ADDU2(reg,con)\n"  2
reg: ADDU4(reg,con)   "\talu4I R%c,R%0,%1,adi,adci\n"  1
reg: BANDI2(reg,con)  "\talu2I R%c,R%0,%1,ani,ani\n	;removed ?\tcpy2 R%c,R%0\n"   1
reg: BANDI4(reg,con)  "?\tcpy4 R%c,R%0\n\talu4I R%c,R%0,%1,ani,ani\n"   1
reg: BORI2(reg,con)   "\talu2I R%c,R%0,%1,ori,ori ;removed copy\n"    1
reg: BORI4(reg,con)   "?\tcpy4 R%c,R%0\n\talu4I R%c,R%0,%1,ori,ori\n"    1
reg: BXORI2(reg,con)  "\talu2I R%c,R%0,%1,xri,xri ;removed copy\n"
reg: BXORI4(reg,con)  "?\tcpy4 R%c,R%0\n\talu2I R%c,R%0,%1,xri,xri\n"   1
reg: BANDU2(reg,con)  "\talu2I R%c,R%0,%1,ani,ani ;removed copy;BANDU2(reg,con)  \n"   1
reg: BANDU4(reg,con)  "?\tcpy4 R%c,R%0\n\talu4I R%c,R%0,%1,ani,ani\n"   1
reg: BORU2(reg,con)   "\talu2I R%c,R%0,%1,ori,ori ;removed copy\n"    1
reg: BORU4(reg,con)   "?\tcpy4 R%c,R%0\n\talu4I R%c,R%0,%1,ori,ori\n"    1
reg: BXORU2(reg,con)  "\talu2I R%c,R%0,%1,xri,xri ;removed copy\n"   1
reg: BXORU4(reg,con)  "?\tcpy4 R%c,R%0\n\talu2I R%c,R%0,%1,xri,xri\n"   1
reg: SUBI2(reg,con)   "\talu2I R%c,R%0,%1,smi,smbi\n"  1
reg: SUBI4(reg,con)   "\talu4I R%c,R%0,%1,smi,smbi\n"  1
reg: SUBP2(reg,con)   "\talu2I R%c,R%0,%1,smi,smbi\n"  1
reg: SUBU2(reg,con)   "\talu2I R%c,R%0,%1,smi,smbi\n"  1
reg: SUBU4(reg,con)   "\talu4I R%c,R%0,%1,smi,smbi\n"  1

reg: LSHI2(reg,reg)  "?\tcpy2 R%c,R%0\n\tshl2R R%c,R%1; lshi2(r,r)\n"   2
reg: LSHI4(reg,reg)  "?\tcpy4 R%c,R%0\n\tshl4R R%c,R%1\n"  10
reg: LSHU2(reg,reg)  "?\tcpy2 R%c,R%0\n\tshl2R R%c,R%1; lshu2(r,r)\n"   2
reg: LSHU4(reg,reg)  "?\tcpy4 R%c,R%0\n\tshl4R R%c,R%1\n"  10
reg: RSHI2(reg,reg)  "?\tcpy2 R%c,R%0\n\tshrI2R R%c,R%1\n"   1
reg: RSHI4(reg,reg)  "?\tcpy4 R%c,R%0\n\tshRI4R R%c,R%1\n"  10
reg: RSHU2(reg,reg)  "?\tcpy2 R%c,R%0\n\tshrU2R R%c,R%1\n"   1
reg: RSHU4(reg,reg)  "?\tcpy4 R%c,R%0\n\tshRU4R R%c,R%1\n"  10

reg: LSHI2(reg,con)  "?\tcpy2 R%c,R%0\n\tshl2I R%c,%1\n"  1
reg: LSHI4(reg,con)  "?\tcpy4 R%c,R%0\n\tshl4I R%c,%1; LSHI4(reg,con)\n"  1
reg: LSHU2(reg,con)  "?\tcpy2 R%c,R%0\n\tshl2I R%c,%1\n"  1
reg: LSHU4(reg,con)  "?\tcpy4 R%c,R%0\n\tshl4I R%c,%1; LSHU4(reg,con)\n"  1
reg: RSHI2(reg,con)  "?\tcpy2 R%c,R%0\n\tshrI2I R%c,%1\n"  1
reg: RSHI4(reg,con)  "?\tcpy4 R%c,R%0\n\tshrI4I R%c,%1\n"  1
reg: RSHU2(reg,con)  "?\tcpy2 R%c,R%0\n\tshrU2I R%c,%1\n"   1
reg: RSHU4(reg,con)  "?\tcpy4 R%c,R%0\n\tshrU4I R%c,%1\n"  1

reg: BCOMI2(reg)  "\talu2I R%c,R%0,-1,xri,xri; was?\tcpy2 R%c,R%0+xor2I R%c,-1\n"   1
reg: BCOMI4(reg)  "\talu4I R%c,R%0,-1,xri,xri; was?\tcpy4 R%c,R%0+xor4I R%c,-1\n"   1
reg: BCOMU2(reg)  "\talu2I R%c,R%0,-1,xri,xri; was?\tcpy2 R%c,R%0+xor2I R%c,-1\n"   1
reg: BCOMU4(reg)  "\talu4I R%c,R%0,-1,xri,xri; was?\tcpy4 R%c,R%0+xor4I R%c,-1\n"   1
reg: NEGI2(reg)   "\tnegI2 R%c,R%0 ;was alu2I R%c,R%0,0,sdi,sdbi\n"  1
reg: NEGI4(reg)   "\tnegI4 R%c,R%0 ;was alu4I R%c,R%0,0,sdi,sdbi\n"  1
reg: LOADI1(reg)  "?\tcpy1 R%c,R%0;LOADI1(reg)\n"  move(a)
reg: LOADU1(reg)  "?\tcpy1 R%c,R%0;LOADU1(reg)\n"  move(a)
reg: LOADI2(reg)  "?\tcpy2 R%c,R%0 ;LOADI2(reg)\n"  move(a)+10

reg: LOADU2(SUBU2(reg,consm))  "\tdecm R%c,%1	;LOADU2(SUBU2(reg,consm))\n"  0

reg: LOADU2(reg)  "?\tcpy2 R%c,R%0 ;LOADU2*(reg)\n"  move(a)+10
reg: LOADI4(reg)  "?\tcpy4 R%c,R%0; LOADI4*\n"  move(a)+1
reg: LOADP2(reg)  "?\tcpy2 R%c,R%0 ;LOADP2(reg)\n"  move(a)+1
reg: LOADU4(reg)  "?\tcpy4 R%c,R%0; LOADU4(reg)\n"  move(a)+1

reg:  INDIRF4(addr)     "\tld4 R%c,%0;INDIRF4(addr)\n"   fp()
stmt: ASGNF4(addr,reg)  "\tst4 R%1,%0; ASGNF4(addr,reg)\n"  fp()
reg: ADDF4(reg,reg)  "\tCcall fp_add ;ADDF4(reg,reg)\n"   fp()
reg: DIVF4(reg,reg)  "\tCcall fp_div ;DIVF4(reg,reg)\n"   fp()
reg: MULF4(reg,reg)  "\tCcall fp_mul ;MULF4(reg,reg)\n"   fp()
reg: SUBF4(reg,reg)  "\tCcall fp_sub ;SUBF4(reg,reg)\n"   fp()
reg: LOADF4(reg)     "?\tcpy4 R%c,R%0; LOADU4(reg)\n" fp()+move(a)
reg: NEGF4(reg)      "\tnegf4 R%c,R%0; NEGF4(reg)"  fp()
reg: CVII2(reg)  "?\tcpy1 R%c,R%0\n\tsExt R%c ;CVII2: widen signed char to signed int (sign extend)\n"  1
reg: CVIU2(reg)  "?\tcpy1 R%c,R%0\n\tsExt R%c ;CVIU2: widen signed char to signed int (sign extend)\n"  1
reg: CVUI2(reg)  "?\tcpy1 R%c,R%0\n\tzExt R%c ;CVUI2(reg): widen unsigned char to signed int (zero extend)*\n"    1
reg: CVUU2(reg)  "?\tcpy1 R%c,R%0\n\tzExt R%c ;CVUU2: widen unsigned char to unsigned int (zero extend)*\n"  1
reg: CVII4(reg)  "?\tcpy2 R%c,R%0\n\tsext4 R%c; CVII4\n"  1
reg: CVIU4(reg)  "?\tcpy2 R%c,R%0\n\tsext4 R%c; *CVIU4(reg)\n"  1
reg: CVIU4(CVUI2(INDIRU1(addr))) "\tld1 R%c,%0\n\tzExt R%c\n\tzExt4 R%c; CVIU4(INDIRU1(addr)):*HOORAY*widen unsigned char to long\n"  1
reg: CVUI4(reg)  "?\tcpy2 R%c,R%0\n\tzext4 R%c; CVUI4 jan 16\n"  1
reg: CVUU4(reg)  "?\tcpy2 R%c,R%0\n\tzext4 R%c ; CVUU4\n"  1
reg: CVFF4(reg)  ""  fp()
reg: CVIF4(reg)  "#\tcvif4 %c,%0 ; CVIF4(reg) convert int/long to float\n"  fp()
reg: CVFI2(reg)  "\tccall cvfi4; CVFI4(reg) convert float to long(should work for int's)\n"  fp()
reg: CVFI4(reg)  "\tccall cvfi4; CVFI4(reg) convert float to long\n"  fp()
stmt: LABELV  "%a:\n"
stmt: JUMPV(reg)   "\tjumpv R%0; JUMPV(reg)\n"  1
stmt: JUMPV(acon)  "\tlbr %0\n"   1+wjrpixie
stmt: JUMPV(acon)  "\txbr %0 ;**PIXIES**\n"   1
stmt: EQI2(reg,reg)  "\tjeqI2 R%0,R%1,%a; EQI2(reg,reg)\n"   2
stmt: EQU2(reg,reg)  "\tjeqI2 R%0,R%1,%a;EQU2(reg,reg)\n"   2
stmt: GEI2(reg,reg)  "\tjcI2 R%0,R%1,lbdf,%a; GE is flipped test from LT\n"   2
stmt: GEU2(reg,reg)  "\tjcU2 R%0,R%1,lbdf,%a; GE is flipped test from LT\n"  2
stmt: GTI2(reg,reg)  "\tjcI2 R%1,R%0,lbnf,%a ;GT is reversed operands from LT\n"   2
stmt: GTU2(reg,reg)  "\tjcU2 R%1,R%0,lbnf,%a ;GT same as LT but operands reversed\n"  2
stmt: LEI2(CVUI2(reg),CVUI2(reg))  "\tjcU1 R%1,R%0,lbdf,%a ;LE is flipped test & operands **opt 20**\n"  1
stmt: LEI2(reg,reg)  "\tjcI2 R%1,R%0,lbdf,%a ;LE is flipped test & operands\n"  2
stmt: LEU2(reg,reg)  "\tjcU2 R%1,R%0,lbdf,%a ;LE is flipped test & operands\n"  2
stmt: LTI2(CVUI2(reg),CVUI2(reg))  "\tjcU1 R%0,R%1,lbnf,%a; LT=lbnf i.e. subtract B from A and jump if borrow **opt 20**\n"   1
stmt: LTI2(reg,reg)  "\tjcI2 R%0,R%1,lbnf,%a; LT=lbnf i.e. subtract B from A and jump if borrow \n"   2
stmt: LTU2(reg,reg)  "\tjcU2 R%0,R%1,lbnf,%a; LT=lbnf i.e. subtract B from A and jump if borrow \n"  2
stmt: NEI2(CVUI2(reg),CVUI2(reg))  "\tjneU1 R%0,R%1,%a; NE - nopromo 20-05-12 wjr\n"   1
stmt: NEI2(reg,reg)  "\tjneU2 R%0,R%1,%a; NE\n"   2
stmt: NEU2(reg,reg)  "\tjneU2 R%0,R%1,%a; NE\n"   2
stmt: EQI4(reg,reg)  "\tjeqI4 R%0,R%1,%a\n"   2
stmt: EQU4(reg,reg)  "\tjeqI4 R%0,R%1,%a\n"   2
stmt: GEI4(reg,reg)  "\tjcI4 R%0,R%1,lbdf,%a; GE is flipped test from LT\n"   2
stmt: GEU4(reg,reg)  "\tjcU4 R%0,R%1,lbdf,%a; GE is flipped test from LT\n"   2
stmt: GTI4(reg,reg)  "\tjcI4 R%1,R%0,lbnf,%a ;GT is reveresed operands from LT\n"   2
stmt: GTU4(reg,reg)  "\tjcU4 R%1,R%0,lbnf,%a ;GT same as LT but operands reversed\n"  2
stmt: LEI4(reg,reg)  "\tjcI4 R%1,R%0,lbdf,%a ;LE is flipped test & operands\n"  2
stmt: LEU4(reg,reg)  "\tjcU4 R%1,R%0,lbdf,%a ;LE is flipped test & operands\n"  2
stmt: LTI4(reg,reg)  "\tjcI4 R%0,R%1,lbnf,%a; LT=lbnf i.e. subtract B from A and jump if borrow \n"   2
stmt: LTU4(reg,reg)  "\tjcU4 R%0,R%1,lbnf,%a; LT=lbnf i.e. subtract B from A and jump if borrow \n"  2
stmt: NEI4(reg,reg)  "\tjneU4 R%0,R%1,%a; NE\n"   2   
stmt: NEU4(reg,reg)  "\tjneU4 R%0,R%1,%a; NE\n"   2   

stmt: EQI2(CVUI2(reg),con8bit)  "\tjeqU1I R%0,%1,%a;EQI2(CVUI2(reg),con8bit)**opt20**\n"   1
stmt: EQI2(reg,con)  "\tjeqU2I R%0,%1,%a;EQI2(reg,con)\n"   2
stmt: EQI4(reg,con)  "\tjeqU4I R%0,%1,%a\n"   2
stmt: EQU2(reg,con)  "\tjeqU2I R%0,%1,%a;EQU2(reg,con)*\n"   2
stmt: EQU4(reg,con)  "\tjeqU4I R%0,%1,%a\n"   2
stmt: GEI2(CVUI2(reg),con)  "\tjcI1I R%0,%1,lbdf,%a; GE is flipped test from LT J2020-4\n"   1
stmt: GEI2(reg,con)  "\tjcI2I R%0,%1,lbdf,%a; GE is flipped test from LT\n"   2
stmt: GEI4(reg,con)  "\tjgeI4I R%0,%1,%a; GE\n"   2
stmt: GEU2(reg,con)  "\tjcI2I R%0,%1,lbdf,%a; GE is flipped test from LT\n"  2
stmt: GEU4(reg,con)  "\tjgeU4I R%0,%1,%a; GE\n"  2
stmt: GTI2(reg,con)  "\tjnI2I R%0,%1,lbnf,%a; GT reverse  the subtraction\n"   2
stmt: GTI4(reg,con)  "\tjgtI4I R%0,%1,%a\n"   2
stmt: GTU2(reg,con)  "\tjnU2I R%0,%1,lbnf,%a; GT reverse the subtraction J2020-5\n"  1
stmt: GTU4(reg,con)  "\tjgtU4I R%0,%1,%a\n"  2
stmt: LEI2(reg,con)  "\tjnI2I R%0,%1,lbdf,%a ;LEI2 %1 %0 %a; LE is flipped test & subtraction\n"   2
stmt: LEI4(reg,con)  "\tjleI4I R%0,%1,%a\n"   2
stmt: LEU2(reg,con)  "\tjnU2I R%0,%1,lbdf,%a ;LEU2 %1 %0 %a; LE is flipped test & subtraction\n"  2
stmt: LEU4(reg,con)  "\tjleU4I R%0,%1,%a\n"  2
stmt: LTI2(CVUI2(reg),con)  "\tjcI1I R%0,%1,lbnf,%a  ;LTI2=lbnf i.e. subtract immedB from A and jump if borrow - nopromo 20-05-12 J2020-6\n"   1
stmt: LTI2(reg,con)  "\tjcI2I R%0,%1,lbnf,%a  ;LT=lbnf i.e. subtract immedB from A and jump if borrow J2020-7\n"   2
stmt: LTI4(reg,con)  "\tjltI4I R%0,%1,%a\n"   2
stmt: LTU2(CVUI2(reg),con)  "\tjcU1I R%0,%1,lbnf,%a ;LTU2=lbnf i.e. subtract immedB from A and jump if borrow - nopromo 20-05-12 J2020-8\n"  1
stmt: LTU2(reg,con)  "\tjcU2I R%0,%1,lbnf,%a ;LT=lbnf i.e. subtract immedB from A and jump if borrow J2020-9\n"  2
stmt: LTU4(reg,con)  "\tjltU4I R%0,%1,%a\n" 2
stmt: NEI2(CVUI2(reg),con0) "\tjnzU1 R%0,%a; NEI2(CVUI2(reg),con0)\n"   1
stmt: NEI2(reg,con0) "\tjnzU2 R%0,%a; NE 0\n"   1
stmt: EQI2(CVUI2(reg),con0) "\tjzU1 R%0,%a; EQ 0 - nopromo 20-05-12\n"   1
stmt: EQI2(reg,con0) "\tjzU2 R%0,%a; EQ I 0\n"   1
stmt: EQU2(reg,con0) "\tjzU2 R%0,%a; EQ U 0\n"   1
stmt: EQU2(CVUI2(reg),con0) "\tjzU1 R%0,%a; EQ 0 - nopromo 20-05-12\n"   1
stmt: NEI2(reg,con)  "\tjneU2I R%0,%1,%a; NE\n"   2
stmt: NEI4(reg,con)  "\tjneI4I R%0,%1,%a; NE\n"   2
stmt: NEU2(reg,con0) "\tjnzU2 R%0,%a; NE 0 \n"   1 
stmt: NEU2(reg,con)  "\tjneU2I R%0,%1,%a; NE\n"   2
stmt: NEU4(reg,con)  "\tjneU4IR R%0,%1,%a; NE\n"   2

stmt: NEI2(CVUI2(reg),con8bit)  "\tjneU1I R%0,%1,%a	; DH 4\n"  1 

stmt: EQF4(reg,reg)  "\tjeqI4 R%0,R%1,%a; EQF4(reg,reg)\n"  fp()
stmt: GTF4(reg,reg)  "\tjcF4 R%1,R%0,lbnf,%a;GTF4(reg,reg) - reverse operands\n"  fp()
stmt: GEF4(reg,reg)  "\tjcF4 R%0,R%1,lbdf,%a;GEF4(reg,reg) - reverse test\n"  fp()+10
stmt: LEF4(reg,reg)  "\tjcF4 R%1,R%0,lbdf,%a; LEF4(reg,reg) - reverse test and operands\n"  fp()
stmt: LTF4(reg,reg)  "\tjcF4 R%0,R%1,lbnf,%a;LTF4(reg,reg)LT=lbnf i.e. subtract B from A and jump if borrow\n"  fp()+10
stmt: NEF4(reg,reg)  "\tjneU4 R%0,R%1,%a; NEF4(reg,reg)\n"  fp()
ar:   ADDRGP2     "%a"

reg:  CALLF4(ar)  "\tCcall %0;CALLF4(ar)\n"  fp()
reg:  CALLI2(ar)  "\tCcall %0; CALLI2(ar)\n"  1
reg:  CALLI4(ar)  "\tCcall %0\n"  1
reg:  CALLP2(ar)  "\tCcall %0\n"  1
reg:  CALLU2(ar)  "\tCcall %0;CALLU2(ar)*\n"  1
reg:  CALLU4(ar)  "\tCcall %0\n"  1
stmt: CALLV(ar)  "\tCcall %0\n"  1
ar: reg    "*R%0"
ar: CNSTP2  "%a"   range(a, 0, 0x0ffff)
stmt: RETF4(reg)  "# retn\n"  fp()
stmt: RETI2(reg)  "# retn\n"  1
stmt: RETI4(reg)  "# retn\n"  1
stmt: RETU2(reg)  "# retn\n"  1
stmt: RETU4(reg)  "# retn\n"  1
stmt: RETP2(reg)  "# retn\n"  1
stmt: RETV(reg)   "# retn\n"  1
stmt: ARGF4(reg)  "# arg\n"  fp()
stmt: ARGI2(reg)  "# arg\n"  1
stmt: ARGI4(reg)  "# arg\n"  1
stmt: ARGP2(reg)  "# arg\n"  1
stmt: ARGU2(reg)  "# arg\n"  1
stmt: ARGU4(reg)  "# arg\n"  1

stmt: ARGB(INDIRB(reg))       "# argb %0\n"      1
stmt: ASGNB(reg,INDIRB(reg))  "\tblkcpy R%0,R%1,%a; ASGNB(reg,INDIRB(reg))\n"  1

stmt: IASMV                 "# emit inline assembly\n"

%%
static void progend(void){
	if (wjrelfos){
		print(	"\tinclude lcc1802epiloOS.inc\n");	//elfos epilog
	} else {
		print(	"\tinclude lcc1802epiloCX.inc\n");	//standard epilog
	}	
	if (wjrpixie==1){
		print(	"\tinclude lcc1802PXcode.inc\n");	//needed for 1861
	}
	if (wjrfloats){
		print("\tinclude LCC1802fp.inc\n"); //floating point code
	}
	print("\tinclude IO1802.inc\n");
	print("\tinclude LCC1802finale.inc\n");

}
static void progbeg(int argc, char *argv[]) {
        int i;
        time_t now;
        struct tm* ptmNow;
        static char rev[] = "$Version: 5.2 - XR18CX $";
 
        {
                union {
                        char c;
                        int i;
                } u;
                u.i = 0;
                u.c = 1;
                swap = ((int)(u.i == 1)) != IR->little_endian;
        }
        parseflags(argc, argv);
        for (i = 0; i < argc; i++){
        	//fprintf(stderr,"arg %d is %s\n",i,argv[i]);
                if (strstr(argv[i], "-elfos") != 0){ //accept combined args
                	fprintf(stderr,"there are elves in high memory!\n");
                        wjrelfos = 1;
                }
                if (strstr(argv[i], "-pixie3") != 0){ 
                	fprintf(stderr,"dedicated long branch register with reduced temps - no long arithmetic\n");
                        wjrpixie = 3;
                } else if (strstr(argv[i], "-pixie2") != 0){ 
                	fprintf(stderr,"dedicated long branch register\n");
                        wjrpixie = 2;
                } else if (strstr(argv[i], "-pixie") != 0){ 
                	fprintf(stderr,"long branch emulation\n");
                        wjrpixie = 1;
                }
                if (strstr(argv[i], "-mulcall") != 0){ //forces subroutine calls for multiply
                	fprintf(stderr,"subroutine calls for multiply\n");
                        wjrMulInlineWeight=15; //disallows inline multiply
                }
                if (strstr(argv[i], "-volatile") != 0){ //accept combined args
                	//fprintf(stderr,"going volatile\n");
                        wjrvolatile = 1;
                }
                if (strstr(argv[i], "-cpu1805") != 0){ //accept combined args
                	fprintf(stderr,"allowing regs 4&5 for variables\n");
                        wjrcpu1805 = 1;
                }
                if (strstr(argv[i], "-env=") != 0){ //accept combined args
                        wjrenv = strstr(argv[i], "-env=")+5;//point to the environment variable
                	fprintf(stderr,"environment specified %s\n",wjrenv);
                }
                if (strstr(argv[i], "-argbufloc=") != 0){ //accept combined args
                        argbufloc = atoi(strstr(argv[i], "-argbufloc=")+11);
                	fprintf(stderr,"argument buffer relocated to 0x%x\n",argbufloc);
                }
                if (strstr(argv[i], "-romsyms=") != 0){ //accept combined args -- __romlink support, see decl.c
                        char *path = strstr(argv[i], "-romsyms=")+9;
                	fprintf(stderr,"loading ROM symbol table from %s\n",path);
                        romsyms_load(path);
                }
	}
	fprintf(stderr,"September, I'll remember the fourth wave\n");  //just so I know who's playing
        time(&now);
        ptmNow = localtime(&now);
        printf("; generated by lcc-xr18CX/OS-%d %s on %s\n", wjrelfos, rev, asctime(ptmNow));
        printf("SP:\tequ	%d ;stack pointer\n" "memAddr: equ	%d\n" "retAddr: equ	%d\n" //pass on reg definitions to assembler
        	"retVal:\tequ\t%d\n" "regArg1: equ	%d\n" "regArg2: equ	%d\n",
        	reg_sp_actual,REG_MEMADDR,REG_RETADDR,REG_RETVAL,REG_FIRST_ARG,REG_FIRST_ARG+1);
        printf("ARGBUF:\tequ\t%d ;fixed RAM area for C function arguments, %d bytes\n",
        	argbufloc,ARGBUF_SIZE);
	if (wjrcpu1805){ //compiling for 1804/1805/1806
		print("\tcpu\t1805a\n");
	}
	if (wjrenv){ //environment includes specified
		print("\tinclude %sprolog.inc\n",wjrenv);
	}
	if (wjrpixie==1){
		print("\tlisting off\n" "\tinclude lcc1802proloPX.inc\n" "\tlisting on\n");//include pixilated macro package
	}else if (wjrpixie==2){
		print("\tlisting off\n" "\tinclude lcc1802proloPX2.inc\n" "\tlisting on\n");//include pixilated macros with branch assist
	}else if (wjrpixie==3){
		print("\tlisting off\n" "\tinclude lcc1802proloPX2.inc\n" "\tlisting on\n");//include pixilated macros with branch assist
	}else{
		print("\tlisting off\n" "\tinclude lcc1802proloCX.inc\n" "\tlisting on\n");//include standard macro package
	}
		

        for (i = 0; i < NUM_IREGS; i++)
                ireg[i] = mkreg("%d", i, 1, IREG);  //makes a symbol for each of the integer regs
        ireg[reg_sp_actual]->x.name = "sp";		//renames the stack pointer to show as "sp"
        for (i = 0; i < NUM_IREGS; i += 2)
                lreg[i] = mkreg("L%d", i, 3, IREG); //makes one long-reg symbol for each ireg pair (0-1, 2-3 etc) intvar and inttmp still control the usage
        for (i = 0; i < NUM_IREGS; i += 2)
                freg2[i] = mkreg("L%d", i, 3, IREG); //(was FREG)makes one float-reg symbol for each ireg pair (0-1, 2-3 etc) intvar and inttmp still control the usage
        
        //for (i = 0; i < 31; i ++)
        //        freg2[i] = mkreg("F%d", i, 1, FREG); //makes symbols for 32 float regs 

        rp1p2 = mkreg("p1p2", REG_FIRST_ARG, 3, IREG);	//makes a symbol for the parameter register pair when used to hold a long.

        freg2w = mkwildcard(freg2);	//wildcards represent the set of registers for floats, ints, and longs
        iregw = mkwildcard(ireg);
        lregw = mkwildcard(lreg);
        tmask[IREG] = INTTMP; tmask[FREG] = INTTMP;	//tmask & vmask show what regs can be used for vars and temps
        vmask[IREG] = vmask[FREG] = INTVAR; //1802 register variables
        if (wjrvolatile){
        	//fprintf(stderr,"reduced register variables\n");
        	vmask[IREG] = vmask[FREG] =INTVAR & ~REGSVOLATILE; //eliminate 0&1
        }
        if (2==wjrpixie){
        	fprintf(stderr,"pixie branch assist\n");
        	vmask[IREG] = vmask[FREG] =INTVAR & ~REGSPIXIE2; //eliminate 7,0,1!
        }
        if (3==wjrpixie){
        	fprintf(stderr,"pixie branch assist, shuffled temps->vars\n");
        	vmask[IREG] = vmask[FREG] = PX3VARS;	//regs 8,6 for variables
        	tmask[IREG] = tmask[FREG] = PX3TMP;	//regs 11,10,9 only for temps
        }
        if (wjrcpu1805){
        	fprintf(stderr,"1805 register variables\n");
        	vmask[IREG] = vmask[FREG] | REGS1805; //allow 4&5
        }
        	/* R6 is REG_RETADDR: every Ccall/Cretn (SCAL 6 / SRET 6 on -cpu1805,
        	   see lcc1802proloCX.inc) clobbers it as the call-linkage register,
        	   so no mode above may leave it eligible for ordinary variable
        	   storage; a variable placed there survives only until the next
        	   function call anywhere in its scope, then silently reads back
        	   garbage. Enforced unconditionally here rather than trusting each
        	   branch above (INTVAR, REGSPIXIE2, PX3VARS) to individually keep
        	   excluding it, since that's exactly how this got missed before. */
        	vmask[IREG] &= REGS6SAFE; vmask[FREG] &= REGS6SAFE;
        // fprintf(stderr,"Register mask for variables %X\n",vmask[IREG]);
        blkreg = mkreg(SZ_REG_FIRST_TEMP, REG_FIRST_TEMP, 7, IREG);	//which regs to use for block copies and moves
}
static Symbol rmap(int opk) {
        switch (optype(opk)) {
        case I: case U:
                return (opsize(opk) == 4) ? lregw : iregw;
        case P: case B:
                return iregw;
        case F:
                fp();
                return freg2w;
        default:
                return 0;
        }
}
/* Tracks, per va_alist function actually called from this translation unit,
   the largest total argument byte count (named + variadic) that any call
   site here has been seen to pass it; gen.c's docall() already computes
   this exact total for every CALL node (in p->syms[0], read via
   varargcallbytes() below) before target()/emit2() ever see the node, so
   this just remembers the running maximum per callee.

   function() uses this (see varargmaxfor()) to size a va_alist function's
   vararg tail-copy to what this file actually needs instead of the full
   ARGBUF_SIZE ceiling. This is only sound if every call to a given va_alist
   function is compiled; i.e. textually appears; before that function's
   own definition, so the maximum is complete by the time function() reads
   it; if no call was seen yet function() falls back to the safe ARGBUF_SIZE
   bound. Putting va_alist function definitions after all their call sites
   in the same file (as this project's printf()/stdlib.c already does)
   keeps this exact. */
#define MAX_VARARG_TRACK 8
static Symbol vatrack_sym[MAX_VARARG_TRACK];
static int    vatrack_max[MAX_VARARG_TRACK];
static int    vatrack_n = 0;

static int varargcallbytes(Node p) {
        return p->syms[0]->u.c.v.i;	/* set by gen.c's docall() before target()/emit2() run */
}
static void varargtrack(Node p) {
        Symbol f;
        int bytes, i;
        if (generic(p->op) != CALL || p->kids[0] == NULL || p->kids[0]->syms[0] == NULL)
                return;
        f = p->kids[0]->syms[0];	/* callee for a direct call, from its ADDRGP2 */
        if (!isfunc(f->type) || !variadic(f->type))
                return;
        bytes = varargcallbytes(p);
        for (i = 0; i < vatrack_n; i++)
                if (vatrack_sym[i] == f) {
                        if (bytes > vatrack_max[i])
                                vatrack_max[i] = bytes;
                        return;
                }
        if (vatrack_n < MAX_VARARG_TRACK) {
                vatrack_sym[vatrack_n] = f;
                vatrack_max[vatrack_n] = bytes;
                vatrack_n++;
        }
}
static int varargmaxfor(Symbol f) {
        int i;
        for (i = 0; i < vatrack_n; i++)
                if (vatrack_sym[i] == f)
                        return vatrack_max[i];
        return ARGBUF_SIZE;	/* no call seen (yet) in this file; safe fallback */
}
static void target(Node p) {
	int sz = opsize(p->op);
        assert(p);
        varargtrack(p);
        switch (specific(p->op)) {
        case CALL+V:
                break;
        case CALL+F:
                setreg(p, freg2[REG_FIRST_ARG]);
                break;
        case CALL+I: case CALL+P: case CALL+U:
        	if (sz<4){
                	setreg(p, ireg[REG_RETVAL]);
                	break;
                } else{
			setreg(p,lreg[REG_FIRST_ARG]);
			break;          	

		}
        case RET+F:
                	//targeting is done in emit2
                break;
        case RET+I: case RET+U: case RET+P:
        	if (sz<4){
                	rtarget(p, 0, ireg[REG_RETVAL]);
                	break;
                } else{
                	//targeting is done in emit2
			break;          	

		}
                break;
        case ARG+F: case ARG+I: case ARG+P: case ARG+U:
                /* Arguments are always stored to the fixed ARGBUF area (see
                   emit2()'s ARG+x case and function() below), never targeted
                   to a hardware register, so there is nothing to do here
                   beyond the bounds check that argreg() performs. */
                argreg(p->x.argno, p->syms[2]->u.c.v.i, optype(p->op), opsize(p->op), 0);
                break;
         case CVI+F:	//wjr targetting conversion to float
        	//fprintf(stderr,"target selection for CV+I\n");
		setreg(p,lreg[REG_FIRST_TEMP]);
		//if (opsize((p->kids[0])->op)>2){
        	//	fprintf(stderr,"target is long\n");
			rtarget(p, 0, lreg[REG_FIRST_TEMP]);
		//} else{
        	//	fprintf(stderr,"target is short\n");
		//	rtarget(p, 0, lreg[REG_LAST_TEMP-1]);
		//}
		break;
        case CVF+I:	//wjr targetting conversion from float
        	//fprintf(stderr,"target selection for CV+F\n");
		setreg(p,lreg[REG_FIRST_TEMP]);
		rtarget(p, 0, lreg[REG_FIRST_TEMP]);
		break;
        case ASGN+B: rtarget(p->kids[1], 0, blkreg); break;
        case ARG+B:  rtarget(p->kids[0], 0, blkreg); break;
        case MUL+I: case MUL+U:
        case DIV+I: case MOD+I: 
        case DIV+U: case MOD+U:
        case DIV+F: case MUL+F: case ADD+F: case SUB+F:
        /* REVIEW: LSH, RSH? */
        	sz = opsize(p->op);
        	if (sz<4){
			assert(REG_FIRST_ARG+1 <= REG_LAST_ARG);
			setreg(p, ireg[REG_RETVAL]);
			rtarget(p, 0, ireg[REG_FIRST_ARG]);
			rtarget(p, 1, ireg[REG_FIRST_ARG+1]);
			break;
		}else{
			setreg(p,lreg[REG_FIRST_TEMP]);
			rtarget(p, 0, lreg[REG_FIRST_TEMP]);
			rtarget(p, 1, lreg[REG_LAST_TEMP-1]);
			break;
		}

        }
}
static void clobber(Node p) {
        assert(p);
        switch (specific(p->op)) {
        case CALL+F:
                spill(INTTMP | INTRET, IREG, p);
                //spill(FLTTMP,          FREG, p);
                break;
        case CALL+I: case CALL+P: case CALL+U:
                spill(INTTMP,          IREG, p);
                //spill(FLTTMP | FLTRET, FREG, p);
                break;
        case CALL+V:
                spill(INTTMP | INTRET, IREG, p);
                //spill(FLTTMP | FLTRET, FREG, p);
                break;
        }
}
extern void dumptree(Node);

static void emitmulcon(int c){ //going to multiply the value in R13 by the constant, result in R15
	int bits=c;
	print("	;inline multiplication by a constant %d\n",c);
	if (c&1){ //bottom bit set
		print("	cpy2 R15,R13\n");
	} else {
		print("	ld2z R15\n");
	}
	bits=bits>>1;
	while(bits!=0){
		print("	shl2I	R13,1\n");
		if (bits&1){
			print("	alu2 R15,R15,R13,add,adc\n");
		}
		bits=bits>>1;
	}
}
static void emit2(Node p) {
        int dst, n, src, sz, ty;
        int szkids0;
    	int op = specific(p->op); 
        switch (specific(p->op)) {
        //default: dumptree(p); fputc('\n',stderr); break; /* debugging only*/
        case MUL+I: case MUL+U: //handling multiplication by a small constant
        	assert(opsize(p->op)==2);
        	assert(generic(p->kids[0]->op)==CNST);
         	emitmulcon(p->kids[0]->syms[0]->u.c.v.i);
         	break;
        case RET+I: case RET+U: case RET+F: //trying to handle long returns
                ty = optype(p->op);
                sz = opsize(p->op);
                if (sz==4){ //long return value goes in rp1p2
              		print("\tcpy4 rp1p2,R%s\n",p->kids[0]->syms[2]->x.name);
                }
                break;
        case ARG+F: case ARG+I: case ARG+P: case ARG+U:
                /* Outgoing arguments always go to the fixed RAM argument area
                   (ARGBUF), never to a hardware register; this is what lets
                   any number/mix of arguments be passed, and lets hand-written
                   or linked assembly read them back by a fixed symbolic
                   address instead of having to guess which register the
                   compiler chose. */
                sz = opsize(p->op);
                argreg(p->x.argno, p->syms[2]->u.c.v.i, optype(p->op), sz, 0); /* bounds check only */
                src = getregnum(p->x.kids[0]);
                if (2==sz)
                        print("\tst2 R%d,'D',ARGBUF+%d,0 ;store outgoing arg into fixed RAM argument area\n",src, p->syms[2]->u.c.v.i);
                else
                        print("\tst4 RL%d,'D',ARGBUF+%d,0 ;store outgoing arg into fixed RAM argument area\n",src, p->syms[2]->u.c.v.i);
                break;
        case ASGN+B:
		fprintf(stderr,"ASGN+B\n");
                dalign = salign = p->syms[1]->u.c.v.i; //not an issue for the 180x
                blkcopy(getregnum(p->x.kids[0]), 0,
                        getregnum(p->x.kids[1]), 0,
                        p->syms[0]->u.c.v.i, tmpregs);
                break;
        case ARG+B:
		fprintf(stderr,"ARG+B - should not occur\n");
                assert(0);

                break;
        case IASM+V:	//wjr jan 31 -3 lines
                asminline(p);
                break;
         case CVI+F:	//wjr april 10
		//fprintf(stderr,"CVI+F\n");
		//dumptree(p->kids[0]);
                ty = optype(p->op);
                sz = opsize(p->op);
                szkids0 = opsize((p->kids[0])->op);
                if (szkids0==2){
                	print("\tsext4 R%s; emit2:extend int to long for float conversion\n",p->syms['c' - 'a']->x.name);
                }
                print("\tCcall cvif4; emit2\n");
                //fprintf(stderr,"\t ty=%d,sz=%d,szkids0=%d\n ",ty,sz,szkids0);
                //print("\tst%d RL%d,'O',sp,(%d+1);\n\n",sz,src, p->syms[2]->u.c.v.i);  //17-02-05 1802                
		//fputs(p->syms['c' - 'a']->x.name, stderr);
                break;
       }
}
/* All C-level arguments live in the fixed ARGBUF memory area now, so this no
   longer picks a hardware register; it only checks that the argument list
   fits in ARGBUF_SIZE.  Kept as a function (instead of inlining the check)
   so every call site; target(), emit2(), function(); enforces the same
   limit the same way. argno/ty0 are unused but kept so the three call sites
   don't need to change shape. */
static Symbol argreg(int argno, int offset, int ty, int sz, int ty0) {
        assert((offset&1) == 0);
        if (offset + sz > ARGBUF_SIZE)
                error("argument list needs %d bytes, but only %d are available in the fixed ARGBUF area (see ARGBUF_SIZE)\n",
                        offset + sz, ARGBUF_SIZE);
        return NULL;
}
static void doarg(Node p) {
        static int argno;
        int align;

        if (argoffset == 0)
                argno = 0;
        p->x.argno = argno++;
        align = p->syms[1]->u.c.v.i < 2 ? 2 : p->syms[1]->u.c.v.i;
        p->syms[2] = intconst(mkactual(align,
                p->syms[0]->u.c.v.i));
}
static void local(Symbol p) {
        if (askregvar(p, rmap(ttob(p->type))) == 0)
                mkauto(p);
}
static void gensaveregs(){  //routine to save registers extracted from function()
	int i;
	for (i = 0; i < NUM_IREGS; i++)	//now we save the int registers
                if (usedmask[IREG]&(1<<i)) {
                        print("\tpushr R%d\n", i);  //push the register
                }
        for (i = 20; i <= 30; i += 2)	//now we save the float registers
                if (usedmask[FREG]&(3<<i)) {
                        print("\tpushfloat RF%d\n", i);
                }
}
static void genrldregs(){ //reload any saved registers 
	int i;
        for (i = NUM_IREGS; i >=0 ; i--)	//now we reload the int registers
                if (usedmask[IREG]&(1<<i)) {
                        print("\tpopr R%d\n", i);  
                }
        for (i = 30; i >= 20; i -= 2)	//now we reload the float registers
                if (usedmask[FREG]&(3<<i)) {
                        print("\tpopfloat RF%d\n", i);
                }
}

static void function(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {
        int i, sizefsave, sizeisave, varargs, retlink;

        usedmask[0] = usedmask[1] = 0;
        freemask[0] = freemask[1] = ~(unsigned)0;
        offset = maxoffset = maxargoffset = 0;
        for (i = 0; callee[i]; i++) //find the last parameter
                ;
        varargs = variadic(f->type) //set the flag for variable arguments if the function is typed variadic 
                || i > 0 && strcmp(callee[i-1]->name, "va_alist") == 0; //or if the last argument is "va_alist"

	//this loop assigns every parameter its byte offset inside the fixed
	//ARGBUF area; that's where the caller already deposited the value
	//(see emit2()'s ARG+x case); and decides whether the callee keeps
	//its own copy of the value in a register instead of re-reading it
	//from ARGBUF every time; unlike the old register-window scheme this
	//is no longer limited to the first couple of parameters, since every
	//parameter arrives in memory now.
	for (i = 0; callee[i]; i++) {
                Symbol p = callee[i];
                Symbol q = caller[i];
                assert(q);
                offset = roundup(offset, q->type->align);
                p->x.offset = q->x.offset = offset;	//byte offset within ARGBUF
                p->x.name = q->x.name = stringd(offset);
                offset = roundup(offset + q->type->size, 2);
                if (!varargs && !isstruct(q->type) && !p->addressed) {
                        p->sclass = REGISTER;	//askregvar() requires this to already be set; it
                                                //downgrades back to AUTO itself if no register is free
                        if (askregvar(p, rmap(ttob(p->type)))) {
                                q->sclass = REGISTER;
                                q->type = p->type;
                        } else
                                q->sclass = AUTO;
                } else {
                        p->sclass = q->sclass = AUTO;
                }
        }
        assert(!caller[i]);
        if (offset > ARGBUF_SIZE)
                error("%s needs %d bytes of arguments; only %d are available in the fixed ARGBUF area (see ARGBUF_SIZE)\n",
                        f->name, offset, ARGBUF_SIZE);

        /* retlink: how much phantom space to reserve, right above the local
           frame, for the caller's return-linkage word(s); see below.
           Only matters for functions that emit a parameter copy-out (any
           AUTO-class callee[], or a vararg tail): those are the only ones
           whose prologue writes into this phantom area at all. Everyone
           else keeps the original 2-byte reservation, unchanged.

           A plain SCAL-based call (CcallD, used for every direct call in
           this codebase) leaves exactly one linkage word; the return
           address; sitting right above the callee's own frame, and the
           original code sized this phantom gap for exactly that (offset=2).

           But a function reached through the app-side syscall trampolines
           (Ccall *R9, see syscall.inc / the "Ccall addr" indirect-call
           macro in lcc1802proloCX.inc) is entered with a *second* linkage
           word above that: the trampoline's own "pushr r6" (it must save
           the real return address somewhere while it borrows R6 to jump
           through R9, then SRET's built-in pop restores it). A parameter
           copy-out sized for only one linkage word writes its first 2
           bytes squarely on top of that second word; the *caller's own*
           saved-R6 link, needed only once this function returns; with
           whatever stale bytes happen to be sitting past ARGBUF's actually-
           used prefix. The caller then returns into garbage.
           Root-caused by tracing a minimal repro (app code calling a
           variadic BIOS function; printf(); through the syscall
           trampoline, from a helper that is itself called, not inlined
           into main()) against the emulator instruction-by-instruction:
           R6 at the corrupted return matched exactly the address that had
           been loaded as printf's own string-literal argument moments
           earlier, i.e. leftover ARGBUF bytes from a prior call, landing
           exactly 3 bytes above the callee's entry SP as predicted by this
           formula. Any C function can potentially be reached both directly
           and through a trampoline (same compiled body either way), so the
           extra word must be reserved unconditionally for any function
           that copies a parameter out of ARGBUF; the compiler can't know
           at compile time which calling path a given call site will use. */
        retlink = varargs ? 4 : 2;
        for (i = 0; !varargs && callee[i]; i++)
                if (callee[i]->sclass == AUTO)
                        retlink = 4;

        offset = retlink; //wjr jan 8 allow for spot taken by saved return address
        gencode(caller, callee);  //while generating the dag tree, gencode will set offsets for locals,
        			//count calls in ncalls and mark what registers are used in usedmask
        /* Which regs actually get pushed/popped in this function's own
           prologue/epilogue if it uses them, i.e. which "variable"
           registers a caller can trust to survive a call to this
           function. MUST track vmask[IREG]; the SAME mask the
           register allocator (askregvar(), rmap(), local()) actually
           hands out variables from; not a fixed subset of it: any
           register the allocator considers fair game for a variable has
           to be saved here too, or a callee that happens to reuse that
           physical register for its OWN variable/parameter silently
           clobbers the caller's value with no save/restore in between.
           This used to be the static macro INT_CALLEE_SAVE (== INTVAR),
           which missed REGS1805 (registers 4-5, added to vmask below
           under -cpu1805) entirely; e.g. sd_command()'s own `arg`
           parameter landing in R4:R5 would clobber sd_write_block()'s
           `block` argument, also R4:R5, the instant sd_command() was
           entered. See gpio-img-test/marta-test.c's R6 bug (a related,
           earlier-fixed instance of the same class of mistake: a
           register eligible for variables that the calling convention
           doesn't actually protect) for the general shape of this bug. */
        usedmask[IREG] &= vmask[IREG];
        usedmask[FREG] &= 0x00000000;		//not saving the float temps
        //maxargoffset (the largest *outgoing* argument list this function
        //itself builds for calls it makes) no longer needs a home in this
        //function's own frame: outgoing arguments go to the shared, fixed
        //ARGBUF area instead of a per-call stack slot, so it plays no part
        //in framesize any more; one less thing every call-making function
        //has to carry around.
        sizefsave = 4*bitcount(usedmask[FREG]);
        sizeisave = 2*bitcount(usedmask[IREG]);
        framesize = sizefsave + sizeisave 	//the float and int reg save areas,
                + roundup(maxoffset, 2);       	// and the area for locals
 	print(";;function_start %s %t\n",f->x.name, f->type);
        printf("%s:\t\t;framesize=%d\n", f->x.name,framesize); //wjr june 27 2013
        if (framesize > retlink) { 
        		if (0!=(usedmask[IREG]+usedmask[FREG])){  //if there are regs to save
        			if (roundup(maxoffset,2)>retlink){
        				print("\treserve %d; save room for local variables\n", roundup(maxoffset,2)-retlink);
        			}
				gensaveregs(); //save the registers
			} else {
				print("\treserve %d\n", framesize-retlink); //just reserve the stack frame 
			}
        }

        //Copy every incoming parameter out of the shared ARGBUF area before
        //the body runs and possibly calls something else (recursively or
        //not): a nested call's own outgoing arguments reuse these very same
        //ARGBUF bytes, so anything left unread there would be clobbered.
        //Parameters bound to a register just get loaded straight into it;
        //everything else lands in this function's own frame, at the same
        //offset (relative to framesize) that the ADDRFP2 rule's "%A" format
        //already expects for any other reference to it in the body.
        for (i = 0; callee[i]; i++) {
                Symbol p = callee[i];
                int off = p->x.offset;
                int islong = caller[i]->type->size == 4;
                if (p->sclass == REGISTER) {
                        int rn = p->x.regnode->number;
                        if (islong)
                                print("\tld4 RL%d,'D',ARGBUF+%d,0 ;fetch arg from fixed RAM argument area\n", rn, off);
                        else
                                print("\tld2 R%d,'D',ARGBUF+%d,0 ;fetch arg from fixed RAM argument area\n", rn, off);
                } else {
                        int dst = off + framesize;	//same "+framesize" convention the ADDRFP2/%A rule uses
                        if (islong)
                                print("\tld4 RL%s,'D',ARGBUF+%d,0\n\tst4 RL%s,'O',sp,(%d+1) ;copy arg from ARGBUF into local frame\n",
                                        SZ_REG_FIRST_TEMP, off, SZ_REG_FIRST_TEMP, dst);
                        else
                                print("\tld2 R%s,'D',ARGBUF+%d,0\n\tst2 R%s,'O',sp,(%d+1) ;copy arg from ARGBUF into local frame\n",
                                        SZ_REG_FIRST_TEMP, off, SZ_REG_FIRST_TEMP, dst);
                }
        }
        if (varargs && callee[i-1]) {
                //Old-style (va_alist) varargs: a single-pass compiler has no
                //way to know, in general, how many bytes any given call will
                //pass in the variadic tail. varargmaxfor() gives the largest
                //total this file's own calls to `f` were seen to use (see
                //varargtrack() above); tight as long as every call to `f`
                //was already compiled by this point, which holds whenever
                //the va_alist function is defined after all its call sites
                //in the same file (true here: printf() comes from stdlib.c,
                //#include'd at the very end). Falls back to the full
                //ARGBUF_SIZE ceiling if no call was seen yet, so this is
                //never smaller than what's actually safe.
                int vmax = varargmaxfor(f);
                int start = roundup(callee[i-1]->x.offset + callee[i-1]->type->size, 2);
                int j;
                for (j = start; j < vmax; j += 2)
                        print("\tld2 R%s,'D',ARGBUF+%d,0\n\tst2 R%s,'O',sp,(%d+1) ;copy vararg tail from ARGBUF\n",
                                SZ_REG_FIRST_TEMP, j, SZ_REG_FIRST_TEMP, j + framesize);
        }

        emitcode();

        if (framesize > retlink) { 
        	if (0!=(usedmask[IREG]+usedmask[FREG])){  //if there are regs to restore
			genrldregs(); //reload the registers
			if (roundup(maxoffset,2)>retlink){
				print("\trelease %d; release room for local variables \n", roundup(maxoffset,2)-retlink);
			}
		} else {
			print("\trelease %d\n", framesize-retlink); //just release the stack frame 
		}
        }
        print("\tCretn\n\n");
 	print(";;function_end$$ %s\n",f->x.name);
}
static void defconst(int suffix, int size, Value v) {
        if (suffix == F && size == 4) {
                float f = v.d;
                print("\tdd 0x%x\n", *(unsigned *)&f);
        }        else if (suffix == P)
                print("\tdw %u\n", v.p);
        else if (size == 1)
                print(suffix==I ? "\tdb %d\n" : "\tdb %u\n", suffix == I ? v.i : v.u);
        else if (size == 2)
                print(suffix==I ? "\tdw %d\n" : "\tdw %u\n", suffix == I ? v.i : v.u);
        else if (size == 4)
                print(suffix==I ? "\tdd %d\n" : "\tdd %u\n", suffix == I ? v.i : v.u);
}
static void defaddress(Symbol p) {
        print("\tdw %s\n", p->x.name);
}
static void defstring(int n, char *str) {
        char *s;

        for (s = str; s < str + n; s++)
                print("\tdb %d\n", (*s)&0377);
}
static void export(Symbol p) {
        //print("\t;global %s\n", p->x.name);
}
static void import(Symbol p) {
        //if (!isfunc(p->type))
                //print("\t;global %s\n", p->x.name); /* good enough? */
}
static void defsymbol(Symbol p) {
        if (p->scope >= LOCAL && p->sclass == STATIC)
                p->x.name = stringf("L%d", genlabel(1));
        else if (p->generated)
                p->x.name = stringf("L%s", p->name);
        else if (p->scope == GLOBAL || p->sclass == EXTERN)
                p->x.name = stringf("_%s", p->name);
        else
                assert(p->scope != CONSTANTS || isint(p->type) || isptr(p->type)),
                p->x.name = p->name;
}
static void address(Symbol q, Symbol p, long n) {
        if (p->scope == GLOBAL
        || p->sclass == STATIC || p->sclass == EXTERN)
                q->x.name = stringf("%s%s%D", p->x.name,
                        n >= 0 ? "+" : "", n);
        else {
                assert(n <= INT_MAX && n >= INT_MIN);
                q->x.offset = p->x.offset + n;
                q->x.name = stringd(q->x.offset);
        }
}
static void global(Symbol p) {
	int dbsize;
        if (p->type->align > 1)
                print("\talign %d\n", p->type->align);
        print("%s:\n", p->x.name);
        if (p->u.seg == BSS){
        	if ((p->type->size)<1000) { //do small declares as one piece
                	printf("\tglobss %d; define global BSS\n", p->type->size);
                } 
                else{
                	for(dbsize=p->type->size;dbsize>1000;dbsize-=1000){//do it in 1000 byte chunks
                		printf("\tglobss %d; define global BSS\n", 1000);
                	}
                	if (dbsize>0) {	//emit remainder
                		printf("\tglobss %d; define global BSS\n", dbsize);
                	}
                }
	}
                	
}
static void segment(int n) {
	char * segnames[]={"N/A ","CODE","BSS ","DATA","CODE"}; //code, bss, initialized data, literals
	if (1==n || 4==n){ //compiler generated code or literals
	    print("\torgc\n"); //   reset the location counter to just after the jump
	} else{
	    if (2==n || 3==n){ //if this is compiler generated global data
	        print ("\torgd\n");
	    }
	}
        cseg = n;
}
static void space(int n) {
        if (cseg != BSS)
                print("\tdb %d dup(0) ;zerofill\n", n); //was print("\tds %d\n", n);
}
static void blkloop(int dreg, int doff, int sreg, int soff, int size, int tmps[]) { //probably not used
        int lab = genlabel(1);

        print("addi R%d,R%d,%d\n", sreg, sreg, size&~7);
        print("addi R%d,R%d,%d\n", tmps[2], dreg, size&~7);
        blkcopy(tmps[2], doff, sreg, soff, size&7, tmps);
        print("L%d:\n", lab);
        print("addi r%d,r%d,%d\n", sreg, sreg, -8);
        print("addi r%d,r%d,%d\n", tmps[2], tmps[2], -8);
        blkcopy(tmps[2], doff, sreg, soff, 8, tmps);
        print("cmp r%d,r%d\nbltu L%d\n", dreg, tmps[2], lab);
}
static void blkfetch(int size, int off, int reg, int tmp) {//probably not used
	fprintf(stderr,"blkfetch(size=%d,off=%d,reg=%d,tmp=%d,salign=%d\n",size,off,reg,tmp,salign);
        assert(size == 1 || size == 2);
        if (size == 1)
                print("lb r%d,%d(r%d)\n",  tmp, off, reg);
        else {
                //assert(salign >= size); //wjr why?
                print("lw r%d,%d(r%d)\n",   tmp, off, reg);
        }
}
static void blkstore(int size, int off, int reg, int tmp) {
        assert(size == 1 || size == 2);
        if (size == 1)
                print("sb r%d,%d(r%d);blkstore\\n",  tmp, off, reg);
        else {
                //assert(dalign >= size);//wjr why?
                print("blkstore sw r%d,%d(r%d);blkstore\n",  tmp, off, reg);
        }
}
static void stabinit(char *, int, char *[]);
static void stabline(Coordinate *);
static void stabsym(Symbol);

static char *currentfile;

static int bitcount(unsigned mask) {
        unsigned i, n = 0;

        for (i = 1; i; i <<= 1)
                if (mask&i)
                        n++;
        return n;
}

/* stabinit - initialize stab output */
static void stabinit(char *file, int argc, char *argv[]) {
/*        if (file) {
                print("file \"%s\"\n", file);
                currentfile = file;
        }
*/
}

/* stabline - emit stab entry for source coordinate *cp */
static void stabline(Coordinate *cp) {
/*        if (cp->file && cp->file != currentfile) {
                print("file \"%s\"\n", cp->file);
                currentfile = cp->file;
        }
        print("loc %d\n", cp->y);
*/
}

/* stabsym - output a stab entry for symbol p */
static void stabsym(Symbol p) {
/*        if (p == cfunc && IR->stabline)
                (*IR->stabline)(&p->src);
*/
}

static int fp() {
    if (!wjrfloats) {
    	//fprintf(stderr,"Floating point used\n");
    	wjrfloats=1;
    }
    return 10;
}

static int samesame(){
	fprintf(stderr,"same called\n");
	return 1;
}

Interface xr18CXIR = {	//each entry represents size, alignment, out-of-line literals needed
        1, 1, 0,  /* char */
        2, 1, 0,  /* short */
        2, 1, 0,  /* int */
        4, 4, 0,  /* long jan 20*/
        4, 4, 0,  /* long long  jan 20*/
        4, 4, 1,  /* float  jan 20*/
        4, 4, 1,  /* double  jan 20*/
        4, 4, 1,  /* long double  jan 20*/
        2, 1, 0,  /* pointer */
        0, 1, 0,  /* struct */
        0,  /* little_endian */
        1,  /* mulops_calls */
        0,  /* wants_callb */
        0,  /* wants_argb */
        1,  /* left_to_right */
        0,  /* wants_dag */
        1,  /* 21-08-04 unsigned_char */
        address,
        blockbeg,
        blockend,
        defaddress,
        defconst,
        defstring,
        defsymbol,
        emit,
        export,
        function,
        gen,
        global,
        import,
        local,
        progbeg,
        progend,
        segment,
        space,
        0, 0, 0, stabinit, stabline, stabsym, 0,
        {
                2,      /* max_unaligned_load */
                rmap,
                blkfetch, blkstore, blkloop,
                _label,
                _rule,
                _nts,
                _kids,
                _string,
                _templates,
                _isinstruction,
                _ntname,
                emit2,
                doarg,
                target,
                clobber,

        }
};
static char rcsid[] =
	"XR18 Next Generation Optimized COMX Compliant - LCC backend for 1802/4/5/6 target";
//there is a peculiar problem with respect to longs and variadic functions
//if printf(char*,...) is called with printf("%l",(long)1) (or any variadic called with a 2 16 bit and a 32 bit argument,
//the 16 bit thing gets stored in a register and the 32 bit thing on the stack frame.
//However, the line flagged **see bottom** stores the 2nd argument register over top of the beginning of the 32 bit one.
//I've avoided that by making the alignment of longs=4 which will leave a 2 byte gap but that's pretty ugly
