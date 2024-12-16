
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zforth.h"

/*
 * Sys callback function
 */

zf_input_state zf_host_sys(zf_ctx *ctx, zf_syscall_id id, const char *input) {
  switch ((int)id) {

    /* The core system callbacks */

  case ZF_SYSCALL_EMIT:
  case ZF_SYSCALL_PRINT:
    zf_pop(ctx);
    break;

  case ZF_SYSCALL_TELL: {
    zf_cell len = zf_pop(ctx);
    zf_cell addr = zf_pop(ctx);
    if (addr >= ZF_DICT_SIZE - len) {
      zf_abort(ctx, ZF_ABORT_OUTSIDE_MEM);
    }
  } break;

  default:
    break;
  }

  return ZF_INPUT_INTERPRET;
}

/*
 * Parse number
 */

zf_cell zf_host_parse_num(zf_ctx *ctx, const char *buf) {
  zf_cell v;
  int n = 0;
  int r = sscanf(buf, ZF_SCAN_FMT "%n", &v, &n);
  if (r != 1 || buf[n] != '\0') {
    zf_abort(ctx, ZF_ABORT_NOT_A_WORD);
  }
  return v;
}

/*
 * Main
 */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  zf_ctx ctx;

  zf_init(&ctx, 0);
  zf_bootstrap(&ctx);

  /* Turn input into NUL-terminated string */
  char *buf = malloc(size + 1);
  if (buf == NULL) {
    return 0;
  }
  memcpy(buf, data, size);
  buf[size] = 0;

  zf_eval(&ctx, buf);
  free(buf);

  return 0;
}

/*
 * End
 */
