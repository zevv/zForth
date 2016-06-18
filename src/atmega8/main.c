
/*
 * Example zforth main app for atmega8. This is about the smallest environment
 * in which zForth can be useful. Memory could be saved by leaving out ZF_ENABLE_BOOTSTRAP
 * and provide the basic dictionary through some other method, but that would make this
 * example overly complicated.
 */

#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include "zforth.h"

#define UART_BAUD(baudrate)     ((F_CPU / baudrate) / 16 - 1)

void uart_init(uint16_t baudrate);
static int uart_tx(char c, FILE *f);
int uart_rx(FILE *);


int main(void)
{

	/* Setup stdin/stdout */

	uart_init(UART_BAUD(115200));
        fdevopen(uart_tx, uart_rx);


	/* Initialize zforth */

	zf_init(0);
	zf_bootstrap();


	/* Main loop: read words and eval */

	char buf[64];
	uint8_t l = 0;

	for(;;) {
		int c = getchar();
		putchar(c);
		if(c == 10 || c == 13 || c == 32) {
			zf_result r = zf_eval(buf);
			if(r != ZF_OK) printf("A%d\n", r);
			l = 0;
		} else if(l < sizeof(buf)-1) {
			buf[l++] = c;
		}

		buf[l] = '\0';
	}

}



zf_input_state zf_host_sys(zf_syscall_id id, const char *input)
{
	switch((int)id) {

		case ZF_SYSCALL_EMIT:
			putchar((char)zf_pop());
			fflush(stdout);
			break;

		case ZF_SYSCALL_PRINT:
			printf(ZF_CELL_FMT " ", zf_pop());
			break;
	}

	return 0;
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

