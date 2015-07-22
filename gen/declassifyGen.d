import std.stdio;
import std.string;

struct Event {
  string name;
  int args;
}

Event[] events = [
                 { "Text", 1 },
                 { "SelfClosing", 0 },
                 { "OpenStart", 1 },
                 { "OpenEnd", 1 },
                 { "Close", 1 },
                 { "AttrName", 1 },
                 { "AttrEnd", 0 },
                 { "AttrValue", 1 },
                 { "Comment", 1 },
                 { "Declaration", 1 },
                 { "ProcessingInstruction", 1 },
                 { "CDATA", 1 },
                 { "NamedEntity", 1 },
                 { "NumericEntity", 1 },
                 { "HexEntity", 1 },
                 { "DocumentEnd", 0 }];
                                   

void main() {
  write("enum Event {\n  ");
  bool first = true;
  foreach (Event event; events) {
    if(first) {
      first = false;
    } else {
      write(",\n  ");
    }
    write(event.name);
  }
  write(q"(
}

enum string[Event] names = [
  )");
  first = true;
  foreach (Event event; events) {
    if(first) {
      first = false;
    } else {
      write(",\n  ");
    }
    write("Event." ~ event.name ~ ": \"" ~ event.name ~ '"');
  }
  write(q"(
];

interface Handler {
  void onEntity(const(char)[] data, const(char)[] decoded); // ugh...
  void handle(Event event, const(char)[] data);
  void handle(Event event);
}

struct Declassifier {
  Handler h;
)");
  foreach(Event event; events) {
    write("  void on");
    write(event.name);
    write('(');
    bool derpfirst = true;
    for(int i=0;i<event.args;++i) {
      if(derpfirst) {
        derpfirst = false;
      } else {
        write(", ");
      }
      write(format("const(char)[] arg%x",i));
    }
    write(')');
    write("{ this.h.handle(Event." ~ event.name);
    for(int i=0;i<event.args;++i) {
      write(format(", arg%x",i));
    }
    write("); }\n");
  }
  write(q"(
  void onEntity(const(char)[] data, const(char)[] decoded){ this.h.onEntity(data,decoded); }
}
)");
}
