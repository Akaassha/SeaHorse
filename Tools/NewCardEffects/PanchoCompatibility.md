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
| Gnushor | Pierwsze wykonanie zbiera wszystkie pary, w tym Pancha. Drugie nie uruchamia się, bo Gnushora nie ma już w Strefie; Pancho pozostaje na Stosie Zwycięstwa. |
| Hans | Dwa przekazania Stref w prawo. BN pomijane; ochrona Paulusa nie wyłącza Strefy z przekazania. |
| Kurt — Kapłan | Dwie niezależne wymiany z wyborem kart i odbiorcy, także BN. |
| Olga | Dwa osobne cele; para już wysłana na Stos Zwycięstwa nie pojawia się ponownie w wyborze. |
| Otfried | Dwa dodatkowe dobrania ze źródła pierwszego dobrania. |
| Pancho | Można wzmocnić drugą parę Pancho, która następnie wzmacnia dwie różne pary. |
| Paulus — Cichy lider | Dwa wybory gracza i źródła jego dobierania, zapisywane w istniejącej kolejce wymuszonych źródeł. |
| Paulus — Łowca czarownic Wu | Ochrona zostaje ustawiona dwukrotnie, ale wygasa na początku tej samej następnej własnej kolejki. |
| Thronri — Zabójca trolli | Usuwa dwie wybrane pary, własną usuwa tylko raz; brak drugiego celu zwraca Pancha, ale nie przywraca usuniętych par. |
| Wilhelm | Przekazuje do dwóch posiadanych Koników Morskich. Przy jednym egzemplarzu transfer pozostaje w mocy, a Pancho wraca. |
| Ye-Hesha | Dwa tasowania i rozdania; liczba kart zostaje zachowana, także w stosach BN. |

Pozostałe definicje nie są celami Pancho: oba Gieselbrechty reagują poza własną
turą, Jamniki i Konik Morski mają zdolności pasywne, Kurt — Szampierz i
Thronri — Wilkołak nie mogą być aktywowani, a Szczuroludzie rozstrzygają się
podczas parowania. Ograniczenia etapu kolejki nadal obowiązują.

Jedna aktywacja podwojonej pary otwiera jedno okno reakcji. Skuteczne anulowanie
blokuje oba wykonania; przejęcie pary czeka na zakończenie obu. Przy braku
legalnego celu drugiego wykonania pierwszy wynik pozostaje w mocy.

Po wybraniu celu oba egzemplarze Pancha pozostają w jego Strefie i są chwilowo
niedostępne do ponownej aktywacji. Pancho trafia na Stos Zwycięstwa dopiero po
skutecznym wykonaniu obu efektów wskazanej pary. Jeśli drugie wykonanie się nie
uda, Pancho po prostu odzyskuje gotowość w tej samej Strefie — nie wykonuje
wcześniej ruchu na Stos Zwycięstwa i nie przyznaje tymczasowego punktu.
Pierwszy wykonany efekt celu pozostaje w mocy; los samej wskazanej pary wynika
z jej zasad. Niewykorzystane podwojenie wygasa z odblokowaniem Pancha na końcu
tury. Dotyczy to także anulowania aktywacji oraz zebrania lub usunięcia celu,
zanim wykorzysta podwojenie.

Wyjątkiem jest efekt, który w pierwszym wykonaniu sam zbiera Pancha. Gnushor
przenosi wtedy Pancha na Stos Zwycięstwa jako część swojego poprawnie wykonanego
efektu. Brak Gnushora w Strefie uniemożliwia drugie wykonanie, lecz nie cofa już
zapłaconego w ten sposób Pancha.

Jeśli podwojony Pancho wzmocnił dwie pary, niepowodzenie jednej zwraca go tylko
raz i usuwa jego pozostałe niewykorzystane podwojenia. Efekty już rozpoczęte
kończą się normalnie. Zwrot zachowuje pierwotną kolejność sparowania i nie jest
ponownie zbierany przez trwający efekt zbiorowego zebrania par.

Własne zadania efektów w Blueprint mogą zgłaszać wynik przez
`Set Effect Successful` przed `Finish Effect`, lub nadpisywać
`Was Effect Successful`. Wynik opisuje wykonanie efektu niezależnie od miejsca,
do którego trafia jego para; samo rozpoczęcie animacji nie oznacza sukcesu.
Obecne natywne zadania zgłaszają wynik w C++.

Testy: `SeaHorse.Gameplay.Effects.PanchoAllDefinitions`, `SupportPairs`,
`DoubledZoneRotation`, `PanchoRefund`, `PanchoRefundDuringBulkCollection`
oraz `StandardActivationPresentation`.
Są to testy automatyczne logiki i prezentacji bez renderowania obrazu.
