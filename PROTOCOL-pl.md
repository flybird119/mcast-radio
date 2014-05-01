Mateusz Machalica; 305678; SIK; "Radio internetowe"
===================================================

OPIS PROTOKOŁU
==============

Wstęp
-----

Wszystkie wielobajtowe wartości liczbowe przesyłane są w sieciowym porządku bajtów.
Wszystkie wartości napisowe przesyłane są jako NULL Terminated String w blokach o
określonej długości (całkowitej liczby oktetów oktetach), dopełnione do długości
bloku końcowymi zerami.

Format pakietu
---------------

Pakiety przesyłane są w formie datagramów UDP.
Pakiet składa się z nagłówka i następującej po nim sekcji danych.
Format sekcji danych jest zależny od flag ustawionych w nagłówku.

Format nagłówka
---------------

Nagłówek ma następujący format:

|      2 oktety     | 1 oktet | 1 oktet |
+-------------------+---------+---------+
|       numer sekwencyjny pakietu       |
+-------------------+---------+---------+
|  długość pakietu  |  flagi  |  wersja |
+-------------------+---------+---------+

Numer sekwencyjny pakietu to 32-bitowa liczba bez znaku, ma znaczenie tylko dla
pakietów z ustawioną flagami DATA, RETQUERY (patrz dalej).

Długość pakietu to 16-bitowa liczba bez znaku, równa długości pakietu (długość
nagłówka + długość danych) wyrażonej w oktetach.

Wersja to 8-bitowa liczba bez znaku, równa wersji protokołu, obecnie 1.

Flagi to 8-bitowa liczba bez znaku, równa sumie bitowej podzbioru następujących
wartości:
	RETQUERY    0x1
	IDQUERY     0x2
	IDRESP      0x4
	DATA        0x8
	FAIL        0xF
Powiemy, że flaga X jest ustawiona w nagłówku, jeśli pole flagi nagłówka ma
wartość Y i Y & X != 0.  Podobnie flaga jest ustawiona w pakiecie jeśli jest
ustawiona w nagłówku tego pakietu.

Pewne pary flag X != Y nazwiemy sprzecznymi.  Pakiet z ustawionymi dwiema
sprzecznymi flagami nie jest poprawnym pakietem protokołu.  Niech każde dwie
różne flagi są sprzeczne, chyba że w dalszej części specyfikacji zaznaczono
inaczej.

Format sekcji danych
--------------------

Maksymalna długość sekcji danych wynosi 2^15 oktetów.

Format sekcji danych zależy od ustawionych flag w nagłówku pakietu.

Jeśli ustawiona jest jedna z flag:
*	RETQUERY (pakiet z prośbą o retransmisję, numer sekwencyjny w nagłówku jest
	numerem pakietu o którego retransmisję prosimy)
*	IDQUERY (pakiet z prośbą o identyfikację)
*	FAIL (pakiet informujący o niemożności retransmisji pakietu o numerze sekwencyjnym
	takim jak numer w nagłówku pakietu)
to sekcja danych jest wtedy pusta.

Flagi RETQUERY i IDQUERY nie są sprzeczne. Pakiet, który zawiera ustawione obie
flagi jest traktowany jak dwa osobne pakiety z ustawionymi flagami pojedynczo.

Jeśli ustawiona jest flaga IDRESP (pakiet z odpowiedzią na prośbę o
identyfikację) to sekcja danych ma następujący format:
*	32 oktety, blok zawierający wartość napisową - nazwę stacji wysyłającej ten pakiet
*	32 oktety, blok zawierający wartość napisową - nazwę aplikacji wysyłającej ten pakiet
*	16-bitowa liczba bez znaku - maksymalny rozmiar sekcji danych (w oktetach) w pakiecie
	z ustawioną flagą DATA, wysyłanym przez tą stację
*	128 oktetów, blok adresowy - zawiera adres i numer portu adresu rozgłoszeniowego,
	na którym nadaje stacja wysyłająca ten pakiet
