#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tipos.h"
#include "db.h"
#include "parser.tab.h"

Estadisticas estadisticas = { 0, 0, 0, 0, 0 };

void parser_iniciar(void);
void parser_liberar(void);
void lexer_reiniciar_posicion(void);

int main(int argc, char **argv) {
    const char *ruta_bd = "taller.db";
    const char *texto   = NULL;
    int limpiar = 0, silencioso = 0, opt;
    
    while ((opt = getopt(argc, argv, "e:d:lqh")) != -1) {
        switch (opt) {
            case 'e': texto = optarg; break;
            case 'd': ruta_bd = optarg; break;
            case 'l': limpiar = 1; break;
            case 'q': silencioso = 1; break;
            case 'h': return 0;
        }
    }

    YY_BUFFER_STATE buffer_texto = NULL;
    FILE *entrada = NULL;

    if (texto != NULL) {
        buffer_texto = yy_scan_string(texto);
    } else if (optind < argc) {
        entrada = fopen(argv[optind], "r");
        if (!entrada) { perror(argv[optind]); return 2; }
        yyin = entrada;
    } else {
        yyin = stdin;
    }

    if (limpiar) remove(ruta_bd);
    if (db_abrir(ruta_bd) != 0) return 2;

    parser_iniciar();
    lexer_reiniciar_posicion();

    yyparse();

    printf("\n--- RESUMEN ---\n");
    printf("Insertadas: %d\n", estadisticas.insertadas);
    printf("Errores lexicos: %d\n", estadisticas.errores_lexicos);
    printf("Errores sintacticos: %d\n", estadisticas.errores_sintacticos);
    
    if (!silencioso) db_mostrar_tablas();

    parser_liberar();
    db_cerrar();
    if (buffer_texto) yy_delete_buffer(buffer_texto);
    if (entrada) fclose(entrada);
    
    return 0;
}