CC       = gcc
LIBS     = -lz
#BUILDNO = `cat build`
BUILDNO=$(shell cat build)
LOCALE_DIR = locale
CFLAGS   = -O2  -Wunused -Wno-unused-result -D BUILDNO=$(BUILDNO) -DLOCALE_DIR='"$(LOCALE_DIR)"' $(LIBS) 
.PHONY: all clean

POFILES := $(wildcard po/*.po)
MOFILES := $(patsubst po/%.po,locale/%/LC_MESSAGES/balongflash.mo,$(POFILES))

all:    balong_flash $(MOFILES)

clean: 
	rm *.o
	rm balong_flash
	rm -r locale

balong_flash: balong_flash.o hdlcio_linux.o ptable.o flasher.o util.o signver.o
	@gcc $^ -o $@ $(LIBS) 
	@echo Current buid: $(BUILDNO)
	@echo $$((`cat build`+1)) >build

locale/%/LC_MESSAGES/balongflash.mo: po/%.po
	mkdir -p $(dir $@) && msgfmt --output-file=$@ $<

po/balongflash.pot: balong_flash.c flasher.c hdlcio_linux.c hdlcio_win32.c ptable.c signver.c util.c
	xgettext --language=C --from-code=UTF-8 --keyword=_ --keyword=_noop --add-comments \
	         --copyright-holder=forth32 \
	         --package-name=balongflash \
	         --package-version=3.0.$(BUILDNO) \
	         --output=$@ $^

.PHONY: pot
pot: po/balongflash.pot
