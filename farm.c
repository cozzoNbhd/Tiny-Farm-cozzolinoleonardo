#include "xerrori.h"
#include <ctype.h>
#include <unistd.h>

#define QUI __LINE__, __FILE__

int nthread = 4;
int qlen = 8;
int delay = 0;


bool isNumber(char number[]) {
	int i = 0;
	//checking for negative numbers
	if (number[0] == '-')
		i = 1;
	for (; number[i] != 0; i++) {
		//if (number[i] > '9' || number[i] < '0')
		if (!isdigit(number[i]))
			return false;
	}
	return true;
}

// struct generico worker
typedef struct {
	int *cindex; // indice del buffer
	char **buffer; // buffer (array di stringhe contenente i nomi dei file)
	pthread_mutex_t *cmutex; 
	int *somma; // valore che contiene la somma calcolata da ciascun worker
	sem_t *sem_free_slots;
	sem_t *sem_data_items;
} wdati;

// struct del gestore
typedef struct {
	bool *termina;
} gdati;

// funzione eseguita da tutti i workers
void *tbodyw (void *arg) {
	
	wdati *a = (wdati *) arg;

	/*
	// blocco segnali che non voglio ricevere
	sigset_t mask;
  sigfillset(&mask); // insieme di tutti i segnali
  sigdelset(&mask, SIGQUIT); // elimino sigquit
  pthread_sigmask(SIG_BLOCK, &mask, NULL); // blocco tutto tranne sigquit
	*/
	
	char *nome_file;
	FILE *f;
	int e;

	int N; // dimensione dell'array che rappresenta il file di long

	long somma = 0, t;

	do {
		
		xsem_wait(a->sem_data_items, QUI);
		xpthread_mutex_lock(a->cmutex, QUI);
		
		nome_file = a->buffer[*(a->cindex) % qlen];
		*(a->cindex) += 1;

		xpthread_mutex_unlock(a->cmutex, QUI); 
		xsem_post(a->sem_free_slots, QUI);

		// controllo di uscita
		if (strcmp("-1", nome_file) == 0) break;
		
		// processo il dato prelevato
		printf("Ricevuto: %s \n", nome_file);
		
		f = fopen(nome_file, "r");
		if (f == NULL) xtermina("Errore apertura file\n", QUI);

		//calcolo dimensione del file
		e = fseek(f, 0, SEEK_END); //sposto il puntatore in fondo
		if (e != 0) termina("Errore fseek");
		t = ftell(f); 
		if (t < 0) termina("Errore ftell");
	
		N = t / 8;
		
		// ritorno all'inizio del file
		rewind(f);

		long file[N];

		e = fread(file, sizeof(long), N, f);
		if (e != N)
	  	termina("Errore lettura");

		for (int i = 0; i < N; i++) {
			somma += i * file[i];
		}

		printf("Ho calcolato il valore %ld \n", somma);
		
		// accedo alla sezione critica
		xpthread_mutex_lock(a->cmutex, QUI);

		// invio il segnale al thread gestore dei segnali
		// kill(getpid(), SIGINT);

		// esco dalla sezione critica
		xpthread_mutex_unlock(a->cmutex, QUI); 

		// resetto la variabile somma
		somma = 0;
		
	} while (true);

	pthread_exit(NULL);
	
}

// thread che effettua la gestione del segnale SIGINT
void *fgestore(void *arg) {

	gdati *g = (gdati *) arg;
	
	// definisco la maschera dei segnali
	sigset_t mask;
  sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGUSR2);
	
	// variabile in cui memorizzo i segnali che mi arrivano
  int s;
	
  while(true) {
		int e = sigwait(&mask, &s);
		if(e != 0) perror("Errore sigwait");

		printf("Ho ricevuto il segnale SIGINT!\n");
		
		if (s == SIGINT) {
			*(g->termina) = true;
			pthread_exit(NULL);
		}

		if (s == SIGUSR2) {
			pthread_exit(NULL);
		}
  }

  return NULL;
	
}




int main(int argc, char *argv[]) {

	// controlla numero argomenti
  if (argc < 2) {
		printf("Uso: %s file [file ...] \n",argv[0]);
		return 1;
  }

	// blocco i segnali che voglio far gestire il gestore
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

  int c;
	int skip = 0;

  while ((c = getopt (argc, argv, "n:q:t:")) != -1) {
    switch (c) {
      case 'n':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -n NON è un numero intero!\n", QUI);
				nthread = atoi(optarg);
				skip += 2;
        break;
      case 'q':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -q NON è un numero intero!\n", QUI);
				qlen = atoi(optarg);
				skip += 2;
        break;
      case 't':
				if (!isNumber(optarg))
					xtermina("Il valore passato in -t NON è un numero intero!\n", QUI);
				delay = atoi(optarg);
				skip += 2;
        break;
	 	}
	}

	// creazione buffer produttori consumatori e relativi semafori
	char *buffer[qlen];
	sem_t sem_free_slots, sem_data_items;

	// Inizializzazione dei semafori
	xsem_init(&sem_free_slots, 0, qlen, QUI);
	xsem_init(&sem_data_items, 0, 0, QUI);

	// variabili condivise tra i consumatori
	int windex = 0;
	pthread_mutex_t wmutex = PTHREAD_MUTEX_INITIALIZER;
	
	// creo l'array di thread  workers 
	pthread_t workers[nthread];

	// creo l'informazione che deve portare ciascun worker
	wdati a;
	a.buffer = buffer;
	a.cindex = &windex;
	a.cmutex = &wmutex;
	a.sem_data_items = &sem_data_items;
	a.sem_free_slots = &sem_free_slots;

	// variabile di terminazione condivisa tra masterworker e gestore
	bool fine = false;

	// definisco il thread gestore e i dati che gli serviranno
	pthread_t pgestore;
	gdati g;
	g.termina = &fine;

	// lancio il gestore dei segnali
	if (xpthread_create(&pgestore, NULL, fgestore, &g, QUI) != 0)
		termina("Errore creazione thread");

	// lancio i workers
	for (int i = 0; i < nthread; i++) 
		if (xpthread_create(&workers[i], NULL, tbodyw, &a, QUI) != 0)
			termina("Errore creazione thread");

	// variabili che servono al produttore
	int pindex = 0;
	pthread_mutex_t pmutex = PTHREAD_MUTEX_INITIALIZER;

	// inserisco nel buffer i nomi dei file da processare
	for (int j = skip + 1; j < argc ; j++) {
		
		if (fine) break;
		
		xsem_wait(&sem_free_slots, QUI);
		xpthread_mutex_lock(&pmutex, QUI);
		
		buffer[pindex % qlen] = argv[j];
		pindex += 1;

		xpthread_mutex_unlock(&pmutex, QUI);
		xsem_post(&sem_data_items, QUI);

		sleep(delay);
		
	}

	// se sono uscito perché ho processato tutti i nomi di file, termino il gestore
	if (fine == false) {
		pthread_kill(pgestore, SIGUSR2);
	}

	// join del thread gestore
	xpthread_join(pgestore, NULL, QUI);
	
	// invio valore di terminazione ai workers
	for (int i = 0; i < nthread; i++) {
		
		xsem_wait(&sem_free_slots, QUI);
		xpthread_mutex_lock(&pmutex, QUI);
	
		buffer[pindex % qlen] = "-1";
		pindex += 1;
		
		xpthread_mutex_unlock(&pmutex, QUI);
		xsem_post(&sem_data_items, QUI);
		
	}

 	// join dei thread workers
  for (int i = 0; i < nthread; i++)
    xpthread_join(workers[i], NULL, QUI);
	
	return 0;
	
}