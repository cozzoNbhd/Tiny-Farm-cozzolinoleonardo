# Tiny-Farm-cozzolinoleonardo
Progetto finale anno 2021/22 per chi ha superato gli esoneri.

Il progetto consiste in 2 processi: un processo MasterWorker (che riceve in ingresso uno o più file di tipo long, nonché dei parametri opzionali), e un processo 
collector che funge da Server.

Il processo Master Worker inizialmente genera un numero a scelta di thread ausiliari Worker e un thread aggiuntivo con l'unico compito di
catturare 2 tipi diversi di segnali: il segnale SIGINT (CTRL-C), inviato dall'utente mediante tastiera, e il segnale SIGUSR2, inviato internamente dal processo Master.

Il processo Master Worker ha successivamente il compito di inviare ai thread ausiliari i nomi dei file ricevuti in ingresso, ed esso avviene mediante un buffer condiviso
stile produttore-consumatore. Per riuscire a far comunicare correttamente il processo Master Worker e i vari Thread ausiliari tra loro sono stati utilizzati 
dei semafori e della variabili di mutua esclusione.

I Thread Ausiliari Worker, una volta prelevato dal buffer un generico nome file, si occupano di aprirlo e di calcolare, mediante una formula specifica, un risultato 
che dipende dal contenuto del file. Quest'ultimi, poi, riusciranno a capire quando sono finiti i file da processare mediante l'inserimento nel buffer di un valore 
speciale da parte del processo Master Worker.

Il valore calcolato verrà poi inviato al Server Collector, che si occuperà di stamparlo insieme al nome del file a cui fa riferimento.

Naturalmente il Server è progettato per accettare e gestire richieste simultanee di comunicazione, mediante la creazione di un thread per ogni richiesta ricevuta.

Il metodo con cui il generico Thread ausiliario e il Server comunicano è il seguente:

1) Il Thread invia al Server 2 interi: il numero di cifre del risultato appena calcolato e la dimensione del nome del file relativo al risultato.
2) Il Server, una volta ricevuti suddetti interi, invia un messaggio di conferma al Thread
3) Il Thread, non appena ha ricevuto il messaggio di conferma, invia le informazioni vere e proprie, conscio del fatto che il Server sa quanti byte aspettarsi
4) Il Server riceve le informazioni e infine stampa su stdout il risultato.

Una volta che i file sono stati processati tutti, il processo Master Worker invia 2 valori speciali per far capire al Server che non ci sono più dati da ricevere, e di
conseguenza verrà interrotto.

Se però l'utente, durante l'esecuzione, invia il segnale SIGINT, esso ha l'effetto di risvegliare il Thread gestore dei segnali e di far terminare anticipatamente il 
processo Master Worker, non riuscendo di conseguenza a processare tutti i file ricevuti da linea di comando. 

Il Server, i Thread Ausiliari e il Thread gestore verranno comunque interrotti, a prescindere dall'avvenuta ricezione del Segnale.
