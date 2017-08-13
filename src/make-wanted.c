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
#include <fcntl.h> // open O_*

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

	// note, CANNOT sort by tag size, since same prefix = different sizes
	// would have to use "depth" parameter in traversal, to decide between
	// little or big suffixes first...
	int compare_nodes(struct trie* a, struct trie* b) {
		return a->c - b->c;
	}
	
	void sort_level(struct trie* cur) {
		if(cur->nsubs == 0) return;

		qsort(&cur->subs[0],cur->nsubs, sizeof(cur->subs[0]),(void*)compare_nodes);
		int i;
		for(i=0;i<cur->nsubs;++i) {
			sort_level(&cur->subs[i]);
		}
	}
	sort_level(&root);

	/* aab aac abc ->
		 a: (a1 b)
		 a1: (b1 c)
		 b: (c1)

		 output a, then recurse on a1, a again, then on b
		 output self, then child, self, then child
		 -> aabaacabc add separators if at top
	*/

	int fd = -1;

	void WRITE(const char* buf, size_t n) {
		ssize_t res = write(fd,buf,n);
		perror("umm");
		assert(res == n);
	}
	
	char tag[0x100]; // err... 0x100 should be safe-ish.
	void indent(int level) {
		int i=0;
		char* buf = alloca(level);
		for(i=0;i<=level;++i) {
			buf[i] = '\t';
		}
		WRITE(buf,level+1);
	}
	void writei(int i) {
		char buf[0x100];
		WRITE(buf, snprintf(buf,0x100,"%d",i));
	}

	
	void dumptrie(struct trie* cur, int level) {
		if(!cur) return;
		indent(level);
		if(cur->c)
			WRITE(&cur->c,1);
		else
			WRITE(LITLEN("\\0"));
		WRITE("\n",1);
		int i;
		for(i=0;i<cur->nsubs;++i) {
			dumptrie(&cur->subs[i],level+1);
		}
	}
	//dumptrie(&root,0);


	void dump_memcmp(char* dest, struct trie* cur, int level, int len) {
		if(cur->nsubs == 0) {
			indent(level);
			WRITE(LITLEN("return "));
			WRITE(tag,dest-tag);
			WRITE(LITLEN(";\n"));
			return;
		}
		indent(level);
		WRITE(LITLEN("if("));
		struct trie* place = cur;
		void oneshortcut(const char* prefix, size_t plen) {
			if(plen) WRITE(prefix,plen);
			WRITE(LITLEN("buf["));
			writei(level);
			WRITE(LITLEN("] == '"));
			WRITE(&place->c,1);
			*dest++ = toupper(place->c);
			WRITE(LITLEN("')\n"));
			place = &place->subs[0];
		}
		switch(len) {
		case 2:
			oneshortcut(NULL,0);
			break;
		case 3:
			oneshortcut(NULL,0);
			oneshortcut(LITLEN("' && "));
			WRITE(LITLEN("')\n"));
			break;
		default:
			WRITE(LITLEN("0==strncmp(&buf["));
			writei(level);
			WRITE(LITLEN("],\""));
			int num = 0;
			while(cur && cur->c) {
				WRITE(&cur->c,1);
				*dest++ = toupper(cur->c);
				++num;
				cur = &cur->subs[0];
			}
			WRITE(LITLEN("\", "));
			writei(num);
			WRITE(LITLEN("))\n"));
		};
		indent(level+1);
		WRITE(LITLEN("return "));
		WRITE(tag,dest-tag);
		WRITE(LITLEN(";\n"));
		indent(level);
		WRITE(LITLEN("return UNKNOWN_TAG;\n"));
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
		WRITE(LITLEN("switch (buf["));
		writei(level);
		WRITE(LITLEN("]) {\n"));

		for(i=0;i<cur->nsubs;++i) {
			char c = cur->subs[i].c;
			*dest = toupper(c);
			indent(level);
			WRITE(LITLEN("case '"));
			if(c) {
				WRITE(&c,1);
			} else {
				WRITE(LITLEN("\\0"));
			}
			WRITE(LITLEN("':\n"));
			if(!c) {
				indent(level+1);
				WRITE(LITLEN("return "));
				WRITE(tag,dest-tag);
				WRITE(LITLEN(";\n"));
			} else if(cur->nsubs == 0 || cur->subs[i].nsubs == 0) {
				WRITE(LITLEN("ehunno\n"));
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
		WRITE(LITLEN("default:\n"));
		indent(level+1);
		WRITE(LITLEN("return UNKNOWN_TAG\n"));
		indent(level);
		WRITE(LITLEN("};\n"));
	}

	void dump_enum(char* dest, struct trie* cur) {
		int i = 0;
		for(;i<cur->nsubs;++i) {
			char c = cur->subs[i].c;
			if(c) {
				*dest = toupper(c);
				dump_enum(dest+1,&cur->subs[i]);
			} else {
				WRITE(LITLEN(",\n\t"));
				WRITE(tag,dest-tag);
			}
		}
	}

	char tname[] = ".tmpXXXXXX";
	fd = mkstemp(tname);
	assert(fd >= 0);
	WRITE(LITLEN("enum wanted_tags {\n\tUNKNOWN_TAG"));
	dump_enum(tag, &root);
	WRITE(LITLEN("\n};\n"));
	WRITE(LITLEN("enum wanted_tags lookup_wanted(const char* tag);\n"));
	
	close(fd);
	rename(tname,"wanted_tags.gen.h");
	fd = open(tname,O_WRONLY|O_CREAT|O_TRUNC,0644);
	assert(fd >= 0);
	WRITE(LITLEN("#include \"wanted_tags.h\"\n"));
	WRITE(LITLEN("enum wanted_tags lookup_wanted(const char* tag) {\n"));
	dump_tag(tag, &root, 0);
	WRITE(LITLEN("}\n"));
	close(fd);
	rename(tname,"wanted_tags.gen.c");

}
