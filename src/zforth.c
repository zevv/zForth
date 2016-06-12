
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "zforth.h"


/* Flags and length encoded in words */

#define ZF_FLAG_IMMEDIATE (1<<6)
#define ZF_FLAG_PRIM      (1<<5)
#define ZF_FLAG_LEN(v)    (v & 0x1f)


/* Define all primitives, make sure the two tables below always match.  The
 * names are defined as a \0 separated list, terminated by double \0. This
 * saves space on the pointers compared to an array of strings */

#define _(s) s "\0"

typedef enum {
	PRIM_EXIT,    PRIM_LIT,       PRIM_LTZ,  PRIM_COL,     PRIM_SEMICOL,  PRIM_ADD,
	PRIM_SUB,     PRIM_MUL,       PRIM_DIV,  PRIM_MOD,     PRIM_DROP,     PRIM_DUP,
	PRIM_EXECUTE, PRIM_IMMEDIATE, PRIM_PEEK, PRIM_POKE,    PRIM_SWAP,     PRIM_ROT,
	PRIM_JMP,     PRIM_JMP0,      PRIM_TICK, PRIM_COMMENT, PRIM_PUSHR,    PRIM_POPR,
	PRIM_EQUAL,   PRIM_SYS,       PRIM_PICK, PRIM_COMMA,   PRIM_KEY,      PRIM_LITS,

	PRIM_COUNT
} zf_prim;

const char prim_names[] =
	_("exit")    _("lit")        _("<0")    _(":")     _("_;")        _("+")
	_("-")       _("*")          _("/")     _("%")     _("drop")      _("dup")
	_("execute") _("_immediate") _("@")     _("!")     _("swap")      _("rot")
	_("jmp")     _("jmp0")       _("'")     _("_(")    _(">r")        _("r>")
	_("=")       _("sys")        _("pick")  _(",")     _("key")       _("lits");


/* Stacks and dictionary memory */

static zf_cell rstack[ZF_RSTACK_SIZE];
static zf_cell dstack[ZF_DSTACK_SIZE];
static uint8_t dict[ZF_DICT_SIZE];

/* State and stack and interpreter pointers */

static zf_result state;
static zf_addr dsp;
static zf_addr rsp;
static zf_addr ip;


/* User variables are variables which are shared between forth and C. From
 * forth these can be accessed with @ and ! at pseudo-indices in low memory, in
 * C they are stored in an array of zf_addr with friendly reference names
 * through some macros */

#define HERE      uservar[0]    /* compilation pointer in dictionary */
#define LATEST    uservar[1]    /* pointer to last compiled word */
#define TRACE     uservar[2]    /* trace enable flag */
#define COMPILING uservar[3]    /* compiling flag */
#define POSTPONE  uservar[4]    /* flag to indicate next imm word should be compiled */


#define USERVAR_COUNT 5

const char uservar_names[] =
	_("here")   _("latest") _("trace")  _("compiling")  _("_postpone");

static zf_addr *uservar = (zf_addr *)dict;

/* Prototypes */

static zf_result do_prim(zf_prim prim, const char *input);
static zf_addr dict_get_cell(zf_addr addr, zf_cell *v);


/* Tracing functions. If disabled, the trace() function is replaced by an empty
 * macro, allowing the compiler to optimize away the function calls to
 * op_name() */

#if ZF_ENABLE_TRACE

static void trace(const char *fmt, ...)
{
	if(TRACE) {
		va_list va;
		va_start(va, fmt);
		zf_host_trace(fmt, va);
		va_end(va);
	}
}


static const char *op_name(zf_addr addr)
{
	zf_addr w = LATEST;
	static char name[32];

	while(TRACE && w) {
		zf_cell link;
		zf_addr p = w;

		zf_cell d;
		int lenflags;
		p += dict_get_cell(p, &d);
		lenflags = d;

		p += dict_get_cell(p, &link);

		zf_addr xt = p + ZF_FLAG_LEN(lenflags);

		zf_cell op2 = 0;
		dict_get_cell(xt, &op2);

		if(((lenflags & ZF_FLAG_PRIM) && addr == (zf_addr)op2) || addr == w || addr == xt) {
			int l = ZF_FLAG_LEN(lenflags);
			memcpy(name, &dict[p], l);
			name[l] = '\0';
			return name;
		}

		w = link;

	}
	return "?";
}

