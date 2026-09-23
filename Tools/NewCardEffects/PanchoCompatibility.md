# Pancho — przegląd zgodności

Sprawdzono 25 zapisanych definicji kart, w tym wszystkie 18 posiadających
aktywację. Testy uruchamiają Pancho, wskazują parę, następnie aktywują ją
i sprawdzają wynik obu wykonań oraz zwolnienie kolejki, wyboru celu i animacji.
Definicje Blueprint są wczytywane bez modyfikowania assetów ani talii.

| Karta | Wynik podwojenia |
| --- | --- |
| Aramdila | Dwa przekazania talii w lewo, także przez BN. |
| Bodgy | Dwie sekwencje dobierz–zwróć: do czterech dobranych i dwóch zwróconych kart. |
| Crumo i Ursula | Dwa dodatkowe dobrania od gracza innego niż źródło pierwszego dobrania. |
| Fimarik | Dwa wykonania zadania BP; para na Stosie Zwycięstwa nadal daje jeden punkt. |
| Gloria | Dwa wybory gracza i dwa zaplanowane pominięcia kolejki. |
| Gniew — Martwy herold | Dwa niezależne wybory źródła; przekazuje tylko dostępne egzemplarze Bodgiego. |
| Gniew — Żywy herold | Przejęcie dwóch uprawnionych par z osobnym wyborem celu. |
| Gnushor | Pary trafiają na stosy właścicieli tylko raz; drugie wykonanie nie powiela kart ani punktów. |
| Hans | Dwa przekazania Stref w prawo. BN pomijane; ochrona Paulusa nie wyłącza Strefy z przekazania. |
| Kurt — Kapłan | Dwie niezależne wymiany z wyborem kart i odbiorcy, także BN. |
| Olga | Dwa osobne cele; para już wysłana na Stos Zwycięstwa nie pojawia się ponownie w wyborze. |
| Otfried | Dwa dodatkowe dobrania ze źródła pierwszego dobrania. |
| Pancho | Można wzmocnić drugą parę Pancho, która następnie wzmacnia dwie różne pary. |
| Paulus — Cichy lider | Dwa wybory gracza i źródła jego dobierania, zapisywane w istniejącej kolejce wymuszonych źródeł. |
| Paulus — Łowca czarownic Wu | Ochrona zostaje ustawiona dwukrotnie, ale wygasa na początku tej samej następnej własnej kolejki. |
| Thronri — Zabójca trolli | Usuwa dwie wybrane pary, własną usuwa tylko raz; brak drugiego celu nie przywraca zużytej pary. |
| Wilhelm | Przekazuje do dwóch posiadanych Koników Morskich. Przy jednym egzemplarzu drugie wykonanie niczego nie duplikuje. |
| Ye-Hesha | Dwa tasowania i rozdania; liczba kart zostaje zachowana, także w stosach BN. |

Pozostałe definicje nie są celami Pancho: oba Gieselbrechty reagują poza własną
turą, Jamniki i Konik Morski mają zdolności pasywne, Kurt — Szampierz i
Thronri — Wilkołak nie mogą być aktywowani, a Szczuroludzie rozstrzygają się
podczas parowania. Ograniczenia etapu kolejki nadal obowiązują.

Jedna aktywacja podwojonej pary otwiera jedno okno reakcji. Skuteczne anulowanie
blokuje oba wykonania; przejęcie pary czeka na zakończenie obu. Przy braku
legalnego celu drugiego wykonania pierwszy wynik pozostaje w mocy.

Testy: `SeaHorse.Gameplay.Effects.PanchoAllDefinitions`, `SupportPairs`,
`DoubledZoneRotation` oraz `StandardActivationPresentation`.
Są to testy automatyczne logiki i prezentacji bez renderowania obrazu.
