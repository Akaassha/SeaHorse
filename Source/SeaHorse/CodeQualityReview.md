# Przegląd jakości C++ — 2026-09-08

Przegląd objął kod rozgrywki, prezentacji graczy, frontend, ustawienia,
sesje online i avatary. Zmiany zachowują istniejące nazwy funkcji używanych
w Blueprintach i autorytet serwera. Assety Blueprint nie są modyfikowane.

## Poprawione problemy

- Kolejka aktywacji par oddaje sterowanie, kiedy para czeka na zakończenie
  układania kart. Poprzednio ponawiała ten sam element w nieskończonej pętli.
  Zabezpieczenie przed ponownym wejściem i ponowne pobieranie początku kolejki
  chronią także obsługę synchronicznych callbacków prezentacji i efektów.
- Wybór zwycięzcy uwzględnia również sytuację, w której wszystkie uprawnione
  osoby mają ujemną punktację.
- Replikacja nazwy w PlayerState powiadamia PlayerRepresentation przez
  istniejące OnRepresentationChanged. Zmiana reprezentowanego gracza odpina
  poprzednią subskrypcję.
- Wiersze ustawień odpinają delegaty przy zwolnieniu lub zmianie przypisanego
  elementu i usuwają odwołania do poprzednich danych. Aktualizacja suwaka
  z danych nie powoduje ponownego zapisu ustawienia.
- Remapowanie i potwierdzenie resetu klawisza zachowują odwołanie do ustawienia,
  dla którego otwarto okno, nawet jeśli lista ponownie wykorzystała wiersz.
- Asynchroniczne węzły UI używają słabych referencji i kończą się najwyżej raz.
  Otwarcie ekranu sprawdza aktualność layoutu i jego świata po wczytaniu klasy.
- Ustawienia po ponownym włączeniu usuwają nieaktualny powód blokady.
  Nie można dodać pustej zależności ani zależności ustawienia od siebie.
- Callbacki sesji sprawdzają dostępność świata i lokalnego kontrolera przed
  podróżą. Pomiar czasu podtrzymania loading screenu zachowuje precyzję double.

## Weryfikacja

- Kompilacja UE 5.7: SeaHorseEditor oraz SeaHorse, Win64 Development.
- CompileAllBlueprints: 0 błędów, 0 ostrzeżeń kompilatora i 0 problemów
  z wczytaniem Blueprintów.
- Unreal Automation: prefiks `SeaHorse`, 13 testów zakończonych powodzeniem.
  Pięć testów emituje ostrzeżenia; raport: `Saved/Diagnostics/CodeQualityTests/index.json`.
- Nowe regresje: `SeaHorse.Gameplay.Effects.ActivationQueueReadiness`
  oraz `SeaHorse.Frontend.EditConditionsRefresh`.
- `git diff --check`.

## Kontrola w działającej grze

Testy natywne bez renderowania nie zastępują meczu dwóch procesów przez Steam.
Podczas takiego testu należy sprawdzić HUD i późno wczytaną nazwę na kliencie,
aktywację kilku par, końcowe wyniki i reset przez hosta. W menu należy sprawdzić
przewijanie i ponowne otwieranie ustawień, suwaki, remapowanie oraz zmianę mapy
w trakcie otwierania ekranu.

Duże klasy PlayerController i GameMode nadal łączą wiele obowiązków.
Ewentualny podział na komponenty powinien zachować funkcje będące wejściami
Blueprintów i otrzymać testy scenariuszy sieciowych; ten przegląd nie jest
gwarancją braku pozostałych błędów ani pełnym audytem grafów Blueprint.
