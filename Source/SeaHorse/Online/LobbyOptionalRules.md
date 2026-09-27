# Opcjonalne zasady Szczuroludzi

Kod nie zmienia layoutu widgetów. Obie opcje są zapisane w replikowanym `SHLobbyGameState.GetLobbyInfo().OptionalRules`.

## Lobby

1. Przy otwieraniu lobby oraz przy `OnLobbyChanged` pobierz `GetLobbyInfo`, potem `OptionalRules`.
2. Pierwszy checkbox: `Allow Orphaned Ratfolk Removal` — pozwala usunąć Szczuroludzi z ręki, jeśli żaden z ich dozwolonych Paulusów nie pozostał w talii, ręce ani Strefie. Domyślnie wyłączony.
3. Drugi checkbox: `Remove Other Paulus After Ratfolk Pair` — po sparowaniu Szczuroludzi z Paulusem usuwa drugi egzemplarz tej samej definicji Paulusa. Domyślnie włączony.
4. Na zmianę checkboxa zmodyfikuj odpowiednie pole pobranej struktury i wywołaj `SHLobbyPlayerController.ServerSetOptionalRules`. Przekaż całą strukturę, zachowując drugą opcję.
5. Kontrolki edycji pokazuj tylko przy `IsLobbyHost`. Ich dostępność określa `CanSetOptionalRules`. Klienci mogą pokazywać aktualne wartości jako tekst lub nieaktywne checkboxy.

Serwer odrzuca żądania klientów oraz zmiany po rozpoczęciu przejścia na mapę. Zmiana zasad zeruje gotowość gości. `OnLobbyChanged` aktualizuje widok także u hosta. Aktualizowanie stanu checkboxów z tego zdarzenia nie powinno wysyłać kolejnego żądania, jeżeli wartość się nie zmieniła.

## Rozgrywka

Wybrane zasady przechodzą z lobby w opcjach `ServerTravel` i można je odczytać z `SHGameState.GetOptionalRules` na wszystkich klientach. Rozgrywka uruchomiona bez lobby używa wartości domyślnych.

Przycisk usunięcia karty może wywoływać `SHPlayerController.ServerRemoveOrphanedRatfolk(Card)`. W podglądzie karty argument pochodzi z `CardInfoWidget.GetInspectedCard`. Po weryfikacji serwer emituje u właściciela kontrolera `OnOrphanedRatfolkRemovalResult(bool Removed)`.

Żądanie jest akceptowane wyłącznie dla własnych Szczuroludzi w ręce, w pierwszym lub drugim etapie własnej kolejki, bez trwającego efektu, wyboru celu albo animacji blokującej turę. Wymagana jest włączona pierwsza opcja i brak wszystkich dopuszczalnych Paulusów w aktywnej grze, także w stosach BN. Karty na Stosach Zwycięstwa i karty usunięte nie blokują usunięcia. Nie przyznaje ono punktu, nie zużywa parowania i nie uruchamia aktywacji.

Warunek obecności Paulusa sprawdza serwer. Widget nie powinien sam przeszukiwać definicji zakrytych kart przeciwników — klient ich nie otrzymuje. Odrzucenie żądania nie zmienia żadnej karty.
