/*
 * parser.y  --  Analizador sintáctico (Bison)
 * ---------------------------------------------------------------------------
 * Implementa la gramática libre de contexto del lenguaje de inserción.
 * La estructura principal es:
 *   sentencia -> INSERTAR EN TABLA TABLE_ID VALORES : lista FIN ;
 *   lista -> asignacion (COMA asignacion)*
 *   asignacion -> CAMPO = valor
 * ---------------------------------------------------------------------------
 */
%{
#include <stdio.h>
#include <stdlib.h>
#include "tipos.h"
#include "db.h"

void yyerror(const char *s);
int yylex(void);

ListaCampos lista_actual;
int linea_sentencia = 0;
int columna_sentencia = 0;

void ejecutar_insercion(char *tabla, int linea);
%}

%locations
%define parse.error verbose

%union {
    long long entero;
    char     *cadena;
    int       booleano;
    Valor    *valor;
}

%token INSERTAR EN TABLA VALORES FIN
%token DOSPUNTOS PUNTOYCOMA COMA IGUAL ERROR_LEXICO
%token <cadena> TABLE_ID CAMPO
%token <entero> ENTERO
%token <cadena> CADENA
%token <booleano> BOOLEANO
%type <valor> valor

%%

programa:
    sentencia
  | programa sentencia
  ;

sentencia:
    INSERTAR EN TABLA TABLE_ID VALORES DOSPUNTOS
    {
        lista_campos_iniciar(&lista_actual);
        linea_sentencia = @1.first_line;
        columna_sentencia = @1.first_column;
    }
    lista_asignaciones FIN PUNTOYCOMA
    {
        ejecutar_insercion($4, linea_sentencia);
        free($4);
        lista_campos_vaciar(&lista_actual);
    }
  | error PUNTOYCOMA
    {
        yyerrok;
        lista_campos_vaciar(&lista_actual);
    }
  ;

lista_asignaciones:
    asignacion
  | lista_asignaciones COMA asignacion
  ;

asignacion:
    CAMPO IGUAL valor
    {
        lista_campos_agregar(&lista_actual, $1, $3,
                             @1.first_line, @1.first_column);
    }
  ;

valor:
    ENTERO   { $$ = valor_crear_entero($1); }
  | CADENA   { $$ = valor_crear_cadena($1); }
  | BOOLEANO { $$ = valor_crear_booleano($1); }
  ;

%%

void ejecutar_insercion(char *tabla, int linea) {
    db_insertar(tabla, &lista_actual, linea, 1);
}

void yyerror(const char *s) {
    if (ultimo_token_fue_error_lexico) return;
    fprintf(stderr, "Error sintáctico [línea %d]: %s\n", yylloc.first_line, s);
    estadisticas.errores_sintacticos++;
}

void parser_iniciar(void) { lista_campos_iniciar(&lista_actual); }
void parser_liberar(void) { lista_campos_vaciar(&lista_actual); }