
all: setup utils test
	$(MAKE) -C unit || exit 1;
	$(MAKE) -C benchmark || exit 1;
	
setup:
	mkdir $(OBJ)/unit
	mkdir $(OBJ)/test
	mkdir $(BIN)/benchmark

utils:
	gcc -I$(TINC) -c utils.c
	mv -t $(OBJ)/test utils.o

test:
	@printf '$(bold)************  BUILDING TEST APPLICATION ************\n$(sgr0)'
	gcc -I$(LINC) -I$(TINC) -pthread -o test.out test.c \
	    $(OBJ)/lib/lgroups.o $(OBJ)/test/utils.o
	mv -t $(BIN) test.out
