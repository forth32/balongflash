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

ptable.o: ptable.c
	$(CC) -c ptable.c

balong_flash: balong_flash.o hdlcio.o ptable.o
	@gcc $^ -o $@ $(LIBS)
