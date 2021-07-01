SRC_DIR := src
OBJ_DIR := obj
# all src files
SRC := $(wildcard $(SRC_DIR)/*.c)
SRCPP := $(wildcard $(SRC_DIR)/*.cpp)
# all objects
OBJ := $(OBJ_DIR)/y.tab.o $(OBJ_DIR)/lex.yy.o $(OBJ_DIR)/parse.o $(OBJ_DIR)/icws.o $(OBJ_DIR)/connection.o $(OBJ_DIR)/work_queue.o
# all binaries
BIN := icws
# C compiler
CC  := gcc
# C++ compiler
CPP := g++
# C PreProcessor Flag
CPPFLAGS :=  -pthread
# compiler flags
CFLAGS   := -g -Wall
# DEPS = parse.h y.tab.h connection.h

default: all
all : icws

icws: $(OBJ)
	$(CPP) $(CPPFLAGS) $^ -o $@

$(SRC_DIR)/lex.yy.c: $(SRC_DIR)/lexer.l
	flex -o $@ $^

$(SRC_DIR)/y.tab.c: $(SRC_DIR)/parser.y
	yacc -Wno-yacc -d $^
	mv y.tab.c $@
	mv y.tab.h $(SRC_DIR)/y.tab.h

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR)
	$(CPP) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

#echo_server: $(OBJ_DIR)/echo_server.o
#	$(CC) -Werror $^ -o $@

#echo_client: $(OBJ_DIR)/echo_client.o
#	$(CC) -Werror $^ -o $@

$(OBJ_DIR):
	mkdir $@

clean:
	$(RM) $(OBJ) $(BIN) $(SRC_DIR)/lex.yy.c $(SRC_DIR)/y.tab.*
	$(RM) -r $(OBJ_DIR)
