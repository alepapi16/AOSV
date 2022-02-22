
all: 
	@printf '$(bold)************  BUILDING LIBRARY ************\n$(sgr0)'
	gcc -I$(LINC) -I$(KINC) -c lgroups.c
	mkdir $(OBJ)/lib
	mv -t $(OBJ)/lib lgroups.o
