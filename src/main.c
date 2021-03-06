#include "libxmlfixes.h"

#include "wordcount.h"
#include "gui.h"
#include "mystring.h"

#include "wanted_tags.gen.h"

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/HTMLparser.h>

#include <sys/mman.h> // mmap
#include <sys/stat.h>

#include <pcre.h>
#include <error.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <error.h>
#include <ctype.h> // isspace
#include <unistd.h> // isatty
#include <stdlib.h> // malloc

// meh
#define WARN(a...) error(0,0,"warning: " a)
#define INFO(a...) error(0,0,"info: " a)

#define max(a,b) ((a) < (b) ? (b) : (a))
#define min(a,b) ((a) > (b) ? (b) : (a))

FILE* output = NULL; // = open_memstream(...)

mstring story = {};
mstring author = {};
mstring title = {};

pcre* dentpat = NULL; // = regex("&([^;\\s&]+);");

static xmlNode* get_title(xmlNode* top) {
	if(!top) return NULL;
	switch(top->type) {
	case XML_ELEMENT_NODE:
		if(lookup_wanted(top->name) == W_TITLE) {
			return top;
		}
		// fall through
	case XML_DOCUMENT_NODE:
	case XML_HTML_DOCUMENT_NODE: // libxml!!!
	{
		xmlNode* found = get_title(top->children);
		if(found) return found;
	}
	};

	return get_title(top->next);
}

/* T deEntitize(T)(T inp) { */
/* 	T replace(Captures!T m) { */
/* 		auto res = getEntityUTF8(m[1]); */
/* 		if(res == null) { */
/* 			throw new Exception(("What is " ~ m[1] ~ "?").to!string); */
/* 		} */
/* 		return res; */
/* 	} */
/* 	return replaceAll!replace(inp,dentpat); */
/* } */

const string getContents(int i) {
  switch(i) {
  case 0:
    return *((const string*)&title);
    break;
  case 1:
		return *((const string*)&story);
    break;
  case 2:
		return *((const string*)&author);
    break;
  default:
    error(23,0,"Bad index %d",i);
  }
}

static bool whitestuff(const mstring cur) {
	gunichar c = g_utf8_get_char(cur.s);
	if(g_unichar_isspace(c)) return true;
	if(!g_unichar_isgraph(c)) return true;
	if(!c) return true;
	if(c==-1) return true;
	return false;
}

static void next_char(mstring* cur) {
	char* next = g_utf8_next_char(cur->s);
	assert(next);
	cur->l += (next - cur->s);
	cur->s = next;
}

static bool prev_char(const mstring cur, mstring* tail) {
	while(tail->l < cur.l) {
		--tail->s;
		++tail->l;
		if(*tail->s < 0x80 || (*tail->s > 0xc1 && *tail->s < 0xfe)) {
			return true;
		}
	}
	return false;
}

static void trim(mstring* base) {
	if(base->l == 0) return;
	mstring cur = {base->s, base->l};
	if(cur.l == 1) {
		/* XXX: whole string is validated earlier */
		if(whitestuff(cur)) {
			base->s[0] = '\0';
			base->l = 0;
		}
		return;
	}

	while(cur.l > 0 && whitestuff(cur)) {
		next_char(&cur);
	}
	while(cur.l > 0 && whitestuff(cur)) {
		next_char(&cur);
	}
	mstring tail = {cur.s+cur.l, 0};
	for(;;) {
		if(false == prev_char(cur, &tail)) {
			base->s[0] = '\0';
			base->l = 0;
			return;
		}
		if(!whitestuff(tail)) break;
	}

	const mstring nowhite = {
		.s = cur.s,
		.l = cur.l - tail.l
	};
	if(nowhite.l == 0) {
		base->s[0] = '\0';
		base->l = 0;
	} else if(nowhite.s > base->s) {
		if(nowhite.s - base->s > nowhite.l) {
			memcpy(base->s, nowhite.s, nowhite.l);
		} else {
			memmove(base->s, nowhite.s, nowhite.l);
		}
		base->l = nowhite.l;
		base->s[base->l] = '\0';
	} else if(base->l == nowhite.l + 1) {
	} else {
		base->l = nowhite.l;
		base->s[nowhite.l+1] = '\0';
	}
}

