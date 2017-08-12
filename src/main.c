#include "wordcount.h"
#include "gui.h"
#include "string.h"
#include "html_when.h"

#include <libxml/parser.h>
#include <pcre.h>

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

enum wanted_tags { A

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
	}
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
	};
	if(e.isTextNode()) {
		output(deEntitize(e.text));
		return;
	} else if(e.isCommentNode()) {
		tracef("Comment stripped: %s",e.firstChild.text);
		return;
	}
	switch(e.tag) {
	case "a": return argTag("url",e.attr("href"));
  case "chat": return dumbTag("quote");
	case "i": return dumbTag("i");
	case "b": return dumbTag("b");
	case "u": return dumbTag("u");
	case "s": return dumbTag("s");
	case "hr": return dumbTag("hr");
	case "blockquote": return dumbTag("quote");
	case "font": return argTag("color",e.attr("color"));
	case "small": return argTag("size","0.75em");
	case "ul": return dolist!false();
	case "ol": return dolist!true();
	case "h3": return argTag("size","2em");
	case "div":
		switch(e.attr("class")) {
		case "spoiler":
			return dumbTag("spoiler");
		default:
			break;
		}
		goto case;
	case "root":
	case "p":
	case "title":
		// strip
		return pkids();
	case "img":
		auto src = e.attr("data-fimfiction-src");
		if(src is null) {
			src = e.attr("src");
			if(src is null) {
				warningf("Skipping sourceless image");
				return;
			}
		}
		output("[img]" ~ src ~ "[/img]");
		return;
	default:
		warningf("Skipping tag %s",e.tag);
		return;
	}
} 

alias word = wordcount;

void main(string[] args)
{
	import std.traits: EnumMembers;
	void setlvl() {
		import std.experimental.logger: LogLevel, globalLogLevel;
		auto lvl = environment.get("log");
		if(lvl !is null) {
			foreach(member; __traits(allMembers,LogLevel)) {
				if(member == lvl) {
					globalLogLevel = __traits(getMember, LogLevel, member);
					return;
				}
			}
			assert(false,"What log level is " ~ lvl);
		} else {
			globalLogLevel = LogLevel.info;
		}
	}
	setlvl();

	// fimfiction is dangerous, so default to censored
	if(environment.get("uncensored") is null) {
		environment["censored"] = "fimfiction sux";
	}

  string source = args[1];

  void recalculate() {
		import html_when: process_when;
		import std.file: readText;
		auto doc = createDocument(readText(source));
		auto storyE = doc.root;
		process_when(storyE);
		auto titleE = storyE.find("title");
    //info("recalculating by loading file again.",dest);
		if(!titleE.empty) {
			import std.conv: to;
			auto e = titleE.front;
			e.detach();
			info("Found title ",e.text);
			titleE.popFront();
			assert(titleE.empty());
			dest = appender!string();
			process(e);
			titleS = dest.data;
		}
		authorS = null;
		dest = appender!string();
		foreach(ref authorNotesE; storyE.find("div.author")) {
			authorNotesE.detach();
			process(authorNotesE);
		}
		authorS = strip(dest.data);

		dest = appender!string();
		process(storyE);
		storyS = strip(dest.data);

		int i = 0;
		ulong wid = 20;
    if(titleS !is null && titleS.length > 0) {
			import std.algorithm: max;
			wid = max(titleS.length,wid);
      refreshRow(++i,
                 0,
                 "title",
                 toStringz(titleS),
                 toStringz(format("%d",word.count(titleS))));
    }
		info("WID",wid);
    if(storyS.length > 0) {
			auto summ = storyS[0..min(wid,$)];
      refreshRow(++i,
                 1,
                 "body",
                 toStringz(summ),
                 toStringz(format("%d",word.count(storyS))));
    }
    if(authorS !is null && authorS.length > 0) {
			auto summ = authorS[0..min(wid,$)];
      refreshRow(++i,
                 2,
                 "author",
                 toStringz(summ),
                 toStringz(format("%d",word.count(authorS))));
    }
  }

  void delegate() rc = &recalculate;
  guiLoop(toStringz(source),rc.ptr,cast(void*)(rc.funcptr));
}
