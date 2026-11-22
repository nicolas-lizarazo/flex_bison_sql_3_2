/*
 * lexer.l  --  Analizador léxico (Flex) del lenguaje de inserción
 * ---------------------------------------------------------------------------
 * Reconoce los tokens de sentencias como:
 * 
 * insertar en tabla t_clientes valores: id = 1, nombre = 'Ana' fin;
 * 
 * Flex compila todas las reglas en UN solo autómata finito determinista
 * (AFD) y lo ejecuta con la política "coincidencia más larga; en empate,
 * la regla escrita primero". Por eso las palabras reservadas están ANTES
 * que la regla de identificadores.
 * ---------------------------------------------------------------------------
 */
%option noyywrap nounput noinput yylineno 8bit

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "tipos.h"
#include "parser.tab.h"

static int columna = 1;

#define YY_USER_ACTION                                   \
    yylloc.first_line   = yylineno;                      \
    yylloc.first_column = columna;                       \
    yylloc.last_line    = yylineno;                      \
    yylloc.last_column  = columna + yyleng - 1;          \
    columna += yyleng;

int ultimo_token_fue_error_lexico = 0;
#define DEVOLVER(t)  do { ultimo_token_fue_error_lexico = 0; return (t); } while (0)

#define ERROR_LEX(...)                                                     \
    do {                                                                   \
        fprintf(stderr, "Error léxico [línea %d, col %d]: ",               \
                yylloc.first_line, yylloc.first_column);                   \
        fprintf(stderr, __VA_ARGS__);                                      \
        fputc('\n', stderr);                                               \
        estadisticas.errores_lexicos++;                                    \
        ultimo_token_fue_error_lexico = 1;                                 \
        return ERROR_LEXICO;                                               \
    } while (0)

static char *copiar(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (p == NULL) { fprintf(stderr, "Error fatal: memoria insuficiente.\n"); exit(2); }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *desescapar_cadena(const char *lexema, size_t largo) {
    char *res = malloc(largo);
    size_t j = 0;
    if (res == NULL) { fprintf(stderr, "Error fatal: memoria insuficiente.\n"); exit(2); }
    for (size_t i = 1; i + 1 < largo; i++) {
        res[j++] = lexema[i];
        if (lexema[i] == '\'' ) i++;
    }
    res[j] = '\0';
    return res;
}
%}

LETRA       [A-Za-z_]
DIGITO      [0-9]
ENTERO      -?{DIGITO}+
CUERPO      ([^'\n]|'')*
CADENA_OK   '{CUERPO}'
CADENA_ABIERTA  '{CUERPO}
COMENTARIO  "--"[^\n]*

%%

"insertar"      { DEVOLVER(INSERTAR); }
"en"            { DEVOLVER(EN); }
"tabla"         { DEVOLVER(TABLA); }
"valores"       { DEVOLVER(VALORES); }
"fin"           { DEVOLVER(FIN); }

"verdadero"     { yylval.booleano = 1; DEVOLVER(BOOLEANO); }
"falso"         { yylval.booleano = 0; DEVOLVER(BOOLEANO); }

/* Conjunto finito de tablas válidas (prefijo t_) */
t_(clientes|productos|pedidos)  { yylval.cadena = copiar(yytext, yyleng); DEVOLVER(TABLE_ID); }

/* Identificadores de columna/campo */
{LETRA}({LETRA}|{DIGITO})*      { yylval.cadena = copiar(yytext, yyleng); DEVOLVER(CAMPO); }

{ENTERO}        {
                    errno = 0;
                    long long n = strtoll(yytext, NULL, 10);
                    if (errno == ERANGE)
                        ERROR_LEX("entero fuera del rango de 64 bits: %s", yytext);
                    yylval.entero = n;
                    DEVOLVER(ENTERO);
                }

{CADENA_OK}     {
                    yylval.cadena = desescapar_cadena(yytext, (size_t)yyleng);
                    DEVOLVER(CADENA);
                }
{CADENA_ABIERTA} {
                    ERROR_LEX("cadena sin cerrar (falta la comilla ' final)");
                }

":"|";"|","|"="  { DEVOLVER(yytext[0]); }

{COMENTARIO}    { /* ignorar */ }
[ \t\r]+        { /* ignorar */ }
\n              { columna = 1; }

[\x80-\xFF]+    { ERROR_LEX("carácter no ASCII fuera de una cadena"); }
.               {
                    if (yytext[0] == '"')
                        ERROR_LEX("las cadenas se delimitan con comilla simple ('), no doble (\")");
                    else
                        ERROR_LEX("carácter no reconocido '%c'", yytext[0]);
                }

%%
void lexer_reiniciar_posicion(void) {
    columna = 1;
    yylineno = 1;
}