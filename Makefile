BUILD_DIR = bin

CFLAGS = -D_GNU_SOURCE -Wall -Wextra -Wshadow -o3 -g
LIBS = -lcurl -lzip

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

OUTPUT = $(BUILD_DIR)/Pwootie
OUTPUT_TEST = $(BUILD_DIR)/PwootieTest

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@gcc $(CFLAGS) -c $< -o $@ -I include/

$(OUTPUT_TEST): $(OBJS)
	gcc $(CFLAGS) $(LIBS) $(OBJS) -fsanitize=address -fsanitize=leak -o $@

$(OUTPUT): $(OBJS)
	gcc $(CFLAGS) $(LIBS) $(OBJS) -o $@

run:
	./$(OUTPUT_TEST)
clean:
	rm $(BUILD_DIR)/*.*

test: $(OUTPUT_TEST)
release: $(OUTPUT)

.PHONY: test release run clean
