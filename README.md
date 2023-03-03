INIZIALIZZAZIONE DEL SERVIZIO DI MESSAGGISTICA

Modulo per la ricerca della syscall table

1) make all                                 ho eliminato l'inserimento automatico di una syscall
2) sudo insmod the_usctm.ko

CONFIGURAZIONE
* configurare nel makefile il numero di blocchi di dati da inserire nell'immagine (sb e inode esclusi)
* in user_hdr.h inserire lo stesso valore del punto precedente in NBLOCKS e cambiare il PATH_TO_IMAGE con il path per raggiungere il file immagine
* in umessage_header.h: scrivere in NBLOCKS il numero di blocchi di dati effettivamente inseriti nell'immagine (valore del punto precedente)
                        scrivere in MAX_BLOCKS il numero massimo di blocchi gestibili dal driver
                        definire FORCE_SYNC se si vuole che la scrittura sul device sia sincrona
                        definire TEST se si vuole eseguire il test con richieste multiple (opzione 6 del codice utente)
* se si vuole rendere persistente il contenuto dei blocchi bisogna eliminare i primi due comandi nel make file (create-fs). Questi due comandi
  vanno eseguiti solo durante la prima creazione
* in user_hdr.h e in umessage_header.h definire PATH_TO_IMAGE con il path per il file immagine

Modulo sviluppato

3) make 
4) make create-fs                           formattazione del block device logico (file)
5) sudo make mount-mod                      definisce l'implementazione di: syscall, device driver e filesystem
                                            chiama get_entries per trovare dove installare le syscall implementate, infine le installa
                                            installa il driver e il filesystem
6) sudo make mount-fs                       monta il filesystem presente sul block device logico con -o loop
