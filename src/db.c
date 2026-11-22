#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sqlite3.h>
#include "db.h"

static sqlite3 *bd = NULL;

/* CAMBIO 1: Nombres de tablas con el prefijo t_ */
static const char *const TABLAS[] = { "t_clientes", "t_productos", "t_pedidos" };
#define NUM_TABLAS ((int)(sizeof TABLAS / sizeof TABLAS[0]))

/* CAMBIO 2: Esquema SQL actualizado con el prefijo t_ */
static const char *const ESQUEMA =
   "CREATE TABLE IF NOT EXISTS t_clientes ("
   "  id      INTEGER PRIMARY KEY,"
   "  nombre  TEXT    NOT NULL,"
   "  ciudad  TEXT,"
   "  activo  BOOLEAN"
   ");"
   "CREATE TABLE IF NOT EXISTS t_productos ("
   "  codigo      INTEGER PRIMARY KEY,"
   "  nombre      TEXT    NOT NULL,"
   "  stock       INTEGER,"
   "  disponible  BOOLEAN"
   ");"
   "CREATE TABLE IF NOT EXISTS t_pedidos ("
   "  id          INTEGER PRIMARY KEY,"
   "  cliente_id  INTEGER NOT NULL REFERENCES t_clientes(id),"
   "  total       INTEGER,"
   "  pagado      BOOLEAN"
   ");";

#define MAX_COLUMNAS 64

typedef struct {
    char nombre[64];
    char tipo[32];
    int  obligatoria;
} Columna;

static void error_semantico(int linea, int col, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "Error semántico [línea %d, col %d]: ", linea, col);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int db_abrir(const char *ruta) {
    char *msg = NULL;
    if (sqlite3_open(ruta, &bd) != SQLITE_OK) {
        fprintf(stderr, "No se pudo abrir la base de datos '%s': %s\n",
                ruta, sqlite3_errmsg(bd));
        return -1;
    }
    if (sqlite3_exec(bd, ESQUEMA, NULL, NULL, &msg) != SQLITE_OK) {
        fprintf(stderr, "No se pudo crear el esquema: %s\n", msg);
        sqlite3_free(msg);
        return -1;
    }
    if (sqlite3_exec(bd, "PRAGMA foreign_keys = ON; BEGIN;", NULL, NULL, &msg) != SQLITE_OK) {
        fprintf(stderr, "No se pudo iniciar la transacción: %s\n", msg);
        sqlite3_free(msg);
        return -1;
    }
    return 0;
}

void db_cerrar(void) {
    if (bd == NULL) return;
    sqlite3_exec(bd, "COMMIT;", NULL, NULL, NULL);
    sqlite3_close(bd);
    bd = NULL;
}

