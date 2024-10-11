#ifndef __TINY_DB_LEX_H
#define __TINY_DB_LEX_H

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEX_MAX_TOKENS 255

typedef enum LEX_TOKEN
{
  LEX_TOKEN_EOF = -1,
  LEX_TOKEN_STRING,
  LEX_TOKEN_NUMBER,
  LEX_TOKEN_IDENTIFIER,
  LEX_TOKEN_COMMAND,
} LEX_TOKEN;

typedef struct Token
{
  LEX_TOKEN type;
  char* value;
  int32_t col;
  int32_t line;
} Token;

typedef struct Lexer
{
  int32_t cursor;
  int32_t token_count;
  Token tokens[LEX_MAX_TOKENS];
} Lexer;

char*
Lexer_Token_To_String(LEX_TOKEN tok);

char
Lexer_Peek(Lexer* lexer, const uint8_t* buf);

char
Lexer_Consume(Lexer* lexer, const uint8_t* buf);

void
Lexer_Lex(Lexer* lexer, const uint8_t* buf, size_t len);

void
Lexer_Reset(Lexer* lexer);

void
Lexer_Print_Tokens(Lexer* lexer);

void
Lexer_Free_Token_Value(Token* tok);

#endif // __TINY_DB_LEX_H