enum { NONE, NEEDASPACE, NEEDNL } needs = NONE;

void output_f(const char* s, int l) {
	switch(needs) {
	case NEEDASPACE:
		fputc(' ',output);
		needs = NONE;
		break;
	case NEEDNL:
		fputc('\n',output);
		needs = NONE;
		break;
	};
	if(!l) return;
	int res = fwrite(s,l,1,output);
	assert(res == 1);
}
#define OUTLIT(a) output_f(a,sizeof(a)-1)
#define OUTSTR(a) output_f(a.s,a.l);
#define OUTF(a...) fprintf(output,a)
#define OUTS output_f

bool remove_blank = true;

bool doublespace = false;

void parse_text(const char* text, int len) {
	if(!doublespace) {
		OUTS(text, len);
		return;
	}
	int i;
	for(i=0;i<len;++i) {
		switch(text[i]) {
		case '\n':
			fwrite("\n\n",2,1,output);
			break;
		case '\\':
			fwrite(text+i,2,1,output);
			++i;
		default:
			fputc(text[i], output);
		}
	}
	OUTS(NULL, 0);
}

void parse(xmlNode* cur, int listitem, int listlevel) {
	if(!cur) return;

	void pkids() {
		return parse(cur->children, listitem, listlevel);
	}
	void dumbTagderp(const char* realname, size_t len) {
		OUTLIT("[");
		OUTS(realname,len);
		OUTLIT("]");
		if(cur->children != NULL) {
			pkids();
			OUTLIT("[/");
			OUTS(realname,len);
			OUTLIT("]");
			needs = NEEDASPACE;
		}
	}
#define dumbTag(a) dumbTagderp(a,sizeof(a)-1)
	void argTagderp(const char* realname, size_t rlen, const char* arg) {
		if(!arg) return;
		size_t alen = strlen(arg);
		OUTLIT("[");
		OUTS(realname,rlen);
		OUTLIT("=");
		OUTS(arg,alen);
		OUTLIT("]");
		if(cur->children != NULL) {
			pkids();
			OUTLIT("[/");
			OUTS(realname,rlen);
			OUTLIT("]");
			needs = NEEDASPACE;
		}
	}
#define argTag(a,arg) argTagderp(a,sizeof(a)-1,arg)

	switch(cur->type) {
	case XML_ELEMENT_NODE:
		switch(lookup_wanted(cur->name)) {
		case W_UL:
			parse(cur->children,-1,listlevel+1);
			OUTLIT("\n");
			break;
		case W_OL:
			parse(cur->children,0,listlevel+1);
			OUTLIT("\n");
			break;
		case W_LI: {
			int i;
			for(i=1;i<listlevel;++i) {
				OUTLIT("  ");
			}
			if(listitem >= 0) {
				OUTF("%d",++listitem);
				OUTLIT(") ");
			} else {
				OUTLIT("• ");
			}
			// still check children for sublists
			pkids();
			needs = NEEDNL;
			break;
		}

		case W_A: argTag("url",findProp(cur,"href"));
			break;
		case W_CHAT:
			doublespace = true;
			dumbTag("quote");
			doublespace = false;
			break;
		case W_I: dumbTag("i");
			break;
		case W_B: dumbTag("b");
			break;
		case W_U: dumbTag("u");
			break;
		case W_S: dumbTag("s");
			break;
		case W_HR: dumbTag("hr");
			break;
		case W_BLOCKQUOTE: dumbTag("quote");
			break;
		case W_FONT: argTag("color",findProp(cur,"color"));
			break;
		case W_SMALL: argTag("size","0.75em");
			break;
		case W_H3: argTag("size","2em");
			break;
		case W_DIV:
		case W_SPAN:
			if(ISLIT(findProp(cur,"class"),"spoiler")) {
				dumbTag("spoiler");
			}
			break;
		case W_ROOT:
		case W_P:
		case W_HTML:
		case W_HEAD:
		case W_BODY:
		case W_TITLE:
			// strip silently
			pkids();
			break;
		case W_IMG: {
			const char* src = findProp(cur,"data-fimfiction-src");
			if(src == NULL) {
				src = findProp(cur,"src");
				if(src == NULL) {
					WARN("Skipping sourceless image");
					return;
				}
			}
			OUTLIT("[img]");
			OUTS(src, strlen(src));
			OUTLIT("[/img]");
		}
			break;
		default:
			WARN("Stripping tag %s",cur->name);
			pkids();
		}
		break;
	case XML_DOCUMENT_NODE:
	case XML_HTML_DOCUMENT_NODE: // libxml!!!
		pkids();
		break;
	case XML_COMMENT_NODE:
		WARN("comment stripped %s",cur->content);
		break;
	case XML_TEXT_NODE:
		if(cur->content) {
			size_t len = strlen(cur->content);
			if(remove_blank) {
				size_t noblank = 0;
				for(;noblank<len;++noblank) {
					if(!isspace(cur->content[noblank]))
						break;
				}
				if(noblank != len) {
					parse_text(cur->content+noblank,len-noblank);
				}
				remove_blank = false;
			} else {
				parse_text(cur->content,strlen(cur->content));
			}
		}
		break;
	};
	return parse(cur->next,listitem,listlevel);
}