#else
#define trace(...) {}
#endif


/*
 * Check if given flag is set on a word definition 
 */

static int word_has_flag(zf_addr w, int flag)
{
	zf_cell d;
	w += dict_get_cell(w, &d);
	return !!((int)d & flag);
}


/*
 * Stack operations. Boundary checking can be disabled for smaller binary
 * and faster execution.
 */

#if ZF_ENABLE_BOUNDARY_CHECKS
#define CHECK(exp) exp
#else
#define CHECK(exp) 1
#endif

void zf_push(zf_cell v)
{
	if(CHECK(dsp < ZF_DSTACK_SIZE)) {
		trace("»" ZF_CELL_FMT " ", v);
		dstack[dsp++] = v;
	} else {
		state = ZF_ABORT_DSTACK_OVERRUN;
	}
}


zf_cell zf_pop(void)
{
	zf_cell v = 0;
	if(CHECK(dsp > 0)) {
		v = dstack[--dsp];
		trace("«" ZF_CELL_FMT " ", v);
	} else {
		state = ZF_ABORT_DSTACK_UNDERRUN;
	}
	return v;
}


zf_cell zf_pick(zf_addr n)
{
	zf_cell v = 0;
	if(CHECK(n < dsp)) {
		v = dstack[dsp-n-1];
	} else {
		state = ZF_ABORT_OUTSIDE_MEM;
	}
	return v;
}


static void zf_pushr(zf_cell v)
{
	if(CHECK(rsp < ZF_RSTACK_SIZE)) {
		trace("r»" ZF_CELL_FMT " ", v);
		rstack[rsp++] = v;
	} else {
		state = ZF_ABORT_RSTACK_OVERRUN;
	}
}


static zf_cell zf_popr(void)
{
	zf_cell v = 0;
	if(CHECK(rsp > 0)) {
		v = rstack[--rsp];
		trace("r«" ZF_CELL_FMT " ", v);
	} else {
		state = ZF_ABORT_RSTACK_OVERRUN;
	}
	return v;
}


/*
 * zf_cells are encoded in the dictionary with a variable length:
 *
 * encode:
 *
 *    integer   0 ..   127  0xxxxxxx
 *    integer 128 .. 16383  10xxxxxx xxxxxxxx
 *    else                  xxxxxxxx [IEEE float val]
 */

static zf_addr dict_put_cell2(zf_addr addr, unsigned int vi)
{
	dict[addr++] = (vi >> 8) | 0x80;
	dict[addr++] = vi;
	trace(" ²");
	return 2;
}

static zf_addr dict_put_cell(zf_addr addr, zf_cell v)
{
	unsigned int vi = v;
	int l = 0;

	trace("\n+" ZF_ADDR_FMT " " ZF_ADDR_FMT, addr, (zf_addr)v);

	if((v - vi) == 0) {
		if(vi < 128) {
			dict[addr++] = vi;
			trace(" ¹");
			return 1;
		}
		if(vi < 16384) return dict_put_cell2(addr, vi);
	}

	dict[addr] = 0xff;
	memcpy(&dict[addr+1], &v, sizeof(v));
	return sizeof(v) + 1;
	trace(" ⁵");

	return l;
}


static zf_addr dict_get_cell(zf_addr addr, zf_cell *v)
{
	uint8_t a = dict[addr];

	if(a & 0x80) {
		if(a == 0xff) {
			memcpy(v, dict+addr+1, sizeof(*v));
			return 1 + sizeof(*v);
		} else {
			*v = ((a & 0x3f) << 8) + dict[addr+1];
			return 2;
		}
	} else {
		*v = dict[addr];
		return 1;
	}
}


static void dict_add_cell(zf_cell v)
{
	HERE += dict_put_cell(HERE, v);
	trace(" ");
}



/*
 * Generic dictionary adding
 */

