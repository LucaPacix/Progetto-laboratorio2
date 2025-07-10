# Progetto lab 24/25



## 1. Parsing del file name.basics.tsv
Il parsing del file **_name.basics.tsv_** viene effettuato tramite un ciclo **while** che legge una riga del file alla volta utilizzando la funzione **readLine()**, e termina al raggiungimento della fine del file. All'interno del ciclo, ogni riga viene suddivisa nei singoli campi mediante la funzione **.split("\t")**, in quanto i dati sono separati da tabulazioni (**\\t**).Successivamente, viene verificato che il terzo campo, corrispondente all’anno di nascita, sia definito, ovvero diverso da **"\N"**. In caso contrario, la riga viene ignorata e si passa all’iterazione successiva del ciclo.
A questo punto, viene effettuato uno **split(",")** sul quinto campo, che rappresenta le professioni associate alla persona, e si controlla, attraverso un ciclo **for**, che almeno una di esse corrisponda a **"actor"** o **"actress"**, utilizzando il metodo **.equals()** per il confronto.Se anche questa condizione fosse soddisfatta, i dati vengono salvati all’interno di una struttura dati **TreeMap**, in cui la chiave rappresenta il codice numerico dell’attore, ottenuto rimuovendo il prefisso **"nm"** dal primo campo tramite **.substring(2)** e convertito in intero mediante **Integer.parseInt()**.
Il valore associato è un oggetto di tipo **Attore** (definito dalla classe _Attore_) che conterrà le informazioni raccolte: codice, nome (preso dal secondo campo) e anno di nascita (dal terzo campo).Al termine del parsing si ottiene dunque una struttura **TreeMap<Integer, Attore>** ordinata per codice, in cui ogni attore è rappresentato da codice, nome e data di nascita.


## 2. Implementazione della coda FIFO nell'algoritmo BFS
L’implementazione della __coda FIFO__ nell’algoritmo __BFS__ si basa su una struttura dinamica __coda_FIFO__ che mantiene due puntatori, _inizio_ e _fine_, per gestire una __lista concatenata dinamica__ di nodi (_nodo_coda_), ognuno contenente il __codice intero di un attore__ e un __puntatore al nodo successivo__.  
La gestione della coda avviene tramite __allocazioni dinamiche__ con __malloc__ e __realloc__. La memoria viene sempre correttamente liberata con __free__, ad esempio tramite la funzione **FIFO_free**.  
La coda consente di inserire nuovi codici in fondo tramite la funzione **enqueue(coda, codice)** e di estrarre in testa con **dequeue(coda)**, garantendo così l’ordine FIFO necessario all’esplorazione della __BFS__.  
Ogni elemento della coda memorizza soltanto il codice dell’attore da visitare, mentre informazioni aggiuntive, come i __padri dei nodi__, sono memorizzate separatamente in un albero binario di ricerca (ABR) che tiene traccia sia dei __nodi già visitati__ sia del __nodo padre__ di ciascun elemento (necessario per la __ricostruzione del cammino minimo__).  
La coda, inizializzata con **coda_init()**, assicura che gli attori vengano processati in ordine di scoperta, permettendo di visitare completamente tutti i vicini di un dato livello prima di passare al successivo.  
L’inserimento di un vicino nella coda è condizionato dal fatto che il nodo non sia già stato visitato (cioè non presente nell’ABR), evitando cicli e visite ripetute.  

