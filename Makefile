CFLAGS = -Wall -Wextra -Wshadow -o3
LIBS = -lcurl
SRCS = $(wildcard src/*.c)

all:
	mkdir -p bin
	gcc $(CFLAGS) $(LIBS) $(SRCS) -fsanitize=address -g -o bin/Pwootie -I include/ -lzip

run:
	./bin/Pwootie install
