#include "xerrori.h"
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define HOST "127.0.0.1"
#define PORT 65432

#define QUI __LINE__, __FILE__

int nthread = 4;
int qlen = 8;
int delay = 0;

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n) {
 	size_t nleft;
 	ssize_t nread;
 
 	nleft = n;
 	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (nleft == n) return -1; /* error, return -1 */
				else break; /* error, return amount read so far */
		} else if (nread == 0) break; /* EOF */
	 	nleft -= nread;
	 	ptr   += nread;
 	}
 	return (n - nleft); /* return >= 0 */
}


/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n) {  
	size_t nleft;
 	ssize_t nwritten;
 
 	nleft = n;
 	while (nleft > 0) {
	 	if ((nwritten = write(fd, ptr, nleft)) < 0) {
			if (nleft == n) return -1; /* error, return -1 */
			else break; /* error, return amount written so far */
	 	} else if (nwritten == 0) break; 
	 	nleft -= nwritten;
	 	ptr   += nwritten;
 	}
 	return(n - nleft); /* return >= 0 */
}

typedef unsigned char BYTE;

// funzione per convertire una stringa in un array di byte
void encode(char* input, BYTE* output) {
	
	int loop;
	int i;
	
	loop = 0;
	i = 0;
	
	while (input[loop] != '\0') {
		output[i++] = input[loop++];
	}

}

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

int numeroCifre(long long n) {  
	int counter=0; // variable declaration  
	while (n != 0) {  
		n = n / 10;  
		counter++;  
	}  
	return counter;  
}


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

	// ----------------- dati relativi al socket -----------
	size_t e1;
	int tmp;
	
	int dim;
	

	int fd_skt_w = 0; // file descriptor associato al socket
	struct sockaddr_in serv_addr;

	// crea socket
	if ((fd_skt_w = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		termina("Errore creazione socket");

	// assegna indirizzo
	serv_addr.sin_family = AF_INET;

	// il numero della porta deve essere convertito in network order 
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(HOST);
	
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

		// calcolo la somma con la formula
		for (int i = 0; i < N; i++) {
			somma += i * file[i];
		}
		
		// -------- invio i valori al server ------------

		dim = strlen(nome_file);
		
		// apre connessione
		if (connect(fd_skt_w, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
			xtermina("Errore apertura connessione! \n", QUI);
		}	

		// ottengo il numero di cifre di somma (convertendola inizialmente a string
		// in modo tale da riuscire a catturare correttamente il segno
		char temp[100];
		sprintf(temp, "%ld", somma);
		int cif = strlen(temp);
		
		// invio il numero di cifre della somma
		tmp = htonl(cif);
	  e1 = writen(fd_skt_w, &tmp, sizeof(tmp));
	  if (e1 != sizeof(int)) termina("Errore write");

		// invio la dimensione del file
		tmp = htonl(dim);
	  e1 = writen(fd_skt_w, &tmp, sizeof(tmp));
	  if (e1 != sizeof(int)) termina("Errore write");

		// aspetto il check dell'avvenuto ricevimento delle dimensioni
		e1 = readn(fd_skt_w, &tmp, sizeof(tmp));
	  if (e1 != sizeof(int)) termina("Errore read");
	  int n = ntohl(tmp);

		// if (n != 1) xtermina("Hack??", QUI);
		assert(n == 1);

		// inizializzo la variabile size come la somma delle dimensioni
		int size = cif + dim;

		// preparo il buffer contenente i dati da inviare
		char *buffer = malloc(size * sizeof(char));
		sprintf(buffer, "%ld%s", somma, nome_file);

		// invio il buffer contenente i dati al server
	  e1 = writen(fd_skt_w, buffer, size);
	  if (e1 != size) termina("Errore write");

		// chiudo la connessione
		if (close(fd_skt_w) < 0)
			perror("Errore chiusura socket");
		
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
		
		if (s == SIGINT) {
			printf("\nHo ricevuto il segnale SIGINT, terminazione in corso...\n");
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

	// --- invio segnale di terminazione al server ---
	
	int fd_skt = 0; // file descriptor associato al socket
  struct sockaddr_in serv_addr;

  // crea socket
  if ((fd_skt = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    termina("Errore creazione socket");

  // assegna indirizzo
  serv_addr.sin_family = AF_INET;

  // il numero della porta deve essere convertito in network order 
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = inet_addr(HOST);

	
	
  // apre connessione
	if (connect(fd_skt, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		xtermina("Errore apertura connessione! \n", QUI);
	}

	size_t e;
	int tmp;

	
	tmp = htonl(-1);
	e = writen(fd_skt, &tmp, sizeof(tmp));
	if (e != sizeof(int)) termina("Errore write");

	tmp = htonl(-1);
	e = writen(fd_skt, &tmp, sizeof(tmp));
	if (e != sizeof(int)) termina("Errore write");
	
	// chiudo la connessione
  if (close(fd_skt) < 0)
    perror("Errore chiusura socket");

	
	return 0;
	
}