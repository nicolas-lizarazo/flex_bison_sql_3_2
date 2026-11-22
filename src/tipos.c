/*
 * tipos.c
 * ---------------------------------------------------------------------------
 * Implementación de los constructores/destructores declarados en tipos.h.
 * 
 * REGLA DE PROPIEDAD DE MEMORIA (importante para evitar fugas y dobles free):
 * - Las cadenas que produce el analizador léxico (identificadores y
 *   literales de cadena) son memoria dinámica (strdup / malloc).
 * - Mientras un símbolo está en la pila de Bison, Bison es su dueño y lo
 *   libera con %destructor si la recuperación de errores lo descarta.
 * - Cuando una acción semántica pasa un puntero a las funciones
 *   valor_crear_cadena() o lista_campos_agregar(), la propiedad se
 *   TRANSFIERE a la estructura y será liberada por valor_liberar() /
 *   lista_campos_vaciar().
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "tipos.h"

static void *reservar(size_t n) {
    void *p = malloc(n);
    if (p == NULL) {
        fprintf(stderr, "Error fatal: memoria insuficiente.\n");
        exit(2);
    }
    return p;
}

Valor *valor_crear_entero(long long n) {
    Valor *v = reservar(sizeof *v);
    v->tipo = V_ENTERO;
    v->dato.entero = n;
    return v;
}

Valor *valor_crear_cadena(char *cadena) {
    Valor *v = reservar(sizeof *v);
    v->tipo = V_CADENA;
    v->dato.cadena = cadena;          /* posesión transferida */
    return v;
}

Valor *valor_crear_booleano(int b) {
    Valor *v = reservar(sizeof *v);
    v->tipo = V_BOOLEANO;
    v->dato.booleano = b ? 1 : 0;
    return v;
}

void valor_liberar(Valor *v) {
    if (v == NULL) return;
    if (v->tipo == V_CADENA) free(v->dato.cadena);
    free(v);
}

const char *tipo_valor_nombre(TipoValor t) {
    switch (t) {
        case V_ENTERO:   return "ENTERO";
        case V_CADENA:   return "CADENA";
        case V_BOOLEANO: return "BOOLEANO";
    }
    return "DESCONOCIDO";
}

void lista_campos_iniciar(ListaCampos *l) {
    l->items = NULL;
    l->cantidad = 0;
    l->capacidad = 0;
}

void lista_campos_agregar(ListaCampos *l, char *nombre, Valor *valor,
                          int linea, int columna) {
    if (l->cantidad == l->capacidad) {
        size_t nueva = (l->capacidad == 0) ? 8 : l->capacidad * 2;
        Campo *tmp = realloc(l->items, nueva * sizeof *tmp);
        if (tmp == NULL) {
            fprintf(stderr, "Error fatal: memoria insuficiente.\n");
            exit(2);
        }
        l->items = tmp;
        l->capacidad = nueva;
    }
    Campo *c = &l->items[l->cantidad++];
    c->nombre  = nombre;
    c->valor   = valor;
    c->linea   = linea;
    c->columna = columna;
}

void lista_campos_vaciar(ListaCampos *l) {
    for (size_t i = 0; i < l->cantidad; i++) {
        free(l->items[i].nombre);
        valor_liberar(l->items[i].valor);
    }
    free(l->items);
    lista_campos_iniciar(l);
}