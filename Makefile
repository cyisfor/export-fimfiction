DERP:=$(shell $(MAKE) -C html_when)

CFLAGS+=-ggdb -O2
CFLAGS+=-Io -Ihtml_when/src -Ilibxml2/include -Ilibxmlfixes/ -Ilibxmlfixes/o
P=gtk+-3.0 glib-2.0 gio-2.0

LDLIBS+=$(shell pkg-config --libs $P) -lpcre
LDLIBS+=$(shell xml2-config --libs | sed -e's/-xml2//g')
LINK=$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

O=$(patsubst %,o/%.o,$N) libxml2/.libs/libxml2.a html_when/libhtmlwhen.a \
	libxmlfixes/libxmlfixes.a \
	$(foreach name,$(N),$(eval targets:=$$(targets) $(name)))

derp: setup
	$(MAKE) -C html_when
	$(MAKE) export-fimfiction

N=gui wordcount main
export-fimfiction: $O
	$(LINK)

-include $(patsubst %,o/%.d,$N)

o/main.o: libxml2/include/libxml/xmlversion.h 

libxml2/include/libxml/xmlversion.h libxml2/.libs/libxml2.a: libxml2/configure

libxml2/configure:
	$(MAKE) -C html_when

o:
	mkdir o

COMPILE=$(CC) $(CFLAGS) `pkg-config --cflags $(P)` -c -o $@ $<

o/%.d: src/%.c | o
	$(COMPILE) -MM

o/%.o: src/%.c | o
	$(COMPILE)

o/main.d o/main.o: libxmlfixes/o/wanted_tags.gen.h

clean:
	rm -rf o

.PHONY: all stuff setup derp

setup:
	sh setup.sh
	$(MAKE) -C html_when setup

-include $(patsubst %, o/%.d,$(targets))
