#include "libxmlfixes.h"

#include "wordcount.h"
#include "gui.h"
#include "mystring.h"
#include "html_when.h"

#include "wanted_tags.gen.h"

#include <libxml/parser.h>
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

static bool whitestuff(char c) {
	if(isspace(c)) return true;
	if(!isgraph(c)) return true;
	return false;
}
	

static void trim(mstring* s) {
	if(s->l == 0) return;
	if(s->l == 1) {
		if(whitestuff(s->s[0])) {
			s->s[0] = '\0';
			s->l = 0;
		}
		return;
	}
	
	int start = 0;
	while(start < s->l && whitestuff(s->s[start])) ++start;
	int end = s->l - 1;
	while(end > start && whitestuff(s->s[end])) --end;
	if(start == end) {
		s->s[0] = '\0';
		s->l = 0;
	} else if(start > 0) {
		memmove(s->s, s->s + start, end - start + 1);
		s->l = end - start + 2;
		s->s[end - start + 1] = '\0';
	} else if(s->l - 1 == end) {
	} else {
		s->l = end + 1;
		s->s[end] = '\0';
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

void main(int argc, char** argv) {
	// fimfiction is dangerous, so default to censored
	if(NULL==getenv("uncensored")) {
		setenv("censored","1",1);
	}

	LIBXML_TEST_VERSION;

	void on_error(void * userData, xmlErrorPtr error) {
		if(html_when_handled_error(error)) return;
		if(error->code == XML_HTML_UNKNOWN_TAG) {
			const char* name = error->str1;
			if(lookup_wanted(name) != UNKNOWN_TAG) return;
		}
		fprintf(stderr,"xml error %s %s\n",error->message,
						error->level == XML_ERR_FATAL ? "fatal..." : "ok");
	}
	xmlSetStructuredErrorFunc(NULL,on_error);

	const char* path = NULL;

	wordcount_setup();

	xmlDoc* (*getdoc)(void);
	if(argc > 1) {
		path = argv[1];
		xmlDoc* getdoc_arg(void) {
			return htmlReadFile(path, "UTF-8",
													HTML_PARSE_RECOVER |
													HTML_PARSE_NOERROR |
													HTML_PARSE_NOBLANKS |
													HTML_PARSE_COMPACT);
		}
		getdoc = getdoc_arg;
	} else {
		char* mem;
		off_t size;

		xmlDoc* getdoc_mem(void) {
			return htmlReadMemory(mem,size,"http://nothing.nowhere","UTF-8",
														HTML_PARSE_RECOVER |
														HTML_PARSE_NOERROR |
														HTML_PARSE_NOBLANKS |
														HTML_PARSE_COMPACT);
		}
		getdoc = getdoc_mem;
		void setit(void) {
			struct stat info;
			if(0==fstat(STDIN_FILENO,&info)) {
				mem = mmap(NULL,info.st_size,PROT_READ,MAP_PRIVATE,STDIN_FILENO,0);
				if(mem != MAP_FAILED) {
					size = info.st_size;
					return;
				}
				// it's a pipe, I guess?
			}
			if(isatty(STDIN_FILENO)) {
				WARN("You probably should supply an argument if you aren't redirecting a file or piping a command to stdin.");
				exit(23);
			}
			mem = malloc(0x1000);
			size = 0x1000;
			off_t off = 0;
			for(;;) {
				if(size-off < 0x100) {
					size += 0x1000;
					mem = realloc(mem,size);
				}
				ssize_t amt = read(STDIN_FILENO,mem+off,size-off);
				if(amt == 0) {
					size = off;
					mem = realloc(mem,size);
					return;
				}
				if(amt < 0) {
					perror("read failed?");
					abort();
				}
				off += amt;
			}
		}
		setit();
	}
			


  void recalculate() {
		xmlDoc* doc = getdoc();

		xmlNode* storyE = (xmlNode*)doc;
		html_when(storyE);
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

  guiLoop(path,recalculate);
}
