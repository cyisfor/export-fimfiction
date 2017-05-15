import wordcount;

import html.dom: createDocument;

import std.experimental.logger;

import std.algorithm;
import std.process;
import std.stdio;
import std.string;
import std.array;

extern (C) void guiLoop(const char*, void*, void*);
extern (C) void refreshRow(int, int, const char*, const char*, const char*);

Appender!string title;
Appender!string story;
Appender!string authorNotes;

Appender!string* dest = null;

void output(T)(T item) {
	tracef("POOT (%s)",item);
  dest.put(item);
}

extern (C) static void invoke(void* ptr, void* funcptr) {
  void delegate() dg;
  dg.funcptr = cast(void function())(funcptr);
  dg.ptr = ptr;
  dg();
}

extern(C) static immutable(char)* getContents(int i) {
  static immutable(char)* oo;
  string o;
  switch(i) {
  case 0:
    o = title.data;
    break;
  case 1:
    o = story.data;
    break;
  case 2:
    o = authorNotes.data;
    break;
  default:
    throw new Exception(format("Bad index %s",i));
  }

  infof("pasting string %d %s",i,o[0..min(20,$)]);

  oo = std.string.toStringz(o);
  return oo;
}

void process(NodeType)(ref NodeType e) {
	void pkids() {
		foreach(ref kid; e.children) {
			process(kid);
		}				
	}
	void dumbTag(string realname) {
		output("[" ~ realname ~ "]");
		pkids();
		output("[/" ~ realname ~ "]");
	}
	void argTag(string realname, string arg) {
		output("[" ~ realname ~ "=" ~ arg ~ "]");
		pkids();
		output("[/" ~ realname ~ "]");
	}

	void dolist(bool ordered) {
		foreach(ref kid; e.children) {
			if(ordered) output((++i).to!string() ~ ") ");
			else output("• ");
			foreach(ref kkid; kid.children) {
				// assert(kkid.name == "li")
				process(kkid);
			}
			output("\n");
		}
	}
	if(isTextNode(e)) {
		output(e.txt);
		return;
	} else if(isCommentNode(e)) {
		tracef("Comment stripped: %s",e.firstChild.text);
		return;
	}
	switch(e.tag) {
	case "a": return argTag("url",e.attr("href"));
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
			goto case;
		}
		goto case;
	case "p":
		// strip
		return pkids();
	case "img":
		auto src = img.attr("data-fimfiction-src");
		if(src is null) {
			src = img.attr("src");
			if(src is null) {
				warnf("Skipping sourceless image");
				return;
			}
		}
		output("[img]" ~ src ~ "[/img]");
		return;
	default:
		warnf("Skipping tag %s",e.tag);
		return;
	};
} 

alias word = wordcount;

void main(string[] args)
{
	import std.traits: EnumMembers;
	void setlvl() {
		auto lvl = environment.get("log");
		if(lvl !is null) {
			static foreach(i, member; EnumMembers!LogLevel) {
				if(member == lvl) {
					globalLogLevel = i;
					return;
				}
			}
			assert(false,"What log level is " ~ lvl);
		} else {
			globalLogLevel = LogLevel.info;
		}
	}
	setlvl();

  string source = args[1];

  void recalculate() {
		import html_when: process_when;
		import std.file: readText;
		auto doc = createDocument(readText(source));
		auto storyE = doc.root;
		process_when(storyE);
		
		auto titleE = story.find("title");

    //info("recalculating by loading file again.",dest);
    title = appender!string();
		if(titleE) {
			titleE.detach();
			dest = &title;
			process(titleE);
		}
		dest = &authorNotes;
    authorNotes= appender!string();
		foreach(ref authorNotesE; story.find_all("div.author")) {
			authorNotesE.detach();
			process(authorNotesE);
		}
		dest = &story;
		story= appender!string();
		process(storyE);
  
    if(title.data.length > 0) {
      auto titleS = strip(title.data);
      refreshRow(++i,
                 0,
                 "title",
                 std.string.toStringz(title.data),
                 std.string.toStringz(format("%d",word.count(titleS))));
    }
    if(story.data.length > 0) {
			auto storyS = strip(story.data);
      refreshRow(++i,
                 1,
                 "body",
                 std.string.toStringz(story.data[0..min(20,$)]),
                 std.string.toStringz(format("%d",word.count(storyS))));
    }
    if(authorNotes.data.length > 0) {
			auto authorS = strip(authorNotes.data)[0..min(20,$)];
      refreshRow(++i,
                 2,
                 "author",
                 std.string.toStringz(authorS),
                 std.string.toStringz(format("%d",word.count(authorS))));
    }
  }

  void delegate() rc = &recalculate;
  guiLoop(std.string.toStringz(dest),rc.ptr,cast(void*)(rc.funcptr));
}
