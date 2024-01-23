
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "zforth.h"



/*
 * Evaluate buffer with code, check return value and report errors
 */

zf_result do_eval(const char *src, int line, const char *buf)
{
	const char *msg = NULL;

	zf_result rv = zf_eval(buf);

	switch(rv)
	{
		case ZF_OK: break;
		case ZF_ABORT_INTERNAL_ERROR: msg = "internal error"; break;
		case ZF_ABORT_OUTSIDE_MEM: msg = "outside memory"; break;
		case ZF_ABORT_DSTACK_OVERRUN: msg = "dstack overrun"; break;
		case ZF_ABORT_DSTACK_UNDERRUN: msg = "dstack underrun"; break;
		case ZF_ABORT_RSTACK_OVERRUN: msg = "rstack overrun"; break;
		case ZF_ABORT_RSTACK_UNDERRUN: msg = "rstack underrun"; break;
		case ZF_ABORT_NOT_A_WORD: msg = "not a word"; break;
		case ZF_ABORT_COMPILE_ONLY_WORD: msg = "compile-only word"; break;
		case ZF_ABORT_INVALID_SIZE: msg = "invalid size"; break;
		case ZF_ABORT_DIVISION_BY_ZERO: msg = "division by zero"; break;
		case ZF_ABORT_INVALID_USERVAR: msg = "invalid user variable"; break;
		default: msg = "unknown error";
	}

	if(msg) {
		fprintf(stderr, "\033[31m");
		if(src) fprintf(stderr, "%s:%d: ", src, line);
		fprintf(stderr, "%s", msg);
		const char* abort_buf = zf_abort_get_buf ();
		if(abort_buf) fprintf(stderr, ": %s", abort_buf);
		fprintf(stderr, "\033[0m\n");
	}

	return rv;
}


/*
 * Load given forth file
 */

void include(const char *fname)
{
	char buf[256];

	FILE *f = fopen(fname, "rb");
	int line = 1;
	if(f) {
		while(fgets(buf, sizeof(buf), f)) {
			do_eval(fname, line++, buf);
		}
		fclose(f);
	} else {
		fprintf(stderr, "error opening file '%s': %s\n", fname, strerror(errno));
	}
}


/*
 * Save dictionary
 */

static void save(const char *fname)
{
	size_t len;
	void *p = zf_dump(&len);
	FILE *f = fopen(fname, "wb");
	if(f) {
		fwrite(p, 1, len, f);
		fclose(f);
	}
}


/*
 * Load dictionary
 */

static void load(const char *fname)
{
	size_t len;
	void *p = zf_dump(&len);
	FILE *f = fopen(fname, "rb");
	if(f) {
		fread(p, 1, len, f);
		fclose(f);
	} else {
		perror("read");
	}
}


/*
 * Sys callback function
 */

zf_input_state zf_host_sys(zf_syscall_id id, const char *input)
{
	switch((int)id) {


		/* The core system callbacks */

		case ZF_SYSCALL_EMIT:
			putchar((char)zf_pop());
			fflush(stdout);
			break;

		case ZF_SYSCALL_PRINT:
			printf(ZF_CELL_FMT " ", zf_pop());
			break;

		case ZF_SYSCALL_TELL: {
			zf_cell len = zf_pop();
			zf_cell addr = zf_pop();
			if(addr >= ZF_DICT_SIZE - len) {
				zf_abort(ZF_ABORT_OUTSIDE_MEM, NULL);
			}
			void *buf = (uint8_t *)zf_dump(NULL) + (int)addr;
			(void)fwrite(buf, 1, len, stdout);
			fflush(stdout); }
			break;


		/* Application specific callbacks */

		case ZF_SYSCALL_USER + 0:
			printf("\n");
			exit(0);
			break;

		case ZF_SYSCALL_USER + 1:
			zf_push(sin(zf_pop()));
			break;

		case ZF_SYSCALL_USER + 2:
			if(input == NULL) {
				return ZF_INPUT_PASS_WORD;
			}
			include(input);
			break;
		
		case ZF_SYSCALL_USER + 3:
			save("zforth.save");
			break;

		default:
			printf("unhandled syscall %d\n", id);
			break;
	}

	return ZF_INPUT_INTERPRET;
}


/*
 * Tracing output
 */

void zf_host_trace(const char *fmt, va_list va)
{
	fprintf(stderr, "\033[1;30m");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\033[0m");
}


/*
 * Parse number
 */

zf_cell zf_host_parse_num(const char *buf)
{
	zf_cell v;
	int n = 0;
	int r = sscanf(buf, ZF_SCAN_FMT"%n", &v, &n);
	if(r != 1 || buf[n] != '\0') {
		zf_abort(ZF_ABORT_NOT_A_WORD, buf);
	}
	return v;
}


void usage(void)
{
	fprintf(stderr, 
		"usage: zfort [options] [src ...]\n"
		"\n"
		"Options:\n"
		"   -h         show help\n"
		"   -t         enable tracing\n"
		"   -l FILE    load dictionary from FILE\n"
		"   -q         quiet\n"
	);
}


/*
 * Main
 */

int main(int argc, char **argv)
{
	int i;
	int c;
	int trace = 0;
	int line = 0;
	int quiet = 0;
	const char *fname_load = NULL;

	/* Parse command line options */

	while((c = getopt(argc, argv, "hl:tq")) != -1) {
		switch(c) {
			case 't':
				trace = 1;
				break;
			case 'l':
				fname_load = optarg;
				break;
			case 'h':
				usage();
				exit(0);
			case 'q':
				quiet = 1;
				break;
		}
	}
	
	argc -= optind;
	argv += optind;


	/* Initialize zforth */

	zf_init(trace);


	/* Load dict from disk if requested, otherwise bootstrap fort
	 * dictionary */

	if(fname_load) {
		load(fname_load);
	} else {
		zf_bootstrap();
	}


	/* Include files from command line */

	for(i=0; i<argc; i++) {
		include(argv[i]);
	}

	if(!quiet) {
		zf_cell here;
		zf_uservar_get(ZF_USERVAR_HERE, &here);
		printf("Welcome to zForth, %d bytes used\n", (int)here);
	}

	/* Interactive interpreter: read a line using readline library,
	 * and pass to zf_eval() for evaluation*/

#ifdef USE_READLINE

	read_history(".zforth.hist");

	for(;;) {

		char *buf = readline("");
		if(buf == NULL) break;

		if(strlen(buf) > 0) {

			do_eval("stdin", ++line, buf);
			printf("\n");

			add_history(buf);
			write_history(".zforth.hist");

		}

		free(buf);
	}
#else
	for(;;) {
		char buf[4096];
		if(fgets(buf, sizeof(buf), stdin)) {
			do_eval("stdin", ++line, buf);
			printf("\n");
		} else {
			break;
		}
	}
#endif

	return 0;
}


/*
 * End
 */