static void dict_add_op(zf_addr op)
{
	dict_add_cell(op);
	trace("+%s", op_name(op));
}


static void dict_add_lit(zf_cell v)
{
	dict_add_op(PRIM_LIT);
	dict_add_cell(v);
}


static void dict_add_str(const char *s)
{
	trace("\n+" ZF_ADDR_FMT " " ZF_ADDR_FMT " s '%s'", HERE, 0, s);
	size_t l = strlen(s);
	memcpy(&dict[HERE], s, l);
	HERE += l;
}


/*
 * Create new word, adjusting HERE and LATEST accordingly
 */

static void create(const char *name, int flags)
{
	trace("\n=== create '%s'", name);
	zf_addr here_prev = HERE;
	dict_add_cell((strlen(name)) | flags);
	dict_add_cell(LATEST);
	dict_add_str(name);
	LATEST = here_prev;
	trace("\n===");
}


/*
 * Find word in dictionary, returning address and execution token
 */

static int find_word(const char *name, zf_addr *word, zf_addr *code)
{
	zf_addr w = LATEST;
	size_t namelen = strlen(name);

	while(w) {
		zf_cell link, d;
		zf_addr p = w;
		p += dict_get_cell(p, &d);
		size_t len = ZF_FLAG_LEN((int)d);
		p += dict_get_cell(p, &link);
		if(len == namelen) {
			const char *name2 = (void *)&dict[p];
			if(memcmp(name, name2, len) == 0) {
				*word = w;
				*code = p + len;
				return 1;
			}
		}
		w = link;
	}

	return 0;
}


static void make_immediate(void)
{
	zf_cell lenflags;
	dict_get_cell(LATEST, &lenflags);
	dict_put_cell(LATEST, (int)lenflags | ZF_FLAG_IMMEDIATE);
}


static zf_result run(const char *input)
{
	for(;;) {

		if(ip >= ZF_DICT_SIZE) {
			return ZF_ABORT_OUTSIDE_MEM;
		}

		zf_cell d;
		zf_addr l = dict_get_cell(ip, &d);
		zf_addr code = d;

		trace("\n "ZF_ADDR_FMT " " ZF_ADDR_FMT " ", ip, code);

		zf_addr i;
		for(i=0; i<rsp; i++) trace("┊  ");

		ip += l;

		if(code <= PRIM_COUNT) {
			zf_result rv = do_prim(code, input);
			if(rv == ZF_INPUT_WORD || rv == ZF_INPUT_CHAR) ip -= l;
			if(rv != ZF_OK) return rv;
			if(ip == 0) return ZF_OK;
		} else {
			trace("%s/" ZF_ADDR_FMT " ", op_name(code), code);
			zf_pushr(ip);
			ip = code;
		}

		input = NULL;
	}
}

/*
 * Run code on address 'addr', like forth's "inner interpreter"
 */

static zf_result execute(zf_addr addr)
{
	ip = addr;
	rsp = 0;
	zf_pushr(0);

	trace("\n[%s/" ZF_ADDR_FMT "] ", op_name(ip), ip);
	return run(NULL);

}


/*
 * Run primitive opcode
 */

