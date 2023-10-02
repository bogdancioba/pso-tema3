# Server de Fisiere si Client

Acest proiect contine un server si un client simplu care permite operatii de baza pe fisiere.

## file_server.c

### Descriere:
Acest fisier defineste serverul. Serverul asculta pentru conexiuni de la clienti si accepta mai multe operatii precum `LIST`, `GET`, `PUT`, `DELETE`, `UPDATE`, `SEARCH`.

### Functionalitati:
1. **Indexarea fisierelor**: O functie dedicata pentru indexarea fisierelor este rulata intr-un thread separat. Aceasta creeaza un index al primelor 10 cuvinte cele mai frecvente din fiecare fisier.
2. **Jurnalizarea operatiilor**: Serverul scrie operatiile intr-un fisier `server.log`.
3. **Procesarea operatiilor**: Serverul poate procesa diferite operatii venite de la clienti. Acestea sunt procesate in functii separate pentru claritate.
4. **Gestionarea semnalelor**: Serverul poate fi inchis cu gratie folosind semnalele SIGTERM sau SIGINT.

### Cum sa rulezi:
1. Compileaza fisierul: `make` sau `make client`
2. Ruleaza: `./server`

## client.c

### Functionalitati:
Clientul trimite cereri pentru a lista fisiere, prelua fisiere, incarca fisiere, sterge fisiere, actualiza fisiere si cauta cuvinte in index.

### Cum sa rulezi:
1. Compileaza fisierul: `make` sau `make client`
2. Ruleaza: `./client`


### Observatii

Am reusit sa rezolv sa mearga aceste operatii , doar ca nu stiu de ce , nu imi afiseaza in client cand fac update put sau delete , dar de exemplu daca fac delete pe `file1.txt` si nu fac put inainte nu imi va merge si imi da o eroare ca fisierul nu exista, deci merge doar ca nu afiseaza.