#define PARSE(a) parse(a,-1,0)

void on_error(void * userData, xmlErrorPtr error) {
	if(error->code == XML_HTML_UNKNOWN_TAG) {
		const char* name = error->str1;
		if(lookup_wanted(name) != UNKNOWN_TAG) return;
	}
	fprintf(stderr,"xml error %s %s\n",error->message,
			error->level == XML_ERR_FATAL ? "fatal..." : "ok");
}

/* C sucks */
struct getdoc_closure {
	const char* path;
	xmlDoc* doc;
	char* mem;
	size_t size;
	bool mmap;
};

xmlDoc* getdoc(struct getdoc_closure* c) {
	if(!c->doc)  {
		c->doc = htmlReadFile(c->path, "UTF-8",
							  HTML_PARSE_RECOVER |
							  HTML_PARSE_NOERROR |
							  HTML_PARSE_NOBLANKS |
							  HTML_PARSE_COMPACT);
	}
}

void gui_recalculate(void* ctx) {
	xmlDoc* doc = getdoc((struct getdoc_closure*)ctx);
	xmlNode* storyE = (xmlNode*)doc;
	xmlNode* titleE = get_title(storyE);
	if(titleE) {
		xmlUnlinkNode(titleE);
		INFO("Found title %s",titleE->children->content);
		title.l = strlen(titleE->children->content);
		// stupid GTK requires null termination
		title.s = realloc(title.s, title.l+1);
		memcpy(title.s, titleE->children->content, title.l);
		trim(&title);
		title.s[title.l] = '\0';
		xmlFreeNode(titleE);
	}
	free(author.s);
	author.s = NULL;
	author.l = 0;
	if(output) fclose(output);
	output = open_memstream(&author.s,&author.l);
	void find_notes(xmlNode* cur) {
		if(!cur) return;
		switch(cur->type) {
		case XML_ELEMENT_NODE:
			if(lookup_wanted(cur->name) == W_DIV) {
				xmlChar* val = findProp(cur,"class");
				INFO("vclass %s",val);
				if(val && ISLIT(val,"author")) {
					PARSE(cur->children);
					xmlNode* next = cur->next;
					xmlUnlinkNode(cur);
					xmlFreeNode(cur);
					return find_notes(next);
				}
			}
		case XML_DOCUMENT_NODE:
		case XML_HTML_DOCUMENT_NODE: // libxml!!!
			find_notes(cur->children);
		default:
			return find_notes(cur->next);
		};
	}
	find_notes(storyE);
	fclose(output);
	trim(&author);

	output = open_memstream(&story.s,&story.l);
	remove_blank = true;
	PARSE(storyE);
	fputc('\0',output);
	fclose(output);
	trim(&story);
	output = NULL;
	int i = 0;
	size_t wid = 20;
    if(title.s != NULL && title.l > 0) {
		wid = max(title.l,wid); // the width of the title but not less than 20
		// GTK sucks, by the way, so let's waste more cycles copying data again.
		size_t swid = min(wid,title.l);
		char* summ = alloca(swid+1);
		memcpy(summ,title.s,swid);
		summ[swid] = 0;
		refreshRow(++i,
				   0,
				   "title",
				   summ,
				   word_count(title.s,title.l));
    }
    if(story.l > 0) {
		size_t swid = min(wid,story.l);
		char* summ = alloca(swid+1);
		memcpy(summ,story.s,swid);
		summ[swid] = 0;
		refreshRow(++i,
				   1,
				   "body",
				   summ,
				   word_count(story.s,story.l));
    }
    if(author.s != NULL && author.l > 0) {
		size_t swid = min(wid,author.l);
		char* summ = alloca(swid+1);
		memcpy(summ,author.s,swid);
		summ[swid] = 0;
		refreshRow(++i,
				   2,
				   "author",
				   summ,
				   word_count(author.s,author.l));
    }
}

