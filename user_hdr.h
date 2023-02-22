#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"  /* Device file name in /dev/ - not mandatory  */
#define DEFAULT_BLOCK_SIZE 4096
#define METADATA_SIZE sizeof(void*)
#define DATA_SIZE DEFAULT_BLOCK_SIZE - METADATA_SIZE
#define NBLOCKS 5
#define change_validity(ptr)    (void*)((unsigned long) ptr^MASK)
#define MASK 0x8000000000000000




char* testo[] = {
"Ei fu. Siccome immobile, Dato il mortal sospiro, Stette la spoglia immemore Orba di tanto spiro, Così percossa, attonita La terra al nunzio sta,",
"Muta pensando all’ultima Ora dell’uom fatale; Nè sa quando una simile Orma di piè mortale La sua cruenta polvere A calpestar verrà.", 
"Lui folgorante in solio Vide il mio genio e tacque; Quando, con vece assidua, Cadde, risorse e giacque, Di mille voci al sonito Mista la sua non ha:",
"Vergin di servo encomio E di codardo oltraggio, Sorge or commosso al subito Sparir di tanto raggio: E scioglie all’urna un cantico Che forse non morrà.",
"Dall’Alpi alle Piramidi, Dal Manzanarre al Reno, Di quel securo il fulmine Tenea dietro al baleno; Scoppiò da Scilla al Tanai, Dall’uno all’altro mar." };


#endif