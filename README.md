This thing parses (sort of (not really (it cheats))) my peculiar markup, and reformats in a way friendly with fimfiction.net. Because they have their own bizarre markup language. I bet they define it with regular expressions, too. Mine just uses libxml to... enhance whitespace.

The markup is https://github.com/cyisfor/htmlish but this program pretty much just ignores it and switches all the <> for [].
