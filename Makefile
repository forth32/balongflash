CC       = gcc
LIBS     = -lz
#BUILDNO = `cat build`
BUILDNO=$(shell cat build)
CFLAGS   = -O2 -Wunused -Wno-unused-result -D BUILDNO=$(BUILDNO)  -D_7ZIP_ST $(LIBS) 
.PHONY: all clean

all:    balong_flash

clean: 
	rm -f *.o lzma/*.o
	rm -f balong_flash

balong_flash: balong_flash.o hdlcio_linux.o ptable.o flasher.o util.o signver.o lzma/Alloc.o lzma/LzmaDec.o
	@gcc $^ -o $@ $(LIBS) 
	@echo Current buid: $(BUILDNO)
	@echo $$((`cat build`+1)) >build
	
