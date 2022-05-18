#include "xerrori.h"

#include <unistd.h>

#define QUI __LINE__, __FILE__

bool isNumber(char number[]) {
	int i = 0;

	//checking for negative numbers
	if (number[0] == '-')
			i = 1;
	for (; number[i] != 0; i++)
	{
			//if (number[i] > '9' || number[i] < '0')
			if (!isdigit(number[i]))
					return false;
	}
	return true;
}

int main(int argc, char *argv[]) {
	
	// controlla numero argomenti
  if (argc < 2) {
		printf("Uso: %s file [file ...] \n",argv[0]);
		return 1;
  }

  int c;
	int nthread = 4;
	int qlen = 8;
	int delay = 0;

  while ((c = getopt (argc, argv, "n:q:t:")) != -1) {
    switch (c) {
      case 'n':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -n NON è un numero intero!\n", QUI);
				nthread = atoi(optarg);
        break;
      case 'q':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -q NON è un numero intero!\n", QUI);
				qlen = atoi(optarg);
        break;
      case 't':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -t NON è un numero intero!\n", QUI);
				delay = atoi(optarg);
        break;
	 	}
	}

	printf("Il numero di thread da usare è %d \n", nthread);
	printf("La lunghezza del buffer è %d \n", qlen);
	printf("Il delay del programma è %d \n", delay);
	
	
	return 0;
}