

all:
	make -C src/linux

clean:
	make -C src/linux clean
	make -C src/atmega8 clean