void main(int argc, char** argv) {
	// fimfiction is dangerous, so default to censored
	if(NULL==getenv("uncensored")) {
		setenv("censored","1",1);
	}

	LIBXML_TEST_VERSION;

	xmlSetStructuredErrorFunc(NULL,on_error);

	const char* path = NULL;

	wordcount_setup();

	struct getdoc_closure getdoc = {};
	/* XXX: make sure these readers validate that this is UTF-8! */
	if(argc > 1) {
		getdoc.path = argv[1];
	} else {
		void setit(void) {
			struct stat info;
			if(0==fstat(STDIN_FILENO,&info)) {
				getdoc.mem = mmap(NULL,
								  info.st_size,PROT_READ,
								  MAP_PRIVATE,STDIN_FILENO,
								  0);
				if(getdoc.mem != MAP_FAILED) {
					getdoc.size = info.st_size;
					getdoc.mmap = true;
					return;
				}
				// it's a pipe, I guess?
			}
			if(isatty(STDIN_FILENO)) {
				WARN("You probably should supply an argument if you aren't redirecting a file or piping a command to stdin.");
				exit(23);
			}
			getdoc.mem = malloc(0x1000);
			size_t size = 0x1000;
			for(;;) {
				if(size-getdoc.size < 0x100) {
					size += 0x1000;
					getdoc.mem = realloc(getdoc.mem,size);
				}
				ssize_t amt = read(STDIN_FILENO,getdoc.mem+getdoc.size,
								   size-getdoc.size);
				if(amt == 0) {
					getdoc.mem = realloc(getdoc.mem,getdoc.size);
					return;
				}
				if(amt < 0) {
					perror("read failed?");
					abort();
				}
				getdoc.size += amt;
			}
		}
		setit();

		getdoc.doc = htmlReadMemory(getdoc.mem,
									getdoc.size,
									"http://nothing.nowhere",
									"UTF-8",
									HTML_PARSE_RECOVER |
									HTML_PARSE_NOERROR |
									HTML_PARSE_NOBLANKS |
									HTML_PARSE_COMPACT);
		assert(getdoc.doc);
		/* it can never reload though :/ */
	}
	guiLoop(path, &getdoc);
	if(getdoc.doc) {
		xmlFreeDoc(getdoc.doc);
		if(getdoc.mmap) {
			munmap(getdoc.mem, getdoc.size);
		} else if(getdoc.mem) {
			g_free(getdoc.mem);
		}
	}
}
