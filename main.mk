include coolmake/top.mk

CFLAGS+=-ggdb
CFLAGS+=-Io -Ihtml_when/src -Ilibxml2/include -Ilibxmlfixes/src -Ilibxmlfixes/o
P=gtk+-3.0 glib-2.0 gio-2.0

LDLIBS+=$(shell xml2-config --libs | sed -e's/-xml2//g')
LDLIBS+=-lpcre

LIBS=libxml2/libxml2.la \
	html_when/libhtmlwhen.la \
	libxmlfixes/libxmlfixes.la

N=gui wordcount main
OUT=export-fimfiction
$(eval $(PROGRAM))

$(O)/main.lo: libxml2/include/libxml/xmlversion.h libxmlfixes/o/wanted_tags.gen.h



libxml2/include/libxml/xmlversion.h libxml2/.libs/libxml2.a: libxml2/configure

libxml2/libxml2.la libxml2/configure: | libxml2
	$(MAKE) -C html_when

html_when/libhtmlwhen.la html_when/libxml2: | html_when
	$(MAKE) -C html_when

libxml2: | html_when/libxml2
	ln -rs $| $@

html_when:
	sh setup.sh

all: export-fimfiction
