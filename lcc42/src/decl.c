#include "c.h"

static char rcsid[] = "$Id: decl.c,v 1.1 2002/08/28 23:12:42 drh Exp $";

#define add(x,n) (x > inttype->u.sym->u.limits.max.i-(n) ? (overflow=1,x) : x+(n))
#define chkoverflow(x,n) ((void)add(x,n))
#define bits2bytes(n) (((n) + 7)/8)
static int regcount;

static List autos, registers;
/* VLA-declared locals (see dcllocal()'s is_vla handling) awaiting an
   auto-free at the end of their own block -- accumulated and consumed
   the same way autos/registers are, including the same save/restore
   dance in compound() around a nested scope (see that function's own
   comment for why: a nested block's compound() call resets its
   accumulators unconditionally, so whatever the enclosing scope had
   pending would otherwise be lost the moment a nested block runs). */
static List vla_pending;
Symbol cfunc;		/* current function */
Symbol retv;		/* return value location for structs */

static void checkref(Symbol, void *);
static Symbol dclglobal(int, char *, Type, Coordinate *);
static Symbol dcllocal(int, char *, Type, Coordinate *);
static Symbol dclparam(int, char *, Type, Coordinate *);
static Type dclr(Type, char **, Symbol **, int);
static Type dclr1(char **, Symbol **, int);
static void decl(Symbol (*)(int, char *, Type, Coordinate *));
extern void doconst(Symbol, void *);
static void doglobal(Symbol, void *);
static void doextern(Symbol, void *);
static void exitparams(Symbol []);
static void fields(Type);
static void funcdefn(int, char *, Type, Symbol [], Coordinate, int, int);
static void initglobal(Symbol, int);
static void oldparam(Symbol, void *);
static Symbol *parameters(Type);
static void parse_inline_body(Symbol, Symbol []);
static Type specifier(int *);
static Type structdcl(int);
static Type tnode(int, Type);
void program(void) {
	int n;
	
	level = GLOBAL;
	for (n = 0; t != EOI; n++)
		if (kind[t] == CHAR || kind[t] == STATIC
		|| t == ID || t == '*' || t == '(') {
			decl(dclglobal);
			deallocate(STMT);
			if (!(glevel >= 3 || xref))
			deallocate(FUNC);
		} else if (t == ';') {
			warning("empty declaration\n");
			t = gettok();
		} else {
			error("unrecognized declaration\n");
			t = gettok();
		}
	if (n == 0)
		warning("empty input file\n");
}
/* Set by specifier() whenever it consumes an `inline` specifier, valid
   until the next specifier() call; decl() reads it right after calling
   specifier() to decide whether the function it's about to parse (if
   this turns out to be a function definition at all) should go through
   parse_inline_body() instead of the normal compound()/codegen path.
   Not threaded through specifier()'s own return value/sclass out-param
   since `inline` is orthogonal to storage class (e.g. `static inline`
   is legal) and every other specifier() caller can just ignore it. */
static int seen_inline;

/* Set by specifier() whenever it consumes a `__bank(N)` specifier;
   valid until the next specifier() call, same handoff shape as
   seen_inline above and for the same reason (orthogonal to storage
   class -- `static __bank(1)` is legal). See enode.c's call()/idtree()
   for what actually happens with a symbol's bank once decl()/dclglobal()
   below store it. Only 0 (the default) and 1 are accepted for now --
   see sbc1806/bankcompile.h's own top comment for why banks 2/3 aren't
   supported yet (the assembler's own 16-bit address ceiling for this
   CPU target, confirmed by hand -- not a design choice). */
static int seen_bank;

/* === __bank(1) placement (see enode.c's call()/idtree() for the other
   half: bank-crossing codegen at use sites) ===

   Every bank-1 function/global's real compiled bytes need to land in
   ROM bank 1's own 32KB region of the output (real address 32768+),
   while every label/address *inside* that code still needs to resolve
   to the normal CPU-visible 0x0000-0x7FFF range -- both banks map to
   that same window at runtime, selected by the latch, so a label
   sitting at "real" 32768 needs a *value* of 0, not 32768, or every
   call/jump target compiled against it would be wrong the instant
   bank 1 is actually selected. The assembler's PHASE/DEPHASE
   directives do exactly this split (label value vs. real output
   position); ORG still controls the real position, which is what
   makes the final image land bank 1's bytes at file offset 32768.

   Wrapping every bank-1 declaration in ORG+PHASE separately (rather
   than one contiguous ORG 32768 block) is required, not a style
   choice: bank-0 and bank-1 declarations can be interleaved arbitrarily
   in the source, and this compiler has no separate link/relocation
   pass to sort them out afterward -- each bank-1 declaration has to
   "resume" wherever bank 1's own content last left off (bank1pos,
   persisted via the assembler's own SET label,$, the same idiom
   BIOS_GPU.c's syscall() function already uses for its jump table) and
   "return" to wherever the normal bank-0 flow was (bankhome), every
   single time.

   Confirmed by hand (not guessed) that this only reaches bank 1: the
   assembler's own real-address tracking for this CPU target is
   strictly 16-bit regardless of PHASE, so ORG 65536 (bank 2) errors
   with "address overflow" -- banks 2/3 would need genuinely separate
   assembler invocations later combined, like romlink-test/'s Makefile
   already does by hand, not a single-pass trick like this one. */
static void bank_enter(int n) {
	print("\tSET bankhome,$\n");
	print("\tIFDEF bank%dstarted\n", n);
	print("\tORG bank%dpos\n", n);
	print("\tELSE\n");
	print("\tORG %d\n", n * 32768);
	print("bank%dstarted SET 1\n", n);
	print("\tENDIF\n");
	/* PHASE $-N*32768, not a constant 0: $ is wherever the ORG above
	   just landed us -- 32768 the first time, bank%dpos (wherever this
	   bank's content previously left off) every time after. A constant
	   "PHASE 0" would reset the logical address back to 0 on every
	   re-entry instead of continuing where the bank's own content left
	   off, corrupting every later declaration's own labels by aliasing
	   them all onto the *first* one's addresses. Caught by hand: a
	   second bank-1 declaration landed at logical address 0, the same
	   as the first, instead of continuing after it. */
	print("\tPHASE $-%d\n", n * 32768);
}
static void bank_exit(int n) {
	print("\tDEPHASE\n");
	print("\tSET bank%dpos,$\n", n);
	print("\tORG bankhome\n");
}

/* === `__romlink` support (see enode.c's bank_wrap_call() for the other
   half: redirecting a call through sbc1806/bankcall.inc using these
   values instead of a local label) ===

   `__romlink int foo(int);` marks a *declaration-only* function (never
   given a body in this translation unit -- decl() below turns giving
   one into an error) whose real (bank, address) comes from a table
   loaded via -Wf,-romsyms=<path> (parsed in xr18CX.md's progbeg(),
   which calls romsyms_load() before any C source is parsed, so the
   table is always complete before the first __romlink declaration can
   be seen). That table is generated by toolchain/bin/romlink from a
   *separately built* ROM image's own assembler listing -- see that
   script's own header comment. This is what lets a RAM program call a
   function that's already sitting in ROM (BIOS-provided or otherwise)
   without paying to recompile and re-link its own copy of it.

   Deliberately explicit (a source-level marker) rather than "auto-
   detect any call to an undefined function that happens to match the
   table": this compiler is single-pass with no lookahead, so at the
   point a call is parsed there's no way to know whether a local
   definition for that name will still turn up later in the same file.
   Requiring __romlink up front sidesteps that ambiguity entirely. */
struct romsym {
	char *name;
	int bank;
	unsigned long addr;
	struct romsym *next;
};
static struct romsym *romsyms;

void romsyms_load(const char *path) {
	FILE *f = fopen(path, "r");
	char name[256];
	int bank;
	unsigned long addr;

	if (!f) {
		error("cannot open -romsyms file `%s'\n", path);
		return;
	}
	while (fscanf(f, "%255s %d %lu", name, &bank, &addr) == 3) {
		struct romsym *s;
		NEW0(s, PERM);
		s->name = string(name);
		s->bank = bank;
		s->addr = addr;
		s->next = romsyms;
		romsyms = s;
	}
	fclose(f);
}

static int romsyms_lookup(const char *name, int *bank, unsigned long *addr) {
	struct romsym *s;
	for (s = romsyms; s; s = s->next)
		if (s->name == name) { /* both interned via string(), pointer compare is enough */
			*bank = s->bank;
			*addr = s->addr;
			return 1;
		}
	return 0;
}

