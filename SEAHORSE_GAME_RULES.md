# SeaHorse / Konik Morski — zasady gry

> Dokument opisuje ustalone zasady gameplayowe dla cyfrowej wersji gry **Konik Morski / SeaHorse**.
> Ma być traktowany jako źródło kontekstu dla Codexa podczas pracy nad kodem.
>
> **Najważniejsza zasada:** tekst / efekt konkretnej karty ma pierwszeństwo przed zasadami ogólnymi.

---

## 1. Gracze i talia

- Gra jest przeznaczona dla co najmniej **2 graczy**.
- W cyfrowej wersji planowane są układy stołu dla **2, 3 i 4 graczy**.
- W dotychczasowych ustaleniach przyjęta została talia **52 kart używanych w meczu**.
- W talii występują duplikaty kart — większość par tworzona jest z dwóch kart o tej samej definicji / nazwie.
- Istnieją również karty pojedyncze, m.in. **Konik Morski** i **Szczuroludzie**.
- Dokładny skład talii powinien wynikać z danych talii (`DeckDefinition`), a nie być zakodowany na sztywno w logice meczu.

---

## 2. Przygotowanie meczu

1. Talia jest tworzona i tasowana.
2. Wszystkie karty są rozdawane graczom.
3. Karty mogą zostać rozdane nierówno — nie jest wymagane, aby każdy gracz miał dokładnie tyle samo kart.
4. Po rozdaniu każdy gracz może **jednorazowo ustawić kolejność kart w swojej ręce**.
5. Po zakończeniu początkowego ustawiania kolejność ręki zostaje zablokowana.
6. Gracz nie może później dowolnie przestawiać kart znajdujących się już w ręce.
7. Gracz, który otrzymał **ostatnią rozdawaną kartę**, rozpoczyna grę.

> Uwaga implementacyjna: rozpoczęcie gry przez odbiorcę ostatniej rozdanej karty jest zasadą gry. Nie powinien być wybierany niezależny losowy `StartingPlayer`.

---

## 3. Ręka gracza

### 3.1. Kolejność kart

Kolejność kart w ręce jest elementem stanu gry.

Po początkowym ustawieniu gracz:

- nie może dowolnie zmieniać kolejności istniejących kart,
- nie może tasować swojej ręki,
- może zmienić układ tylko wtedy, gdy pozwala na to konkretna zasada / efekt karty.

### 3.2. Dobieranie nowej karty

Kiedy gracz dobiera kartę z ręki przeciwnika:

- karta zostaje zabrana z ręki przeciwnika,
- trafia do ręki dobierającego,
- dobierający wybiera **miejsce, w którym nowa karta zostanie wstawiona**,
- istniejące karty nie mogą zostać przy tej okazji dowolnie przeorganizowane.

Przykład:

```text
Przed:
A B C D

Nowa karta: X

Dozwolone:
A B X C D

Niedozwolone:
D B X A C
```

### 3.3. Informacje publiczne i prywatne

Inni gracze:

- znają **liczbę kart** w ręce każdego gracza,
- widzą **kolejność / pozycje** kart,
- widzą, **w którym miejscu została wstawiona nowo dobrana karta**,
- nie znają tożsamości kart znajdujących się w cudzej ręce.

Właściciel ręki widzi swoje karty awersem.

Pozostali gracze widzą karty tej ręki rewersem.

Tożsamość karty w ręce jest więc prywatną informacją właściciela, natomiast jej pozycja w ręce jest informacją publiczną.

---

## 4. Struktura tury

Standardowa tura składa się z trzech etapów:

```text
Etap 1 — FirstPairing
    ↓
Etap 2 — DrawCard
    ↓
Etap 3 — SecondPairing
    ↓
koniec tury
```

Następnie tura przechodzi na kolejnego gracza.

---

## 5. Etap 1 — First Pairing

W pierwszym etapie gracz może:

- utworzyć **dokładnie jedną parę**, albo
- zrezygnować z tworzenia pary.

Nie może utworzyć dwóch lub większej liczby par w tym etapie.

Po utworzeniu pary lub pominięciu tego działania standardowo następuje etap dobierania karty.

### Skrót do dobrania karty

Gracz może również od razu dobrać kartę.

Dobranie karty w `FirstPairing` oznacza jednocześnie:

- rezygnację z tworzenia pary w Etapie 1,
- wykonanie obowiązkowego dobrania z Etapu 2.

Po takim dobraniu gra przechodzi bezpośrednio do:

```text
SecondPairing
```

czyli:

```text
FirstPairing
    └─ CardDrawn
         ↓
SecondPairing
```

---

## 6. Etap 2 — Draw Card

W standardowej turze dobranie karty jest **obowiązkowe**.

Gracz:

