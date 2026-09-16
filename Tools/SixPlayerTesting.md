# Testy mapy sześciu miejsc

Mapa: `/Game/SeaHorse/Maps/L_Game_SixPlayers`.

Po ponownym uruchomieniu edytora otwórz mapę i ustaw w opcjach Play:
- Net Mode: Play As Listen Server (lub Play As Client z dedykowanym serwerem).
- Number of Players: od 2 do 6.

Przy bezpośrednim PIE GameMode pobiera liczbę ludzi z ustawień Play. Czeka na cały skład przed rozdaniem kart. Miejsca bez ludzi zajmują BN. Liczbę miejsc odczytuje z aktorów BP_Hand na mapie; LayoutSeatIndex musi być unikalny i ciągły od zera (tutaj 0–5). Starsza mapa czterech miejsc nadal obsługuje do czterech ludzi.

Poza PIE można uruchomić serwer z adresem mapy:
`/Game/SeaHorse/Maps/L_Game_SixPlayers?SHExpectedPlayers=6`
Parametr serwera SHExpectedPlayers ma pierwszeństwo przed ustawieniami PIE. Bez parametru poza PIE obowiązuje ExpectedPlayerCount z GameMode (domyślnie 2). Liczba graczy przekraczająca pojemność mapy powoduje odrzucenie startu.

Lobby pozostaje przy dotychczasowym limicie czterech graczy i mapie; obsługa wyboru mapy i udostępnienie sześciu miejsc w lobby to kolejny etap.

Testy automatyczne: SeaHorse.Gameplay.Players.TwoToSix oraz SeaHorse.Gameplay.UI.DelayedPlayerState. Pierwszy sprawdza 2–6 ludzi na sześciu miejscach i 2–4 na czterech, BN, rozdanie, kolejność tur, obrót rąk i błędną numerację. Drugi sprawdza inicjalizację klienta dla czterech i sześciu miejsc przy opóźnionym PlayerState.
