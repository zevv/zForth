
/*
 * Example zforth main app for atmega8. This is about the smallest environment
 * in which zForth can be useful. Memory could be saved by leaving out
 * ZF_ENABLE_BOOTSTRAP and provide the basic dictionary through some other
 * method, but that would make this example overly complicated.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <avr/io.h>
#include "zforth.h"

#define UART_BAUD(baudrate)     ((F_CPU / baudrate) / 16 - 1)

static void uart_init(uint16_t baudrate);
static int uart_tx(char c, FILE *f);
static int uart_rx(FILE *);
static FILE f = FDEV_SETUP_STREAM(uart_tx, uart_rx, _FDEV_SETUP_RW);

static char buf[32];


int main(void)
{
	/* Setup stdin/stdout */

	uart_init(UART_BAUD(9600));
	stdout = stdin = &f;


	/* Initialize zforth */

	zf_init(1);
	zf_bootstrap();
	zf_eval(": . 1 sys ;");


	/* Main loop: read words and eval */

	uint8_t l = 0;

	for(;;) {
		int c = getchar();
		putchar(c);
		if(c == 10 || c == 13 || c == 32) {
			zf_result r = zf_eval(buf);
			if(r != ZF_OK) puts("A");
			l = 0;
		} else if(l < sizeof(buf)-1) {
			buf[l++] = c;
		}

		buf[l] = '\0';
	}

}


zf_input_state zf_host_sys(zf_syscall_id id, const char *input)
{
	char buf[16];

	switch((int)id) {

		case ZF_SYSCALL_EMIT:
			putchar((char)zf_pop());
			fflush(stdout);
			break;

		case ZF_SYSCALL_PRINT:
			itoa(zf_pop(), buf, 10);
			puts(buf);
			break;
	}

	return 0;
}


zf_cell zf_host_parse_num(const char *buf)
{
	char *end;
        zf_cell v = strtol(buf, &end, 0);
	if(*end != '\0') {
                zf_abort(ZF_ABORT_NOT_A_WORD);
        }
        return v;
}


void uart_init(uint16_t baudrate)
{
	UBRRH = (baudrate >> 8);			/* Set the baudrate [p.132] */
	UBRRL = (baudrate & 0xff);			
	UCSRB = (1<<RXCIE) | (1<<RXEN) | (1<<TXEN);	/* Enable receiver and transmitter and rx interrupts [p.136] */
	UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);	/* Set to no parity, 8 data bits, 1 stopbit */

}


static int uart_tx(char c, FILE *f)
{
	while(!(UCSRA & (1<<UDRE)));
	UDR = c;
	return 0;
}


int uart_rx(FILE *f)
{
	while( ! (UCSRA & (1<<RXC)));
	return(UDR);
}


/*
 * End
 */

