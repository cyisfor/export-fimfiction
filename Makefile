CFLAGS+=-Io -Ihtml_when/source -Ihtml_when/libxml2/include
P=gtk+-3.0 glib-2.0 gio-2.0

LDLIBS+=$(shell pkg-config --libs $P) -lpcre
LDLIBS+=$(shell xml2-config --libs | sed -e's/-xml2//g')
LINK=$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

O=$(patsubst %,o/%.o,$N) html_when/libxml2/.libs/libxml2.a html_when/libhtmlwhen.a

N=gui wordcount main wanted_tags.gen
export-fimfiction: $O
	$(LINK)

-include $(patsubst %,o/%.d,$N)


html_when/libhtmlwhen.a:
	$(MAKE) -C html_when
.PHONY: html_when/libhtmlwhen.a

o/main.o: html_when/libxml2/include/libxml/xmlversion.h 

html_when/libxml2/include/libxml/xmlversion.h html_when/libxml2/.libs/libxml2.a: html_when/libxml2/configure
	cd html_when/libxml2/ && make

html_when/libxml2/configure:
	cd html_when/libxml2/ && sh autogen.sh

N=make-wanted
o/make-wanted: o/make-wanted.o
	$(LINK)

o/make-wanted: LDLIBS=

o:
	mkdir o

COMPILE=$(CC) $(CFLAGS) `pkg-config --cflags $(P)` -c -o $@ $<

o/%.d: src/%.c | o
	$(COMPILE) -MM

o/%.o: src/%.c | o
	$(COMPILE)

o/main.d o/main.o: o/wanted_tags.gen.h

o/wanted_tags.gen.c o/wanted_tags.gen.h: o/make-wanted src/tags.wanted | o 
	cd o && ./make-wanted < ../src/tags.wanted

clean:
	rm -rf o

.PHONY: all stuff
