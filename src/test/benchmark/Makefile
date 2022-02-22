
build:
	@printf '$(bold)************  BUILDING BENCHMARKS ************\n$(sgr0)'
	gcc -I$(TINC) -I$(LINC) -pthread -o rw_tps.out rw_tps.c \
	    $(OBJ)/test/utils.o  $(OBJ)/lib/lgroups.o
	mkdir $(BIN)/benchmark/data
	mkdir $(BIN)/benchmark/results
	cp -t $(BIN)/benchmark  plot.py
	mv -t $(BIN)/benchmark  rw_tps.out
