# Bodgy – Mistrz Shao Li

Definicja: `/Game/SeaHorse/Cards/Definitions/Card_BodgyShaoLiMaster`.
Efekt: `RevealHandEffectTask`, fragment: `RevealHandEffectFragment`.
Definicja korzysta z istniejącej grafiki `T_Bodgy_Shaoli_Master`; nie jest automatycznie dodawana do talii.

Aktywujący wskazuje rękę przeciwnika albo stos BN. Chroniony gracz i pusta ręka nie są celami.
Karty widzą wyłącznie aktywujący i właściciel wybranej ręki. Dla BN podgląd dostaje tylko aktywujący.
Właściciel z Konikiem Morskim może przeciągać wszystkie swoje karty. Zmiana miejsca w układzie
jest zatwierdzana przez serwer i widoczna u obu uczestników; nowa kolejność pozostaje po podglądzie.
Przycisk „Gotowe” lub Escape u aktywującego kończy efekt. Zwykłe akcje planszy czekają na jego zakończenie.

## Wygląd i Blueprint

W `Card Fragments` definicji karty można przypisać:

- `Reveal Pawn Class`: Blueprint dziedziczący po `SHHandRevealPawn`.
- `Reveal Widget Class`: Widget Blueprint dziedziczący po `HandRevealWidget`.

Pawn udostępnia `ViewRoot`, `CardsRoot`, `CardLayoutSpline`, `RevealCamera` oraz ustawienia układu i hovera.
Pojawia się przy aktualnej kamerze każdego uczestnika i zachowuje jej pozycję, obrót, projekcję,
pole widzenia oraz ustawienia postprocessu. Stół pozostaje w tym samym kadrze.
Karty są ustawiane przed kamerą i korzystają z oświetlenia mapy w tej okolicy.
Układ wachlarza jest generowany z parametrów `Hand Reveal | Layout`:

- `Cards Distance From Camera`: odległość płaszczyzny kart od kamery (domyślnie 100 cm).
- `Screen Width Fraction` i `Screen Height Fraction`: maksymalna część kadru zajmowana przez układ.
- `Card Scale`, rozstaw i parametry wachlarza: wielkość kart względem całego układu.
- `Minimum View Width`: minimalna wirtualna szerokość układu; większa wartość zmniejsza karty.

Dopasowanie ręki zmienia skalę `CardsRoot`, a nie zoom kamery. Parametry `RevealCamera` są
przejmowane z aktualnego widoku gracza przy otwarciu podglądu.
`Get Cards` zwraca prywatny zestaw kart, ich definicje i referencje do oryginałów.
`Get Presentation Cards` zwraca lokalne kopie wizualne z kompletnym wyglądem i tekstami.
`Can Reorder Cards`, `Can Finish Viewing` i `On Presentation Changed` służą do prezentacji uprawnień.

Widget zachowuje layout wykonany w Designerze. Można dodać zwykły `Button` o nazwie
`FinishButton` i `TextBlock` o nazwie `InformationText` — baza obsłuży je automatycznie.
Alternatywnie własny przycisk może wywoływać `Finish Viewing`; widoczność ustaw przez
`Can Finish Viewing`. `Get Reveal Pawn`, `Get Source Hand` i `On Reveal Changed` udostępniają kontekst.
Pusta baza tworzy prosty komunikat i przycisk „Gotowe”.

Każdy uczestnik ma oddzielnego prywatnego pawna używanego lokalnie jako `ViewTarget`.
Kontroler kieruje do niego wejście myszy bez zmiany posiadanego pawna rozgrywki, dzięki czemu
replikacja `Possess` nie może nadpisać kamery po zamknięciu. Po zakończeniu przywracana jest
poprzednia kamera. Wyjście uczestnika, zniszczenie pawna oraz zmiana mapy sprzątają podgląd.

## SceneCapture2D w kolejnym kroku

Do własnego Blueprintu pawna można później dodać `SceneCaptureComponent2D` i Render Target.
Lista `Get Presentation Cards` pozwala ograniczyć przechwytywaną scenę przez `Show Only Actors`.
Kopie należą do pawna, mają wyłączoną kolizję oraz replikację i nie zawierają efektów aktywacji.
Obecna wersja korzysta z kamery pawna; wyświetlanie przechwyconego obrazu i przeliczenie kursora
z obszaru `Image` będą osobnym krokiem integracji.

Definicje kart trafiają wyłącznie przez RPC do kontrolerów dwóch uczestników. Efekt nie używa
publicznego `SHCard::Reveal()` ani nie zmienia właściciela prawdziwych kart. Uprawnienia,
kolejność oraz zakończenie podglądu zatwierdza serwer; widget przechowuje tylko stan prezentacji.