1. wybiera innego gracza,
2. bierze jedną nieznaną kartę z jego ręki,
3. dodaje ją do własnej ręki,
4. wybiera miejsce wstawienia nowej karty.

Ponieważ tożsamości cudzych kart są ukryte, gracz nie wybiera karty na podstawie jej definicji / nazwy.

Po dobraniu gra przechodzi do `SecondPairing`.

---

## 7. Etap 3 — Second Pairing

W trzecim etapie gracz może:

- utworzyć **dokładnie jedną parę**, albo
- zrezygnować z tworzenia pary.

Utworzenie pary w tym etapie **nie kończy automatycznie tury**.

Etap pozostaje aktywny, ponieważ gracz może jeszcze wykonywać dozwolone aktywacje par / kart.

Dopiero ręczne zakończenie / pominięcie etapu kończy turę.

---

## 8. Tworzenie par

Para powstaje z dwóch pasujących kart.

Standardowo karty są kompatybilne, gdy:

```text
CardA.CardDefinition == CardB.CardDefinition
```

czyli reprezentują tę samą kartę / mają tę samą nazwę.

Nie można:

- sparować karty z samą sobą,
- utworzyć pary z dwóch różnych definicji kart,
- utworzyć więcej niż jednej pary podczas jednego etapu parowania, chyba że efekt karty wyraźnie zmienia tę zasadę.

W standardowej turze możliwe jest więc maksymalnie:

- 1 para w `FirstPairing`,
- 1 para w `SecondPairing`.

---

## 9. Player Zone / Activation Area

Po utworzeniu pary:

1. obie karty są usuwane z ręki gracza,
2. para trafia do jego **Player Zone / Activation Area**,
3. karty pary są ujawniane i stają się publiczne,
4. para oczekuje tam na aktywację.

Para znajdująca się w Player Zone nie daje jeszcze punktu.

---

## 10. Aktywowanie par

Para może zostać aktywowana tylko raz.

Standardowa para bez dodatkowych ograniczeń może być aktywowana:

- podczas tury jej właściciela,
- w dowolnym dozwolonym momencie jego tury.

Gracz może aktywować więcej niż jedną posiadaną parę podczas swojej tury, o ile zasady konkretnych kart na to pozwalają.

Samo utworzenie pary i aktywowanie pary to dwie różne akcje.

```text
Hand
  ↓ create pair
Player Zone
  ↓ activate
Card Effect
  ↓ effect finished
Victory Stack
```

---

## 11. Ograniczenia aktywacji kart

Nie wszystkie pary używają standardowych zasad aktywacji.

Karta może określać:

### Turn restriction

- `OwnTurn` — tylko podczas własnej tury właściciela,
- `OutsideOwnTurn` — tylko poza własną turą,
- `AnyTurn` — podczas własnej lub cudzej tury.

### Phase restriction

Karta może ograniczać aktywację do konkretnych faz, np.:

- tylko `FirstPairing`,
- tylko `SecondPairing`,
- kilku wybranych faz.

Brak listy ograniczonych faz oznacza brak dodatkowego ograniczenia fazą.

### Brak możliwości aktywacji

Niektóre pary mogą być oznaczone jako:

```text
bCanBeActivated = false
```

i nie posiadają standardowej aktywacji.

### Domyślna zasada

Jeżeli karta nie ma specjalnych reguł aktywacji, obowiązuje standard:

```text
aktywacja podczas własnej tury
```

---

## 12. Efekty kart

Efekt jest wykonywany po aktywacji pary.

Efekty kart mogą zmieniać standardowy przebieg gry. Ustaliliśmy, że system musi obsługiwać m.in. efekty takie jak:

- dodatkowe dobieranie kart,
- dobranie kolejnej karty od tego samego gracza,
- dobranie karty od innego gracza,
- reakcje wykonywane poza własną turą,
- anulowanie / kontrowanie innych efektów,
- pominięcie tury,
- ujawnienie ręki,
- przekazywanie kart,
- przekazywanie / przenoszenie par,
- wpływanie na Victory Stack,
- czasowa lub trwała odporność na określone efekty,
- efekty działające tylko w określonym etapie tury.

Efekt konkretnej karty może wprowadzić wyjątek od zasad ogólnych.

### Fimarik

Dla **Fimarik** nie ma dodatkowego efektu gameplayowego.

Po aktywacji jego effect task może zakończyć się od razu, a para przechodzi standardowo do Victory Stack.

---

## 13. Reakcje i efekty poza turą

Nie wszystkie działania muszą należeć do aktualnego gracza.

Niektóre karty mogą być aktywowane jako reakcja poza turą właściciela.

Dlatego:

- brak własnej tury nie oznacza automatycznie braku możliwych akcji,
- system nie może zakładać, że tylko `CurrentPlayer` może zawsze wykonać jakąkolwiek akcję,
- dostępność aktywacji zależy od reguł konkretnej karty.

