/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#define sstrncpy(d,s,l) ((d)[l]='\0',strncpy(d,s,l))

char* sstrtok( const char* str, const char* delim, char** tok, int tok_max, 
	       int* tok_len, char** s );

void stolower( char* str );

