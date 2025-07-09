import java.util.Set;
import java.util.HashSet;

public class Attore {
    int codice;                   // codice univoco dell'attore
    String nome;                  // none (e cognome) dell'attore
    int anno;                     // anno di nascita
    Set<Integer> coprotagonisti;  // codici degli attori che hanno recitato con this 

    public Attore (int codice, String nome, int anno){
        this.codice = codice;
        this.nome = nome;
        this.anno = anno;
        this.coprotagonisti = new HashSet<Integer>();
    }
}
