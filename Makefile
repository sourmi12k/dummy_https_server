SRC_DIR := ${CURDIR}/src
OBJ_DIR := ${CURDIR}/obj
COMPILE_COMMANDS := $(OBJ_DIR)/compile_commands.json
# all src files
SRC := $(wildcard $(SRC_DIR)/*.c)
BASIC_OBJ := $(OBJ_DIR)/acceptor.o $(OBJ_DIR)/buffer.o \
			       $(OBJ_DIR)/cgi.o $(OBJ_DIR)/eventloop.o \
			       $(OBJ_DIR)/log.o $(OBJ_DIR)/main.o \
			       $(OBJ_DIR)/utils.o $(OBJ_DIR)/threadpool.o \
			       $(OBJ_DIR)/tcpconn.o $(OBJ_DIR)/tlsconn.o
# all objects
OBJ := $(BASIC_OBJ) $(OBJ_DIR)/parse.o
# all binaries
BIN := liso_server cgi_worker
# C compiler
CC  := gcc -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
# C PreProcessor Flag
CPPFLAGS := -I${CURDIR}/include
# compiler flags
CFLAGS   := -g -Wall -Werror
# DEPS = http.h y.tab.h

default: all
all : liso_server cgi_worker init_file
check-code : check-lint check-clang-tidy

init_file: $(OBJ_DIR)
	$(RM) $(COMPILE_COMMANDS)
	@echo "[" > $(COMPILE_COMMANDS)

liso_server: $(OBJ)
	$(CC) $^ -o $@ -lpthread  -lssl -lcrypto

cgi_worker: $(OBJ_DIR)/cgi_worker.o $(OBJ_DIR)/buffer.o
	$(CC) $^ -o $@ -lpython2.7 -lssl -lcrypto

check-lint:
	python2 cpplint.py \
	--verbose=2 --quiet --linelength=120 \
	--filter=-readability/casting,-legal/copyright,-build/include_subdir \
	include/*.h src/*.c

check-clang-tidy: init_file $(OBJ)
	sed -i '$$s/.$$//' $(COMPILE_COMMANDS)
	@echo ']' >> $(COMPILE_COMMANDS)
	@python2 run_clang_tidy.py -clang-tidy-binary /usr/bin/clang-tidy-8 -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	@echo '{\n  "directory": "$(OBJ_DIR)",\n  "command": "$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@",\n  "file": "$<"\n},' >> $(COMPILE_COMMANDS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir $@

clean:
	$(RM) $(OBJ) $(BIN)
	$(RM) -r $(OBJ_DIR)
