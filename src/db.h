#ifndef DB_H
#define DB_H

#include "tipos.h"

int  db_abrir(const char *ruta);
void db_cerrar(void);
int  db_insertar(const char *tabla, const ListaCampos *campos, int linea, int columna);
void db_mostrar_tablas(void);

#endif