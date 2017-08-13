#include "wordcount.h"
#include <pcre.h>
#include <assert.h>

#define connectors "’'_–-"
#define letter "\\w"

#define word "[" letter "][" letter connectors "]+|[IaA]"

pcre* pat = NULL;
pcre_extra* study;

void wordcount_setup(void) {
	const char* err=NULL;
	int erroffset = 0;
	pat = pcre_compile(word,
										 0,
										 &err,&erroffset,
										 NULL);
	assert(pat);
	study = pcre_study(pat, PCRE_STUDY_JIT_COMPILE,&err);
	assert(study);
}

int word_count(const char* c, int l) {  
  int count = 0;
	for(;;) {
		int ovec[3];
		int rc = pcre_exec(pat,
											 study,
											 c,l,0,
											 0,
											 ovec,3);
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
