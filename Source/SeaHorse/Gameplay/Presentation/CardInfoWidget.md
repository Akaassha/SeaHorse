# Podgląd karty w Blueprint

## Zamykanie kliknięciem poza kartą

W `On Card Info Opened` wywołaj `Set Click Outside Targets` na Self. Podłącz swój Image do `Card Image`, a przycisk zamknięcia do `Close Button`. Nazwy elementów w Designerze są dowolne; muszą mieć zaznaczone `Is Variable`.

Podgląd nie ma pełnoekranowego tła przechwytującego kliknięcia. UserWidget i kontenery ustaw na `Not Hit-Testable (Self Only)`, a Image karty i Close Button na `Visible`. Jeśli dodano wcześniej pełnoekranowy Border, usuń go lub ustaw jako nieprzechwytujący kliknięć (Self Only, jeśli zawiera Image/przycisk; Self & All Children tylko dla dekoracji).

Kliknięcie lewym przyciskiem, które dociera do kontrolera gry, zamyka podgląd i przechodzi dalej bez konsumowania: może równocześnie wybrać kartę lub wykonać zwykłą interakcję z planszą. Kliknięcie Image jest konsumowane i pozostawia podgląd otwarty. Close Button korzysta ze swojego OnClicked → Close Card Info. Zaokrąglone narożniki należą do prostokątnego obszaru Image. Jeśli Blueprint nadpisuje On Preview Mouse Button Down, wywołaj rodzica i zachowaj wynik Handled. Kliknięcia przechwycone przez inne, niezależne widgety nie docierają do kontrolera gry.
## Kompletny obraz karty w Image

W `On Card Info Opened` wywołaj `Set Brush` na swoim Image i podłącz `Get Card Face Brush` (Target: Self) do wejścia Brush. Obraz zawiera układ, teksty i grafikę z renderowanego widgetu karty, bez efektów materiału mesha w świecie. Zachowaj proporcje obrazu w swoim layoucie.

`Get Card Face Render Target` udostępnia również sam render target, np. do parametru tekstury własnego materiału UI. Nie podłączaj go do `Set Brush from Texture` ani `Make Brush from Texture`, które wymagają Texture2D. Oba gettery sprawdzają dostęp do karty; przed wyrenderowaniem zwracają odpowiednio pusty brush lub null. Brush współdzieli istniejący obraz, nie tworzy kopii aktora ani kolejnego renderowania.

1. Utwórz Widget Blueprint dziedziczący po `CardInfoWidget`. Układ tworzysz w Designerze; baza C++ nie dodaje żadnych elementów.
2. W domyślnych ustawieniach używanego Blueprintu `SHPlayerController` przypisz go do `Cards > Inspection > Card Info Widget Class`.
3. W zdarzeniu `On Card Info Opened` pobierz `Get Card Definition`. Zwraca obiekt domyślny definicji: `CardName`, `SkillName`, `SkillDesc`, `AdditionalText`, `CardTextrue` (obecna nazwa pola w projekcie).
4. `Get Card Definition Class` udostępnia klasę do `Find Fragment By Class`. `Get Inspected Card` udostępnia aktora, np. jego strefę. Sprawdzaj ważność zwróconych obiektów.
5. Przycisk zamknięcia podłącz do `Close Card Info` na Self.

Prawy przycisk na karcie otwiera panel lokalnego gracza. Escape zamyka go; prawy przycisk na panelu również. Nie jest zmieniany tryb wejścia ani zatrzymywana gra. Dobierz widoczność/hit testing elementów swojego widgetu do tego, czy ma zasłaniać interakcję ze stołem. Zdarzenia wejścia obsłużone przez własne dzieci widgetu mogą wymagać przekazania zamknięcia do `Close Card Info`.

Dozwolona jest własna ręka i publicznie odkryte karty. Zakryte karty innych graczy i BN, talia oraz usunięte karty są niedostępne, także dla hosta. Podgląd zamyka się automatycznie po utracie dostępu lub otwarciu pytania o reakcję. Nie otwiera się podczas przeciągania ani pytania o reakcję. Bez przypisanej klasy nie pojawia się żaden zastępczy overlay.

Nie zapisuj definicji jako stanu rozgrywki w widgetach. Gettery sprawdzają dostęp przy każdym odczycie; zamknięty widget nie może odczytać kontekstu kolejnego panelu ani go zamknąć.

