LIBS=gtk+-3.0 glib-2.0 gio-2.0

all: stuff
	dub build -v

stuff: source/declassifier.d build/gui.o | build

build:
	mkdir build

build/%.o: source/%.c | build
	gcc -ggdb `pkg-config --cflags $(LIBS)` -c -o $@ $^

build/declassifier.d: build/declassifyGen | build
	./build/declassifyGen > $@.temp
	mv $@.temp $@

build/declassifyGen: gen/declassifyGen.d | build
	dmd -of$@ $^

clean:
	rm -rf build .dub

.PHONY: all stuff
