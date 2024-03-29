# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-g -Wall -O -std=c99
LDLIBS=-lm -lrt -pthread

# eseguibili da costruire
EXECS=farm

.PHONY: clean

all: $(EXECS)

# dipendenze
farm: farm.o xerrori.o

# target che cancella eseguibili e file oggetto
clean:
	rm -f $(EXECS) *.o  *.zip

# target che crea l'archivio dei sorgenti
zip:
	zip farm.zip makefile *.c *.h