/* Mirrors seen_bank's own handoff shape and hazards exactly -- see
   seen_bank's comment above. Reset to 0 by specifier() on every call,
   so it never leaks from one declaration into the next. */
static int seen_romlink;

/* === VLA support (see dclr1()'s '[' case, and vla_rewrite() near
   dcllocal()) ===
   in_local_declarator: true only while parsing a *local* variable's
   own declarator (decl() sets it based on whether dcllocal is the
   `dcl` callback it was given -- see compound()'s decl(dcllocal) call
   -- so it's false for parameters (dclparam), struct fields (fields()
   never goes through decl() at all), and globals). A non-constant
   array size is only meaningful -- and only handled -- while this is
   true; everywhere else the existing intexpr()-based "must be
   constant" error still applies unchanged.
   vla_size_tree / is_vla: single-slot handoff from dclr1() (which
   parses the size expression, deep inside dclr()'s recursion, with no
   direct path back to dcllocal()) to dcllocal() (which does the actual
   rewrite once the full declarator and its Symbol both exist). Safe as
   plain globals rather than something reentrant: array declarators
   don't nest (you can't write a VLA bound *inside* another array
   bound's brackets), and is_vla is consumed (reset to 0) immediately
   after dcllocal() reads it. */
static int in_local_declarator;
static Tree vla_size_tree;
static int is_vla;

static Type specifier(int *sclass) {
	int cls, cons, sign, size, type, vol;
	Type ty = NULL;

	cls = vol = cons = sign = size = type = 0;
	seen_inline = 0;
	seen_bank = 0;
	seen_romlink = 0;
	if (sclass == NULL)
		cls = AUTO;
	for (;;) {
		int *p, tt = t;
		switch (t) {
		case INLINE:   seen_inline = 1; t = gettok(); continue;
		case BANK:
			t = gettok();
			if (t != '(') {
				error("expected `(' after `__bank'\n");
				continue;
			}
			t = gettok();
			seen_bank = intexpr(')', 1);
			if (seen_bank != 0 && seen_bank != 1)
				error("__bank(%d): only banks 0 and 1 are supported\n", seen_bank);
			continue;
		case ROMLINK: seen_romlink = 1; t = gettok(); continue;
		case AUTO:
		case REGISTER: if (level <= GLOBAL && cls == 0)
		               	error("invalid use of `%k'\n", t);
		               p = &cls;  t = gettok();      break;
		case STATIC: case EXTERN:
		case TYPEDEF:  p = &cls;  t = gettok();      break;
		case CONST:    p = &cons; t = gettok();      break;
		case VOLATILE: p = &vol;  t = gettok();      break;
		case SIGNED:
		case UNSIGNED: p = &sign; t = gettok();      break;
		case LONG:     if (size == LONG) {
		                       size = 0;
		                       tt = LONG+LONG;
		               }
		               p = &size; t = gettok();      break;
		case SHORT:    p = &size; t = gettok();      break;
		case VOID: case CHAR: case INT: case FLOAT:
		case DOUBLE:   p = &type; ty = tsym->type;
		                          t = gettok();      break;
		case ENUM:     p = &type; ty = enumdcl();    break;
		case STRUCT:
		case UNION:    p = &type; ty = structdcl(t); break;
		case ID:
			if (istypename(t, tsym) && type == 0
			&& sign == 0 && size == 0) {
				use(tsym, src);
				ty = tsym->type;
				if (isqual(ty)
				&& ty->size != ty->type->size) {
					ty = unqual(ty);
					if (isconst(tsym->type))
						ty = qual(CONST, ty);
					if (isvolatile(tsym->type))
						ty = qual(VOLATILE, ty);
					tsym->type = ty;
				}
				p = &type;
				t = gettok();
			} else
				p = NULL;
			break;
		default: p = NULL;
		}
		if (p == NULL)
			break;
		if (*p)
			error("invalid use of `%k'\n", tt);
		*p = tt;
	}
	if (sclass)
		*sclass = cls;
	if (type == 0) {
		type = INT;
		ty = inttype;
	}
	if (size == SHORT     && type != INT
	||  size == LONG+LONG && type != INT
	||  size == LONG      && type != INT && type != DOUBLE
	||  sign && type != INT && type != CHAR)
		error("invalid type specification\n");
	if (type == CHAR && sign)
		ty = sign == UNSIGNED ? unsignedchar : signedchar;
	else if (size == SHORT)
		ty = sign == UNSIGNED ? unsignedshort : shorttype;
	else if (size == LONG && type == DOUBLE)
		ty = longdouble;
	else if (size == LONG+LONG) {
		ty = sign == UNSIGNED ? unsignedlonglong : longlong;
		if (Aflag >= 1)
			warning("`%t' is a non-ANSI type\n", ty);
	} else if (size == LONG)
		ty = sign == UNSIGNED ? unsignedlong : longtype;
	else if (sign == UNSIGNED && type == INT)
		ty = unsignedtype;
	if (cons == CONST)
		ty = qual(CONST, ty);
	if (vol  == VOLATILE)
		ty = qual(VOLATILE, ty);
	return ty;
}
static void decl(Symbol (*dcl)(int, char *, Type, Coordinate *)) {
	int sclass, is_inline, is_banked, is_romlink;
	Type ty, ty1;
	static char stop[] = { CHAR, STATIC, ID, 0 };

	ty = specifier(&sclass);
	is_inline = seen_inline;
	/* Captured into decl()'s own local, not read again later from the
	   file-scope seen_bank: dclr() below may itself parse a nested
	   parameter list (a function-typed declarator, e.g. a callback
	   global) whose own specifier() calls would otherwise clobber
	   seen_bank before it's used -- same reasoning as is_inline just
	   above, just a real (not hypothetical) hazard here because
	   funcdefn()'s own parameter parsing always does this. */
	is_banked = seen_bank;
	is_romlink = seen_romlink; /* same capture-immediately reasoning as is_banked */
	if (t == ID || t == '*' || t == '(' || t == '[') {
		char *id;
		Coordinate pos;
		id = NULL;
		pos = src;
		if (level == GLOBAL) {
			Symbol *params = NULL;
			ty1 = dclr(ty, &id, &params, 0);
			if (is_inline && !isfunc(ty1))
				error("`inline' is only valid on a function\n");
			if (params && id && isfunc(ty1)
			    && (t == '{' || istypename(t, tsym)
			    || (kind[t] == STATIC && t != TYPEDEF))) {
				if (sclass == TYPEDEF) {
					error("invalid use of `typedef'\n");
					sclass = EXTERN;
				}
				if (ty1->u.f.oldstyle) {
					if (is_inline)
						error("inline function `%s' must use ANSI-style parameters, not an old-style definition\n", id);
					exitscope();
				}
				if (is_romlink)
					error("`%s' is declared __romlink and cannot be given a body -- "
						"it's resolved from an externally built ROM image's own "
						"symbol table instead (see toolchain/bin/romlink)\n", id);
				/* Cleared regardless of is_romlink's value: funcdefn() below
				   calls dclglobal() internally to register cfunc, and that
				   call must never see a stale seen_romlink left over from
				   this declaration -- same hazard seen_bank's own comment
				   above documents, and moot here anyway since a __romlink
				   function is never valid with a body. */
				seen_romlink = 0;
				funcdefn(sclass, id, ty1, params, pos, is_inline && !ty1->u.f.oldstyle, is_banked);
				return;
			} else if (params)
				exitparams(params);
		} else {
			in_local_declarator = (dcl == dcllocal);
			ty1 = dclr(ty, &id, NULL, 0);
			in_local_declarator = 0;
		}
		for (;;) {
			if (Aflag >= 1 && !hasproto(ty1))
				warning("missing prototype\n");
			if (id == NULL)
				error("missing identifier\n");
			else if (sclass == TYPEDEF)
				{
					Symbol p = lookup(id, identifiers);
					if (p && p->scope == level)
						error("redeclaration of `%s'\n", id);
					p = install(id, &identifiers, level,
						level < LOCAL ? PERM : FUNC);
					p->type = ty1;
					p->sclass = TYPEDEF;
					p->src = pos;
				}
			else {
				/* Re-sync the file-scope seen_bank right before the
				   call: dclr() above may have clobbered it (a nested
				   specifier() call, e.g. for a function-pointer
				   declarator's own parameter list -- see is_banked's
				   own comment above). dclglobal() reads seen_bank
				   directly and must see *this* declaration's value,
				   not some parameter's -- and must see it immediately,
				   since an initialized global (`= ...`) triggers
				   defglobal() synchronously from inside dclglobal()
				   itself, before this call even returns, too early for
				   a "set p->bank on the returned Symbol" fixup here to
				   still be in time. */
				seen_bank = is_banked;
				seen_romlink = is_romlink; /* same re-sync reasoning as seen_bank just above */
				(void)(*dcl)(sclass, id, ty1, &pos);
			}
			if (t != ',')
				break;
			t = gettok();
			id = NULL;
			pos = src;
			ty1 = dclr(ty, &id, NULL, 0);
		}
	} else if (ty == NULL
	|| !(isenum(ty) ||
	     isstruct(ty) && (*unqual(ty)->u.sym->name < '1' || *unqual(ty)->u.sym->name > '9')))
		error("empty declaration\n");
	test(';', stop);
}
static Symbol dclglobal(int sclass, char *id, Type ty, Coordinate *pos) {
	Symbol p;

	if (sclass == 0)
		sclass = AUTO;
	else if (sclass != EXTERN && sclass != STATIC) {
		error("invalid storage class `%k' for `%t %s'\n",
			sclass, ty, id);
		sclass = AUTO;
	}
	p = lookup(id, identifiers);
	if (p && p->scope == GLOBAL) {
		if (p->sclass != TYPEDEF && eqtype(ty, p->type, 1))
			ty = compose(ty, p->type);
		else
			error("redeclaration of `%s' previously declared at %w\n", p->name, &p->src);

		if (!isfunc(ty) && p->defined && t == '=')
			error("redefinition of `%s' previously defined at %w\n", p->name, &p->src);

		if (p->sclass == EXTERN && sclass == STATIC
		||  p->sclass == STATIC && sclass == AUTO
		||  p->sclass == AUTO   && sclass == STATIC)
			warning("inconsistent linkage for `%s' previously declared at %w\n", p->name, &p->src);

	}
	if (p == NULL || p->scope != GLOBAL) {
		Symbol q = lookup(id, externals);
		if (q) {
			if (sclass == STATIC || !eqtype(ty, q->type, 1))
				warning("declaration of `%s' does not match previous declaration at %w\n", id, &q->src);

			p = relocate(id, externals, globals);
			p->sclass = sclass;
		} else {
			p = install(id, &globals, GLOBAL, PERM);
			p->sclass = sclass;
			(*IR->defsymbol)(p);
		}
		if (p->sclass != STATIC) {
			static int nglobals;
			nglobals++;
			if (Aflag >= 2 && nglobals == 512)
				warning("more than 511 external identifiers\n");
		}
	} else if (p->sclass == EXTERN)
		p->sclass = sclass;
	p->type = ty;
	p->src = *pos;
	/* Must happen before the initializer branch below: an initialized
	   global (`= ...`) calls defglobal() synchronously, from inside
	   initglobal(), before dclglobal() (this function) ever returns --
	   see decl()'s own comment at its (*dcl)(...) call site for why
	   seen_bank (not a parameter) is how this function's caller gets
	   the bank number in here at all. Functions go through funcdefn()
	   instead (an explicit parameter there, not this global), but
	   funcdefn() itself calls dclglobal() too (for cfunc) -- harmless:
	   seen_bank is stale/irrelevant there since funcdefn() sets
	   cfunc->bank itself right after, overwriting whatever this left. */
	p->bank = seen_bank;
	if (seen_romlink) {
		int rb;
		unsigned long ra;
		if (!isfunc(p->type))
			error("`%s': __romlink is only valid on functions\n", p->name);
		else if (p->defined)
			error("`%s' already has a body in this file -- __romlink is for "
				"functions with no local definition at all\n", p->name);
		else if (romsyms_lookup(p->name, &rb, &ra)) {
			p->romlink = 1;
			p->bank = rb;
			p->romaddr = ra;
		} else
			error("__romlink function `%s' not found in the loaded ROM symbol "
				"table -- check -Wf,-romsyms=<path> was given and the ROM "
				"image was actually rebuilt with toolchain/bin/romlink since "
				"`%s' was added\n", p->name, p->name);
	}
	if (t == '=' && isfunc(p->type)) {
		error("illegal initialization for `%s'\n", p->name);
		t = gettok();
		initializer(p->type, 0);
	} else if (t == '=') {
		initglobal(p, 0);
		if (glevel > 0 && IR->stabsym) {
			(*IR->stabsym)(p); swtoseg(p->u.seg); }
	} else if (p->sclass == STATIC && !isfunc(p->type)
	&& p->type->size == 0)
		error("undefined size for `%t %s'\n", p->type, p->name);
	return p;
}
static void initglobal(Symbol p, int flag) {
	Type ty;

	if (t == '=' || flag) {
		/* Wraps defglobal() *and* the initializer() call below in one
		   PHASE/DEPHASE bracket -- defglobal() alone only emits p's
		   label, while the actual initializer bytes (dw/db directives)
		   come from this separate, later initializer() call. Bracketing
		   defglobal() by itself left the label correctly placed at its
		   bank-1 logical address but let the real data bytes escape
		   into whatever bank-0/home address was current at the time,
		   since DEPHASE had already fired before initializer() ran. */
		if (p->bank == 1)
			bank_enter(1);
		if (p->sclass == STATIC) {
			for (ty = p->type; isarray(ty); ty = ty->type)
				;
			defglobal(p, isconst(ty) ? LIT : DATA);
		} else
			defglobal(p, DATA);
		if (t == '=')
			t = gettok();
		ty = initializer(p->type, 0);
		if (p->bank == 1)
			bank_exit(1);
		if (isarray(p->type) && p->type->size == 0)
			p->type = ty;
		if (p->sclass == EXTERN)
			p->sclass = AUTO;
	}
}
void defglobal(Symbol p, int seg) {
	p->u.seg = seg;
	swtoseg(p->u.seg);
	if (p->sclass != STATIC)
		(*IR->export)(p);
	(*IR->global)(p);
	p->defined = 1;
}

