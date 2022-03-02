all:
	gcc -o flash-util \
		-I../firmware/include \
		-Iinclude \
		\
		src/serial.c \
		src/debug.c \
		src/main.c

clean:
	rm -f flash-util