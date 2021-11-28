#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "zforth.h"

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)

#define BUF_SIZE (1024)
void zf_host_trace(const char *fmt, va_list va)
{
        printf("\033[1;30m");
        vprintf(fmt, va);
        printf("\033[0m");
}

zf_input_state zf_host_sys(zf_syscall_id id, const char *last_word) {
	char c;
	zf_cell len;
	uint32_t *addr;
	uint32_t val;

	switch ((int)id) {

		case ZF_SYSCALL_EMIT:
			c=(char) zf_pop();
			putchar(c);
			break;
		case ZF_SYSCALL_PRINT:
			printf(ZF_CELL_FMT " \n", zf_pop());
			break;
		case ZF_SYSCALL_TELL:
			len = zf_pop();
			char *msg = (void *)zf_dump(NULL) + (int)zf_pop();
			for (int i=0; i<len; i++) putchar(*(msg+i));
			break;
		case ZF_SYSCALL_USER + 4: // readmem
			addr = (uint32_t*)zf_pop();
			zf_push( (zf_cell)(*addr) );
		     break;
		 case ZF_SYSCALL_USER + 5: // writemem
		     addr = (uint32_t*)zf_pop();
		     val = (uint32_t)zf_pop();
		     *addr = val;
		     break;
 

	}
	return ZF_INPUT_INTERPRET;
}

zf_cell zf_host_parse_num(const char *buf)
{
        zf_cell v;
	int r;
	if ( *buf=='0' && *(buf+1)=='x' ) r = sscanf(buf,"0x%x", &v); //hex
        else r = sscanf(buf, "%d", &v); //decimal
        if(r == 0) {
                zf_abort(ZF_ABORT_NOT_A_WORD);
        }
        return v;
}

/*
 * Evaluate buffer with code, check return value and report errors
 */

zf_result do_eval(const char *buf)
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
                default: msg = "unknown error";
        }

        if(msg) {
                printf(" \033[31m");
                printf("%s\033[0m\n", msg);
        }

        return rv;
}

static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint8_t *act_data = data;

    zf_init(0);
    zf_bootstrap();
    
    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, act_data, 1, 20 / portTICK_RATE_MS);
	if (*act_data == 0xd) {
		putchar(0x0d);
		putchar(0x0a);
		*act_data=0;
		do_eval( (const char *) data);
		act_data = data;
	}
	else {
		if (*act_data > 31) {
			uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) act_data, len);
			act_data += len;
		}
	}
    }
}

void app_main(void)
{
    xTaskCreate(echo_task, "zForth", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}
