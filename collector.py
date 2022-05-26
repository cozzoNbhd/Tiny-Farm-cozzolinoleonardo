#! /usr/bin/env python3
# server che fornisce l'elenco dei primi in un dato intervallo 
# gestisce più clienti contemporaneamente usando i thread
import sys, struct, socket, threading, os, signal

# host e porta di default
HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 65399  # Port to listen on (non-privileged ports are > 1023)

def main(host = HOST, port = PORT):
  # creiamo il server socket
  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:
      s.bind((host, port))
      s.listen()
      while True:
			  # mi metto in attesa di una connessione
        conn, addr = s.accept()
			  # lavoro con la connessione appena ricevuta
        t = threading.Thread(target=gestisci_connessione, args=(conn,addr))
        t.start()
    except KeyboardInterrupt:
      pass
    s.shutdown(socket.SHUT_RDWR)


# gestisci una singola connessione con un client	
def gestisci_connessione(conn,addr):

  with conn:

		# Mi aspetto inizialmente di ricevere 8 byte
		# i 4 byte più significativi rappresentano il numero di cifre della somma
		# i 4 byte meno significativi rappresentano la dimensione del nome del file
    data = recv_all(conn, 8)
    assert len(data) == 8

		# faccio l'unpack degli 8 byte nel formato !i (in quanto ho ricevuto interi)
    num_cifre = struct.unpack("!i",data[:4])[0]
    dim_file = struct.unpack("!i",data[4:])[0]

		# se il numero di cifre e la dimensione del file sono entrambi uguali a -1,
		# capisco di aver ricevuto il segnale di terminazione dal processo MasterWorker
    if num_cifre == -1 and dim_file == -1:
			# attaverso una funzione del modulo os, termino il thread corrente e invio
			# un signale SIGINT al Thread Main (che è impostato per catturare le 
			# Keyboard Interruption - tra cui SIGINT). Di consequenza farò terminare il
			# server
      os.kill(os.getpid(), signal.SIGINT)
			
    else: # altrimenti
			# invio il valore di conferma al Worker che mi ha inviato le dimensioni,
			# in modo che esso capisca che sono pronto a ricevere i dati effettivi
      conn.sendall(struct.pack("!i", 1))

			# inizializzo size come la somma delle dimensioni
      size = num_cifre + dim_file

			# mi aspetto di ricevere esattamente size byte dal Worker
      data = recv_all(conn, size)
      assert len(data) == size

			# eseguo l'unpack del buffer
			# i primi num_cifre byte saranno i byte che rappresentano la somma
			# tutti i byte che vanno da num_cifre fino all'ultimo elemento sono quelli
			# che rappresentano il nome del file
      somma = struct.unpack(f"{num_cifre}s", data[:num_cifre])[0]
      nome_file = struct.unpack(f"{dim_file}s", data[num_cifre:])[0]

			# decodifico nel formato utf-8 le due variabili
      somma = somma.decode("utf-8")
      nome_file = nome_file.decode("utf-8")

			# stampo il risultato su stdout
      print(f"{somma} {nome_file}", file = sys.stdout)


# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn che abbiamo visto nel C
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks
 


# restituisce lista dei primi in [a,b]
def elenco_primi(a,b):
  ris = []
  for i in range(a,b+1):
    if primo(i):
      ris.append(i);
  return ris


# dato un intero n>0 restituisce True se n e' primo
# False altrimenti
def primo(n):
    assert n>0, "L'input deve essere positivo"
    if n==1:
        return False
    if n==2:
        return True
    if n%2 == 0:
        return False
    assert n>=3 and n%2==1, "C'e' qualcosa che non funziona"
    for i in range(3,n//2,2):
        # fa attendere solamente questo thread 
        # threading.Event().wait(.5)
        if n%i==0:
            return False
        if i*i > n:
            break    
    return True


if len(sys.argv)==1:
  main()
elif len(sys.argv)==2:
  main(sys.argv[1])
elif len(sys.argv)==3:
  main(sys.argv[1], int(sys.argv[2]))
else:
  print("Uso:\n\t %s [host] [port]" % sys.argv[0])



