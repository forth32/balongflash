CC       = gcc
LIBS     = 
#BUILDNO = `cat build`
BUILDNO=$(shell cat build)
CFLAGS   = -O2  -Wno-unused-result -D BUILDNO=$(BUILDNO)
.PHONY: all clean

all:    balong_flash

clean: 
	rm *.o
	rm balong_flash

balong_flash: balong_flash.o hdlcio_linux.o ptable.o
	@gcc $^ -o $@ $(LIBS) 
	@echo Current buid: $(BUILDNO)
	@echo $$((`cat build`+1)) >build
	