static Type dclr(Type basety, char **id, Symbol **params, int abstract) {
	Type ty = dclr1(id, params, abstract);

	for ( ; ty; ty = ty->type)
		switch (ty->op) {
		case POINTER:
			basety = ptr(basety);
			break;
		case FUNCTION:
			basety = func(basety, ty->u.f.proto,
				ty->u.f.oldstyle);
			break;
		case ARRAY:
			basety = array(basety, ty->size, 0);
			break;
		case CONST: case VOLATILE:
			basety = qual(ty->op, basety);
			break;
		default: assert(0);
		}
	if (Aflag >= 2 && basety->size > 32767)
		warning("more than 32767 bytes in `%t'\n", basety);
	return basety;
}
static Type tnode(int op, Type type) {
	Type ty;

	NEW0(ty, STMT);
	ty->op = op;
	ty->type = type;
	return ty;
}
static Type dclr1(char **id, Symbol **params, int abstract) {
	Type ty = NULL;

	switch (t) {
	case ID:                if (id)
					*id = token;
				else
					error("extraneous identifier `%s'\n", token);
				t = gettok(); break;
	case '*': t = gettok(); if (t == CONST || t == VOLATILE) {
					Type ty1;
					ty1 = ty = tnode(t, NULL);
					while ((t = gettok()) == CONST || t == VOLATILE)
						ty1 = tnode(t, ty1);
					ty->type = dclr1(id, params, abstract);
					ty = ty1;
				} else
					ty = dclr1(id, params, abstract);
				ty = tnode(POINTER, ty); break;
	case '(': t = gettok(); if (abstract
				&& (t == REGISTER || istypename(t, tsym) || t == ')')) {
					Symbol *args;
					ty = tnode(FUNCTION, ty);
					enterscope();
					if (level > PARAM)
						enterscope();
					args = parameters(ty);
					exitparams(args);
				} else {
					ty = dclr1(id, params, abstract);
					expect(')');
					if (abstract && ty == NULL
					&& (id == NULL || *id == NULL))
						return tnode(FUNCTION, NULL);
				} break;
	case '[': break;
	default:  return ty;
	}
	while (t == '(' || t == '[')
		switch (t) {
		case '(': t = gettok(); { Symbol *args;
					  ty = tnode(FUNCTION, ty);
					  enterscope();
					  if (level > PARAM)
					  	enterscope();
					  args = parameters(ty);
					  if (params && *params == NULL)
					  	*params = args;
					  else
					  	exitparams(args);
 }
		          break;
		case '[': t = gettok(); { int n = 0;
					  if (kind[t] == ID) {
					  	if (in_local_declarator) {
					  		/* VLA candidate: parse the bound as an
					  		   ordinary (not necessarily constant)
					  		   expression instead of going through
					  		   intexpr(), which unconditionally rejects
					  		   anything but a constant. If it *does*
					  		   turn out to be constant, treat it exactly
					  		   like the plain-array case always has --
					  		   only a genuinely non-constant bound goes
					  		   through the VLA path below, so ordinary
					  		   `char buf[16];`-style locals are
					  		   unaffected either way. See dcllocal()
					  		   for where vla_size_tree/is_vla actually
					  		   get turned into a malloc() call -- can't
					  		   be done here, before the declarator even
					  		   has a name or a Symbol yet. */
					  		Tree sizeexpr = expr1(']');
					  		if (sizeexpr->op == CNST+I || sizeexpr->op == CNST+U) {
					  			n = cast(sizeexpr, inttype)->u.v.i;
					  			if (n <= 0) {
					  				error("`%d' is an illegal array size\n", n);
					  				n = 1;
					  			}
					  		} else {
					  			vla_size_tree = sizeexpr;
					  			is_vla = 1;
					  			n = 0;
					  		}
					  	} else {
					  		n = intexpr(']', 1);
					  		if (n <= 0) {
					  			error("`%d' is an illegal array size\n", n);
					  			n = 1;
					  		}
					  	}
					  } else
					  	expect(']');
					  ty = tnode(ARRAY, ty);
					  ty->size = n; } break;
		default: assert(0);
		}
	return ty;
}
static Symbol *parameters(Type fty) {
	List list = NULL;
	Symbol *params;

	if (kind[t] == STATIC || istypename(t, tsym)) {
		int n = 0;
		Type ty1 = NULL;
		for (;;) {
			Type ty;
			int sclass = 0;
			char *id = NULL;
			if (ty1 && t == ELLIPSIS) {
				static struct symbol sentinel;
				if (sentinel.type == NULL) {
					sentinel.type = voidtype;
					sentinel.defined = 1;
				}
				if (ty1 == voidtype)
					error("illegal formal parameter types\n");
				list = append(&sentinel, list);
				t = gettok();
				break;
			}
			if (!istypename(t, tsym) && t != REGISTER)
				error("missing parameter type\n");
			n++;
			ty = dclr(specifier(&sclass), &id, NULL, 1);
			if ( ty == voidtype && (ty1 || id)
			||  ty1 == voidtype)
				error("illegal formal parameter types\n");
			if (id == NULL)
				id = stringd(n);
			if (ty != voidtype)
				list = append(dclparam(sclass, id, ty, &src), list);
			if (Aflag >= 1 && !hasproto(ty))
				warning("missing prototype\n");
			if (ty1 == NULL)
				ty1 = ty;
			if (t != ',')
				break;
			t = gettok();
		}
		fty->u.f.proto = newarray(length(list) + 1,
			sizeof (Type *), PERM);
		params = ltov(&list, FUNC);
		for (n = 0; params[n]; n++)
			fty->u.f.proto[n] = params[n]->type;
		fty->u.f.proto[n] = NULL;
		fty->u.f.oldstyle = 0;
	} else {
		if (t == ID)
			for (;;) {
				Symbol p;
				if (t != ID) {
					error("expecting an identifier\n");
					break;
				}
				p = dclparam(0, token, inttype, &src);
				p->defined = 0;
				list = append(p, list);
				t = gettok();
				if (t != ',')
					break;
				t = gettok();
			}
		params = ltov(&list, FUNC);
		fty->u.f.proto = NULL;
		fty->u.f.oldstyle = 1;
	}
	if (t != ')') {
		static char stop[] = { CHAR, STATIC, IF, ')', 0 };
		expect(')');
		skipto('{', stop);
	}
	if (t == ')')
		t = gettok();
	return params;
}
static void exitparams(Symbol params[]) {
	assert(params);
	if (params[0] && !params[0]->defined)
		error("extraneous old-style parameter list\n");
	if (level > PARAM)
		exitscope();
	exitscope();
}

