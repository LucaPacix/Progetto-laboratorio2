import sys

def read_partecipazioni(file_partecipazioni):
    partecipazioni = {}
    with open(file_partecipazioni, "r") as f:
        for riga in f:
            campi = riga.split("\t")
            if len(campi) < 2:
                raise ValueError(
                    f"Errore parsing partecipazioni.txt a riga {riga}: "
                )
            id_att = int(campi[0])
            if (len(campi)-2) != int(campi[1]):
                raise ValueError(
                    f"Errore numero di partecipazioni a riga {riga}: "
                )
            ids_titoli = set(map(int, campi[2:]))
            partecipazioni[id_att] = ids_titoli
    return partecipazioni

def read_titoli(file_titoli):
    titoli = {}
    with open(file_titoli, encoding="utf-8") as f:
        next(f)
        for riga in f:
            campi = riga.strip().split("\t")
            id_titolo = int(campi[0][2:])
            titolo = campi[3]
            titoli[id_titolo] = titolo
    return titoli

def stampa_collaborazioni(id1, id2, partecipazioni, titoli):
    titoli_comuni = partecipazioni[id1] & partecipazioni[id2]
    if not titoli_comuni:
        print(str(id1) + "." + str(id2) + " nessuna collaborazione")
        return
    else:
        print(str(id1) + "." + str(id2) + ": " + str(len(titoli_comuni)) + " collaborazioni:")
        tit_com_odrinati = sorted(titoli_comuni) #non so ancora se devo sortare o no i codici
        for codice in tit_com_odrinati:
            print(str(codice) + " " + titoli[codice])

def main():
    if(len(sys.argv) < 5):
        print("Non hai passato abbastanza argomenti")
        sys.exit(1)

    file_partecipazioni = sys.argv[1]
    file_titoli = sys.argv[2]
    ids_attori_passati = list(map(int, sys.argv[3:]))
    partecipazioni = read_partecipazioni(file_partecipazioni) 
    titoli = read_titoli(file_titoli)
    for id_att in ids_attori_passati:
        if not (id_att in partecipazioni):
            print ("Id {} sconosciuto".format(id_att))
            sys.exit(1)
    for i in range(len(ids_attori_passati) -1):
        stampa_collaborazioni(ids_attori_passati[i], ids_attori_passati[i+1], partecipazioni, titoli)
        print()
    print("=== Fine")

if __name__ == "__main__":
    main()