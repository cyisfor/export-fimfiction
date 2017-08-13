#include <pcre.h>

#define connectors "’'_–-"
#define letter = "\\w"

#define word "[" letter "][" letter connectors "]+|[IaA]"

pcre* pat = NULL;
pcre_extra* study;

void wordcount_setup(void) {
	const char* err=NULL;
	int erroffset = 0;
	pat = pcre_compile(word,
										 0,
										 &err,&erroff,
										 NULL);
	assert(pat);
	study = pcre_study(pat, PCRE_JIT_COMPILE,&err);
	assert(study);
}

int word_count(const char* c, size_t l) {  
  int count = 0;
	for(;;) {
		int ovec[3];
		int res = pcre_exec(pat,study,c,l,0,ovec,3);
		if(rc < 0) {
			if(rc == PCRE_ERROR_NOMATCH) break;
			abort();
		}
		if(ovec[0] < ovec[1]) {
			++count;
		}
		if(ovec[1] >= l) break;
		c += ovec[1];
		l -= ovec[1];
	}
	return count;
}
