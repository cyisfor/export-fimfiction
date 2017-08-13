CFLAGS+=-Io -Ihtml_when/source -Ihtml_when/libxml2/include
P=gtk+-3.0 glib-2.0 gio-2.0

LDLIBS+=$(shell pkg-config --libs $P) html_when/libxml2/.libs/libxml2.a

LINK=$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

O=$(patsubst %,o/%.o,$N)

N=gui word-count main wanted_tags.gen
export-fimfiction: $O
	$(LINK)

N=make-wanted
o/make-wanted: $O
	$(LINK)

o:
	mkdir o

o/%.o: src/%.c | o
	gcc -ggdb `pkg-config --cflags $(P)` -c -o $@ $^

o/wanted_tags.gen.c o/wanted_tags.gen.h: | o o/make-wanted
	cd o && ./make-wanted < ../src/tags.wanted

clean:
	rm -rf o

.PHONY: all stuff
