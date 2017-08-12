#include <sys/mman.h> // mmap
#include <sys/stat.h>
#include <stdlib.h> // size_t
#include <assert.h>
#include <string.h>
#include <ctype.h> // isspace
#include <error.h>
#include <unistd.h> // write
#include <stdbool.h>

#define LITLEN(a) a,sizeof(a)-1

int main(int argc, char *argv[])
{
	/*
		enum wanted_tags { A, CHAT, ..., UNWANTED };

		enum wanted_tags lookup_wanted(const char* tag) {
		  switch(tag[0]) {
			 case 'a': return A
			 ...
			 }
			 ...
		}

		switch(lookup_wanted(tag)) {
		case A:
		  return argTag("url","href");
		case CHAT:
		  return dumbTag("quote");
		...
		default:
		  error("unknown tag! %.*s",tlen,tag);
		};
	*/

	struct trie {
		char c;
		struct trie* subs;
		size_t nsubs;
	};

	struct trie root = {};

	void insert(const char* tag, size_t len) {
		void visit(struct trie* cur, size_t off) {
			if(off != len) {
				char c = tag[off];
				size_t i;
				// TODO: make subs sorted, and binary search to insert
				// that'd be faster for LOTS of tags, probably slower otherwise
				// because mergesort/indirection/etc
				for(i=0;i<cur->nsubs;++i) {
					struct trie* sub = &cur->subs[i];
					if(sub->c == c) {
						return visit(sub,off+1);
					}
				}
				cur->subs = realloc(cur->subs,sizeof(*cur->subs)*(cur->nsubs+1));
				cur = &cur->subs[cur->nsubs++];
				cur->c = c;
				// we don't need to traverse the subs we create. just finish the string here
				// as children.
				for(++off;off<len;++off) {
					c = tag[off];
					cur->subs = malloc(sizeof(*cur->subs));
					cur->nsubs = 1;
					cur = &cur->subs[0];
					cur->c = c;
				}
			}
			// final one, be sure to add a terminator
			cur->subs = malloc(sizeof(*cur->subs));
			cur->nsubs = 1;
			cur = &cur->subs[0];
			cur->c = 0;
			cur->subs = NULL;
			cur->nsubs = 0;
			return;
		}
		return visit(&root, 0);
	}

	struct stat winfo;
	assert(0==fstat(0,&winfo));
	char* src = mmap(NULL,winfo.st_size,PROT_READ,MAP_PRIVATE,0,0);
	assert(src != MAP_FAILED);
	char* cur = src;
	while(isspace(*cur)) {
		if(++cur == src + winfo.st_size) {
			// whitespace file]
			error(23,0,"only whitespace?");
		}
	}
	for(;;) {
		// cur should always point to non-whitespace here.
		char* nl = memchr(cur,'\n',winfo.st_size-(cur-src));
		char* tail;
		if(nl == NULL) {
			// no newline at the end of file
			tail = src + winfo.st_size-1;
		} else {
			tail = nl-1;
		}
		if(tail != cur) {
			while(isspace(*tail)) {
				while(isspace(*tail)) {
					if(--tail == cur) break;
				}
			}
		}
		insert(cur,tail - cur + 1);
		if(nl == NULL) break;
		cur = nl+1;
		if(cur == src + winfo.st_size) {
			// trailing newline
			break;
		}
		while(isspace(*cur)) {
			if(++cur == src + winfo.st_size) {
				// trailing whitespace
				break;
			}
		}
	}
	munmap(src,winfo.st_size);

	/* aab aac abc ->
		 a: (a1 b)
		 a1: (b1 c)
		 b: (c1)

		 output a, then recurse on a1, a again, then on b
		 output self, then child, self, then child
		 -> aabaacabc add separators if at top
	*/
	char tag[0x100]; // err... 0x100 should be safe-ish.
	bool first = false;
	void indent(int level) {
		int i=0;
		char* buf = alloca(level);
		for(i=0;i<level;++i) {
			buf[i] = ' ';
		}
		write(1,buf,level);
	}
	void writebufi(int i) {
		write(1,LITLEN("buf["));
		char buf[0x100];
		write(1,buf, snprintf(buf,0x100,"%d",i));
		write(1,LITLEN("]"));
	}
	void dump_tag(char* dest, struct trie* cur, int level) {
		size_t i;
		if(cur->nsubs == 1)
			if(cur->subs[0].c == 0) {
				indent(level+1);
				write(1,LITLEN("return "));
				write(1,tag,dest-tag);
				write(1,LITLEN(";\n"));
				return;
			} else {
				indent(level);
				write(1,LITLEN("if ("));
				writebufi(level-1);
				write(1,LITLEN("== '"));
				write(1,&cur->subs[0].c,1);
				write(1,LITLEN("') {\n"));
			}
		return;
		}
		indent(level);


		for(i=0;i<cur->nsubs;++i) {
			*dest = toupper(cur->subs[i].c);
			indent(level+1);
			write(1,LITLEN("case '"));
			write(1,&cur->subs[i].c,1);
			write(1,LITLEN("':"));
			dump_tag(dest+1, &cur->subs[i],level+1);
		}
		indent(level);
		write(1,LITLEN("};\n"));
	}
	write(1,LITLEN("enum wanted_tags {"));
	dump_tag(tag, &root, 0);
	write(1,LITLEN("};\n"));
}
