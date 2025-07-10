#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>   
#include <sys/times.h>
#include <sys/stat.h> 

typedef struct{
    int codice;
    char* nome;
    int anno;
    int numcop;
    int* cop;
} attore;

typedef struct{
    char** arr;
    int size;
    int index_pr;
    int index_cns;
    int nelem;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int finito;
} buffer;

typedef struct{
    buffer* buf;
    attore* attori;
    int num_att;
} args_consumatori;

typedef struct{
    buffer* buf;
    const char* filename;
} args_produttore;

typedef struct{
    int cod1;
    int cod2;
    attore* attori;
    int num_att;
} args_richiesta;

typedef struct nodo_abr{
    int codice;
    int padre;
    struct nodo_abr* left;
    struct nodo_abr* right;
} nodo_abr;

typedef struct nodo_coda{
    int codice;
    struct nodo_coda* next;
} nodo_coda;

typedef struct{
    nodo_coda* inizio;
    nodo_coda* fine;
} coda_FIFO;

typedef struct{
    int fase_pipe;
    int fase_programma;
    int termina_pipe;
    pthread_mutex_t mutex_fase;
    buffer* buf;
    attore* attori;
    int num_att;
    pthread_t* arr_cons;
} stato_seg;

void cleanup(stato_seg* stato) {
    for (int i = 0; i < stato->num_att; i++) {
        free(stato->attori[i].nome);
        free(stato->attori[i].cop);
    }
    free(stato->attori);
    free(stato->buf->arr);
    free(stato->arr_cons);
    pthread_mutex_destroy(&stato->buf->mutex);
    pthread_cond_destroy(&stato->buf->not_empty);
    pthread_cond_destroy(&stato->buf->not_full);
    pthread_mutex_destroy(&stato->mutex_fase);
}

void* gestore_segnali(void* arg){
    stato_seg* stato = (stato_seg*)arg;
    fprintf(stdout, "PID processo: %d\n", getpid());
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);
    while(1){
        int segnale;
        if(sigwait(&mask, &segnale) != 0){
            perror("sigwait fallita");
            exit(EXIT_FAILURE);
        }
        int fase_pipe;
        int fase_programma;
        pthread_mutex_lock(&stato->mutex_fase);      
        fase_pipe = stato->fase_pipe;  
        fase_programma = stato->fase_programma;
        pthread_mutex_unlock(&stato->mutex_fase);    

        if(fase_programma){
            fprintf(stderr, "non hai fatto CTRL C dopo apertura pipe\n");
            return NULL;
        }
        if (fase_pipe == 0) {
            fprintf(stdout, "  Costruzione del grafo in corso\n");
            fflush(stdout);
            continue;
        }
        else{
            fprintf(stderr, "  SIGINT ricevuto dopo l'inizio della pipe. Terminazione...\n");
            pthread_mutex_lock(&stato->mutex_fase);
            stato->termina_pipe = 1;
            pthread_mutex_unlock(&stato->mutex_fase);
            return NULL;
        }
    }
}

int shuffle (int n){
    return ((((n & 0x3F) << 26) | ((n>>6) & 0x3FFFFFF)) ^ 0x55555555);
}

int unshuffle(int n) {
    return ((((n >> 26) & 0x3F) | ((n & 0x3FFFFFF) << 6)) ^ 0x55555555);
}

int cmp(const void* a, const void* b){
    int codice_ricercato = *(const int*)a;
    const attore* att = (const attore*)b;
    if(codice_ricercato < att->codice){
        return -1;
    }
    else if(codice_ricercato > att->codice){
        return 1;
    }
    else return 0;
}

int buffer_init(buffer* buf, int cap){
    buf->size = cap;
    buf->arr = malloc(buf->size* sizeof(char*));
    if(buf->arr == NULL){
        perror("Errore nell'allocazione del buffer");
        return -1;
    }
    buf->index_pr = 0;
    buf->index_cns = 0;
    buf->nelem = 0;
    buf->finito = 0;
    if(pthread_mutex_init(&buf->mutex, NULL) != 0){
        perror("mutex fallita");
        free(buf->arr);
        return -1;
    }
    if(pthread_cond_init(&buf->not_empty, NULL) != 0){
        perror("condition variable not_empy fallita");
        free(buf->arr);
        pthread_mutex_destroy(&buf->mutex);
        return -1;
    }
    if(pthread_cond_init(&buf->not_full, NULL) != 0){
        perror("condition variable not_full fallita");
        free(buf->arr);
        pthread_mutex_destroy(&buf->mutex);
        pthread_cond_destroy(&buf->not_empty);
        return -1;
    }
    return 0;
}

