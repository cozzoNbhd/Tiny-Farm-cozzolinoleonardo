#include "xerrori.h"
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define HOST "127.0.0.1"
#define PORT 65399

#define QUI __LINE__, __FILE__

// Variabili globali utilizzate nel programma
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

// Funzione per verificare se una data stringa o carattere rappresenta un numero
bool isNumber(char number[]) {
	int i = 0;
	//checking for negative numbers
	if (number[0] == '-')
		i = 1;
	for (; number[i] != 0; i++) {
		if (!isdigit(number[i]))
			return false;
	}
	return true;
}

// struct generico worker
typedef struct {
	int *cindex; // indice del buffer
	char **buffer; // buffer (array di stringhe contenente i nomi dei file)
	pthread_mutex_t *cmutex; // per gestire la mutua esclusione
	int *somma; // valore che contiene la somma calcolata da ciascun worker
	sem_t *sem_free_slots; // semaforo per verificare che all'interno del buffer ci siano slot liberi
	sem_t *sem_data_items; // semaforo per verificare che nel buffer ci siano elementi da consumare
} wdati;

// struct del gestore
typedef struct {
	bool *termina; // per stabilire quando deve terminare
} gdati;


// funzione eseguita da tutti i workers
void *tbodyw (void *arg) {

	// Eseguo il cast alla struct wdati
	wdati *a = (wdati *) arg;

	// blocco i segnali che voglio far gestire il gestore
	sigset_t mask; // definisco la maschera dei segnali
	sigemptyset(&mask); // inizializzo la maschera vuota
	sigaddset(&mask, SIGINT); // aggiungo alla maschera il segnale SIGINT
	sigaddset(&mask, SIGUSR2); // aggiungo alla maschera il segnale SIGUSR2
	pthread_sigmask(SIG_BLOCK, &mask, NULL); // specifico al thread worker che deve bloccare i segnali
	// specificati dalla maschera, in questo caso solo SIGINT e SIGUSR2

	char *nome_file; // variabile in cui salvo il nome del file prelevato dal buffer
	FILE *f; // variabile di tipo FILE che uso per aprire il file
	int e; // variabile in cui memorizzo il risultato di molteplicità funzioni di libreria

	int N; // dimensione dell'array che rappresenta il file di long

	long somma = 0, t; // variabile in cui memorizzo la somma calcolata dal generico worker e variabile
	// in cui salvo il numero di byte contenuti all'interno del file che apro

	// ----------------- dati relativi al socket -----------
	size_t e1; // variabile che uso per memorizzare il risultato delle chiamate relative al socket
	int tmp; // variabile in cui memorizzo l'informazione che voglio mandare sotto forma di byte
	 
	int dim; // variabile in cui memorizzo la dimensione del file da processare
	
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
		
		xsem_wait(a->sem_data_items, QUI); // aspetto che ci siano elementi da prelevare
		xpthread_mutex_lock(a->cmutex, QUI); // accedo alla sezione critica
		
		nome_file = a->buffer[*(a->cindex) % qlen]; // prelevo l'elemento dal buffer
		*(a->cindex) += 1; // incremento il contatore

		xpthread_mutex_unlock(a->cmutex, QUI); // esco dalla sezione critica
		xsem_post(a->sem_free_slots, QUI); // avverto gli altri worker che c'è uno slot libero 

		// controllo di uscita
		if (strcmp("-1", nome_file) == 0) break; // se ho ricevuto la stringa speciale "-1", esco dal ciclo
		
		// ---- processo il dato prelevato ----

		// Apro il file
		f = fopen(nome_file, "r");
		if (f == NULL) xtermina("Errore apertura file\n", QUI);

		// calcolo la dimensione (numero di caratteri) del nome del file (mi servirà dopo...)
		dim = strlen(nome_file);

		//calcolo dimensione del file
		e = fseek(f, 0, SEEK_END); //sposto il puntatore in fondo al file
		if (e != 0) termina("Errore fseek");
		t = ftell(f); // calcolo il numero di byte del file binario dall'inizio del file fino alla fine
		if (t < 0) termina("Errore ftell");
	
		N = t / 8; // trovo il numero di elementi dividendo il numero di byte per 8 (dimensione di un long)
		
		// ritorno all'inizio del file
		rewind(f);

		// inizializzo un array di long che conterrà tutti i long del file
		long file[N];

		e = fread(file, sizeof(long), N, f); // leggo dal file f esattamente N long e memorizzo il contenuto nell'array file
		if (e != N)
	  	termina("Errore lettura");

		// calcolo la somma con la formula
		for (int i = 0; i < N; i++)
			somma += i * file[i];
		
		// -------- invio i valori al server ------------
	
		// apre connessione
		if (connect(fd_skt_w, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
			xtermina("Errore apertura connessione! \n", QUI);
		}	

		// ottengo il numero di cifre di somma (convertendo somma inizialmente a string in modo tale da riuscire a catturare correttamente il segno
		char temp[100]; // 100 è un valore che mi fa stare tranquillo (difficile che avrò un long che ha 100 cifre!)
		sprintf(temp, "%ld", somma); // mediante la funzione sprintf, memorizzo all'interno di temp il valore somma
		int cif = strlen(temp); // dal momento che dopo la sprintf la dimensione di temp diventa pari alla dimensione del numero appena inserito
		// (ossia al numero di cifre di somma), posso memorizzare all'interno della variabile cif la dimensione di temp
		
		// invio il numero di cifre della somma
		tmp = htonl(cif); // converto il numero cif in byte
	  e1 = writen(fd_skt_w, &tmp, sizeof(tmp)); // invio mediante il socket il contenuto di cif
	  if (e1 != sizeof(int)) termina("Errore write");

		// invio la dimensione del file
		tmp = htonl(dim); // converto la dimensione del file in byte
	  e1 = writen(fd_skt_w, &tmp, sizeof(tmp)); // invio mediante il socket il contenuto di dim
	  if (e1 != sizeof(int)) termina("Errore write");

		// aspetto il check dell'avvenuto ricevimento delle dimensioni da parte del Server
		e1 = readn(fd_skt_w, &tmp, sizeof(tmp)); // leggo mediante il socket e memorizzo all'interno di temp il contenuto
	  if (e1 != sizeof(int)) termina("Errore read");
	  int n = ntohl(tmp); // converto mediante ntohl i byte in un valore intero

		assert(n == 1); // Il server mi invierà il valore 1 come check

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
		
		int e = sigwait(&mask, &s); // attendo l'arrivo dei segnali specificati dalla maschera
		if(e != 0) perror("Errore sigwait");
		
		if (s == SIGINT) { // se ricevo il segnale SIGINT (CTRL-C)
			printf("\nHo ricevuto il segnale SIGINT, terminazione in corso...\n");
			*(g->termina) = true; // setto la variabile termina a true
			pthread_exit(NULL); // termino
		}

		if (s == SIGUSR2) { // se ricevo il segnale SIGUSR2
			pthread_exit(NULL); // termino
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

	// variabili che utilizzo per gestire i parametri opzionali
  int c; // variabile in cui memorizzo il tipo di opzione
	int skip = 0; // variabile in cui memorizzo il numero di opzioni specificate
	// ogni volta che trovo un parametro opzionale, incremento di 2 la variabile skip
	// questo mi servirà quando dovrò scorrere l'elenco di file, in quanto dovrò partire da skip fino ad
	// arrivare alla fine di argv

	// Gestione dei parametri opzionali
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
		
		if (fine) break; // se la variabile fine è stata messa a 1 dal gestore, vuol dire che devo smettere di
		// processare i file (anche se non sono finiti)
		
		xsem_wait(&sem_free_slots, QUI);
		xpthread_mutex_lock(&pmutex, QUI);
		
		buffer[pindex % qlen] = argv[j]; // inserisco nel buffer uno ad uno i nomi dei file
		pindex += 1;

		xpthread_mutex_unlock(&pmutex, QUI);
		xsem_post(&sem_data_items, QUI);

		sleep(delay);
		
	}

	// se sono uscito perché ho processato tutti i nomi di file (quindi la variabile fine è rimasta a false) termino 
	// il gestore inviandogli un segnale SIGUSR2
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

	// Invio i valori speciali -1 e -1 per far terminare il server
	
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