#include "xerrori.h"

int main(int argc, char *argv[]) {
	
	// controlla numero argomenti
  if (argc < 2) {
		printf("Uso: %s file [file ...] \n",argv[0]);
		return 1;
  }

	int nthreadflag = 0;
  int qlenflag = 0;
	int delatyflag = 0;

	int nthread = 0;
	int qlen = 0;
	int delay = 0;

  int c;
	char *value;


  while ((c = getopt (argc, argv, value)) != -1) {
    switch (c) {
      case 'n':
        nthreadflag = 1;
				printf("%s", value);
        break;
      case 'q':
        qlenflag = 1;
				printf("%s", value);
        break;
      case 't':
        delatyflag = 0;
				printf("%s", value);
        break;
		}
	}
	
	return 0;
}