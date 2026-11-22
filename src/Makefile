all: analizador

analizador: lexer.l parser.y db.c main.c tipos.c
	bison -d parser.y
	flex lexer.l
	gcc parser.tab.c lex.yy.c db.c main.c tipos.c -o analizador -lsqlite3

clean:
	rm -f analizador parser.tab.c parser.tab.h lex.yy.c taller.db