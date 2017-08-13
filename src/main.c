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

FILE* output = NULL; // = open_memstream(...)

mstring story = {};
mstring author = {};
mstring title = {};

pcre* dentpat = NULL; // = regex("&([^;\\s&]+);");

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
	fwrite(s,l,1,output);
}
#define OUTLIT(a) output_f(a,sizeof(a)-1)
#define OUTSTR(a) output_f(a.s,a.l);
#define OUTF(a...) fprintf(output,a)						
#define OUTS output_f

void parse(xmlNode* cur, int listitem, int listlevel) {
	void pnext() {
		return parse(cur->next,listitem,listlevel);
	}
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
	case XML_ELEMENT_NODE: {
		size_t len = strlen(cur->name);
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
		case DIV: {
			if(0==strncmp(findProp(cur,"class"),LITLEN("spoiler"))) :
				return dumbTag("spoiler");
		}
			break;
		case W_ROOT:
		case W_P:
		case W_TITLE:
			// strip
			pkids();
			break;
		case W_IMG:
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
			break;
		default:
			warningf("Skipping tag %s",cur->name);
		}
		return pnext();
	}
	};
	// fall-through
	case XML_NODE_DOCUMENT:
		pkids();
		return pnext();
	case XML_NODE_COMMENT:
		INFO("comment stripped %s",cur->children->content);
		return pnext();
	case XML_NODE_TEXT:
		parse_text(cur->children);
		return pnext();
	case XML_NODE_COMMENT:
		warn("Comment stripped: %s",cur->children->content);
		return pnext();
}

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
															 HTML_PARSE_NOIMPLIED);
		xmlNode* storyE = (xmlNode*)doc;
		html_when(storyE);
		xmlNode* titleE = get_title(storyE);
		if(titleE) {
			info("Found title %s",titleE->children->content);
			title.s = titleE->children->content;
			// strdup?
			title.l = strlen(title.s);
		}
		free(author.s);
		author.s = NULL;
		author.l = 0;
		dest = open_memstream(&author.s,&author.l);
		void find_notes(xmlNode* cur) {
			switch(cur->type) {
			case XML_ELEMENT_NODE:
				if(lookup_wanted(cur->name) == DIV) {
					xmlChar* val = findProp(cur,"class");
					if(val && 0==strncmp(val,LITLEN("author"))) {
						process(cur);
						xmlNode* next = cur->next;
						xmlUnlinkNode(cur);
						xmlFreeNode(cur);
						return find_notes(next);
					}
				}
			case XML_DOCUMENT_NODE:
				find_notes(cur->children);
			default:
				return find_notes(cur->next);
			};
		}
		find_notes(storyE);

		dest = open_memstream(&story.s,&story.l);		
		process(storyE);
		
		int i = 0;
		size_t wid = 20;
    if(title.s != NULL && title.l > 0) {
			wid = max(title.l,wid);
      refreshRow(++i,
                 0,
                 "title",
								 title.s,
								 title.l,
								 word_count(title.s,title.l));
    }
    if(story.l > 0) {
			char* summ = alloca(wid+1);
			memcpy(summ,story.s,min(wid,story.l));
			summ[wid] = 0;
      refreshRow(++i,
                 1,
                 "body",
                 summ,
								 word_count(story.s,story.l));
    }
    if(author.s != NULL && author.l > 0) {
			char* summ = alloca(wid+1);
			memcpy(summ,story.s,min(wid,story.l));
			summ[wid] = 0;
      refreshRow(++i,
                 2,
                 "author",
                 summ,
                 word_count(author.s,author.l));
    }
  }

  guiLoop(source,recalculate);
}
