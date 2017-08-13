#include <sys/mman.h> // mmap
#include <sys/stat.h>
#include <stdlib.h> // size_t
#include <assert.h>
#include <string.h>
#include <ctype.h> // isspace
#include <error.h>
#include <unistd.h> // write
#include <stdbool.h>
#include <stdio.h> // snprintf

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
			char c = (off == len) ? 0 : tag[off];

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
			for(++off;off<=len;++off) {
				c = off == len ? 0 : tag[off];
				cur->subs = malloc(sizeof(*cur->subs));
				cur->nsubs = 1;
				cur = &cur->subs[0];
				cur->c = c;
			}
			// final one, be sure to null it
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

	int compare_nodes(struct trie* a, struct trie* b) {
		return a->c - b->c;
	}
	
	void sort_leven(struct trie* cur) {
		if(cur->nsubs == 0) return;

		qsort(&cur->subs[0],sizeof(cur->subs[0]),cur->nsubs,compare_nodes);
	}

	/* aab aac abc ->
		 a: (a1 b)
		 a1: (b1 c)
		 b: (c1)

		 output a, then recurse on a1, a again, then on b
		 output self, then child, self, then child
		 -> aabaacabc add separators if at top
	*/
	char tag[0x100]; // err... 0x100 should be safe-ish.
	void indent(int level) {
		int i=0;
		char* buf = alloca(level);
		for(i=0;i<level;++i) {
			buf[i] = '-';
		}
		write(1,buf,level);
	}
	void writei(int i) {
		char buf[0x100];
		write(1,buf, snprintf(buf,0x100,"%d",i));
	}

	
	void dumptrie(struct trie* cur, int level) {
		if(!cur) return;
		indent(level);
		if(cur->c)
			write(1,&cur->c,1);
		else
			write(1,LITLEN("\\0"));
		write(1,"\n",1);
		int i;
		for(i=0;i<cur->nsubs;++i) {
			dumptrie(&cur->subs[i],level+1);
		}
	}
	dumptrie(&root,0);


	void dump_memcmp(char* dest, struct trie* cur, int level, int len) {
		if(cur->nsubs == 0) {
			indent(level);
			write(1,LITLEN("return1 "));
			write(1,tag,dest-tag);
			write(1,LITLEN(";\n"));
			return;
		}
		indent(level);
		write(1,LITLEN("if("));
		switch(len) {
		case 2:
			write(1,LITLEN("buf["));
			writei(level);
			write(1,LITLEN("] == '"));
			write(1,&cur->c,1);
			*dest++ = toupper(cur->c);
			write(1,LITLEN("')\n"));
			break;
		case 3:
			write(1,LITLEN("buf["));
			writei(level);
			write(1,LITLEN("] == '"));
			write(1,&cur->c,1);
			*dest++ = toupper(cur->c);
			write(1,LITLEN("' && buf["));
			writei(level+1);
			write(1,LITLEN("] == '"));
			write(1,&cur->subs[0].c,1);
			*dest++ = toupper(cur->subs[0].c);
			write(1,LITLEN("')\n"));
			break;
		default:
			write(1,LITLEN("0==strncmp(&buf["));
			writei(level);
			write(1,LITLEN("],\""));
			while(cur && cur->c) {
				write(1,&cur->c,1);
				*dest++ = toupper(cur->c);
				cur = &cur->subs[0];
			}
			write(1,LITLEN("\"))\n"));
		};
		indent(level+1);
		write(1,LITLEN("return2 "));
		write(1,tag,dest-tag);
		write(1,LITLEN(";\n"));
		indent(level);
		write(1,LITLEN("return UNKNOWN_TAG;\n"));
	}

	bool nobranches(struct trie* cur, int* len) {
		while(cur) {
			if(cur->nsubs > 1) return false;
			if(cur->nsubs == 0) return true;
			++*len;
			cur = &cur->subs[0];
		}
	}
	
	void dump_tag(char* dest, struct trie* cur, int level) {
		size_t i;
		indent(level);
		write(1,LITLEN("switch (buf["));
		writei(level);
		write(1,LITLEN("]) {\n"));

		for(i=0;i<cur->nsubs;++i) {
			char c = cur->subs[i].c;
			*dest = toupper(c);
			indent(level);
			write(1,LITLEN("case '"));
			if(c) {
				write(1,&c,1);
			} else {
				write(1,LITLEN("\\0"));
			}
			write(1,LITLEN("':\n"));
			if(!c) {
				indent(level+1);
				write(1,LITLEN("return "));
				write(1,tag,dest-tag);
				write(1,LITLEN(";\n"));
			} else if(cur->nsubs == 0 || cur->subs[i].nsubs == 0) {
				write(1,LITLEN("ehunno\n"));
			} else {
				int len = 0;
				if (nobranches(&cur->subs[i],&len)) {
					*dest = toupper(cur->subs[i].c);
					dump_memcmp(dest+1,&cur->subs[i].subs[0],level+1,len);
				} else {
					dump_tag(dest+1, &cur->subs[i],level+1);
				}
			}
		}
		indent(level);
		write(1,LITLEN("default:\n"));
		indent(level+1);
		write(1,LITLEN("return UNKNOWN_TAG\n"));
		indent(level);
		write(1,LITLEN("};\n"));
	}

	void dump_enum(char* dest, struct trie* cur) {
		int i = 0;
		for(;i<cur->nsubs;++i) {
			char c = cur->subs[i].c;
			if(c) {
				*dest = toupper(c);
				dump_enum(dest+1,&cur->subs[i]);
			} else {
				write(1,LITLEN(",\n\t"));
				write(1,tag,dest-tag);
			}
		}
	}
	
	
	write(1,LITLEN("enum wanted_tags {\n\tUNKNOWN_TAG"));
	dump_enum(tag, &root);
	write(1,LITLEN("\n};\n"));

	write(1,LITLEN("enum wanted_tags lookup_wanted(const char* tag) {\n"));
	dump_tag(tag, &root, 0);
	write(1,LITLEN("}\n"));
}
