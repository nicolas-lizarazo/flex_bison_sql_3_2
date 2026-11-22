/*
 * tipos.h
 * ---------------------------------------------------------------------------
 * Tipos de datos compartidos entre el analizador sintáctico (parser.y) y la
 * capa de base de datos (db.c).
 * 
 * El parser NO conoce SQLite y la capa de base de datos NO conoce Bison:
 * ambos se comunican únicamente a través de las estructuras de este archivo.
 * Esta separación facilita probar cada capa por separado.
 * ---------------------------------------------------------------------------
 */
#ifndef TIPOS_H
#define TIPOS_H

#include <stddef.h>

/* Tipos de literal que reconoce el lenguaje (ver análisis léxico). */
typedef enum {
    V_ENTERO,      /* 42, -7                      */
    V_CADENA,      /* 'Ana', 'O''Brien'           */
    V_BOOLEANO     /* verdadero, falso            */
} TipoValor;

/* Un literal ya convertido a su representación en C. */
typedef struct {
    TipoValor tipo;
    union {
        long long entero;   /* usado si tipo == V_ENTERO   */
        char     *cadena;   /* usado si tipo == V_CADENA (memoria dinámica) */
        int       booleano; /* usado si tipo == V_BOOLEANO (0 o 1) */
    } dato;
} Valor;

/* Una asignación "campo = valor" de la sentencia, con su posición en el
 * texto fuente (se usa para reportar errores semánticos con línea/columna). */
typedef struct {
    char  *nombre;   /* identificador del campo (memoria dinámica) */
    Valor *valor;    /* literal asignado (memoria dinámica)        */
    int    linea;
    int    columna;
} Campo;

/* Lista dinámica de asignaciones de UNA sentencia insertar. */
typedef struct {
    Campo  *items;
    size_t  cantidad;
    size_t  capacidad;
} ListaCampos;

/* Contadores globales que se imprimen en el resumen final. */
typedef struct {
    int insertadas;            /* sentencias válidas y almacenadas en la BD     */
    int rechazadas_sintaxis;   /* sentencias descartadas por error léxico/sintáctico */
    int rechazadas_semantica;  /* sentencias válidas en sintaxis pero rechazadas
                                  por el esquema (tabla/campo/tipo) o por SQLite */
    int errores_lexicos;       /* diagnósticos emitidos por el analizador léxico  */
    int errores_sintacticos;   /* diagnósticos emitidos por el analizador sintáctico */
} Estadisticas;

extern Estadisticas estadisticas;   /* definida en main.c */

/* --- Constructores y destructores de valores ----------------------------- */
Valor *valor_crear_entero(long long n);
Valor *valor_crear_cadena(char *cadena);   /* TOMA POSESIÓN de 'cadena' */
Valor *valor_crear_booleano(int b);
void   valor_liberar(Valor *v);
const char *tipo_valor_nombre(TipoValor t);

/* --- Lista de campos ------------------------------------------------------ */
void lista_campos_iniciar(ListaCampos *l);
/* TOMA POSESIÓN de 'nombre' y 'valor'. */
void lista_campos_agregar(ListaCampos *l, char *nombre, Valor *valor,
                          int linea, int columna);
void lista_campos_vaciar(ListaCampos *l);   /* libera todo y deja la lista vacía */

#endif /* TIPOS_H */