To jest również powód, dla którego automatyczne pomijanie faz nie powinno opierać się wyłącznie na tym, czy gracz może obecnie stworzyć parę.

---

## 14. Victory Stack

Po pełnym zakończeniu efektu aktywowanej pary para trafia do **Victory Stack** jej właściciela.

Karty w Victory Stack:

- nie znajdują się już w ręce,
- nie znajdują się już w Player Zone,
- nie są ponownie aktywowane,
- są traktowane jako zdobyty punkt,
- mogą być prezentowane jako zakryte.

Relacja pomiędzy dwiema kartami nie musi być dalej przechowywana mechanicznie w Victory Stack — dla punktacji wystarczy liczba zdobytych par / kart.

---

## 15. Koniec gry

Gra kończy się, gdy:

```text
łączna liczba kart pozostających w rękach graczy
<
liczba graczy
```

Przykład dla 3 graczy:

```text
3 lub więcej kart w rękach  → gra trwa
2 lub mniej kart w rękach   → warunek końca gry
```

---

## 16. Punktacja

Punkty dają wyłącznie pary, które zakończyły aktywację i trafiły do `Victory Stack`.

```text
1 para w Victory Stack = 1 punkt
```

Nie dają punktów:

- karty pozostające w ręce,
- nieaktywowane pary znajdujące się w Player Zone / Activation Area.

Wygrywa gracz z największą liczbą punktów.

Sposób rozstrzygania remisu nie został jeszcze jednoznacznie ustalony.

---

## 17. Pierwszeństwo zasad kart

Najważniejsza reguła wyjątków:

> Jeżeli tekst / efekt konkretnej karty jest sprzeczny z zasadą ogólną, obowiązuje tekst karty.

Nie należy kodować zasad ogólnych w sposób, który uniemożliwi wyjątki wprowadzane przez efekty kart.

---

# Reguły istotne dla implementacji multiplayer

## Informacja prywatna

Tożsamość karty w ręce powinna być znana:

- serwerowi,
- właścicielowi karty.

Nie powinna być znana pozostałym klientom, dopóki karta nie zostanie ujawniona.

## Informacja publiczna

Publiczne są:

- liczba kart w każdej ręce,
- kolejność / pozycje kart,
- pozycja wstawienia nowo dobranej karty,
- karty znajdujące się w Player Zone,
- aktywowane / ujawnione karty,
- stan Victory Stack potrzebny do prezentacji i punktacji,
- aktualny gracz,
- aktualna faza tury.

## Autorytet

Gameplay jest server-authoritative.

Serwer powinien ostatecznie walidować m.in.:

- czy gracz może wykonać akcję,
- czy może dobrać kartę,
- czy karta rzeczywiście należy do wskazanego gracza,
- czy dwie karty mogą utworzyć parę,
- czy etap pozwala na utworzenie pary,
- czy akcja parowania została już wykorzystana,
- czy para może zostać aktywowana,
- czy efekt może zostać wykonany,
- przejścia pomiędzy strefami kart.

Klient odpowiada głównie za:

- input,
- wybór karty,
- lokalną prezentację,
- animacje,
- podgląd możliwej akcji.

---

# Strefy karty

Każda runtime'owa karta znajduje się logicznie w jednej strefie:

```text
Deck
Hand
Activation / Player Zone
Victory
```

Typowy cykl życia karty:

```text
Deck
  ↓ deal
Hand
  ↓ create pair
Activation
  ↓ activate + finish effect
Victory
```

Dobranie od przeciwnika:

```text
Player A Hand
      ↓ draw
Player B Hand
```

---

# Rzeczy, których nie należy zakładać

Codex nie powinien zakładać, że:

- utworzenie pary zawsze kończy fazę,
- utworzenie pary w `SecondPairing` kończy turę,
- gracz może dowolnie sortować rękę po rozpoczęciu gry,
- wszystkie aktywacje są możliwe tylko podczas własnej tury,
- wszystkie pary są aktywowalne,
- brak możliwej pary oznacza brak możliwych akcji,
- zakończenie efektu następuje synchronicznie,
- może istnieć tylko jeden aktywny effect task,
- tekst zasad ogólnych ma pierwszeństwo przed efektem karty.

---

# Nadal nieustalone / wymagające osobnego doprecyzowania

Poniższych rzeczy nie należy wymyślać podczas implementacji:

- pełna, kanoniczna lista wszystkich kart i dokładnych tekstów ich efektów,
- sposób rozstrzygania remisu,
- dokładny UX początkowego ustawiania ręki,
- dokładny moment prezentacyjny ogłoszenia końca meczu,
- zachowanie konkretnych wyjątków, jeśli nie wynika jeszcze z definicji danej karty.

Jeżeli implementacja wymaga jednej z tych informacji, należy najpierw ją ustalić zamiast zgadywać.