char* buffer_extract(buffer* buf){
    if (pthread_mutex_lock(&buf->mutex) != 0) {
        perror("Errore lock mutex in extract");
        return NULL;
    }
    while(buf->nelem == 0 && buf->finito == 0){
        if(pthread_cond_wait(&buf->not_empty, &buf->mutex) != 0){
            perror("Errore nella wait in extract");
            pthread_mutex_unlock(&buf->mutex);  
            return NULL;           
        }
    }
    if(buf->nelem == 0 && buf->finito == 1){
        pthread_mutex_unlock(&buf->mutex);
        return NULL;
    }
    char* line = buf->arr[buf->index_cns];
    buf->index_cns = (buf->index_cns + 1) % buf->size;
    buf->nelem --;
    if (pthread_cond_signal(&buf->not_full) != 0) {
        perror("Errore cond_signal not_full in extract");
        pthread_mutex_unlock(&buf->mutex);
        return NULL;
    }

    if (pthread_mutex_unlock(&buf->mutex) != 0) {
        perror("Errore unlock mutex in extract");
        return NULL;
    }
    return line;
}

int buffer_insert(buffer* buf, char* line){
if(pthread_mutex_lock(&buf->mutex) != 0){
    perror("Errore lock mutex in insert");
        return -1;
    }
    while(buf->nelem == buf->size){
        if(pthread_cond_wait(&buf->not_full, &buf->mutex) != 0){
            perror("Errore nella wait in insert");
            pthread_mutex_unlock(&buf->mutex); 
            return -1;
        }
    }
    buf->arr[buf->index_pr] = line;
    buf->index_pr= (buf->index_pr + 1) % buf->size;
    buf->nelem ++;
    if(pthread_cond_signal(&buf->not_empty) != 0){
        perror("Errore nella signal in insert");
        pthread_mutex_unlock(&buf->mutex);
        return -1;
    }

    if(pthread_mutex_unlock(&buf->mutex) != 0){
        perror("Errore unlock mutex in insert");
        return -1;
    }
    return 0;
}

void* consumatore(void*arg){
    args_consumatori* args = (args_consumatori*) arg;
    buffer* buf = args->buf;
    attore* attori = args->attori;
    int num_att = args->num_att;
    while(1){
        char* line = buffer_extract(buf);
        if(line == NULL){
            break;;
        }
        char* privato = NULL;
        char* token = strtok_r(line, "\t",&privato);
        if (!token) {
            fprintf(stderr, "Errore parsing codice attore nella linea: %s\n", line);
            free(line);
            exit(EXIT_FAILURE);
        }
        int codice = atoi(token);
        
        token = strtok_r(NULL, "\t",&privato);
        if (!token) {
            fprintf(stderr, "Errore parsing num coprotagonisti linea: %s\n", line);
            free(line);
            exit(EXIT_FAILURE);
        }
        int num_cop = atoi(token);
        token = strtok_r(NULL, " \t\n",&privato);
        int* cop = malloc(num_cop * sizeof(int));
        if(cop == NULL){
            perror("Errore nulla malloc cop");
            free(line);
            exit(EXIT_FAILURE);
        }
        
        for(int i = 0; i < num_cop; i++){
            if(!token){
                fprintf(stderr, "Errore parsing coprotagonista %d nella linea: %s\n", i, line);
                free(cop);
                free(line);
                exit(EXIT_FAILURE);
            }
            cop[i] = atoi(token);
            token = strtok_r(NULL, " \t\n",&privato);
        }
        attore* ricercato = bsearch(&codice, attori, num_att, sizeof(attore), (__compar_fn_t) &(cmp));
        if(ricercato == NULL){
            fprintf(stderr, "Codice attore %d non trovato nell'array attori\n", codice);
            free(cop);
            free(line);
            exit(EXIT_FAILURE);
        }
        ricercato->numcop = num_cop;
        ricercato->cop = cop;
        free(line);
    }
    return NULL;
}

