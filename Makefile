

all: snake

snake:
	gcc src/termios_snake.c -o bin/termios_snake

clean:
	rm -rf bin/*
