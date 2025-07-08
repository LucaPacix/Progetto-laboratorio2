cartellajava = ./Attore.java	./CreaGrafo.java
Cflags = gcc -Wall -Wextra -pthread -g -O3 -o 

all: CreaGrafo.class cammini.out

CreaGrafo.class:$(cartellajava)
	javac	$^
	
cammini.out: ./cammini.c
	$(Cflags) $@ $^


clean:
	rm cammini.out
	rm *.class