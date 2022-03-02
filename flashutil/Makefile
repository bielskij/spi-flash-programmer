all:
	gcc -o flash-util \
		-I../common/include \
		-Iinclude \
		\
		../common/src/crc8.c \
		\
		src/serial.c \
		src/debug.c \
		src/main.c

clean:
	rm -f flash-util