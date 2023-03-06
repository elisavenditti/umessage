# INIZIALIZZAZIONE DEL SERVIZIO DI MESSAGGISTICA

Per eseguire correttamente il servizio di messagistica, bisogna inserire il modulo the_ustm.ko (sudo insmod the_usctm.ko): è necessario per la ricerca della syscall table.

Prima di compilare, bisogna configurare i seguenti parametri:
* makefile: scrivere in NBLOCKS il numero di blocchi di dati da inserire nell'immagine (sb e inode esclusi)
* in user_hdr.h: 
  1) in NBLOCKS inserire lo stesso valore del punto precedente;
  2) cambiare il PATH_TO_IMAGE con il path corretto per raggiungere il file immagine
* in umessage_header.h: 
  1) scrivere in NBLOCKS il numero di blocchi di dati effettivamente inseriti nell'immagine (stesso valore del punto precedente)
  2) scrivere in MAX_BLOCKS il numero massimo di blocchi gestibili dal device driver
  3) definire FORCE_SYNC se si vuole che la scrittura sul device sia sincrona
  4) definire TEST se si vuole eseguire il test con richieste multiple (opzione 6 del codice utente)
  5) cambiare il PATH_TO_IMAGE con il path corretto per raggiungere il file immagine

Se si vuole rendere persistente il contenuto dei blocchi bisogna eliminare i primi due comandi nel make file (create-fs). Questi due comandi vanno eseguiti solo durante la prima creazione perchè servono per la formattazione iniziale del file immagine



Modulo sviluppato

1) make 
2) make create-fs                           formattazione del block device logico (file)
3) sudo make mount-mod                      definisce l'implementazione di: syscall, device driver e filesystem
                                            chiama get_entries per trovare dove installare le syscall implementate, infine le installa
                                            installa il driver e il filesystem
4) sudo make mount-fs                       monta il filesystem presente sul block device logico con -o loop
