INIZIALIZZAZIONE DEL SERVIZIO DI MESSAGGISTICA

Modulo per la ricerca della syscall table

1) make all                                 ho eliminato l'inserimento automatico di una syscall
2) sudo insmod the_usctm.ko


Modulo sviluppato

3) make
4) make create-fs                           formattazione del block device logico (file)
5) sudo make mount                          definisce l'implementazione di: syscall, device driver e filesystem
                                            chiama get_entries per trovare dove installare le syscall implementate, infine le installa
                                            installa il driver e il filesystem
6) make mount-fs                            monta il filesystem presente sul block device logico