static zf_result do_prim(zf_prim op, const char *input)
{
	zf_cell d1, d2, d3;
	zf_addr addr;
	zf_result rv = ZF_OK;

	trace("(%s) ", op_name(op));

	switch(op) {

		case PRIM_COL:
			if(input) {
				create(input, 0);
				COMPILING = 1;
			} else {
				rv = ZF_INPUT_WORD;
				break;
			}
			break;

		case PRIM_LTZ:
			zf_push(zf_pop() < 0);
			break;

		case PRIM_SEMICOL:
			dict_add_op(PRIM_EXIT);
			trace("\n===");
			COMPILING = 0;
			break;

		case PRIM_LIT:
			ip += dict_get_cell(ip, &d1);
			zf_push(d1);
			break;

		case PRIM_EXECUTE:
			rv = execute(zf_pop());
			break;

		case PRIM_EXIT:
			ip = zf_popr();
			break;

		case PRIM_PEEK:
			addr = zf_pop();
			if(addr < USERVAR_COUNT) {
				zf_push(uservar[addr]);
				break;
			}
			if(addr < ZF_DICT_SIZE) {
				dict_get_cell(addr, &d2);
				zf_push(d2);
			} else {
				rv = ZF_ABORT_OUTSIDE_MEM;
			}
			break;

		case PRIM_POKE:
			addr = zf_pop();
			d2 = zf_pop();
			if(addr < USERVAR_COUNT) {
				uservar[addr] = d2;
				break;
			}
			if(addr < ZF_DICT_SIZE) {
				dict_put_cell(addr, d2);
			} else {
				rv = ZF_ABORT_OUTSIDE_MEM;
			}
			break;

		case PRIM_SWAP:
			d1 = zf_pop(); d2 = zf_pop();
			zf_push(d1); zf_push(d2);
			break;

		case PRIM_ROT:
			d1 = zf_pop(); d2 = zf_pop(); d3 = zf_pop();
			zf_push(d2); zf_push(d1); zf_push(d3);
			break;

		case PRIM_DROP:
			zf_pop();
			break;

		case PRIM_DUP:
			d1 = zf_pop();
			zf_push(d1); zf_push(d1);
			break;

		case PRIM_ADD:
			d1 = zf_pop(); d2 = zf_pop();
			zf_push(d1 + d2);
			break;

		case PRIM_SYS:
			d1 = zf_pop();
			rv = zf_host_sys((zf_syscall_id)d1, input);
			if(rv == ZF_INPUT_WORD || rv == ZF_INPUT_CHAR) zf_push(d1); /* re-push id to resume */
			break;

		case PRIM_PICK:
			addr = zf_pop();
			zf_push(zf_pick(addr));
			break;

		case PRIM_SUB:
			d1 = zf_pop(); d2 = zf_pop();
			zf_push(d2 - d1);
			break;

		case PRIM_MUL:
			zf_push(zf_pop() * zf_pop());
			break;

		case PRIM_DIV:
			d2 = zf_pop();
			d1 = zf_pop();
			zf_push(d1/d2);
			break;

		case PRIM_MOD:
			d2 = zf_pop();
			d1 = zf_pop();
			zf_push((int)d1 % (int)d2);
			break;

		case PRIM_IMMEDIATE:
			make_immediate();
			break;

		case PRIM_JMP:
			ip += dict_get_cell(ip, &d1);
			trace("ip " ZF_ADDR_FMT "=>" ZF_ADDR_FMT, ip, (zf_addr)d1);
			ip = d1;
			break;

		case PRIM_JMP0:
			ip += dict_get_cell(ip, &d1);
			if(zf_pop() == 0) {
				trace("ip " ZF_ADDR_FMT "=>" ZF_ADDR_FMT, ip, (zf_addr)d1);
				ip = d1;
			}
			break;

		case PRIM_TICK:
			ip += dict_get_cell(ip, &d1);
			trace("%s/", op_name(d1));
			zf_push(d1);
			break;

		case PRIM_COMMA:
			d1 = zf_pop();
			dict_add_cell(d1);
			break;

		case PRIM_COMMENT:
			if(!input || input[0] != ')') {
				rv = ZF_INPUT_CHAR;
			}
			break;

		case PRIM_PUSHR:
			zf_pushr(zf_pop());
			break;

		case PRIM_POPR:
			zf_push(zf_popr());
			break;

		case PRIM_EQUAL:
			zf_push(zf_pop() == zf_pop());
			break;

		case PRIM_KEY:
			if(input) {
				zf_push(input[0]);
				rv = ZF_OK;
			} else {
				rv = ZF_INPUT_CHAR;
			}
			break;

		case PRIM_LITS:
			ip += dict_get_cell(ip, &d1);
			zf_push(ip);
			zf_push(d1);
			ip += d1;
			break;

		default:
			rv = ZF_ABORT_INTERNAL_ERROR;
			break;
	}

	if(state != ZF_OK) {
		rv = state;
	}

	return rv;
}



/*
 * Handle incoming word. Compile or interpreted the word, or pass it to a
 * deferred primitive if it requested a word from the input stream.
 */