static int cargar_columnas(const char *tabla, Columna cols[], int *n) {
    static const char *const SQL =
    "SELECT name, type, \"notnull\", dflt_value IS NOT NULL, pk "
    "FROM pragma_table_info(?1)";
    sqlite3_stmt *st = NULL;
    *n = 0;
    if (sqlite3_prepare_v2(bd, SQL, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error interno al consultar el catálogo: %s\n", sqlite3_errmsg(bd));
        return -1;
    }
    sqlite3_bind_text(st, 1, tabla, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (*n >= MAX_COLUMNAS) { sqlite3_finalize(st); return -1; }
        Columna *c = &cols[(*n)++];
        snprintf(c->nombre, sizeof c->nombre, "%s", (const char *)sqlite3_column_text(st, 0));
        snprintf(c->tipo, sizeof c->tipo, "%s", (const char *)sqlite3_column_text(st, 1));
        int notnull     = sqlite3_column_int(st, 2);
        int tiene_deflt = sqlite3_column_int(st, 3);
        int es_pk       = sqlite3_column_int(st, 4);
        c->obligatoria  = notnull && !tiene_deflt && !es_pk;
    }
    sqlite3_finalize(st);
    return 0;
}

static int tipo_compatible(const char *tipo_sql, TipoValor v) {
    if (strcasecmp(tipo_sql, "INTEGER") == 0 || strcasecmp(tipo_sql, "INT") == 0)
        return v == V_ENTERO;
    if (strcasecmp(tipo_sql, "TEXT") == 0)
        return v == V_CADENA;
    if (strcasecmp(tipo_sql, "BOOLEAN") == 0)
        return v == V_BOOLEANO;
    return 0;
}

int db_insertar(const char *tabla, const ListaCampos *campos, int linea, int columna) {
    Columna cols[MAX_COLUMNAS];
    int     ncols = 0;
    int     usado[MAX_COLUMNAS] = { 0 };
    int    *destino = NULL;
    int     ok = 1;

    if (strncasecmp(tabla, "sqlite_", 7) == 0) {
        error_semantico(linea, columna, "la tabla '%s' es reservada del sistema", tabla);
        return -1;
    }
    
    if (cargar_columnas(tabla, cols, &ncols) != 0) return -1;
    if (ncols == 0) {
        error_semantico(linea, columna, "la tabla '%s' no existe", tabla);
        return -1;
    }
    
    destino = malloc((campos->cantidad + 1) * sizeof *destino);
    if (destino == NULL) exit(2);
    
    for (size_t i = 0; i < campos->cantidad; i++) {
        const Campo *c = &campos->items[i];
        int j;
        destino[i] = -1;
        for (j = 0; j < ncols; j++)
            if (strcasecmp(cols[j].nombre, c->nombre) == 0) break;
    
        if (j == ncols) {
            error_semantico(c->linea, c->columna, "el campo '%s' no existe en la tabla '%s'", c->nombre, tabla);
            ok = 0;
            continue;
        }
        if (usado[j]) {
            error_semantico(c->linea, c->columna, "el campo '%s' aparece más de una vez", c->nombre);
            ok = 0;
            continue;
        }
        usado[j]   = 1;
        destino[i] = j;
    
        if (!tipo_compatible(cols[j].tipo, c->valor->tipo)) {
            error_semantico(c->linea, c->columna, "el campo '%s' es de tipo %s y no admite un valor %s",
                            cols[j].nombre, cols[j].tipo, tipo_valor_nombre(c->valor->tipo));
            ok = 0;
        }
    }
    
    for (int j = 0; j < ncols; j++) {
        if (cols[j].obligatoria && !usado[j]) {
            error_semantico(linea, columna, "falta el campo obligatorio '%s' de la tabla '%s'", cols[j].nombre, tabla);
            ok = 0;
        }
    }
    
    if (!ok) { free(destino); return -1; }
    
    sqlite3_str *s = sqlite3_str_new(bd);
    sqlite3_str_appendf(s, "INSERT INTO \"%w\" (", tabla);
    for (size_t i = 0; i < campos->cantidad; i++)
        sqlite3_str_appendf(s, "%s\"%w\"", i ? "," : "", cols[destino[i]].nombre);
    sqlite3_str_appendall(s, ") VALUES (");
    for (size_t i = 0; i < campos->cantidad; i++)
        sqlite3_str_appendall(s, i ? ",?" : "?");
    sqlite3_str_appendall(s, ")");
    char *sql = sqlite3_str_finish(s);
    
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(bd, sql, -1, &st, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error de base de datos [línea %d, col %d]: %s\n", linea, columna, sqlite3_errmsg(bd));
        free(destino);
        return -1;
    }
    
    for (size_t i = 0; i < campos->cantidad; i++) {
        const Valor *v = campos->items[i].valor;
        int idx = (int)i + 1;
        switch (v->tipo) {
            case V_ENTERO:   sqlite3_bind_int64(st, idx, v->dato.entero); break;
            case V_CADENA:   sqlite3_bind_text(st, idx, v->dato.cadena, -1, SQLITE_TRANSIENT); break;
            case V_BOOLEANO: sqlite3_bind_int(st, idx, v->dato.booleano); break;
        }
    }
    
    rc = sqlite3_step(st);
    free(destino);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error de base de datos [línea %d, col %d]: %s\n", linea, columna, sqlite3_errmsg(bd));
        sqlite3_finalize(st);
        return -1;
    }
    sqlite3_finalize(st);
    estadisticas.insertadas++;
    printf("[OK] línea %d: 1 fila insertada en '%s' (rowid=%lld)\n", linea, tabla, (long long)sqlite3_last_insert_rowid(bd));
    return 0;
}

static size_t largo_utf8(const char *s) {
    size_t n = 0;
    for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

static const char *texto_celda(sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return "NULL";
    const char *decl = sqlite3_column_decltype(st, c);
    if (decl != NULL && strcasecmp(decl, "BOOLEAN") == 0)
        return sqlite3_column_int(st, c) ? "verdadero" : "falso";
    return (const char *)sqlite3_column_text(st, c);
}

static void imprimir_fila(sqlite3_stmt *st, int nc, const size_t ancho[], int es_encabezado) {
    for (int c = 0; c < nc; c++) {
        const char *t = es_encabezado ? sqlite3_column_name(st, c) : texto_celda(st, c);
        int relleno = (int)(ancho[c] - largo_utf8(t));
        printf("| %s%*s ", t, relleno, "");
    }
    printf("|\n");
}

static void mostrar_tabla(const char *nombre) {
    char sql[128];
    sqlite3_stmt *st = NULL;
    size_t ancho[MAX_COLUMNAS] = { 0 };
    int nc, filas = 0;
    snprintf(sql, sizeof sql, "SELECT * FROM \"%s\"", nombre);
    if (sqlite3_prepare_v2(bd, sql, -1, &st, NULL) != SQLITE_OK) return;
    nc = sqlite3_column_count(st);
    if (nc > MAX_COLUMNAS) nc = MAX_COLUMNAS;
    
    for (int c = 0; c < nc; c++) ancho[c] = largo_utf8(sqlite3_column_name(st, c));
    while (sqlite3_step(st) == SQLITE_ROW) {
        filas++;
        for (int c = 0; c < nc; c++) {
            size_t l = largo_utf8(texto_celda(st, c));
            if (l > ancho[c]) ancho[c] = l;
        }
    }
    sqlite3_reset(st);
    printf("\nTabla %s (%d fila%s)\n", nombre, filas, filas == 1 ? "" : "s");
    imprimir_fila(st, nc, ancho, 1);
    for (int c = 0; c < nc; c++) {
        printf("+");
        for (size_t k = 0; k < ancho[c] + 2; k++) putchar('-');
    }
    printf("+\n");
    while (sqlite3_step(st) == SQLITE_ROW) imprimir_fila(st, nc, ancho, 0);
    sqlite3_finalize(st);
}

void db_mostrar_tablas(void) {
    for (int i = 0; i < NUM_TABLAS; i++) mostrar_tabla(TABLAS[i]);
}