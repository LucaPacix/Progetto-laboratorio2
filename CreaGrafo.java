import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.TreeMap;
import java.util.TreeSet;

public class CreaGrafo{
    public static void main (String[] input){ //prendo l'input di 3 file
        TreeMap<Integer, Attore> attori = new TreeMap<>(); // creo la struttura dati che conterrà i miei attori
        try { //leggo il file
            BufferedReader fileAtt = new BufferedReader(new FileReader(input[0])); //carico il primo file di Attori nel buffer
            String linea = fileAtt.readLine(); // assegno read line per saltare la prima linea
            while((linea = fileAtt.readLine()) != null){ //assegno alla mia variabile una riga del file alla volta
                String[] colonne = linea.split("\\t"); // prendo le prime 5 colonne della mia riga
                if(!(colonne[2].equals("\\N"))){ //controllo se c'è una data di nasciata
                    String[] professioni = colonne[4].split(","); //faccio un array con tutti i ruoli di un possiile attore
                    for(int i = 0; i < professioni.length; i++){
                        if(professioni[i].equals("actress") || professioni[i].equals("actor")){ //controllo se tra i ruoli c'è attore
                            Integer codice = Integer.parseInt(colonne[0].substring(2)); //tolgo nm dal codice
                            Attore Att = new Attore(codice, colonne[1], Integer.parseInt(colonne[2]));  //creo l'istanza di attore 
                            attori.put(codice, Att); // l'aggiungo a AttoriMap
                        }
                    }
                }
            }
            System.out.println("finito di leggere gli attori");
            fileAtt.close();
        }
        catch (Exception e) {
            System.err.println(e+" hai sbagliato qualcosa nella lettura del file Attori");
        }
        try {
            BufferedWriter scrittura = new BufferedWriter(new FileWriter("nomi.txt")); //puntatore per scrivere
            for(Attore Att : attori.values()){
                String linea = Integer.toString(Att.codice) + "\t" +Att.nome + "\t" + Integer.toString(Att.anno); 
                scrittura.write(linea); //accetta solo stringhe
                scrittura.newLine(); //passo a scrivere sulla liena dopo
            }
            System.out.println("finito di stampare gli attori");
            scrittura.close();
        } catch (Exception e) {
            System.err.println(e+" hai sbagliato qualcosa nella scrittura in nomi.txt");
        }
        TreeMap<Integer, TreeSet<Integer>> film = new TreeMap<>(); //creo la mappa che conterrà tutti i film
        TreeMap<Integer, TreeSet<Integer>> attfilm = new TreeMap<>(); //creo la mappa dove a ogni attore sono associati i suoi film
        try { 
            BufferedReader fileFilm = new BufferedReader(new FileReader(input[1])); //puntatore al file
            String linea = fileFilm.readLine(); // assegno read line per saltare la prima linea
            while((linea = fileFilm.readLine())!= null){ //scorro il file
                String[] colonne = linea.split("\\t");  //suddivido le colonne
                Integer codiceFilm = Integer.parseInt(colonne[0].substring(2)); //prendo il codice del film
                Integer codiceForseAtt = Integer.parseInt(colonne[2].substring(2)); //prendo il codice dell'eventuale attore
                if(attori.containsKey(codiceForseAtt)){ //controllo che sia un attore
                    Integer codiceAtt = codiceForseAtt; //ora sono sicuro sia un codice attore
                    film.putIfAbsent(codiceFilm, new TreeSet<Integer>()); //creo un nuovo set se ho un nuovo codicefilm
                    film.get(codiceFilm).add(codiceAtt); //aggiungo attori al mio set associato al film

                    attfilm.putIfAbsent(codiceAtt, new TreeSet<Integer>()); //controllo se il mio attore è già stato inserito
                    attfilm.get(codiceAtt).add(codiceFilm); //aggiungo codicifilm al mio attore
                }
            }
            System.out.println("finito di leggere i film");
            fileFilm.close();
        }
        catch (Exception e){
            System.err.println(e+" hai sbagliato qualcosa nella lettura del file dei film");
        }
        try{
            for(Integer codiceFilm : film.keySet()){   //scorro tutti i film
                for(Integer codAtt1 : film.get(codiceFilm)){ //scorro tutti gli attori di un film
                    for(Integer codAtt2 : film.get(codiceFilm)){ //scorro di nuovo tutti gli attori
                        if(!(codAtt1.equals(codAtt2))){ //evito di aggiungere un attore come coprotagonista di se stesso
                            attori.get(codAtt1).coprotagonisti.add(codAtt2); //aggiungo un attore di uno stesso film come coprotagonista
                            attori.get(codAtt2).coprotagonisti.add(codAtt1); //aggiungo un attore di uno stesso film come coprotagonista
                        }
                    }
                }
            }
            System.out.println("finito di aggiornare i coprotagonisti");
        } 
        catch (Exception e) {
            System.err.println(e+" hai sbagliato qualcosa nello scrivere i coprotagonisti");
        }
        try {
            BufferedWriter scrittura = new BufferedWriter(new FileWriter("grafo.txt")); //puntatore per scrivere
            for(Attore Att : attori.values()){ //scorro tutti gli attori
                String semilinea = Integer.toString(Att.codice) + "\t" +Integer.toString(Att.coprotagonisti.size()); //creo la linea
                StringBuilder linea = new StringBuilder(semilinea);
                for( Integer cop : Att.coprotagonisti){
                    linea.append("\t").append(cop);
                }
                scrittura.write(linea.toString());
                scrittura.newLine();
            }
            System.out.println("finito di stampare i coprotagonisti");
            scrittura.close();
        } 
        catch (Exception e) {
            System.err.println(e+" hai sbagliato qualcosa nella scrittura in grafo.txt");
        }
        try{
            BufferedWriter scrittura = new BufferedWriter(new FileWriter(("partecipazioni.txt")));
            for(Integer codAtt: attori.keySet()){
                StringBuilder linea = new StringBuilder();
                if(attfilm.containsKey(codAtt)){
                    String semilinea = Integer.toString(codAtt) + "\t" + Integer.toString(attfilm.get(codAtt).size());
                    linea = new StringBuilder(semilinea);
                    for(Integer codfilm : attfilm.get(codAtt)){
                        linea.append("\t").append(codfilm);
                    }
                }
                else{
                    linea.append(codAtt).append("\t").append(0);
                }
                scrittura.write(linea.toString());
                scrittura.newLine();
            }
            System.out.println("finito di stamapre i film degli attori");
            scrittura.close();
        }
        catch (Exception e){
            System.err.println(e+" hai sbagliato qualcosa nella scrittura di partecipazioni.txt");
        }
    }
}

