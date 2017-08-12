int main(int argc, char *argv[])
{

#define WANT(what,code)
	
	WANT("a", return argTag("url",e.attr("href")));
  WANT("chat", return dumbTag("quote"));
	WANT("i", return dumbTag("i"));
	WANT("b", return dumbTag("b"));
	WANT("u", return dumbTag("u"));
	WANT("s", return dumbTag("s"));
	WANT("hr", return dumbTag("hr"));
	WANT("blockquote", return dumbTag("quote"));
	WANT("font", return argTag("color",e.attr("color")));
	WANT("small", return argTag("size","0.75em"));
	WANT("ul", return dolist!false());
	WANT("ol", return dolist!true());
	WANT("h3", return argTag("size","2em"));
	WANT("div",
			 {
				 xmlChar* attr = xmlGetProp(cur,"class");
				 if(attr &&
						strlen(attr) == LITSIZ("spoiler") &&
						0 == memcmp(attr,LITSIZ("spoiler"))) {
					 xmlFree(attr);
					 return dumbTag("spoiler");
				 }
				 xmlFree(attr);
			 });
	// strip
	WANT("root", return pkids());
	WANT("p", return pkids());
	WANT("title", return pkids());
	WANT("img", {
			xmlChar* src = xmlGetProp(cur,"data-fimfiction-src");
			if(src == NULL) {
				src = xmlGetProp(cur,"src");
				if(src == NULL) {
					WARN("Skipping sourceless image");
					return;
				}
			}
			OUTLIT("[img]");
			OUTS(src,strlen(src));
			OUTLIT("[/img]");
		});
	return;
}
