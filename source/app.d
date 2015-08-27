import wordcount;
import declassifier;

import html.parser;

import std.experimental.logger;

import std.algorithm;
import std.process;
import std.stdio;
import std.string;
import std.array;

extern (C) void guiLoop(const char*, void*, void*);
extern (C) void refreshRow(int, int, const char*, const char*, const char*);

alias E = declassifier.Event;

Appender!string title;
Appender!string story;
Appender!string authorNotes;

Appender!string* dest = null;

void output(T)(T item) {
  dest.put(item);
}

class Handler : declassifier.Handler {
  Handler parent;
  this(Handler parent) {
    this.parent = parent;
  }
  void handle(E event) {
    //uhm...
  }
  void onEntity(const(char)[] data, const(char)[] decoded) {
    output(decoded);
  }
  void handle(E event, const(char)[] data) {
    switch(event) {
    case E.CDATA:
      goto case;
    case E.Text:
      trace("yay text");
      output(data);
      break;
    default:
      warningf("What is this event? %s %s",declassifier.names[event],data);
    }
  }
}

class AttrFinder : Handler {
  string wanted;
  bool found;
  string value;
  this(Handler parent, string wanted) {
    super(parent);
    this.wanted = wanted;
  }
  override void handle(E event) {
    switch(event) {
    case E.SelfClosing:
      assert(this.found);
      this.writeStart();
      this.writeEnd();
      break;
    default:
      return super.handle(event);
    }
  }

  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.AttrName:
      this.found = data == this.wanted;
      break;
    case E.AttrValue:
      if(this.found) {
        this.value = cast(string)(data);
      }
      break;
    case E.OpenEnd:
      assert(this.found);
      this.writeStart();
      break;
    case E.Close:
      this.writeEnd();
      break;
    default:
      return super.handle(event,data);
    }
  }

  abstract void writeStart();
  abstract void writeEnd();
}

class LinkHandler : AttrFinder {
  this(Handler parent) {
    super(parent,"href");
  }
  override void writeStart() {
    output("[url=" ~ this.value ~ "]");
  }
  override void writeEnd() {
    output("[/url]");
  }
}

class ImageHandler : AttrFinder {
  // images inside links in fimfiction are very screwy
  static bool commentMode = false;

  this(Handler parent) {
    super(parent,"src");
  }
  override void writeStart() {
    output("[img]" ~ this.value ~ "[/img]");
    if(this.commentMode) {
      LinkHandler link = cast(LinkHandler)(this.parent);
      if(link) {
        output("\n" ~ link.value);
      }
    }
  }
  override void writeEnd() {
  }
}

class FontHandler : AttrFinder {
  this(Handler parent) {
    super(parent,"color");
  }
  override void writeStart() {
    output("[color=" ~ this.value ~ "]");
  }
  override void writeEnd() {
    output("[/color]");
  }
}
class SmallHandler : Handler {
  this(Handler parent) {
    super(parent);
  }
  override void handle(E event, const(char)[] data) {
    infof("small handler %s",event);
    switch(event) {
    case E.OpenEnd:
      output("[size=0.75em]");
      break;
    case E.Close:
      output("[/size]");
      break;
    default:
      return super.handle(event,data);
    }
  }
}

class TitleHandler : Handler {
  this(Handler parent) {
    super(parent);
  }
  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.OpenEnd:
      dest = &title;
      break;
    case E.Close:
      dest = &story;
      break;
    default:
      return super.handle(event,data);
    }
  }
}


class DivHandler : Handler {
  bool found;
  bool classboo;
  bool authorial;
  string value;
  this(Handler parent) {
    super(parent);
  }
  
  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.AttrName:
      this.found = (data == "spoiler");
      if(!this.found) {
        this.classboo = data == "class";
      }
      break;
    case E.AttrValue:
      if(!this.found && this.classboo) {
        // TODO: data contains the word spoiler...
        this.found = data == "spoiler";
        if(!this.found) {
          this.authorial = data == "author";
        }
      }
      break;
    case E.OpenEnd:
      if(this.found) {
        output("[spoiler]");
      }
      if(this.authorial) {
        dest = &authorNotes;
      }
      break;
    case E.Close:
      if(this.found) {
        output("[/spoiler]");
      }
      if(this.authorial) {
        dest = &story;
      }
      break;
    default:
      return super.handle(event,data);
    }
  }
}

class DumbTag : Handler {
  string name;
  this(Handler parent, string name) {
    super(parent);
    this.name = name;
  }

  override void handle(E event) {
    switch(event) {
    case E.SelfClosing:
      this._output();
      break;
    default:
      return super.handle(event);
    }
  }

  void _output() {
    output("[" ~ this.name ~ "]");
  }
  
  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.OpenEnd:            
      this._output();
      break;
    case E.Close:
      output("[/" ~ this.name ~ "]");
      break;
    default:
      return super.handle(event,data);
    }
  }
}

class ListHandler : Handler {
  bool numeric = false;
  int number = 1;
  this(Handler parent, bool numeric) {
    super(parent);
    this.numeric = numeric;
  }
}

