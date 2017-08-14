This thing parses (sort of (not really (it cheats))) my peculiar markup, and reformats in a way friendly with fimfiction.net. Because they have their own bizarre markup language. I bet they define it with regular expressions, too. Mine just uses libxml to... enhance whitespace.

The markup is https://github.com/cyisfor/htmlish but this program pretty much just ignores it and switches all the <> for [].

This program makes use of a trie to make a super fast converter from “I’m a stupid string tag without even a length that can’t be switched on because libxml2 is stupid and love wasting time doing strcmps” to integers. It’s basically a hard-coded trie, which I find kind of fascinating. (I wasted a day of my life making this.)

It’s also an example of the use of open_memstream to make a FILE* that writes to a growable buffer.

As with all my libxml stuff, it makes extensive use of (self) recursion to avoid boilerplate and branch errors, so enabling -O2 in Makefile to optimize those tail calls is probably advisable, or at least  -foptimize-sibling-calls.
