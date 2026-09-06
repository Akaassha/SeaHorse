# Sesje i lobby — podłączenie w Blueprintach

Kod korzysta z obecnego OnlineSubsystem (domyślnie Steam), obsługuje listen server
i 2–4 graczy. Subsystem sesji istnieje automatycznie dla GameInstance. Nie trzeba
tworzyć nowej klasy GameInstance. UI nie przechowuje autorytatywnego stanu lobby.

## Ustawienia map i klas

W Project Settings → SeaHorse Multiplayer dostępne są MainMenuMap, LobbyMap,
MatchMap, MainMenuGameMode, LobbyGameMode i limit czasu operacji. Domyślne mapy:

| Rola | Mapa | Klasa bazowa GameMode |
| --- | --- | --- |
| Menu | `/Game/SeaHorse/Maps/L_MainMenu` | `SHMainMenuGameMode` |
| Lobby | `/Game/SeaHorse/Maps/L_Lobby` | `SHLobbyGameMode` |
| Rozgrywka | `/Game/SeaHorse/Maps/L_Test` | istniejący Blueprint po `SHGameMode` |

Config/DefaultEngine.ini przypisuje natywne GameMode przez prefiksy nazw map.
World Settings → GameMode Override ma pierwszeństwo przed prefiksem: jeśli jest
już ustawiony, użyj właściwej klasy lub jej dziecka BP. Tworzenie sesji wymusza
GameMode lobby z SeaHorse Multiplayer, a powrót do menu wymusza klasę menu z tych
samych ustawień. **Wpisz tam swoje klasy BP**, jeśli dodajesz w nich własny
PlayerController, kamerę czy inicjalizację widgetów.

SHLobbyGameMode domyślnie wybiera SHLobbyGameState, SHLobbyPlayerState
i SHLobbyPlayerController. Ich dzieci BP można stosować zamiast klas bazowych.
Menu i lobby nie tworzą pawnów ani nie inicjalizują talii.

Nie zmieniono assetów map ani Blueprintów. Projekt ma listę MapsToCook dla
L_MainMenu, L_Lobby, L_Test i istniejącego L_SandBox. Po zmianie MatchMap dodaj
nową mapę także do listy map do spakowania.

## Main menu

### Proste wyszukiwanie przez node asynchroniczny

Przycisk wyszukiwania można podłączyć bezpośrednio do **Find Matches Async**
(kategoria SeaHorse → Sessions). Nie wymaga ręcznego podpinania delegatów subsystemu.

- **On Success → Results → For Each Loop**: zbuduj listę pokojów.
  Pusta tablica na On Success oznacza, że wyszukiwanie się udało, ale nie znaleziono pokoi.
- **On Failure → Error**: pokaż błąd. Results jest wtedy puste.
- Wiersz pokoju przechowuje cały Result; jego przycisk Dołącz wywołuje
  SHSessionSubsystem.JoinMatch(Result).

Zwykłe wyjście wykonania nodu nie oznacza zakończenia wyszukiwania. Listę buduj
wyłącznie z On Success. Dotychczasowe FindMatches i OnSearchComplete nadal działają
dla istniejących Blueprintów; wybierz jeden sposób obsługi, aby nie wyszukiwać podwójnie.

W lokalnym widgetcie pobierz `Get Game Instance Subsystem → SHSessionSubsystem`.
Najpierw podepnij delegaty, potem wywołuj funkcje:

| Przycisk / akcja | Wywołanie |
| --- | --- |
| Utwórz | `CreateMatch(ServerName, MaxPlayers, bLAN)` |
| Odśwież listę | `FindMatches(bLAN, MaxResults)` |
| Dołącz | `JoinMatch(Result)` |
| Wyjdź z sesji | `LeaveMatch()` |

`ServerName` ma 1–64 znaki, a `MaxPlayers` 2–4, **łącznie z hostem**.
`bLAN=false` służy do wyszukiwania lobby online, `bLAN=true` do zapytań LAN.
Tryb LAN nie przełącza automatycznie projektu ze Steam na Null.

- `OnSearchComplete(bSuccess, Results)` dostarcza listę. Każdy `FSHSessionResult`
  zawiera nazwę serwera, hosta, liczbę zajętych/maksymalnych miejsc, ping i tryb LAN.
  W wierszu listy zachowaj cały Result i przekaż go do JoinMatch.
- `ResultId` jest nieprzezroczystym identyfikatorem. Wyniki poprzedniego wyszukiwania
  wygasają po uruchomieniu kolejnego; nie konstruuj ResultId ani indeksów ręcznie.
- `OnOperationComplete(Operation, bSuccess, Error)` obsługuje wynik każdej operacji,
  również odrzucenie nieprawidłowego żądania. Przy tworzeniu/dołączaniu/wychodzeniu
  sukces oznacza zakończenie ładowania mapy. **Nie wywołuj dodatkowo OpenLevel**.
- `OnOperationChanged` oraz `IsBusy` pozwalają blokować przyciski na czas operacji.
  Operacje są wykonywane pojedynczo; LeaveMatch również wymaga zakończenia bieżącej.
- `OnConnectionError(Error)` zgłasza utratę połączenia, błąd travel lub timeout.
  Subsystem próbuje usunąć lokalną sesję i wrócić do menu. W razie błędu usuwania
  można ponowić LeaveMatch przed kolejną próbą hostowania.
