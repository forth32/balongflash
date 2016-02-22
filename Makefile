CC       = gcc
LIBS     = 
CFLAGS   = -O2  -Wno-unused-result

.PHONY: all clean

all:    balong_flash

clean: 
	rm *.o
	rm balong_flash

#.c.o:
#	$(CC) -o $@ $(LIBS) $^ qcio.o

hdlcio.o: hdlcio.c
	$(CC) -c hdlcio.c

balong_flash: balong_flash.o hdlcio.o
	@gcc $^ -o $@ $(LIBS)
