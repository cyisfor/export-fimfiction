#include "libxmlfixes.h"

#include "wordcount.h"
#include "gui.h"
#include "string.h"
#include "html_when.h"

#include "wanted_tags.gen.h"

#include <libxml/parser.h>
#include <pcre.h>
#include <error.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <error.h>
#include <ctype.h> // isspace

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
    error(23,0,"Bad index %s",i);
  }
}

void output_f(const char* s, int l) {
	int res = fwrite(s,l,1,output);
	assert(res == 1);
}
#define OUTLIT(a) output_f(a,sizeof(a)-1)
#define OUTSTR(a) output_f(a.s,a.l);
#define OUTF(a...) fprintf(output,a)						
#define OUTS output_f

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
		}
	}
#define argTag(a,arg) argTagderp(a,sizeof(a)-1,arg)
	
	switch(cur->type) {
	case XML_ELEMENT_NODE:
		switch(lookup_wanted(cur->name)) {
		case W_UL:
			parse(cur->children,-1,listlevel+1);
			break;
		case W_OL:
			parse(cur->children,0,listlevel+1);
			break;
		case W_LI: {
			int i;
			for(i=0;i<listlevel;++i) {
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
			break;
		}

		case W_A: argTag("url",findProp(cur,"href"));
			break;
		case W_CHAT:  dumbTag("quote");
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
		case W_DIV: {
			if(0==strncmp(findProp(cur,"class"),LITLEN("spoiler")))
				return dumbTag("spoiler");
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
			size_t noblank = 0;
			for(;noblank<len;++noblank) {
				if(!isspace(cur->content[noblank]))
					break;
			}
			if(noblank != len) {
				OUTS(cur->content+noblank,len-noblank);
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

	wordcount_setup();

	assert(argc > 1);
	const char* source = argv[1];

  void recalculate() {
		xmlDoc* doc = htmlReadFile(source, "UTF-8",
															 HTML_PARSE_RECOVER |
															 HTML_PARSE_NOERROR |
															 HTML_PARSE_NOBLANKS |
															 HTML_PARSE_COMPACT);

		xmlNode* storyE = (xmlNode*)doc;
		html_when(storyE);
		xmlNode* titleE = get_title(storyE);
		if(titleE) {
			xmlUnlinkNode(titleE);
			INFO("Found title %s",titleE->children->content);
			title.l = strlen(titleE->children->content);
			title.s = realloc(title.s, title.l);
			memcpy(title.s, titleE->children->content, title.l);
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
					if(val && 0==strncmp(val,LITLEN("author"))) {
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
		output = open_memstream(&story.s,&story.l);
		PARSE(storyE);
		fclose(output);
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

  guiLoop(source,recalculate);
}