- `GetSearchResults`, `GetOperation`, `GetLastError`, `HasSession` i
  `GetOnlineServiceName` udostępniają bieżący stan do UI/diagnostyki.

Podepnij obsługę błędów w trwałym obiekcie, np. własnym GameInstance BP, lub zachowaj
otrzymany tekst w lokalnym modelu prezentacji, jeśli chcesz wyświetlić go po zmianie
mapy. Widget niszczony podczas travel nie odbierze późniejszego zdarzenia.
OnSearchComplete jest emitowany dla rozpoczętego wyszukiwania; natychmiastowe
odrzucenie żądania jest zgłaszane przez OnOperationComplete.

## Lobby

1. Pobierz `Get Game State → Cast to SHLobbyGameState`.
2. Podepnij `OnLobbyChanged`, a następnie **od razu odczytaj stan** — replikacja
   mogła dotrzeć przed utworzeniem widgetu.
3. `GetLobbyInfo` zwraca ServerName, MaxPlayers, Host i bStartingMatch.
   `GetLobbyPlayers` zwraca posortowaną listę SHLobbyPlayerState.
4. Z każdego PlayerState odczytaj `GetPlayerName` i `IsReady`. Możesz też podpiąć
   `OnLobbyPlayerChanged` dla pojedynczego wiersza.
5. Pobierz **własny** PlayerController i rzutuj na SHLobbyPlayerController.
   Przycisk gotowości wywołuje `ServerSetReady(bool)`.
6. Przycisk Start wywołuje `ServerStartMatch()`. Pokaż go hostowi przez `IsLobbyHost`
   i aktywuj przy `GameState.CanStartMatch`. Serwer niezależnie weryfikuje uprawnienia
   i gotowość; samo pokazanie przycisku klientowi niczego nie odblokuje.
7. `OnLobbyRequestRejected(Reason)` na lokalnym kontrolerze pozwala pokazać odmowę.
8. Wyjście wywołuje `SHSessionSubsystem.LeaveMatch`. Wyjście hosta zamyka lobby;
   nie ma migracji hosta. Pozostali klienci obsługują utratę połączenia i wracają do menu.

Host jest automatycznie gotowy. Pozostali gracze zaczynają jako niegotowi.
Start wymaga obecnego hosta i co najmniej dwóch gotowych graczy. Liczba uczestników
nie musi wypełniać wszystkich miejsc lobby. W czasie startu zmiana gotowości
i nowe logowania są blokowane. Jeśli gracz odejdzie podczas uruchamiania sesji
online, host otrzyma błąd i lobby zostanie zamknięte zamiast uruchomić niepełny mecz.

## Przejście do meczu

Serwer wywołuje StartSession, a następnie ServerTravel do MatchMap z opcją
`SHExpectedPlayers=N`. SHGameMode odczytuje ją w InitGame i czeka na właściwą liczbę
graczy (łącznie z hostem). Domyślna wartość C++ to 2. Logowania po starcie rozgrywki są odrzucane.

Lobby używa seamless travel, aby zachować połączenia Steam podczas zmiany mapy.
Silnik wymienia kontrolery i PlayerState na klasy mapy rozgrywki, a następnie wywołuje
HandleStartingNewPlayer, które przypisuje ręce i uruchamia talię po przygotowaniu
wszystkich uczestników. Mapa rozgrywki
zachowuje własny GameMode z World Settings i jego ustawienia kart/komponentów.
Nadal musi zawierać cztery poprawnie skonfigurowane ręce, tak jak obecna rozgrywka.
Gotowość lobby nie jest przenoszona do stanu rozgrywki.

Operacja Start kończy się przez NotifyMatchReady po ustawieniu MatchReady przez
serwer. Samo załadowanie mapy hosta nie oznacza zakończenia startu. Seamless travel
nie emituje PostLoadMapWithWorld, więc ten callback obsługuje pozostałe przejścia.

## Weryfikacja i zakres

Automatyczne testy `SeaHorse.Multiplayer` sprawdzają gotowość, minimalną liczbę
graczy, brak hosta, limit miejsc, blokadę ponownego startu, konfigurację map oraz
ekspozycję funkcji/RPC do BP. Nie zastępują testu dwóch rzeczywistych klientów.

Do testu Steam użyj dwóch osobnych procesów/komputerów i kont Steam, zgodnie
z obecną konfiguracją projektu. SteamDevAppId pozostaje 480, a wyniki są filtrowane
przez znacznik SeaHorse_1. Obserwuj też OnConnectionError i OnOperationComplete.
Pakiet należy sprawdzić osobno — sama kompilacja nie wykonuje cook ani testu Steam.

Scenariusz ręczny: host tworzy lobby → klient odświeża listę i dołącza → klient
ustawia gotowość → host uruchamia mecz → obie osoby trafiają do L_Test. Powtórz
dla 3/4 osób, pełnego lobby, wyjścia klienta i zamknięcia hosta. Przetestuj ponowne
utworzenie sesji po powrocie do menu. Obecne API nie obsługuje zaproszeń Steam,
hasła do lobby, dedicated server, split screen ani dołączania do trwającej partii.
