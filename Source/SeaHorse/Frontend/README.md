# Frontend SeaHorse

Przeniesiony kod CommonUI jest częścią modułu SeaHorse (UE 5.7). Nie wymaga
zewnętrznych pakietów ani modułu Obscurae. Zależności definiuje SeaHorse.Build.cs.
DefaultEngine.ini ustawia CommonGameViewportClient i SHGameUserSettings;
DefaultInput.ini włącza ustawienia użytkownika Enhanced Input.

## Zakres integracji

- SHGameUserSettings zapisuje lokalne preferencje grafiki, języka i dźwięku.
  Nie przechowuje reguł ani stanu meczu. Usunięto obcą opcję trudności,
  niepodłączoną widoczność HUD i niezaimplementowany przełącznik HDR audio.
- Teksty opcji są w natywnej tabeli SeaHorseFrontend, w FrontendGameplayTags.cpp.
  Są to angielskie teksty bazowe; tłumaczenia wymagają zasobów lokalizacji.
- Brak konfiguracji menu kończy asynchroniczne otwarcie wynikiem Failed.
  Węzeł PushSoftWidget emituje wtedy AfterPush z nullptr. Potwierdzenie kończy się
  Canceled, więc brak ekranu nie zatwierdza akcji.
- Loading screen jest opcjonalny i wyłączony na dedicated server oraz w commandletach.
  Brak przypisanego ekranu nie blokuje renderowania gry.

## Zasoby do podłączenia w edytorze

W chwili integracji katalog zawierał wyłącznie kod C++; projekt nie zawierał
Blueprintów nowego menu, list opcji ani loading screenu. Istniejące Blueprinty
SeaHorse nie były modyfikowane. Kod sam nie tworzy ani nie otwiera głównego menu.

1. Utwórz/przenieś widget dziedziczący po WidgetPrimaryLayout. Zarejestruj jego
   kontenery CommonActivatableWidgetStack przez RegisterWidetStack z tagami
   Frontend.WidgetStack.Modal, GameMenu, GameHud i Frontend.
2. Utwórz layout dla lokalnego PlayerControllera, dodaj go do viewportu i wywołaj
   FrontendSubsystem.RegisterCreatedPrimaryLayoutWidget. Aktualna implementacja
   przechowuje jeden layout na GameInstance; split screen wymaga osobnej adaptacji.
3. W Project Settings → Frontend Settings przypisz klasy ekranów do FrontendWidgetMap.
   Opcjonalne ilustracje ustaw w OptionsScreenSoftImageMap. Ekrany muszą dziedziczyć
   po odpowiadających im klasach C++ i zawierać widgety oznaczone BindWidget.
4. Dla list opcji przypisz DataAsset_DataListEntryMapping i właściwe klasy wierszy.
   Skonfiguruj akcje CommonUI (Back/Confirm/Reset), style oraz domyślny fokus.
5. Opcjonalnie przypisz loading screen w LoadingScreenSettings.
6. Opcjonalnie przypisz MusicSoundClass i SFXSoundClass w Frontend Settings i przypisz
   dźwięki do tych klas. Bez przypisania suwaki kategorii są nieaktywne.
   OverallVolume działa globalnie. Kategorie nie powinny być zagnieżdżone jedna w drugiej.
7. Aby wyświetlić remapowanie, skonfiguruj Player Mappable Key Settings w Input Actions
   i zarejestruj odpowiednie Mapping Contexts w Enhanced Input User Settings.
   Samo włączenie bEnableUserSettings nie tworzy mapowań.

## Weryfikacja

Cele kompilacji: SeaHorseEditor Win64 Development i SeaHorse Win64 Development.
Testy Unreal Automation: SeaHorse.Frontend (SettingsAndRegistry, MissingLayout).
Raport z uruchomienia znajduje się w Saved/Automation/Frontend.

Po podłączeniu assetów sprawdź ręcznie fokus, nawigację myszą i gamepadem, ponowne
uruchomienie po zapisie ustawień, zmianę mapy oraz klienta i listen server w PIE.
Testy bez renderowania nie weryfikują wyglądu, dźwięku ani działania ekranów Blueprint.
