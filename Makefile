BUILD_DIR = bin

CFLAGS = -D_GNU_SOURCE -Wall -Wextra -Wshadow
LIBS = -lcurl -lzip

COMPILER = gcc

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

OUTPUT = $(BUILD_DIR)/Pwootie
OUTPUT_TEST = $(BUILD_DIR)/PwootieTest

ifeq ($(MAKECMDGOALS), test)
	CFLAGS += -g3 -ggdb3
else
	CFLAGS += -flto -O3
endif

ifneq (, $(shell which gcc))
	COMPILER = gcc
else

ifneq (, $(shell which clang))
	COMPILER = clang
else
	$(error Unable to find clang or gcc. Make sure one of these are installed before continuing.)
endif

endif

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(COMPILER) $(CFLAGS) -c $< -o $@ -I include/

$(OUTPUT_TEST): $(OBJS)
	@$(COMPILER) $(CFLAGS) $(LIBS) $(OBJS) -fsanitize=address -fsanitize=leak -o $@
	@echo -e "\033[0;32mCompiled test build. \033[0m"

$(OUTPUT): $(OBJS)
	@$(COMPILER) $(CFLAGS) $(LIBS) $(OBJS) -o $@
	@echo -e "\033[0;32mCompiled release build. \033[0m"

run:
	./$(OUTPUT_TEST)
clean:
	rm $(BUILD_DIR)/*.*

install:
	sudo cp $(OUTPUT) /usr/local/bin/pwootie

test: $(OUTPUT_TEST)
release: $(OUTPUT)
all: $(OUTPUT) install

.PHONY: test release run clean install
