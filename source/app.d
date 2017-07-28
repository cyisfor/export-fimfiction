import wordcount;

import html.dom: createDocument;

import std.experimental.logger: info, infof, warningf, tracef;

import std.algorithm: min;
import std.process: environment;
import std.string: format, strip, toStringz;
import std.array: appender, Appender;

extern (C) void guiLoop(const char*, void*, void*);
extern (C) void refreshRow(int, int, const char*, const char*, const char*);

Appender!string dest;

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

string storyS = null;
string authorS = null;
string titleS = null;

import std.regex: regex, Captures, replaceAll;

auto dentpat = regex("&([^;\\s&]+);");

T deEntitize(T)(T inp) {
	T replace(Captures!T m) {
		switch(m[1]) {
		case "amp":
			return "&";
		case "lt":
			return "<";
		case "gt":
			return ">";
		default:
			throw new Exception("What is " ~ m[1] ~ "?");
		}
	}
	return dentpat.replaceAll!replace(inp);
}

extern(C) static immutable(char)* getContents(int i) {
  static immutable(char)* oo;
  string o;
  switch(i) {
  case 0:
    o = titleS;
    break;
  case 1:
    o = storyS;
    break;
  case 2:
    o = authorS;
    break;
  default:
    throw new Exception(format("Bad index %s",i));
  }

  infof("pasting string %d %s",i,o[0..min(20,$)]);

  oo = toStringz(o);
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
		if(e.firstChild !is null) {
			pkids();
			output("[/" ~ realname ~ "]");
		}
	}
	void argTag(T)(string realname, T arg) {
		output("[" ~ realname ~ "=" ~ arg ~ "]");
		pkids();
		output("[/" ~ realname ~ "]");
	}

	void dolist(bool ordered)() {
		static if(ordered) {
			int i = 0;
		}
		foreach(ref kid; e.children) {
			import std.conv: to;
			static if(ordered) output((++i).to!string() ~ ") ");
			else output("• ");
			auto savedest = dest;
			dest = appender!string();
			foreach(ref kkid; kid.children) {
				// assert(kkid.name == "li")
				process(kkid);
			}
			auto item = dest.data;
			dest = savedest;
			output(item.strip());
			output("\n");
		}
	}
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
