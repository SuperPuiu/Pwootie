CFLAGS = -Wall -Wextra -Wshadow -o3
LIBS = -lcurl -lzip
SRCS = $(wildcard src/*.c)

test:
	mkdir -p bin
	gcc $(CFLAGS) $(LIBS) $(SRCS) -fsanitize=address -fsanitize=leak -g -o bin/PwootieTest -I include/

release:
	mkdir -p bin
	gcc $(CFLAGS) $(LIBS) $(SRCS) -o bin/Pwootie -I include/

run:
	./bin/PwootieTest
