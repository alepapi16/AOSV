SUBDIRS = src/kernel src/library src/test

PWD = $(shell pwd)
SRC = $(PWD)/src
INC = $(SRC)/include
export KINC = $(INC)/kernel
export LINC = $(INC)/library
export TINC = $(INC)/test

export OUT = $(shell pwd)/out
export BIN = $(OUT)/bin
export OBJ = $(OUT)/obj
export UDEV = $(OUT)/udev
SCRIPTS = $(OUT)/scripts

export bold = $(shell tput bold)
export sgr0 = $(shell tput sgr0)

build:
	@printf '$(bold)************  AOSV FINAL PROJECT ************\n$(sgr0)'
	mkdir $(OUT);
	mkdir $(BIN);
	mkdir $(OBJ);
	mkdir $(UDEV);
	mkdir $(SCRIPTS);
	
	@printf '$(bold)************  COPYING UDEV RULES ************\n$(sgr0)'
	cp -t $(UDEV) $(SRC)/udev/83-groups.rules
	
	# Make subdirectories
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done
	
	@printf '$(bold)************  COPYING INSTALLATION SCRIPTS ************\n$(sgr0)'
	cp -t $(SCRIPTS) scripts/*


clean:
	@printf '$(bold)************  CLEANING ALL ************\n$(sgr0)'
	rm -rf $(OUT)
	