static Symbol dclparam(int sclass, char *id, Type ty, Coordinate *pos) {
	Symbol p;

	if (isfunc(ty))
		ty = ptr(ty);
	else if (isarray(ty))
		ty = atop(ty);
	if (sclass == 0)
		sclass = AUTO;
	else if (sclass != REGISTER) {
		error("invalid storage class `%k' for `%t%s\n",
			sclass, ty, stringf(id ? " %s'" : "' parameter", id));
		sclass = AUTO;
	} else if (isvolatile(ty) || isstruct(ty)) {
		warning("register declaration ignored for `%t%s\n",
			ty, stringf(id ? " %s'" : "' parameter", id));
		sclass = AUTO;
	}

	p = lookup(id, identifiers);
	if (p && p->scope == level)
		error("duplicate declaration for `%s' previously declared at %w\n", id, &p->src);

	else
		p = install(id, &identifiers, level, FUNC);
	p->sclass = sclass;
	p->src = *pos;
	p->type = ty;
	p->defined = 1;
	if (t == '=') {
		error("illegal initialization for parameter `%s'\n", id);
		t = gettok();
		(void)expr1(0);
	}
	return p;
}
static Type structdcl(int op) {
	char *tag;
	Type ty;
	Symbol p;
	Coordinate pos;

	t = gettok();
	pos = src;
	if (t == ID) {
		tag = token;
		t = gettok();
	} else
		tag = "";
	if (t == '{') {
		static char stop[] = { IF, ',', 0 };
		ty = newstruct(op, tag);
		ty->u.sym->src = pos;
		ty->u.sym->defined = 1;
		t = gettok();
		if (istypename(t, tsym))
			fields(ty);
		else
			error("invalid %k field declarations\n", op);
		test('}', stop);
	}
	else if (*tag && (p = lookup(tag, types)) != NULL
	&& p->type->op == op) {
		ty = p->type;
		if (t == ';' && p->scope < level)
			ty = newstruct(op, tag);
	}
	else {
		if (*tag == 0)
			error("missing %k tag\n", op);
		ty = newstruct(op, tag);
	}
	if (*tag && xref)
		use(ty->u.sym, pos);
	return ty;
}
static void fields(Type ty) {
	{ int n = 0;
	  while (istypename(t, tsym)) {
	  	static char stop[] = { IF, CHAR, '}', 0 };
	  	Type ty1 = specifier(NULL);
	  	for (;;) {
	  		Field p;
	  		char *id = NULL;
	  		Type fty = dclr(ty1, &id, NULL, 0);
			p = newfield(id, ty, fty);
			if (Aflag >= 1 && !hasproto(p->type))
				warning("missing prototype\n");
			if (t == ':') {
				if (unqual(p->type) != inttype
				&&  unqual(p->type) != unsignedtype) {
					error("`%t' is an illegal bit-field type\n",
						p->type);
					p->type = inttype;
				}
				t = gettok();
				p->bitsize = intexpr(0, 0);
				if (p->bitsize > 8*inttype->size || p->bitsize < 0) {
					error("`%d' is an illegal bit-field size\n",
						p->bitsize);
					p->bitsize = 8*inttype->size;
				} else if (p->bitsize == 0 && id) {
					warning("extraneous 0-width bit field `%t %s' ignored\n", p->type, id);

					p->name = stringd(genlabel(1));
				}
				p->lsb = 1;
			}
			else {
				if (id == NULL)
					error("field name missing\n");
				else if (isfunc(p->type))
					error("`%t' is an illegal field type\n", p->type);
				/* A size-0 array here is either `type name[];` or (see
				   dclr1() above) an explicit `type name[0]` -- the
				   latter is rejected right there with its own "illegal
				   array size" error, so anything reaching this point is
				   a genuine flexible array member candidate (C99
				   6.7.2.1p18). Whether it's actually positioned legally
				   (last field, not the struct's only field) can't be
				   answered yet -- there may be more fields still to
				   parse after this one -- so that's deferred to the
				   field-layout loop below, once every field in this
				   struct/union is known. */
				else if (p->type->size == 0 && !isarray(p->type))
					error("undefined size for field `%t %s'\n",
						p->type, id);
			}
			if (isconst(p->type))
				ty->u.sym->u.s.cfields = 1;
			if (isvolatile(p->type))
				ty->u.sym->u.s.vfields = 1;
	  		n++;
	  		if (Aflag >= 2 && n == 128)
	  			warning("more than 127 fields in `%t'\n", ty);
	  		if (t != ',')
	  			break;
	  		t = gettok();
	  	}
	  	test(';', stop);
	  } }
	{ int bits = 0, off = 0, overflow = 0;
	  Field p, *q = &ty->u.sym->u.s.flist;
	  int nfields = 0;
	  ty->align = IR->structmetric.align;
	  for (p = *q; p; p = p->link)
	  	nfields++;
	  for (p = *q; p; p = p->link)
	  	if (isarray(p->type) && p->type->size == 0) {
	  		/* Flexible array member (C99 6.7.2.1p18): legal only as the
	  		   last of at least two fields. p->link (this field's
	  		   position in the ORIGINAL, declaration-order chain, unlike
	  		   *q/q below which the compaction pass further down
	  		   rewrites to drop anonymous bit-field padding entries) is
	  		   NULL exactly when nothing was declared after it. */
	  		if (p->link != NULL)
	  			error("flexible array member `%s' must be the last member of `%t'\n",
	  				p->name, ty);
	  		else if (nfields < 2)
	  			error("flexible array member `%s' cannot be the only member of `%t'\n",
	  				p->name, ty);
	  	}
	  for (p = *q; p; p = p->link) {
	  	int a = p->type->align ? p->type->align : 1;
		if (p->lsb)
			a = unsignedtype->align;
		if (ty->op == UNION)
			off = bits = 0;
		else if (p->bitsize == 0 || bits == 0
		|| bits - 1 + p->bitsize > 8*unsignedtype->size) {
			off = add(off, bits2bytes(bits-1));
			bits = 0;
			chkoverflow(off, a - 1);
			off = roundup(off, a);
		}
		if (a > ty->align)
			ty->align = a;
		p->offset = off;

		if (p->lsb) {
			if (bits == 0)
				bits = 1;
			if (IR->little_endian)
				p->lsb = bits;
			else
				p->lsb = 8*unsignedtype->size - bits + 1
					- p->bitsize + 1;
			bits += p->bitsize;
		} else
			off = add(off, p->type->size);
		if (off + bits2bytes(bits-1) > ty->size)
			ty->size = off + bits2bytes(bits-1);
	  	if (p->name == NULL
	  	|| !('1' <= *p->name && *p->name <= '9')) {
	  		*q = p;
	  		q = &p->link;
	  	}
	  }
	  *q = NULL;
	  chkoverflow(ty->size, ty->align - 1);
	  ty->size = roundup(ty->size, ty->align);
	  if (overflow) {
	  	error("size of `%t' exceeds %d bytes\n", ty, inttype->u.sym->u.limits.max.i);
	  	ty->size = inttype->u.sym->u.limits.max.i&(~(ty->align - 1));
	  } }
}
/* Parses an inline function's body -- required to be exactly one
   `return expr;` -- and, on success, captures it into f->u.f.is_inline/
   inline_params/inline_body for enode.c's inline_expand() to clone at
   every call site (see c.h's comment on those fields for the full
   design, and inline_clone_subst()'s for why a separate, persistent set
   of placeholder parameter symbols is needed rather than reusing
   `callee` directly).

   Consumes the opening `{` itself and, like compound() does for a
   function's own top-level (level == LOCAL) block, leaves the closing
   `}` for the caller (funcdefn()) to consume the same way it already
   does for a normal function body -- one fewer thing for the inline
   path to diverge on. */