void* produttore(void* arg){
    args_produttore* args = (args_produttore*) arg;
    buffer* buf = args->buf;
    const char* filename = args->filename;
    FILE* grafo = fopen(filename, "r");
    if(grafo == NULL){
        perror("Errore apertura file grafo.txt in produttore");
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t len = 0;
    while(getline(&line, &len, grafo) != -1){
        char* copyline = strdup(line);
        if(copyline == NULL){
            perror("Errore strdup in produttore");
            free(line);
            fclose(grafo);
            exit(EXIT_FAILURE);
        }
        if(buffer_insert(buf, copyline) == -1){
            fprintf(stderr, "Errore inserimento buffer in produttore\n");
            free(copyline);
            free(line);
            fclose(grafo);
            exit(EXIT_FAILURE);
        } 
    }
    free(line);
    fclose(grafo);
    if(pthread_mutex_lock(&buf->mutex) != 0){
        perror("Errore lock mutex in produttore fine");
        exit(EXIT_FAILURE);
    }
    buf->finito = 1;
    if(pthread_cond_broadcast(&buf->not_empty) != 0){
        perror("Errore cond_broadcast in produttore");
        pthread_mutex_unlock(&buf->mutex);
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_unlock(&buf->mutex) != 0){
        perror("Errore unlock mutex in produttore");
        exit(EXIT_FAILURE);
    }
    return NULL;
}

int abr_insert(nodo_abr** nodo, int codice, int padre){
    if(*nodo == NULL){
        *nodo = malloc (sizeof(nodo_abr));
        if(*nodo == NULL){
            fprintf(stderr, "Errore malloc abr_insert\n");
            return -1;
        }
        (*nodo)->codice = codice;
        (*nodo)->padre = padre;
        (*nodo)->left = NULL;
        (*nodo)->right = NULL;
        return 1;
    }
    if(codice < (*nodo)->codice){
        return abr_insert(&((*nodo)->left), codice, padre);
    }
    else if(codice > (*nodo)->codice){
        return abr_insert(&((*nodo)->right), codice, padre);
    }
    else return 0;
}

nodo_abr* abr_search(nodo_abr* nodo, int codice_shuffeld) {
    if (nodo == NULL) return NULL;
    if (codice_shuffeld == nodo->codice) return nodo;
    if (codice_shuffeld < nodo->codice){
        return abr_search(nodo->left, codice_shuffeld);
    }
    else{
        return abr_search(nodo->right, codice_shuffeld);
    }
}

int get_padre(nodo_abr* radice, int codice_shuffled){
    if(radice == NULL) return -1;
    if(codice_shuffled == radice->codice){
        return radice->padre;
    }
    else if (codice_shuffled < radice->codice) {
        return get_padre(radice->left, codice_shuffled);
    } 
    else {
        return get_padre(radice->right, codice_shuffled); 
    }
}

void abr_dealloc(nodo_abr* radice){
    if(radice == NULL) return;
    abr_dealloc(radice->left);
    abr_dealloc(radice->right);
    free(radice);
}

coda_FIFO* coda_init(){
    coda_FIFO* coda = malloc(sizeof(coda_FIFO));
    if (coda == NULL) {
        fprintf(stderr, "Errore malloc coda_init\n");
        return NULL;
    }
    coda->inizio = NULL;
    coda->fine = NULL;
    return coda;
}

int enqueue(coda_FIFO* coda, int codice){
    nodo_coda* new_nodo = malloc(sizeof(nodo_coda));
    if(new_nodo == NULL){
        fprintf(stderr, "Errore malloc enqueue\n");
        return -1;
    }
    new_nodo->codice = codice;
    new_nodo->next = NULL;
    if (coda->fine == NULL) {
        coda->inizio = new_nodo;
        coda->fine = new_nodo;
    } else {
        coda->fine->next = new_nodo;
        coda->fine = new_nodo;
    }
    return 0;
}

int dequeue(coda_FIFO* coda){
    if(coda->inizio == NULL){
        return -1;
    }
    nodo_coda* n_tmp = coda->inizio;
    int codice = n_tmp->codice;
    coda->inizio = n_tmp->next;
    if(coda->inizio == NULL){
        coda->fine = NULL;
    }
    free(n_tmp);
    return codice;
}

int FIFO_empty(coda_FIFO* coda){
    return(coda->inizio == NULL);
}

void FIFO_free(coda_FIFO* coda){
    while(!FIFO_empty(coda)){
        dequeue(coda);
    }
    free(coda);
}

void* richiesta(void* arg){ 
    args_richiesta* args = (args_richiesta*) arg;   //inizializzo i campi per il cammino
    int cod1 = args->cod1;
    int cod2 = args->cod2;
    attore* attori = args->attori;
    int num_att = args->num_att;

    struct tms start_tms, end_tms; 
    clock_t start_real = times(&start_tms); //prendo il tempo per il calcolo di ogni cammino
    if (start_real == (clock_t)-1) {
        perror("times() fallita all'inizio");
        free(args);
        return NULL;
    }

    attore* att_cod1 = bsearch(&cod1, attori, num_att, sizeof(attore), (__compar_fn_t) &(cmp));   //mi salvo se cod1 è un attore
    attore* att_cod2 = bsearch(&cod2, attori, num_att, sizeof(attore), (__compar_fn_t) &(cmp));   //mi salvo se cod2 è un attore

    int lenf = snprintf(NULL, 0, "%d.%d", cod1, cod2) + 1;  
    char* filename = malloc(lenf * sizeof(char));     //creo il file che conterrà il risulatoto della mia ricerca
    if(filename == NULL){
        perror("Errore malloc filename");
        free(args);
        return NULL;
    }
    snprintf(filename, lenf, "%d.%d", cod1, cod2);    //do il nome al file
    FILE* f = fopen(filename, "w");        //apro il file per scriverci dentro
    if(f == NULL){
        perror("Errore apertura file output");
        free(filename);
        free(args);
        return NULL;
    }
    if(att_cod1 == NULL){      //se il primo codice non è valido lo scrivo nel file e lo stampo a schermo 
        fprintf(f, "codice %d non valido\n", cod1);
        clock_t end_real = times(&end_tms);
        if(end_real == (clock_t)-1) perror("times() fallita alla fine");
        double elapsed_sec = (double)(end_real - start_real) / sysconf(_SC_CLK_TCK);
        printf("%d.%d: Codice %d non valido. Tempo di elaborazione %.2f secondi\n", cod1, cod2, cod1, elapsed_sec);
        fclose(f);
        free(filename);
        free(args);
        return NULL;
    }
    if(att_cod2 == NULL){     //se il secondo codice non è valido lo scrivo nel file e lo stampo a schermo 
        fprintf(f, "codice %d non valido\n", cod2);
        clock_t end_real = times(&end_tms);
        if(end_real == (clock_t)-1) perror("times() fallita alla fine");
        double elapsed_sec = (double)(end_real - start_real) / sysconf(_SC_CLK_TCK);
        printf("%d.%d: Codice %d non valido. Tempo di elaborazione %.2f secondi\n", cod1, cod2, cod2, elapsed_sec);
        fclose(f);
        free(filename);
        free(args);
        return NULL;
    }


    coda_FIFO* queue = coda_init();   //coda per la BFS
    if(queue == NULL){
        fprintf(stderr, "Errore creazione coda FIFO\n");
        fclose(f);
        free(filename);
        free(args);
        return NULL;
    }
    nodo_abr* esplorati = NULL;      //albero dei visitati
    if(abr_insert(&esplorati, shuffle(cod1), -1) == -1){    //inserisco il primo cod nei visitati
        fprintf(stderr, "Errore inserimento nodo iniziale nell'ABR\n");
        FIFO_free(queue);
        fclose(f);
        free(filename);
        free(args);
        return NULL;
    }
    if(enqueue(queue, cod1) == -1){                         //inserisco il primo cod nella coda
        fprintf(stderr, "Errore enqueue nodo iniziale\n");
        abr_dealloc(esplorati);
        FIFO_free(queue);
        fclose(f);
        free(filename);
        free(args);
        return NULL;
    }
    int trovato = 0;
    while(!FIFO_empty(queue)){     //vado avanti finché non ho più vicini
        int current = dequeue(queue);     //tolgo il primo nodo della coda
        if(current == -1){
            fprintf(stderr, "Errore dequeue\n");
            break;
        }                          //controllo se è quello a cui voglio arrivare
        if(current == cod2){
            trovato = 1;
            break;
        }
        attore* att = bsearch(&current, attori, num_att, sizeof(attore), (__compar_fn_t) &(cmp));        //prendo l'attore corrispondete al mio codice
        if (att == NULL) {
            fprintf(stderr, "Attore %d non trovato nella BFS\n", current);
            break;
        }
        for(int i = 0; i < att->numcop; i++){               //scorro tutti i suoi vicni
            int vicino = att->cop[i];
            if(abr_search(esplorati, shuffle(vicino)) == NULL){           //controllo se l'ho già visitato
                if(abr_insert(&esplorati, shuffle(vicino), current) == -1){         //se non l'ho visitato lo metto tra i visti
                    fprintf(stderr, "Errore inserimento %d nell'ABR\n", vicino);    
                    break;
                }          
                if(enqueue(queue, vicino) == -1){       //lo inserisco in coda 
                    fprintf(stderr, "Errore enqueue %d\n", vicino);
                    break;
                }
                if(vicino == cod2){                 //controllo se è quello a cui voglio arrivare
                    trovato = 1;
                    break;
                }
            }
        }
        if(trovato) break;  //se lo trovo esco dal while
    }
    if(!trovato){    //se non ho trovato un cammino lo scrivo nel file e lo stampo a schermo insieme al tempo di esecuzione
        fprintf(f, "non esistono cammini da %d a %d\n", cod1, cod2);
        clock_t end_real = times(&end_tms);
        if(end_real == (clock_t)-1) perror("times() fallita alla fine");
        double elapsed_sec = (double)(end_real - start_real) / sysconf(_SC_CLK_TCK);
        printf("%d.%d: Nessun cammino. Tempo di elaborazione %.2f secondi\n", cod1, cod2, elapsed_sec);
    }
    else{                   //se l'ho trovato
        int cap = 10;   
        int* cammino = malloc(cap * sizeof(int));           //creo un array per ripercorrere il cammino
        if (cammino == NULL) {
            fprintf(stderr, "Errore malloc cammino\n");
            fclose(f);
            free(filename);
            free(args);
            return NULL;
        }
        int len = 0;
        int current = cod2;   //parto da dove sono arrivato
        while(current != -1){       //finché ci sono nodi padre risalgo il cammino
            if(len == cap){                   //controllo per la realloc
                cap = cap*2;
                int* temp = realloc(cammino, cap * sizeof(int));
                if (temp == NULL) {
                    fprintf(stderr, "Errore realloc cammino\n");
                    free(cammino);
                    fclose(f);
                    free(filename);
                    free(args);
                    return NULL;
                }
                cammino = temp;
            }
            cammino[len++] = current;       //agiungo il mio nodo al cammino
            current = get_padre(esplorati, shuffle(current));       //prendo il padre del mio nodo per risalire
        }
        for(int i = (len-1); i >= 0; i--){          //scorro il mio cammino al contrario
            attore* a = bsearch(&cammino[i], attori, num_att, sizeof(attore), (__compar_fn_t) &(cmp));         //prendo l'attore corrispondente al mio codice cammino
            if(a != NULL){
                fprintf(f, "%d\t%s\t%d\n", a->codice, a->nome, a->anno);            //scrivo i dettagli dell'attore nel file
            }
        }
        free(cammino);
        clock_t end_real = times(&end_tms);
        if(end_real == (clock_t)-1) perror("times() fallita alla fine");
        double elapsed_sec = (double)(end_real - start_real) / sysconf(_SC_CLK_TCK);        //calcolo il tempo di computazione
        printf("%d.%d: Lunghezza minima %d. Tempo di elaborazione %.2f secondi\n", cod1, cod2, len - 1, elapsed_sec);//stampo a schermo i dettagli
    }
    
    abr_dealloc(esplorati);
    FIFO_free(queue);
    fclose(f);                  //dealloco tutto
    free(filename);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]){
    sigset_t mask;      //creo la mia variabile per mascherare i segnali
    sigemptyset(&mask);      //creo un set con la mia variabile
    sigaddset(&mask, SIGINT);     //aggiungo i segnali
    sigaddset(&mask, SIGUSR1);
    stato_seg stato;           //inizializzo una var per tenere traccia delle cose da deallocare
    stato.fase_programma = 0;     //se il programma è terminato o no
    stato.fase_pipe = 0;       //se la pipe è stata letta o no
    stato.termina_pipe = 0;     //se ricevo CTRL C mentre sto leggendo la pipe

    if(pthread_mutex_init(&stato.mutex_fase, NULL) != 0){   //inizializzo una mutex per cambiare le var sopra
        perror("Errore init mutex stato_seg");
        exit(EXIT_FAILURE);
    }
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) { //dico di mascherare i segnali
        perror("pthread_sigmask fallita");
        exit(EXIT_FAILURE);
    }
    pthread_t id_gestore;
    if(pthread_create(&id_gestore, NULL, gestore_segnali, &stato) != 0){      //lancio il gestore segnali
        perror("Errore creazione thread gestore_segnali");
        exit(EXIT_FAILURE);
    }

    if(argc != 4){             //controllo standard
        fprintf(stderr, "Numnero argomenti insufficiente");
        exit(EXIT_FAILURE);
    }


    FILE* filenomi = fopen(argv[1], "r");   //apro il file nomi prima volta
    if(filenomi == NULL){
        perror("Errore apertura nomi.txt prima volta");
        exit(EXIT_FAILURE);
    }
    char* line = NULL;  
    size_t len = 0;
    int num_att = 0;
    while (getline(&line, &len, filenomi) != -1){  //controllo quante righe ci sono
        num_att++;
    }
    free(line);
    fclose(filenomi);

    attore* attori = malloc(num_att * sizeof(attore));  //inizializzo l'arr che conterrà tutti i miei attori
    if(attori == NULL){
        perror("sbagliato qualcosa nella malloc di attori");
        exit(EXIT_FAILURE);
    }
    filenomi = fopen(argv[1], "r");                     //apro il file una seconda volta
    if(filenomi == NULL){
        perror("Errore apertura nomi.txt seconda volta");
        exit(EXIT_FAILURE);
    }
    line = NULL;
    len = 0;
    for(int i = 0; i < num_att; i++){                    //comincio il parsing del file
        if(getline(&line, &len, filenomi) == -1){        //controllo se ho finito di leggere, senno riga dopo
        fprintf(stderr, "Errore lettura linea %d\n", i);
        exit(EXIT_FAILURE);
        }
        char* token = strtok(line,"\t");            //prendo il codice
        if(!token){
            fprintf(stderr, "Errore parsing codice attore (linea %d)\n", i);
            exit(EXIT_FAILURE);
        }
        int codice = atoi(token);
        token = strtok(NULL, "\t");                 // prendo il nome
        if(!token){
            fprintf(stderr, "Errore parsing nome attore (linea %d)\n", i);
            exit(EXIT_FAILURE);
        }
        char *nome = strdup(token);
        token = strtok(NULL, "\t");                 //prendo l'anno di nascita
        if(!token){
            fprintf(stderr, "Errore parsing anno attore (linea %d)\n", i);
            free(nome); 
            exit(EXIT_FAILURE);
        }
        int anno = atoi(token);
        attori[i].codice = codice;            //Dopo aver controllato se tutto il parsing è andato a buon fine
        attori[i].nome = nome;                //Assegno tutti i campi degli elementi in attori
        attori[i].anno = anno;
        attori[i].cop = NULL;
        attori[i].numcop = 0;  
    }
    free(line);
    fclose(filenomi);
    stato.attori = attori;      //Aggiorno lo stato da deallocare
    stato.num_att = num_att;


    int num_cons = atoi(argv[3]);                  //prendo il numero di cons da creare

    buffer buf;                                    //creo il buffer condiviso tra prod e cons
    if(buffer_init(&buf, num_cons*2) != 0){
        exit(EXIT_FAILURE);
    }

    args_produttore arg_prod;               //inizializzo i parametri da passare a prod
    arg_prod.buf = &buf;
    arg_prod.filename = argv[2];     
    pthread_t id_prod;
    if (pthread_create(&id_prod, NULL, produttore, &arg_prod) != 0){    //creo prod
        perror("Errore creazione thread produttore");
        exit(EXIT_FAILURE);
    }

    pthread_t* arr_cons = malloc(num_cons * sizeof(pthread_t));   //contenitore per tutti i cons per poi fare la join
    if (arr_cons == NULL) {
        perror("Errore malloc thread consumatori");
        exit(EXIT_FAILURE);
    }
    args_consumatori arg_cons;             //inizializzo i parametri dei cons
    arg_cons.buf = &buf;
    arg_cons.attori = attori;
    arg_cons.num_att = num_att;
    for(int i = 0; i < num_cons; i++){
        if(pthread_create(&arr_cons[i], NULL, consumatore, &arg_cons)!= 0){     //creo i cons
            perror("Errore creazione thread consumatore");
            exit(EXIT_FAILURE);
        }
    }


    if (pthread_join(id_prod, NULL) != 0) {                  //faccio la join del produttore
        perror("Errore join thread produttore");
        exit(EXIT_FAILURE);
        }

    for (int i = 0; i < num_cons; i++) {                    //faccio la join di tutti i consumatori
        if (pthread_join(arr_cons[i], NULL) != 0) {
            perror("Errore join thread consumatore");
            exit(EXIT_FAILURE);
        }
    }
    stato.buf = &buf;                           //Aggiorno lo stato da deallocare
    stato.arr_cons = arr_cons;


    if (mkfifo("cammini.pipe", 0666) < 0 && errno != EEXIST){     //Creo la pipe
        perror("mkfifo pipe fallita");
        exit(EXIT_FAILURE);
    }
    int pipe = open("cammini.pipe", O_RDONLY);                    //apro la pipe
    if (pipe < 0) {
        perror("Errore apertura cammini.pipe");
        exit(EXIT_FAILURE);
    }


    if(pthread_mutex_lock(&stato.mutex_fase) != 0){           
    perror("Errore lock mutex stato_seg");
        exit(EXIT_FAILURE);
    }
    stato.fase_pipe = 1;                                      //comincio a leggere dalla pipe
    if(pthread_mutex_unlock(&stato.mutex_fase) != 0){
        perror("Errore unlock mutex stato_seg");
        exit(EXIT_FAILURE);
    }


    int32_t* buf_pipe = malloc(2 * sizeof(int32_t));      //creo il buf per i 2 elemnti che leggo dalla pipe alla volta
    if (!buf_pipe){
        perror("malloc buf_pipe");
        close(pipe);
        exit(EXIT_FAILURE);
    }
    ssize_t nread;      //valore di ritorno dalla lettura della pipe

    while(1){     
        pthread_mutex_lock(&stato.mutex_fase);
        int termina = stato.termina_pipe;   //controllo a ogni ciclo se è arrivato un SIGINT
        pthread_mutex_unlock(&stato.mutex_fase);
        if (termina) { //uscita forzata da SIGINT
            break;                                          
        }                     
        nread = read(pipe, buf_pipe, 2 * sizeof(int32_t));  //leggo esattamente 2 int32
        if (nread == 0) break; //finito di leggere la pipe
        if(nread != 2 * sizeof(int32_t)) {             //controllo di aver letto esattamente 2 int32
            fprintf(stderr, "Lettura parziale dalla pipe: %zd byte \n", nread);
            break;
        }
        int cod1 = buf_pipe[0];   //primo elemento nel buf è cod1
        int cod2 = buf_pipe[1];   //secondo elemento nel buf è cod2

        args_richiesta* args = malloc(sizeof(*args));    //qui ogni thread prenderà argomenti diversi, di conseguenza ogni thread richiesta dovrà
        if (!args) {                                     //avere una sua struct dedicata, quindi devo fare malloc
            perror("malloc args_richiesta");
            break;
        }
        args->cod1    = cod1;                                  //inizializzo gli arg della funzione che mi calcolerà il cammino
        args->cod2    = cod2;
        args->attori  = attori;
        args->num_att = num_att;

        pthread_t id_pipe;  
        if (pthread_create(&id_pipe, NULL, richiesta, args) != 0) {             //creo il mio thread per il cammino minimo
            perror("pthread_create");
            free(args);
            break;
        }
        if (pthread_detach(id_pipe) != 0) {              // lo rendo detatched
            perror("pthread_detach");
            break;
        }
    }
    if (nread < 0) {   
        perror("Errore read dalla pipe");
    }
    close(pipe);  
    unlink("cammini.pipe"); 
    free(buf_pipe);   
    
    
    sleep(20);   // aspetto 20s per i thread detatched


    pthread_mutex_lock(&stato.mutex_fase);  
    stato.fase_programma = 1;                   //il programma adesso deve terminare
    pthread_mutex_unlock(&stato.mutex_fase);
    if (pthread_kill(id_gestore, SIGUSR1) != 0) {        //mando SIGUSR1 a gestore_segnale per dirgli che il programma termina, e quindi
        perror("pthread_kill SIGUSR1");                  //deve uscire dalla sigwait e terminare
        exit(EXIT_FAILURE);
    }
    pthread_join(id_gestore, NULL);         //aspetto che il thread gestore termini

    cleanup(&stato); //dealloco tutto 

    return 0;
}
