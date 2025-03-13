#include "tinydb_lex.h"
#include <ctype.h>

static const char* commands[] = { "set",    "get",   "rpush",  "lpush",
                                  "lpop",   "rpop",  "llen",   "lrange",
                                  "pub",    "sub",   "strlen", "incr",
                                  "append", "unsub", "export", "insp" };

char
Lexer_Peek(Lexer* lexer, const uint8_t* buf)
{
  return buf[lexer->cursor];
}

char
Lexer_Consume(Lexer* lexer, const uint8_t* buf)
{
  return buf[lexer->cursor++];
}

char*
Lexer_Token_To_String(LEX_TOKEN tok)
{
  switch (tok) {
    case LEX_TOKEN_EOF:
      return "End of File";
    case LEX_TOKEN_NUMBER:
      return "Number";
    case LEX_TOKEN_STRING:
      return "String";
    case LEX_TOKEN_IDENTIFIER:
      return "Identifier";
    case LEX_TOKEN_COMMAND:
      return "Command";
    default:
      return "Unknown";
  }
}

void
Lexer_Lex(Lexer* lexer, const uint8_t* buf, size_t len)
{
  int32_t line_number = 1;
  int32_t col_number = 1;

  // note (David) reseting the token count, since we are using one instance for
  // messages
  lexer->token_count = 0;

  while (lexer->cursor < len) {
    char c = Lexer_Peek(lexer, buf);

    // skipping the whitespace
    if (isspace(c)) {
      if (c == '\n') {
        line_number++;
        col_number = 1;
      } else {
        col_number++;
      }
      Lexer_Consume(lexer, buf);
      continue;
    }

    // commands
    int32_t num_commands = sizeof(commands) / sizeof(commands[0]);
    int32_t command_found = 0;

    for (int32_t i = 0; i < num_commands; i++) {
      size_t cmd_len = strlen(commands[i]);
      if (lexer->cursor + cmd_len <= len &&
          strncasecmp((const char*)&buf[lexer->cursor], commands[i], cmd_len) ==
            0 &&
          (lexer->cursor + cmd_len == len ||
           isspace(buf[lexer->cursor + cmd_len]))) {
        lexer->tokens[lexer->token_count++] =
          (Token){ .type = LEX_TOKEN_COMMAND,
                   .value = strdup(commands[i]),
                   .col = col_number,
                   .line = line_number };
        lexer->cursor += cmd_len;
        col_number += cmd_len;
        command_found = 1;
        break;
      }
    }

    if (command_found) {
      continue;
    }

    if (c == '"') {
      int32_t start = lexer->cursor;
      col_number++;
      Lexer_Consume(lexer, buf); // opening quote
      while (lexer->cursor < len && Lexer_Peek(lexer, buf) != '"') {
        Lexer_Consume(lexer, buf);
        col_number++;
      }
      if (lexer->cursor < len) {
        Lexer_Consume(lexer, buf); // closing quote
      }

      int32_t length = lexer->cursor - start - 2;
      char* value = malloc(length + 1);
      strncpy(value, (const char*)&buf[start + 1], length);
      value[length] = '\0';

      lexer->tokens[lexer->token_count++] =
        (Token){ .type = LEX_TOKEN_STRING,
                 .value = value,
                 .col = col_number - length - 2, // -2 both quotes
                 .line = line_number };
      col_number++;
      continue;
    }

    // identifiers (unquoted strings)
    if (isalpha(c) || c == '_' || c == '@') {
      int32_t start = lexer->cursor;
      while (lexer->cursor < len && !isspace(Lexer_Peek(lexer, buf))) {
        Lexer_Consume(lexer, buf);
        col_number++;
      }

      int32_t length = lexer->cursor - start;
      char* value = malloc(length + 1);
      strncpy(value, (const char*)&buf[start], length);
      value[length] = '\0';

      lexer->tokens[lexer->token_count++] =
        (Token){ .type = LEX_TOKEN_IDENTIFIER,
                 .value = value,
                 .col = col_number - length,
                 .line = line_number };

      continue;
    }

    // numbers, only integers for now.
    if (isdigit(c)) {
      int32_t start = lexer->cursor;
      while (lexer->cursor < len && isdigit(Lexer_Peek(lexer, buf))) {
        Lexer_Consume(lexer, buf);
        col_number++;
      }

      int32_t length = lexer->cursor - start;
      char* value = malloc(length + 1);
      strncpy(value, (const char*)&buf[start], length);
      value[length] = '\0';

      lexer->tokens[lexer->token_count++] = (Token){ .type = LEX_TOKEN_NUMBER,
                                                     .value = value,
                                                     .col = col_number - length,
                                                     .line = line_number };

      continue;
    }

    // unknown characters
    Lexer_Consume(lexer, buf);
    col_number++;
  }

  // EOF
  lexer->tokens[lexer->token_count++] = (Token){ .type = LEX_TOKEN_EOF,
                                                 .value = strdup(""),
                                                 .col = col_number,
                                                 .line = line_number };
}

void
Lexer_Reset(Lexer* lexer)
{
  lexer->cursor = 0;
  lexer->token_count = 0;
}

void
Lexer_Free_Token_Value(Token* tok)
{
  if (tok->value != NULL) {
    free(tok->value);
    tok->value = NULL;
  }
}

void
Lexer_Print_Tokens(Lexer* lexer)
{
  for (int32_t i = 0; i < lexer->token_count; i++) {
    printf("<Token type=%s, value=%s, line=%d, col=%d />\n",
           Lexer_Token_To_String(lexer->tokens[i].type),
           lexer->tokens[i].value,
           lexer->tokens[i].line,
           lexer->tokens[i].col);
  }
}