/* -*- indent-tabs-mode: t; -*- */
%{

#include <string.h>
#include <stdio.h>
#include "config.tab.h"

%}

%%

\\\n		;
[ \t]		;
\n		{ return LINEBREAK; }
stream		{ return STREAM; }
from		{ return FROM; }
interval	{ return INTERVAL; }
avg		{ return AVG; }
samples		{ return SAMPLES; }
gpio		{ return GPIO; }
url		{ return URL; }
username	{ return USERNAME; }
password	{ return PASSWORD; }
measurement	{ return MEASUREMENT; }
[0-9]+		{ yylval.i = atoi(yytext); return INT; }
[a-zA-Z0-9]+	{ yylval.s = strdup(yytext); return STRING; }

["]([^"]|\\(.|\n))*["] {
	yylval.s = malloc(yyleng - 1);
	memcpy(yylval.s, yytext + 1, yyleng - 2);
	yylval.s[yyleng - 2] = 0;
	return STRING_LITERAL;
}

.		{ fprintf(stderr, "ignoring unknown token: %s\n", yytext); }

%%
