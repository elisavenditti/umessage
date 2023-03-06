# INIZIALIZZAZIONE DEL SERVIZIO DI MESSAGGISTICA

> Per eseguire correttamente il servizio di messagistica, bisogna inserire il modulo the_usctm.ko (sudo insmod the_usctm.ko): è necessario per la ricerca della syscall table.

## Prima di compilare
Bisogna configurare i seguenti parametri:
* in Makefile: scrivere in NBLOCKS il numero di blocchi di dati da inserire nell'immagine (sb e inode esclusi)
* in user_hdr.h: 
  1) in NBLOCKS inserire lo stesso valore del punto precedente;
  2) cambiare il PATH_TO_IMAGE con il path corretto per raggiungere il file immagine
* in umessage_header.h: 
  1) scrivere in NBLOCKS il numero di blocchi di dati effettivamente inseriti nell'immagine (stesso valore del punto precedente)
  2) scrivere in MAX_BLOCKS il numero massimo di blocchi gestibili dal device driver
  3) definire FORCE_SYNC se si vuole che la scrittura sul device sia sincrona
  4) definire TEST se si vuole eseguire il test con richieste multiple (opzione 6 del codice utente)
  5) cambiare il PATH_TO_IMAGE con il path corretto per raggiungere il file immagine



## Compilazione del modulo

1) compilare: 
        ```make```
2) formattare il file immagine (block device logico) per ospitare il file system: ```make create-fs```.
    Se si vuole rendere persistente il contenuto dei blocchi bisogna eliminare i primi due comandi di create-fs nel make file. Questi vanno eseguiti solo durante la prima creazione per formattare il file immagine.


3) inserire il modulo sviluppato con ```sudo make mount-mod```. In questo modo vengono registrati le syscall, il device driver e il filesystem
4) [_opzionale_] monta il filesystem presente sul block device logico con -o loop: ```sudo make mount-fs```.
