/*===========================================================================*/
/**
    @file    cst_lexer.l

    @brief   lex code for csf language

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011-2012 All rights reserved.
        Copyright 2019 NXP

=============================================================================
@endverbatim */
%{
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <csf.h>
#include "cst_parser.h"
%}

%option case-insensitive
%option yylineno

%option noinput
%option nounput
%%

[a-z][a-z0-9]*          yylval.str=strdup(yytext);return WORD;
\"[a-zA-Z0-9\/\\._-]+\" { /* strip off quotes */
                        yytext++;
                        yytext[strlen(yytext) - 1] = 0;
                        /*
                         * Strings with backslashes are allowed.
                         * Backslashes are replaced by forward slashes.
                         */
                        char *c = NULL;
                        while (NULL != (c = strstr(yytext, "\\"))) {
                            *c = '/';
                        }
                        yylval.str=strdup(yytext);
                        return FILENAME;
                        }
0x[0-9a-f]+             yylval.num=strtoul(yytext,NULL,0); return NUMBER;
[0-9]+                  yylval.num=strtoul(yytext,NULL,0); return NUMBER;
\#.*\n                  /* ignore comments - swallow newline */;
\\.*\n                  /* continuation - swallow newline */;
[\t]+                   /* ignore tabs */;
[ ]+                    /* ignore whitespaces */;
\n                      return EOL;
\=                      return EQUALS;
\|                      return OR;
\[                      return LBRACK;
\]                      return RBRACK;
\,                      return COMMA;
\.                      return DOT;
<<EOF>>                 {
                            /*
                             * Always append a newline to the input.
                             * Prevent errors if no newline at end of file.
                             */
                            static int flag_eof = 0;
                            if (0 == flag_eof++) {
                                return EOL;
                            }
                            else {
                                return YY_NULL;
                            }
                        }
.                       /* ignore unexpected chars */;
%%
