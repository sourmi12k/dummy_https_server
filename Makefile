SRC_DIR := src
OBJ_DIR := obj
TEST_DIR := test
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
BIN := liso_server parse_test log_test cgi_worker
# C compiler
CC  := gcc -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -static-libasan
# C PreProcessor Flag
CPPFLAGS := -Iinclude
# compiler flags
CFLAGS   := -g -Wall -Werror
# DEPS = http.h y.tab.h

default: all
all : liso_server log_test cgi_worker

liso_server: $(OBJ)
	$(CC) $^ -o $@ -lpthread  -lssl -lcrypto

# parse_test: $(OBJ_DIR)/buffer.o $(OBJ_DIR)/cgi.o \
#             $(OBJ_DIR)/eventloop.o $(OBJ_DIR)/log.o \
# 						$(OBJ_DIR)/parse.o $(OBJ_DIR)/utils.o \
# 						$(TEST_DIR)/parse_test.o
# 	$(CC) $^ -o $@ -lpthread -lssl -lcrypto

log_test: $(OBJ_DIR)/log.o $(OBJ_DIR)/utils.o $(TEST_DIR)/log_test.o
	$(CC) $^ -o $@ -lpthread

cgi_worker: $(OBJ_DIR)/cgi_worker.o $(OBJ_DIR)/buffer.o
	$(CC) $^ -o $@ -lpython2.7 -lssl -lcrypto

check-lint:
	python2 cpplint.py --linelength=120 --filter=-readability/casting,-legal/copyright,-build/include_subdir include/*.h src/*.c

$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir $@

clean:
	$(RM) $(OBJ) $(BIN) $(TEST_DIR)/*.o
	$(RM) -r $(OBJ_DIR)
