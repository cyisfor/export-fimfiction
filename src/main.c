#include "wordcount.h"
#include "gui.h"
#include "string.h"
#include "html_when.h"

#include "wanted_tags.gen.h"

#include <libxml/parser.h>
#include <pcre.h>

FILE* output = NULL; // = open_memstream(...)

string story = {};
string author = {};
string title = {};

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
  static immutable(char)* oo;
  string o;
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
    ERROR("Bad index %s",i);
  }
}

void parse(xmlNode* cur, int listitem, int listlevel) {
	void pnext() {
		return parse(cur->next,listitem,listlevel);
	}
	void pkids() {
		return parse(cur->children, listordered, listlevel);
	}
	void dumbTag(string realname) {
		OUTLIT("[");
		OUTSTR(realname);
		OUTLIT("]");
		if(cur->children != NULL) {
			pkids();
			OUTLIT("[/");
			OUTSTR(realname);
			OUTLIT("]");
		}
	}
	void argTag(string realname, string arg) {
		OUTLIT("[");
		OUTSTR(realname);
		OUTLIT("=");
		OUTSTR(arg);
		OUTLIT("]");
		if(cur->children != NULL) {
			pkids();
			OUTLIT("[/");
			OUTSTR(realname);
			OUTLIT("]");
		}
	}

	switch(cur->type) {
	case XML_NODE_ELEMENT: {
		size_t len = strlen(cur->name);
#define IS(a) ((LITSIZ(a) == len) && (0 == memcmp(cur->name,a,LITSIZ(a))))
		if(IS("li")) {
			int i;
			for(i=0;i<level;++i) {
				OUTLIT("  ");
			}
			if(listitem >= 0) {
				OUTF("%d",++listitem);
				OUTLIT(") ");
			} else {
				OUTLIT("• ");
			}
			// still check children for sublists
		} else if(IS("ul")) {
			parse(cur->children,false,listlevel+1);
		} else if(IS("ol")) {
			parse(cur->children,true,listlevel+1);
		} 		
		switch(lookup_wanted(e.tag)) {
		case W_A: return argTag("url",findProp(cur,"href"));
		case W_CHAT: return dumbTag("quote");
		case W_I: return dumbTag("i");
		case W_B: return dumbTag("b");
		case W_U: return dumbTag("u");
		case W_S: return dumbTag("s");
		case W_HR: return dumbTag("hr");
		case W_BLOCKQUOTE: return dumbTag("quote");
		case W_FONT: return argTag("color",findProp(cur,"color"));
		case W_SMALL: return argTag("size","0.75em");
		case W_UL: return dolist!false();
		case W_OL: return dolist!true();
		case W_H3: return argTag("size","2em");
		case DIV: {
			if(0==strncmp(findProp(cur,"class"),LITLEN("spoiler"))) :
				return dumbTag("spoiler");
			return;
		}
		case W_ROOT:
		case W_P:
		case W_TITLE:
			// strip
			pkids();
			return pnext();
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
			return;
		default:
			warningf("Skipping tag %s",cur->name);
		}
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
