import std.regex;
import std.stdio;
import std.string;

immutable auto connectors = r"’'_–-";
immutable auto letter = r"\w";

auto word = ctRegex!("["~letter~"]["~letter~connectors~"]+|[IaA]");

int count(const string s) {  
  int count = 0;
  foreach(m; matchAll(s,word)) {
    // everything except surrounded by [] brackets
    if(!(m.pre.length && m.post.length && m.pre[$-1] == '[' && m.post[0] == ']')) {
      ++count;
    }
  }
  return count;
}