*	128 oktetów, blok adresowy - zawiera adres i numer portu lokalnego gniazda,
	z którego nadaje na adres rozgłoszeniowy stacja, wysyłająca ten pakiet

Format bloku adresowego jest zgodny z formatem struktury struct sockaddr_storage
zdefiniowanej w RFC 2553.
Blok adresowy ma długość 128 oktetów i w obecnej wersji protokołu ma następujący format:
*	2 oktety - zarezerwowane
*	16-bitowa liczba bez znaku oznaczająca port TCP/UDP
*	32-bitowa liczba bez znaku oznaczająca adres IPv4, w formacie zgodnym ze
	zwracanym przez funkcję inet_pton, zdefiniowaną w standardzie:
	IEEE Std 1003.1, 2003 Edition, Standard for Information Technology -- POSIX
	(w skrócie: adres IPv4 zapisany jako liczba bez znaku w sieciowym porządku bajtów)
*	120 oktetów -- zarezerwowane

Kompatybilność bloku adresowego z sockaddr_storage pozwala na rozszerzenie
implementacji o wsparcie IPv6 bez zmiany formatu protokołu.

Jeśli adres (adres IP i numer portu) gniazda, z którego będą nadawane pakiety z
danymi na adres rozgłoszeniowy nie będzie zgodny z tym zadeklarowanym przez
stację w bloku adresowym pakietu identyfikacyjnego, to takie pakiety będą
odrzucane przez odbiornik.

Jeśli rozmiar sekcji danych w pakietach z ustawioną flagą DATA będzie większy niż
wartość zadeklarowana w pakiecie identyfikacyjnym danej stacji, to takie
pakiety będą ignorowane.

Jeśli ustawiona jest flaga DATA (pakiet z danymi) to sekcja danych zawiera ciąg
oktetów, które stanowią przesyłane dane.

Opis nadajnika
--------------

Nadajnik nasłuchuje na portach jak opisano w sformułowaniu zadania i odpowiada
na prośby o identyfikację opisanym już pakietem.

Po otrzymaniu prośby o retransmisję pakietu sprawdza czy znajduje się on w
kolejce fifo. Jeśli tak, to zaznacza pakiet jako do retransmisji, jeśli pakiet
nie znajduje się w kolejce fifo, to na adres (i numer portu) z którego otrzymano
prośbę odsyłany jest pakiet z informacją o niemożności wykonania retransmisji.

Co czas RTIME nadajnik ponownie wysyła wszystkie pakiety, jakie znajdują się w
kolejce fifo i zostały zaznaczone jako do retransmisji oraz odznacza je.

Opis odbiornika
---------------

Odbiornik wysyła prośby o identyfikację i odbiera odpowiedzi jak zdefiniowano w
sformułowaniu zadania, nasłuchuje również na tym porcie, z którego prosi o
retransmisje, pakietów informujących o niemożności retransmisji.

Bufor odbiornika składa się z wpisów, wpisem może być odebrany pakiet, luka
(pakiet, który został oznaczony do retransmisji), martwy pakiet (pakiet, o
którego retransmisję nie będziemy już prosić), miejsce wolne (może stać się
jednym z powyższych). Miejscami wolnymi są wszystkie wpisy po ostatnim
(porządek na pakietach indukowany z porządku na numerach sekwencyjnych)
odebranym pakiecie (jeśli nie ma odebranego pakietu, to cały bufor składa się z
wolnych miejsc).
Długością bufora jest sufit(BSIZE/PSZIE).
Bufor przechowuje wpisy dla numerów sekwencyjnych od fseqno do lseqno = fseqno +
długość bufora - 1.  Poprawnym prefiksem nazwiemy ciąg odebranych pakietów w
buforze o kolejnych numerach sekwencyjnych począwszy od fseqno.
Początkowo (i po każdej zmianie stacji) bufor jest niezainicjowany, nie ma
określonego fseqno, nie można wtedy wykonywać na nim żadnych operacji poza
inicjalizacją (ustaleniem fseqno).

