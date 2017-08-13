CFLAGS+=
P=gtk+-3.0 glib-2.0 gio-2.0 libxml2

LDLIBS+=$(shell pkg-config --libs $P)

LINK=$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

O=$(patsubst %.c,%.o,$N)

N=gui word main wanted_tags.gen
export-fimfiction: $O
	$(LINK)

o:
	mkdir build

o/%.o: source/%.c | o
	gcc -ggdb `pkg-config --cflags $(P)` -c -o $@ $^

o/wanted_tags.gen.c o/wanted_tags.gen.h: | o o/make-wanted
	cd o && ./make-wanted < ../src/tags.wanted

clean:
	rm -rf o

.PHONY: all stuff