const wchar bullet = '•';

class ListItemHandler : Handler {
  this(Handler parent) {
    super(parent);
  }

  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.OpenEnd:
      ListHandler derp = cast(ListHandler)(parent);
      if(derp.numeric) {
        output(format("%d",derp.number++));
        output(") ");
      } else {
        // XXX: TODO: nested lists have clear bullets?
        output(bullet);
        output(' ');
      }
      break;
    case E.Close:
      output('\n');
      break;
    default:
      return super.handle(event,data);
    }
  }
}

class H3Handler : Handler {
  this(Handler parent) {
    super(parent);
  }
  
  override void handle(E event, const(char)[] data) {
    switch(event) {
    case E.OpenEnd:
      output("[size=2em]");
      break;
    case E.Close:
      output("[/size]");
      break;
    default:
      return super.handle(event,data);
    }
  }
}

template Construct(T) {
  Handler derp(Handler parent) {
    return new T(parent);
  }
  Handler function(Handler) Construct() {
    return &derp;
  }
}

class Nada : Handler {
  this(Handler parent) {
    super(parent);
  }
  override void handle(E event, const(char)[] data) { }
  override void handle(E event) { }
  override void onEntity(const(char)[] data, const(char)[] decoded) { }
};



Handler pickHandler(string key, Handler parent) {
  infof("ughm %s",key);
  switch(key) {
  case "a": return new LinkHandler(parent);
  case "i": return new DumbTag(parent,"i");
  case "b": return new DumbTag(parent,"b");
  case "u": return new DumbTag(parent,"u");
  case "s": return new DumbTag(parent,"s");
  case "hr": return new DumbTag(parent,"hr");
  case "blockquote": return new DumbTag(parent,"quote");
  case "img": return new ImageHandler(parent);
  case "font": return new FontHandler(parent);
  case "div": return new DivHandler(parent);
  case "ul": return new ListHandler(parent,false);
  case "ol": return new ListHandler(parent,true);
  case "li": return new ListItemHandler(parent);
  case "h3": return new H3Handler(parent);
  case "title": return new TitleHandler(parent);
  case "small": return new SmallHandler(parent);
  default:
    tracef("No idea what is %s",key);
    return new Nada(parent);
    //throw new Exception(format(
  }
}

class FIMBuilder : declassifier.Handler {
  Handler curtag;
  this() {
    curtag = new Handler(null);
  }
  override void handle(E event) {
    tracef("Got %s %s",declassifier.names[event],this.curtag.toString());
    this.curtag.handle(event);
    if(event == E.SelfClosing) {
      this.curtag = this.curtag.parent;
    }
  }
  override void onEntity(const(char)[] data, const(char)[] decoded) {
    output(decoded);
  }
  override void handle(E event, const(char)[] data) {
    tracef("Got %s: %s %d",declassifier.names[event],data,data.length);
    switch(event) {
    case E.OpenStart:
      this.curtag = pickHandler(cast(string)(data),this.curtag);
      break;
    case E.SelfClosing:
      goto case;
    case E.Close:
      this.curtag.handle(event,data);
      this.curtag = this.curtag.parent;
      break;
    default:
      // bail out if no idea what the current tag is?
      this.curtag.handle(event,data);
    }
  }
}

extern (C) static void reloadFile(void* ptr, void* funcptr) {
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

alias word = wordcount;

void main(string[] args)
{
  dest = &story;
  globalLogLevel = LogLevel.info;
  char[100000] buffer;
  ImageHandler.commentMode = (null != environment.get("comment"));
  Declassifier builder = Declassifier(new FIMBuilder);
  string dest = args[1];

  void recalculate() {
    title = appender!string();
    story= appender!string();
    
    authorNotes= appender!string();
    
    File inp = File(dest);
    const(char)[] input = cast(const(char)[])(inp.rawRead(buffer));
    inp.close();
    trace(input);
    trace("--------------");
    parseHTML(input,builder);
    int i = -1;
    if(title.data.length > 0) {
      title = appender!string(strip(title.data));
      story = appender!string(strip(story.data));
      authorNotes = appender!string(strip(authorNotes.data));
            
      refreshRow(++i,
                 0,
                 "title",
                 std.string.toStringz(title.data),
                 std.string.toStringz(format("%d",word.count(title.data))));
    }
    if(story.data.length > 0) {
      refreshRow(++i,
                 1,
                 "body",
                 std.string.toStringz(story.data[0..min(20,$)]),
                 std.string.toStringz(format("%d",word.count(story.data))));
    }
    if(authorNotes.data.length > 0) {
      refreshRow(++i,
                 2,
                 "author",
                 std.string.toStringz(authorNotes.data[0..min(20,$)]),
                 std.string.toStringz(format("%d",word.count(authorNotes.data))));
    }
  }

  void delegate() rc = &recalculate;
  guiLoop(cast(const(char*))(dest),rc.ptr,cast(void*)(rc.funcptr));
}