static void parse_inline_body(Symbol f, Symbol callee[]) {
	Tree p;
	Type rty = freturn(f->type);
	Symbol *placeholders;
	int i, n, save;

	expect('{');
	if (t != RETURN) {
		error("inline function `%s' must have a body consisting of exactly one return statement\n", f->name);
		while (t != '}' && t != EOI)
			t = gettok();
		return;
	}
	t = gettok();
	if (isstruct(unqual(rty)))
		error("inline function `%s' cannot return a struct or union\n", f->name);
	save = where;
	where = PERM; /* this tree must outlive f's own compilation -- see c.h */
	p = pointer(expr(0));
	{
		Type cty = assign(rty, p);
		if (cty == NULL)
			error("illegal return type in inline function `%s'; found `%t' expected `%t'\n",
				f->name, p->type, rty);
		else
			p = cast(p, cty);
	}
	if (t != ';')
		error("inline function `%s' body must be a single return statement\n", f->name);
	else
		t = gettok();
	if (t != '}') {
		error("inline function `%s' body must be exactly one return statement\n", f->name);
		while (t != '}' && t != EOI)
			t = gettok();
	}

	for (n = 0; callee[n]; n++)
		;
	placeholders = (Symbol *)newarray(n + 1, sizeof *placeholders, PERM);
	for (i = 0; i < n; i++) {
		Symbol ph;
		NEW0(ph, PERM);
		ph->name = callee[i]->name;
		ph->type = callee[i]->type;
		ph->scope = PARAM;
		ph->sclass = AUTO;
		placeholders[i] = ph;
	}
	placeholders[n] = NULL;
	p = inline_clone_subst(p, callee, placeholders, n);
	where = save;

	f->u.f.is_inline = 1;
	f->u.f.inline_params = placeholders;
	f->u.f.inline_body = p;
}
static void funcdefn(int sclass, char *id, Type ty, Symbol params[], Coordinate pt, int is_inline, int is_banked) {
	int i, n;
	Symbol *callee, *caller, p;
	Type rty = freturn(ty);

	if (isstruct(rty) && rty->size == 0)
		error("illegal use of incomplete type `%t'\n", rty);
	for (n = 0; params[n]; n++)
		;
	if (n > 0 && params[n-1]->name == NULL)
		params[--n] = NULL;
	if (Aflag >= 2 && n > 31)
		warning("more than 31 parameters in function `%s'\n", id);
	if (ty->u.f.oldstyle) {
		if (Aflag >= 1)
			warning("old-style function definition for `%s'\n", id);
		caller = params;
		callee = newarray(n + 1, sizeof *callee, FUNC);
		memcpy(callee, caller, (n+1)*sizeof *callee);
		enterscope();
		assert(level == PARAM);
		while (kind[t] == STATIC || istypename(t, tsym))
			decl(dclparam);
		foreach(identifiers, PARAM, oldparam, callee);

		for (i = 0; (p = callee[i]) != NULL; i++) {
			if (!p->defined)
				callee[i] = dclparam(0, p->name, inttype, &p->src);
			*caller[i] = *p;
			caller[i]->sclass = AUTO;
			caller[i]->type = promote(p->type);
		}
		p = lookup(id, identifiers);
		if (p && p->scope == GLOBAL && isfunc(p->type)
		&& p->type->u.f.proto) {
			Type *proto = p->type->u.f.proto;
			for (i = 0; caller[i] && proto[i]; i++) {
				Type ty = unqual(proto[i]);
				if (eqtype(isenum(ty) ? ty->type : ty,
					unqual(caller[i]->type), 1) == 0)
					break;
				else if (isenum(ty) && !isenum(unqual(caller[i]->type)))
					warning("compatibility of `%t' and `%t' is compiler dependent\n",
						proto[i], caller[i]->type);
			}
			if (proto[i] || caller[i])
				error("conflicting argument declarations for function `%s'\n", id);

		}
		else {
			Type *proto = newarray(n + 1, sizeof *proto, PERM);
			if (Aflag >= 1)
				warning("missing prototype for `%s'\n", id);
			for (i = 0; i < n; i++)
				proto[i] = caller[i]->type;
			proto[i] = NULL;
			ty = func(rty, proto, 1);
		}
	} else {
		callee = params;
		caller = newarray(n + 1, sizeof *caller, FUNC);
		for (i = 0; (p = callee[i]) != NULL && p->name; i++) {
			NEW(caller[i], FUNC);
			*caller[i] = *p;
			if (isint(p->type))
				caller[i]->type = promote(p->type);
			caller[i]->sclass = AUTO;
			if ('1' <= *p->name && *p->name <= '9')
				error("missing name for parameter %d to function `%s'\n", i + 1, id);

		}
		caller[i] = NULL;
	}
	for (i = 0; (p = callee[i]) != NULL; i++)
		if (p->type->size == 0) {
			error("undefined size for parameter `%t %s'\n",
				p->type, p->name);
			caller[i]->type = p->type = inttype;
		}
	if (Aflag >= 2 && sclass != STATIC && strcmp(id, "main") == 0) {
		if (ty->u.f.oldstyle)
			warning("`%t %s()' is a non-ANSI definition\n", rty, id);
		else if (!(rty == inttype
			&& (n == 0 && callee[0] == NULL
			||  n == 2 && callee[0]->type == inttype
			&& isptr(callee[1]->type) && callee[1]->type->type == charptype
			&& !variadic(ty))))
			warning("`%s' is a non-ANSI definition\n", typestring(ty, id));
	}
	p = lookup(id, identifiers);
	if (p && isfunc(p->type) && p->defined)
		error("redefinition of `%s' previously defined at %w\n",
			p->name, &p->src);
	cfunc = dclglobal(sclass, id, ty, &pt);
	cfunc->bank = is_banked;
	cfunc->u.f.label = genlabel(1);
	cfunc->u.f.callee = callee;
	cfunc->u.f.pt = src;
	cfunc->defined = 1;
	if (xref)
		use(cfunc, cfunc->src);
	if (Pflag)
		printproto(cfunc, cfunc->u.f.callee);
	if (ncalled >= 0)
		ncalled = findfunc(cfunc->name, pt.file);
	labels   = table(NULL, LABELS);
	stmtlabs = table(NULL, LABELS);
	refinc = 1.0;
	regcount = 0;
	codelist = &codehead;
	codelist->next = NULL;
	if (!IR->wants_callb && isstruct(rty))
		retv = genident(AUTO, ptr(unqual(rty)), PARAM);
	if (is_inline) {
		/* No compound()/codegen at all: an inline function is never
		   emitted as real callable code (see c.h's is_inline comment),
		   only cloned into its callers by enode.c's inline_expand().
		   Mirrors just the two things compound() would otherwise have
		   done that still matter here: consuming through the closing
		   '}' (parse_inline_body() does the rest of the body itself)
		   and exitscope() to leave PARAM-level scope, matching the
		   normal path's own exitscope()+expect('}') below exactly. */
		parse_inline_body(cfunc, callee);
		exitscope();
		expect('}');
		labels = stmtlabs = NULL;
		retv = NULL;
		cfunc = NULL;
		return;
	}
	compound(0, NULL, 0);

	definelab(cfunc->u.f.label);
	if (events.exit)
		apply(events.exit, cfunc, NULL);
	walk(NULL, 0, 0);
	exitscope();
	assert(level == PARAM);
	foreach(identifiers, level, checkref, NULL);
	if (!IR->wants_callb && isstruct(rty)) {
		Symbol *a;
		a = newarray(n + 2, sizeof *a, FUNC);
		a[0] = retv;
		memcpy(&a[1], callee, (n+1)*sizeof *callee);
		callee = a;
		a = newarray(n + 2, sizeof *a, FUNC);
		NEW(a[0], FUNC);
		*a[0] = *retv;
		memcpy(&a[1], caller, (n+1)*sizeof *callee);
		caller = a;
	}
	if (!IR->wants_argb)
		for (i = 0; caller[i]; i++)
			if (isstruct(caller[i]->type)) {
				caller[i]->type = ptr(caller[i]->type);
				callee[i]->type = ptr(callee[i]->type);
				caller[i]->structarg = callee[i]->structarg = 1;
			}
	if (glevel > 1)	for (i = 0; callee[i]; i++) callee[i]->sclass = AUTO;
	if (cfunc->sclass != STATIC)
		(*IR->export)(cfunc);
	if (glevel && IR->stabsym) {
		swtoseg(CODE); (*IR->stabsym)(cfunc); }
	swtoseg(CODE);
	if (cfunc->bank == 1)
		bank_enter(1);
	(*IR->function)(cfunc, caller, callee, cfunc->u.f.ncalls);
	if (cfunc->bank == 1)
		bank_exit(1);
	if (glevel && IR->stabfend)
		(*IR->stabfend)(cfunc, lineno);
	foreach(stmtlabs, LABELS, checklab, NULL);
	exitscope();
	expect('}');
	labels = stmtlabs = NULL;
	retv  = NULL;
	cfunc = NULL;
}
static void oldparam(Symbol p, void *cl) {
	int i;
	Symbol *callee = cl;

	for (i = 0; callee[i]; i++)
		if (p->name == callee[i]->name) {
			callee[i] = p;
			return;
		}
	error("declared parameter `%s' is missing\n", p->name);
}
void compound(int loop, struct swtch *swp, int lev) {
	Code cp;
	int nregs;
	List saved_autos, saved_registers, saved_vla_pending;

	walk(NULL, 0, 0);
	cp = code(Blockbeg);
	enterscope();
	assert(level >= LOCAL);
	if (level == LOCAL && events.entry)
		apply(events.entry, cfunc, NULL);
	definept(NULL);
	expect('{');
	/* autos/registers are a single pair of file-scope accumulator lists,
	   not stacked per nesting level, because the original two-loop shape
	   (all decls, then a single ltov() capturing them into
	   cp->u.block.locals, then all statements) guaranteed every nested
	   compound() -- reachable only from the statement half, via
	   statement()'s '{'/if/while/for/... cases -- could only start after
	   the enclosing scope's own accumulation was already flushed. Mixed
	   declarations (see the merged loop below) break that: a nested
	   block, e.g. a for-loop body, can now run *before* the enclosing
	   scope has seen all its own declarations, and it unconditionally
	   resets autos/registers to NULL for its own use -- wiping out
	   whatever the enclosing scope had accumulated so far, silently
	   dropping those locals from its cp->u.block.locals (they'd never
	   get a codegen-side slot at all: found by a crash decoding a NULL
	   x.name for a variable's own zero-initializer, for a variable
	   declared and used entirely before any mixed-declaration feature
	   came into play -- the reset alone was already reachable from a
	   plain trailing block, mixed declarations just made it common).
	   Save/restore across the recursion, same idea as any other
	   recursive-descent state a nested call must not clobber for its
	   caller. */
	saved_autos = autos;
	saved_registers = registers;
	saved_vla_pending = vla_pending;
	autos = registers = NULL;
	vla_pending = NULL;
	if (level == LOCAL && IR->wants_callb
	&& isstruct(freturn(cfunc->type))) {
		retv = genident(AUTO, ptr(unqual(freturn(cfunc->type))), level);
		retv->defined = 1;
		retv->ref = 1;
		registers = append(retv, registers);
	}
	/* Mixed declarations: a declaration is allowed anywhere a statement
	   is (C99 6.8.2p2), not just as a block-leading run before the first
	   statement. The two original loops (all decls, then all statements)
	   are merged into one that re-checks "is this a declaration?" on
	   every iteration instead of only before the first statement; for a
	   traditionally-shaped function (every decl before every statement)
	   this behaves identically to the original two-loop form, since the
	   decl-branch keeps firing until declarations run out and then the
	   statement-branch takes over, exactly as before.
	   `autos`/`registers` are plain accumulator lists appended to by
	   dcllocal() regardless of when it's called (see its AUTO/REGISTER
	   cases) -- decl()'s parse-time position doesn't affect their
	   correctness, so the single ltov() pass that turns them into
	   cp->u.block.locals just needs to happen once, after every
	   declaration in the block (wherever it appeared) has been seen,
	   i.e. after the merged loop instead of between the two old ones.
	   Symbol scoping is unaffected either way: install() (called from
	   dcllocal()) already only makes a name visible for lookups from its
	   declaration point onward, so referencing a variable before its
	   (now possibly mid-block) declaration is correctly rejected same as
	   plain C99 block scoping, with no extra work needed here. */
	for (;;) {
		int isdecl = kind[t] == CHAR || kind[t] == STATIC
			|| istypename(t, tsym) && getchr() != ':';
		if (isdecl)
			decl(dcllocal);
		else if (kind[t] == IF || kind[t] == ID)
			statement(loop, swp, lev);
		else
			break;
	}
	if (vla_pending != NULL) {
		/* Auto-free, straight-line-exit only -- see dcllocal()'s own
		   comment on vla_pending for why an early return/break/goto
		   out of this block isn't covered. Order among multiple VLAs
		   in the same block doesn't matter (each is an independent
		   malloc()/free() pair); whatever order ltov() happens to
		   produce is fine. */
		int i;
		Symbol *v = ltov(&vla_pending, STMT);
		Symbol freesym = lookup(string("free"), identifiers);
		for (i = 0; v[i]; i++)
			walk(root(vcall(freesym, voidtype, idtree(v[i]), NULL)), 0, 0);
	}
	vla_pending = saved_vla_pending;
	{
		int i;
		Symbol *a = ltov(&autos, STMT);
		nregs = length(registers);
		for (i = 0; a[i]; i++)
			registers = append(a[i], registers);
		cp->u.block.locals = ltov(&registers, FUNC);
	}
	/* This scope's own locals are now safely captured in
	   cp->u.block.locals; hand the accumulator back to whichever
	   enclosing compound() (if any) was mid-accumulation when it called
	   into this one -- see the save above. */
	autos = saved_autos;
	registers = saved_registers;
	if (events.blockentry)
		apply(events.blockentry, cp->u.block.locals, NULL);
	walk(NULL, 0, 0);
	foreach(identifiers, level, checkref, NULL);
	{
		int i = nregs, j;
		Symbol p;
		for ( ; (p = cp->u.block.locals[i]) != NULL; i++) {
			for (j = i; j > nregs
				&& cp->u.block.locals[j-1]->ref < p->ref; j--)
				cp->u.block.locals[j] = cp->u.block.locals[j-1];
			cp->u.block.locals[j] = p;
		}
	}
	if (level == LOCAL) {
		Code cp;
		for (cp = codelist; cp->kind < Label; cp = cp->prev)
			;
		if (cp->kind != Jump) {
			if (freturn(cfunc->type) != voidtype) {
				warning("missing return value\n");
				retcode(cnsttree(inttype, 0L));
			} else
				retcode(NULL);
		}
	}
	if (events.blockexit)
		apply(events.blockexit, cp->u.block.locals, NULL);
	cp->u.block.level = level;
	cp->u.block.identifiers = identifiers;
	cp->u.block.types = types;
	code(Blockend)->u.begin = cp;
	if (reachable(Gen))
		definept(NULL);
	if (level > LOCAL) {
		exitscope();
		expect('}');
	}
}
/* C99 6.8.5.3: a for-loop's init-clause may be a declaration instead of
   an expression (for(int i=0;...)), scoped to just the loop -- the
   condition, increment, and body -- not the enclosing block. forstmt()
   (stmt.c) implements that as an implicit nested scope wrapping the
   whole for-statement, built the same way compound() builds an
   explicit `{ ... }` scope: same Blockbeg/Blockend markers, same
   autos/registers save-reset-restore dance (see compound()'s own
   comment on why that save/restore is required), same final
   locals-capture and ref-count sort. It's split into three pieces
   instead of being folded into compound() itself because there's no
   '{'/'}' pair here to `expect()`, no possibility of this being the
   function's own outermost (level == LOCAL) block, and only ever
   exactly one declaration to parse rather than compound()'s
   decl-or-statement loop -- three real shape differences, not just a
   style choice, so duplicating the ~15 lines that do differ was less
   risky than adding flags to compound() to suppress them.
   autos/registers/checkref()/decl()/dcllocal() all stay private to
   this file (as they already were before this existed) -- forstmt()
   only sees this trio via their extern declarations in c.h. */
