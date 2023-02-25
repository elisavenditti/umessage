#ifndef _UMESSAGE_H
#define _UMESSAGE_H

#define MODNAME "MESSAGE KEEPER"
#define AUDIT if(1)
#define DEVICE_NAME "umessage"
#define DEFAULT_BLOCK_SIZE 4096
#define METADATA_SIZE sizeof(unsigned int)
#define DATA_SIZE DEFAULT_BLOCK_SIZE - METADATA_SIZE
#define change_validity(ptr)    (void*)((unsigned long) ptr^MASK)
#define MASK 0x8000000000000000
#define PATH_TO_IMAGE "/home/elisa/Scrivania/umessage/image"


// integer define
#define VALID_MASK 0x80000000
#define INVALID_MASK (~VALID_MASK)
#define set_valid(i)          ((unsigned int) (i) | VALID_MASK)
#define set_invalid(i)        ((unsigned int) (i) & INVALID_MASK)
#define get_validity_int(i)   ((unsigned int) (i) >> (sizeof(unsigned int) * 8 - 1))

char* testo[] = {
"Ei fu. Siccome immobile, Dato il mortal sospiro, Stette la spoglia immemore Orba di tanto spiro, Così percossa, attonita La terra al nunzio sta,",
"Muta pensando all’ultima Ora dell’uom fatale; Nè sa quando una simile Orma di piè mortale La sua cruenta polvere A calpestar verrà.", 
"Lui folgorante in solio Vide il mio genio e tacque; Quando, con vece assidua, Cadde, risorse e giacque, Di mille voci al sonito Mista la sua non ha:",
"Vergin di servo encomio E di codardo oltraggio, Sorge or commosso al subito Sparir di tanto raggio: E scioglie all’urna un cantico Che forse non morrà.",
"Dall’Alpi alle Piramidi, Dal Manzanarre al Reno, Di quel securo il fulmine Tenea dietro al baleno; Scoppiò da Scilla al Tanai, Dall’uno all’altro mar.",
"ciao sono lucia e sono una sirena", "può sembrare strano ma è una storia vera", "la leggenda su di noi è già la verità ...", 
"dragon ball gt, siamo tutti qui", "non c'è un drago più super di così", "dragon ball perchè, ogni sfera è ...", "l'energia che risplende in te!" };


#endif