Parametry luki:
*	liczba retransmisji - ile razy można jeszcze prosić o retransmisję, jeśli
	równe zero to już nie można prosić o retransmisję pakietu (który znalazłby się
	pod tym wpisem)
*	opóźnienie retransmisji - ile razy RTIME trzeba jeszcze czekać, aby poprosić o
	retransmisję (w obecnej implementacji rozdzielczość pomiaru czasu to RTIME)

Przejścia pomiędzy rodzajami (stanami) wpisów:
*	wolne miejsce >>== oznaczenie do retransmisji ==>> luka
	*	ustawia opóźnienie retransmisji na 1
	*	ustawia liczbę retransmisji na 8
*	luka >>== prośba o retransmisję ==>> luka
	*	zmniejszenie liczby retransmisji o 1
	*	ustawienie opóźnienia retransmisji na 2
*	wolne miejsce, luka >>= odebranie pakietu ==>> odebrany pakiet
*	luka >>== odebranie informacji (pakietu) o niemożności retransmisji ==>> martwy pakiet
*	luka >>== liczba retransmisji i opóźnienie retransmisji równe zero ==>> martwy pakiet

Dlaczego opóźnienie pierwszej retransmisji jest równe 1, a kolejnych 2?
Ponieważ na początku chcemy retransmitować najwcześniej jak się da, a kolejną prośbę o
retransmisje powinniśmy wysłać dopiero wtedy, gdy uznamy że poprzednia prośba przepadła.
Nadajnik również odmierza czas ,,tyknięciami'' co RTIME i co RTIME retransmituje pakiety,
o które został poproszony w ciągu ostatniego RTIME, stąd maksymalne opóźnienie pomiędzy
wysłaniem prośby o retransmisją przez odbiornik a retransmisją przez nadajnik jest
większe od RTIME, ale mniejsze od 2*RTIME (pomijamy opóźnienia sieci).

Gdy odbirnik odbiera nowy pakiet (z aktualnie ustawionej stacji),
o numerze sekwencyjnym seqno:
*	jeśli seqno-lseqno > długośc bufora to bufor staje się niezainicjowany
*	jeśli bufor jest niezainicjowany to inicjuje bufor aktualnym seqno
*	jeśli seqno > fseqno, to z bufora jest usuwanych pierwszych seqno - fseqno pakietów
*	wstawia do bufora nowy odebrany pakiet (o numerze sekwencyjnym seqno)
*	wszystkie wpisy w buforze o numerach sekwencyjnych < seqno, które są miejscami
	wolnymi stają się lukami (oznaczone do retransmisji)
*	jeśli w buforze jest poprawny prefiks o długości co najmniej 75% długości
	bufora to opróżnia pierwsze (długość najdłuższego poprawnego prefiksu)-pakietów
	bufora na standardowe wyjście 

Co czas RTIME odbiornik:
*	przegląda bufor w poszukiwaniu ostatniego martwego pakietu i usuwa wszystkie
	pakiety o mniejszym lub równym numerze sekwencyjnym
*	dekrememntuje opóźnienie retransmisji dla każdej luki w buforze o dodatnim
	opóźnieniu retransmisji
*	dla każdej luki, jeśli znajduje się w pierwszej połowie bufora i jeśli opóźnienie
	retransmisji jest równe zero i liczba retransmisji większa od zera to:
	*	wysyła prośbę o retransmisję odpowiadającego jej pakietu
	*	zmienia rodzaj wpisu luka >>==>> luka (aktualizacja liczby i opóźnienia
		retransmisji)

Rozszerzenia
------------

Protokół i implementacja nadajnika oraz odbiornika pozostawiają dowolność odnośnie
implementacji wysyłania przez nadajnik i odbierania przez odbiornik informacji zwrotnych o
niemożności retransmitowania danego pakietu w odpowiedzi na prośbę o retransmisję.
Dowolna kombinacja nadajnika i odbiornika, które będą implementowały, bądź nie tą
funkcjonalność będzie funkcjonowała poprawnie, jeśli spełni resztę wymagań.