Code beginforscope(List *save_autos, List *save_registers) {
	Code cp;

	walk(NULL, 0, 0);
	cp = code(Blockbeg);
	enterscope();
	*save_autos = autos;
	*save_registers = registers;
	autos = registers = NULL;
	return cp;
}
void forinitdecl(void) {
	decl(dcllocal);
}
void endforscope(Code cp, List save_autos, List save_registers) {
	int nregs;

	{
		int i;
		Symbol *a = ltov(&autos, STMT);
		nregs = length(registers);
		for (i = 0; a[i]; i++)
			registers = append(a[i], registers);
		cp->u.block.locals = ltov(&registers, FUNC);
	}
	autos = save_autos;
	registers = save_registers;
	if (events.blockentry)
		apply(events.blockentry, cp->u.block.locals, NULL);
	walk(NULL, 0, 0);
	foreach(identifiers, level, checkref, NULL);
	{
		int i = nregs, j;
		Symbol p;
		for ( ; (p = cp->u.block.locals[i]) != NULL; i++) {
			for (j = i; j > nregs
				&& cp->u.block.locals[j-1]->ref < p->ref; j--)
				cp->u.block.locals[j] = cp->u.block.locals[j-1];
			cp->u.block.locals[j] = p;
		}
	}
	if (events.blockexit)
		apply(events.blockexit, cp->u.block.locals, NULL);
	cp->u.block.level = level;
	cp->u.block.identifiers = identifiers;
	cp->u.block.types = types;
	code(Blockend)->u.begin = cp;
	if (reachable(Gen))
		definept(NULL);
	exitscope();
}
static void checkref(Symbol p, void *cl) {
	if (p->scope >= PARAM
	&& (isvolatile(p->type) || isfunc(p->type)))
		p->addressed = 1;
	if (Aflag >= 2 && p->defined && p->ref == 0) {
		if (p->sclass == STATIC)
			warning("static `%t %s' is not referenced\n",
				p->type, p->name);
		else if (p->scope == PARAM)
			warning("parameter `%t %s' is not referenced\n",
				p->type, p->name);
		else if (p->scope >= LOCAL && p->sclass != EXTERN)
			warning("local `%t %s' is not referenced\n",
				p->type, p->name);
	}
	if (p->sclass == AUTO
	&& (p->scope  == PARAM && regcount == 0
	 || p->scope  >= LOCAL)
	&& !p->addressed && isscalar(p->type) && p->ref >= 3.0)
		p->sclass = REGISTER;
	if (level == GLOBAL && p->sclass == STATIC && !p->defined
	&& isfunc(p->type) && p->ref)
		error("undefined static `%t %s'\n", p->type, p->name);
	assert(!(level == GLOBAL && p->sclass == STATIC && !p->defined && !isfunc(p->type)));
}
static Symbol dcllocal(int sclass, char *id, Type ty, Coordinate *pos) {
	Symbol p, q;
	Type vla_elemty = NULL; /* set below iff this declarator turned out to be a VLA */

	if (is_vla) {
		/* dclr1() already flagged this and left `ty` as ARRAY(elemty)
		   with size 0 -- see its own comment for why the actual
		   rewrite has to happen here instead, once there's a real
		   Symbol to attach it to. Only the *type* changes here (to a
		   plain pointer, so every existing bit of code downstream --
		   the sclass switch below, the register/stack allocation
		   backend, &c. -- just sees an ordinary pointer local and
		   needs no VLA-specific handling of its own); the actual
		   malloc() call is emitted further down, after sclass is
		   resolved, the same place a real `= expr` initializer would
		   be. */
		vla_elemty = ty->type;
		is_vla = 0;
		if (sclass == STATIC || sclass == EXTERN || sclass == REGISTER) {
			error("variable-length array `%s' cannot be `%k'\n", id, sclass);
			vla_elemty = NULL; /* fall through as a plain (broken, but not doubly-broken) size-0 array */
		} else {
			Symbol mallocsym = lookup(string("malloc"), identifiers);
			Symbol freesym = lookup(string("free"), identifiers);
			if (mallocsym == NULL || !isfunc(mallocsym->type)
			||  freesym   == NULL || !isfunc(freesym->type)) {
				error("variable-length array `%s' needs malloc()/free() declared (#include \"heap.h\")\n", id);
				vla_elemty = NULL;
			} else
				ty = ptr(vla_elemty);
		}
	}
	if (sclass == 0)
		sclass = isfunc(ty) ? EXTERN : AUTO;
	else if (isfunc(ty) && sclass != EXTERN) {
		error("invalid storage class `%k' for `%t %s'\n",
			sclass, ty, id);
		sclass = EXTERN;
	} else if (sclass == REGISTER
	&& (isvolatile(ty) || isstruct(ty) || isarray(ty))) {
		warning("register declaration ignored for `%t %s'\n",
			ty, id);
		sclass = AUTO;
	}
	q = lookup(id, identifiers);
	if (q && q->scope >= level
	||  q && q->scope == PARAM && level == LOCAL)
		if (sclass == EXTERN && q->sclass == EXTERN
		&& eqtype(q->type, ty, 1))
			ty = compose(ty, q->type);
		else
			error("redeclaration of `%s' previously declared at %w\n", q->name, &q->src);

	assert(level >= LOCAL);
	p = install(id, &identifiers, level, sclass == STATIC || sclass == EXTERN ? PERM : FUNC);
	p->type = ty;
	p->sclass = sclass;
	p->src = *pos;
	switch (sclass) {
	case EXTERN:   q = lookup(id, globals);
		       if (q == NULL || q->sclass == TYPEDEF || q->sclass == ENUM) {
		       	q = lookup(id, externals);
		       	if (q == NULL) {
		       		q = install(p->name, &externals, GLOBAL, PERM);
		       		q->type = p->type;
		       		q->sclass = EXTERN;
		       		q->src = src;
		       		(*IR->defsymbol)(q);
		       	}
		       }
		       if (!eqtype(p->type, q->type, 1))
		       	warning("declaration of `%s' does not match previous declaration at %w\n", q->name, &q->src);

		       p->u.alias = q; break;
	case STATIC:   (*IR->defsymbol)(p);
		       initglobal(p, 0);
		       if (!p->defined)
		       	if (p->type->size > 0) {
		       		if (p->bank == 1)
		       			bank_enter(1);
		       		defglobal(p, BSS);
		       		(*IR->space)(p->type->size);
		       		if (p->bank == 1)
		       			bank_exit(1);
		       	} else
		       		error("undefined size for `%t %s'\n",
		       			p->type, p->name);
		       p->defined = 1; break;
	case REGISTER: registers = append(p, registers);
		       regcount++;
		       p->defined = 1;
 break;
	case AUTO:     autos = append(p, autos);
		       p->defined = 1;
		       if (isarray(ty))
		       	p->addressed = 1; break;
	default: assert(0);
	}
	if (vla_elemty != NULL) {
		/* `p = (elemty*) malloc(vla_size_tree * sizeof(elemty));` --
		   built rather than parsed, since there's no `= malloc(...)`
		   in the source for a VLA declarator to begin with. Freed
		   automatically at the end of this variable's own block, but
		   -- see compound()'s own comment on vla_pending -- only
		   along the straight-line path out of that block: an early
		   return/break/goto out of it skips the free(), which is a
		   real, deliberate limitation (matching this codebase's other
		   intentionally-scoped features, e.g. inline functions only
		   covering a single-return-statement body) rather than an
		   oversight -- doing this correctly for every possible exit
		   path would need unwind-style bookkeeping this compiler has
		   nowhere else. */
		Tree bytes = (*optree['*'])(MUL,
			cast(vla_size_tree, unsignedtype),
			cnsttree(unsignedtype, (long)vla_elemty->size));
		Symbol mallocsym = lookup(string("malloc"), identifiers);
		Tree mcall = cast(vcall(mallocsym, NULL, bytes, NULL), p->type);
		walk(root(asgn(p, mcall)), 0, 0);
		p->ref = 1;
		vla_pending = append(p, vla_pending);
	} else if (t == '=') {
		Tree e;
		if (sclass == EXTERN)
			error("illegal initialization of `extern %s'\n", id);
		t = gettok();
		definept(NULL);
		if (isscalar(p->type)
		||  isstruct(p->type) && t != '{') {
			if (t == '{') {
				t = gettok();
				e = expr1(0);
				expect('}');
			} else
				e = expr1(0);
		} else {
			Symbol t1;
			Type ty = p->type, ty1 = ty;
			while (isarray(ty1))
				ty1 = ty1->type;
			if (!isconst(ty) && (!isarray(ty) || !isconst(ty1)))
				ty = qual(CONST, ty);
			t1 = genident(STATIC, ty, GLOBAL);
			initglobal(t1, 1);
			if (isarray(p->type) && p->type->size == 0
			&& t1->type->size > 0)
				p->type = array(p->type->type,
					t1->type->size/t1->type->type->size, 0);
			e = idtree(t1);
		}
		walk(root(asgn(p, e)), 0, 0);
		p->ref = 1;
	}
	if (!isfunc(p->type) && p->defined && p->type->size <= 0)
		error("undefined size for `%t %s'\n", p->type, id);
	return p;
}
void finalize(void) {
	foreach(externals,   GLOBAL,    doextern, NULL);
	foreach(identifiers, GLOBAL,    doglobal, NULL);
	foreach(identifiers, GLOBAL,    checkref, NULL);
	foreach(constants,   CONSTANTS, doconst,  NULL);
}
static void doextern(Symbol p, void *cl) {
	(*IR->import)(p);
}
static void doglobal(Symbol p, void *cl) {
	if (!p->defined && (p->sclass == EXTERN
	|| isfunc(p->type) && p->sclass == AUTO))
		(*IR->import)(p);
	else if (!p->defined && !isfunc(p->type)
	&& (p->sclass == AUTO || p->sclass == STATIC)) {
		if (isarray(p->type)
		&& p->type->size == 0 && p->type->type->size > 0)
			p->type = array(p->type->type, 1, 0);
		if (p->type->size > 0) {
			if (p->bank == 1)
				bank_enter(1);
			defglobal(p, BSS);
			(*IR->space)(p->type->size);
			if (p->bank == 1)
				bank_exit(1);
			if (glevel > 0 && IR->stabsym)
				(*IR->stabsym)(p);
		} else
			error("undefined size for `%t %s'\n",
				p->type, p->name);
		p->defined = 1;
	}
	if (Pflag
	&& !isfunc(p->type)
	&& !p->generated && p->sclass != EXTERN)
		printdecl(p, p->type);
}
void doconst(Symbol p, void *cl) {
	if (p->u.c.loc) {
		assert(p->u.c.loc->u.seg == 0); 
		defglobal(p->u.c.loc, LIT);
		if (isarray(p->type) && p->type->type == widechar) {
			unsigned int *s = p->u.c.v.p;
			int n = p->type->size/widechar->size;
			while (n-- > 0) {
				Value v;
				v.u = *s++;
				(*IR->defconst)(widechar->op, widechar->size, v);
			}
		} else if (isarray(p->type))
			(*IR->defstring)(p->type->size, p->u.c.v.p);
		else
			(*IR->defconst)(p->type->op, p->type->size, p->u.c.v);
		p->u.c.loc = NULL;
	}
}
void checklab(Symbol p, void *cl) {
	if (!p->defined)
		error("undefined label `%s'\n", p->name);
	p->defined = 1;
}

