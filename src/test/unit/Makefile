
unit: setup  unit.o  test_delay.o  test_flush.o  test_install_group.o \
	    test_rw_fifo.o  test_max_install.o  test_barrier.o \
	    test_revoke.o  test_stress.o  test_sysfs.o
	
	gcc -pthread -o unit.out  unit.o  test_delay.o  test_flush.o \
	    test_install_group.o  test_rw_fifo.o  test_max_install.o test_barrier.o \
	    test_revoke.o  test_stress.o test_sysfs.o \
	    $(OBJ)/test/utils.o  $(OBJ)/lib/lgroups.o
	mv -t $(OBJ)/unit *.o
	mv -t $(BIN) unit.out
	mkdir $(BIN)/logs

setup:
	@printf '$(bold)************  BUILDING UNIT TESTS ************\n$(sgr0)'

test_barrier.o:
	gcc -I$(LINC) -c test_barrier.c

test_delay.o:
	gcc -I$(TINC) -I$(LINC) -c test_delay.c

test_flush.o:
	gcc -I$(TINC) -I$(LINC) -c test_flush.c

test_install_group.o:
	gcc -I$(TINC) -I$(KINC) -c test_install_group.c

test_max_install.o:
	gcc -I$(TINC) -I$(KINC) -c test_max_install.c

test_revoke.o:
	gcc -I$(TINC) -I$(LINC) -c test_revoke.c

test_rw_fifo.o:
	gcc -I$(TINC) -I$(LINC) -c test_rw_fifo.c

test_stress.o:
	gcc -I$(TINC) -I$(LINC) -c test_stress.c

test_sysfs.o:
	gcc -I$(TINC) -I$(LINC) -c test_sysfs.c

unit.o:
	gcc -c -o unit.o unit.c