static zf_result handle_word(const char *buf)
{
	zf_addr w, c = 0;
	zf_cell d;
	zf_result rv = ZF_OK;

	/* If a word was requested by an earlier operation, resume with the new
	 * word */

	if(state == ZF_INPUT_WORD) {
		state = ZF_OK;
		return run(buf);
	}

	/* Look up the word in the dictionary */

	int found = find_word(buf, &w, &c);

	state = ZF_OK;

	if(found) {

		/* Word found: compile or execute, depending on state */

		if(COMPILING && (POSTPONE || !word_has_flag(w, ZF_FLAG_IMMEDIATE))) {
			if(word_has_flag(w, ZF_FLAG_PRIM)) {
				dict_get_cell(c, &d);
				dict_add_op(d);
			} else {
				dict_add_op(c);
			}
			POSTPONE = 0;
		} else {
			rv = execute(c);
		}
	} else {

		/* Word not found: try to convert to a number and compile or push, depending
		 * on state */

		char *end;
		zf_cell v = strtod(buf, &end);
		if(end != buf) {
			if(COMPILING) {
				dict_add_lit(v);
			} else {
				zf_push(v);
			}
		} else {
			rv = ZF_ABORT_NOT_A_WORD;
		}
	}

	return rv;
}


/*
 * Handle one character. Split into words to pass to handle_word(), or pass the
 * char to a deferred prim if it requested a character from the input stream
 */

static zf_result handle_char(char c)
{
	static char buf[32];
	static size_t len = 0;
	zf_result rv = ZF_OK;

	if(state == ZF_INPUT_CHAR) {
		state = ZF_OK;
		return run(&c);
	}

	if(c == '\0' || isspace(c)) {
		if(len > 0) {
			len = 0;
			rv = handle_word(buf);
		}
	} else {
		if(len < sizeof(buf)-1) {
			buf[len++] = c;
			buf[len] = '\0';
		}
	}

	return rv;
}


/*
 * Initialisation
 */

void zf_init(int enable_trace)
{
	HERE = USERVAR_COUNT * sizeof(zf_addr);
	TRACE = enable_trace;
	LATEST = 0;
	dsp = 0;
	rsp = 0;
	COMPILING = 0;
}


#if ZF_ENABLE_BOOTSTRAP

/*
 * Functions for bootstrapping the dictionary by adding all primitive ops and the
 * user variables.
 */

static void add_prim(const char *name, zf_prim op)
{
	int imm = 0;

	if(name[0] == '_') {
		name ++;
		imm = 1;
	}

	create(name, ZF_FLAG_PRIM);
	dict_add_op(op);
	dict_add_op(PRIM_EXIT);
	if(imm) make_immediate();
}

static void add_uservar(const char *name, zf_addr addr)
{
	create(name, 0);
	dict_add_lit(addr);
	dict_add_op(PRIM_EXIT);
}


void zf_bootstrap(void)
{

	/* Add primitives and user variables to dictionary */

	zf_addr i = 0;
	const char *p;
	for(p=prim_names; *p; p+=strlen(p)+1) {
		add_prim(p, i++);
	} 

	i = 0;
	for(p=uservar_names; *p; p+=strlen(p)+1) {
		add_uservar(p, i++);
	}
}

#else 
void zf_bootstrap(void) {}
#endif


/*
 * Eval forth string
 */

zf_result zf_eval(const char *buf)
{
	zf_result rv = ZF_OK;

	for(;;) {
		rv = handle_char(*buf);

		switch(rv) {
			case ZF_OK:
				/* fine */
				break;
			case ZF_INPUT_CHAR:
			case ZF_INPUT_WORD:
				state = rv;
				break;
			default:
				/* all abort reasons */
				state = ZF_OK;
				COMPILING = 0;
				rsp = 0;
				dsp = 0;
				return rv;
		}

		if(*buf == '\0') {
			return rv;
		}
		buf ++;
	}
}


void *zf_dump(size_t *len)
{
	if(len) *len = sizeof(dict);
	return dict;
}

/*
 * End
 */