Type enumdcl(void) {
	char *tag;
	Type ty;
	Symbol p;
	Coordinate pos;

	t = gettok();
	pos = src;
	if (t == ID) {
		tag = token;
		t = gettok();
	} else
		tag = "";
	if (t == '{') {
		static char follow[] = { IF, 0 };
		int n = 0;
		long k = -1;
		List idlist = 0;
		ty = newstruct(ENUM, tag);
		t = gettok();
		if (t != ID)
			error("expecting an enumerator identifier\n");
		while (t == ID) {
			char *id = token;
			Coordinate s;
			if (tsym && tsym->scope == level)
				error("redeclaration of `%s' previously declared at %w\n",
					token, &tsym->src);
			s = src;
			t = gettok();
			if (t == '=') {
				t = gettok();
				k = intexpr(0, 0);
			} else {
				if (k == inttype->u.sym->u.limits.max.i)
					error("overflow in value for enumeration constant `%s'\n", id);
				k++;
			}
			p = install(id, &identifiers, level,  level < LOCAL ? PERM : FUNC);
			p->src = s;
			p->type = ty;
			p->sclass = ENUM;
			p->u.value = k;
			idlist = append(p, idlist);
			n++;
			if (Aflag >= 2 && n == 128)
				warning("more than 127 enumeration constants in `%t'\n", ty);
			if (t != ',')
				break;
			t = gettok();
			if (Aflag >= 2 && t == '}')
				warning("non-ANSI trailing comma in enumerator list\n");
		}
		test('}', follow);
		ty->type = inttype;
		ty->size = ty->type->size;
		ty->align = ty->type->align;
		ty->u.sym->u.idlist = ltov(&idlist, PERM);
		ty->u.sym->defined = 1;
	} else if ((p = lookup(tag, types)) != NULL && p->type->op == ENUM) {
		ty = p->type;
		if (t == ';')
			error("empty declaration\n");
	} else {
		error("unknown enumeration `%s'\n",  tag);
		ty = newstruct(ENUM, tag);
		ty->type = inttype;
	}
	if (*tag && xref)
		use(p, pos);
	return ty;
}

Type typename(void) {
	Type ty = specifier(NULL);

	if (t == '*' || t == '(' || t == '[') {
		ty = dclr(ty, NULL, NULL, 1);
		if (Aflag >= 1 && !hasproto(ty))
			warning("missing prototype\n");
	}
	return ty;
}

