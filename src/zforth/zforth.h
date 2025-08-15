#ifndef zforth_h
#define zforth_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>

#include "zfconf.h"

/* Abort reasons */

typedef enum {
	ZF_OK,
	ZF_ABORT_INTERNAL_ERROR,
	ZF_ABORT_OUTSIDE_MEM,
	ZF_ABORT_DSTACK_UNDERRUN,
	ZF_ABORT_DSTACK_OVERRUN,
	ZF_ABORT_RSTACK_UNDERRUN,
	ZF_ABORT_RSTACK_OVERRUN,
	ZF_ABORT_NOT_A_WORD,
	ZF_ABORT_COMPILE_ONLY_WORD,
	ZF_ABORT_INVALID_SIZE,
	ZF_ABORT_DIVISION_BY_ZERO,
	ZF_ABORT_INVALID_USERVAR,
	ZF_ABORT_EXTERNAL
} zf_result;

typedef enum {
	ZF_INPUT_INTERPRET,
	ZF_INPUT_PASS_CHAR,
	ZF_INPUT_PASS_WORD
} zf_input_state;

typedef enum {
	ZF_SYSCALL_EMIT,
	ZF_SYSCALL_PRINT,
	ZF_SYSCALL_TELL,
	ZF_SYSCALL_USER = 128
} zf_syscall_id;

typedef enum {
    ZF_USERVAR_HERE = 0,
    ZF_USERVAR_LATEST,
    ZF_USERVAR_TRACE,
    ZF_USERVAR_COMPILING,
    ZF_USERVAR_POSTPONE,
    ZF_USERVAR_DSP,
    ZF_USERVAR_RSP,

    ZF_USERVAR_COUNT
} zf_uservar_id;


typedef struct {
	/* Stacks and dictionary memory */
	zf_cell rstack[ZF_RSTACK_SIZE];
	zf_cell dstack[ZF_DSTACK_SIZE];
	uint8_t dict[ZF_DICT_SIZE];

	/* State and stack and interpreter pointers */
	zf_input_state input_state;
	zf_addr ip;

	/* setjmp env for handling aborts */
	jmp_buf jmpbuf;

	/* Input buffer */
	char read_buf[32];
	size_t read_len;

	/* Name buffer */
	char name_buf[32];

	zf_addr *uservar;
} zf_ctx;


/* True is defined as the bitwise complement of false. */

#define ZF_FALSE ((zf_cell)0)
#define ZF_TRUE ((zf_cell)~(zf_int)ZF_FALSE)

/* ZForth API functions */

void zf_init(zf_ctx *ctx, int trace);
void zf_bootstrap(zf_ctx *ctx);
void *zf_dump(zf_ctx *ctx, size_t *len);
zf_result zf_eval(zf_ctx *ctx, const char *buf);
void zf_abort(zf_ctx *ctx, zf_result reason);

void zf_push(zf_ctx *ctx, zf_cell v);
zf_cell zf_pop(zf_ctx *ctx);
zf_cell zf_pick(zf_ctx *ctx, zf_addr n);

zf_result zf_uservar_set(zf_ctx *ctx, zf_uservar_id uv, zf_cell v);
zf_result zf_uservar_get(zf_ctx *ctx, zf_uservar_id uv, zf_cell *v);

/* Host provides these functions */

zf_input_state zf_host_sys(zf_ctx *ctx, zf_syscall_id id, const char *last_word);
void zf_host_trace(zf_ctx *ctx, const char *fmt, va_list va);
zf_cell zf_host_parse_num(zf_ctx *ctx, const char *buf);

#ifdef __cplusplus
}
#endif

#endif
