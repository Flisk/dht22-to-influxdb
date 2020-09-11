/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
%{

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#include "dht22.h"

extern int yylex();
extern int yyparse();
extern FILE *yyin;

void yyerror(const char *);

static struct stream_ctx *ctx;

%}

%define parse.error verbose

%union {
	int	 i;
	char	*s;
}

%token LINEBREAK
%token STREAM
%token FROM GPIO INTERVAL AVG SAMPLES URL USERNAME PASSWORD MEASUREMENT

%token <i> INT
%token <s> STRING STRING_LITERAL

%%

config:
	  STREAM stream_directives LINEBREAK
	| STREAM stream_directives
	;

stream_directives:
	  stream_directive stream_directives
	| stream_directive
	;

stream_directive:
	  FROM GPIO	INT		{ ctx->pin = $3; }
	| INTERVAL	INT		{ ctx->interval = $2; }
	| AVG SAMPLES	INT		{ ctx->avg_samples = $3; }
	| URL		STRING_LITERAL	{ ctx->influx_url = $2; }
	| USERNAME	STRING_LITERAL	{ ctx->influx_username = $2; }
	| PASSWORD	STRING_LITERAL	{ ctx->influx_password = $2; }
	| MEASUREMENT	STRING_LITERAL	{ ctx->influx_measurement = $2; }
	;

%%

extern char *yytext;

void parse_config(struct stream_ctx *arg_ctx, FILE *f) {
	yyin = f;

	ctx = arg_ctx;

	do {
		yyparse();
	} while (!feof(yyin));
}

void yyerror(const char *s) {
	error(1, 0, "couln't parse config file: '%s': %s", yytext, s);
}
