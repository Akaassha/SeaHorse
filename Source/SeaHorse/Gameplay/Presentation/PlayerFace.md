# Reprezentacja gracza: widget na static meshu

## Cały widget tak jak w Designerze

Do tekstury trafia **całe drzewo WBP_PlayerRepresentation**, włącznie z tłem, dekoracjami i materiałami UI. Dla tego wariantu użyj **/Game/SeaHorse/Board/Materials/M_PlayerWidgetSurface**: przypisz go do slotu mesha albo do Face Base Material w BP_PlayerRepresentation. To Surface / Unlit / AlphaComposite, który przekazuje RGB do Emissive i alfę do Opacity bez Clamp, dodatkowego tła i wpływu świateł. Tło projektuj bezpośrednio w widgecie. Materiał nie wyłącza tonemappingu, ekspozycji, mgły ani pozostałego postprocessingu sceny, więc nie obiecuje identycznych pikseli z podglądem UI.

W Designerze wybierz podgląd **Custom** o znanym rozmiarze, np. 512 × 512. W reprezentacji ustaw identyczne **Face Design Size**. **Face Render Size** to niezależna rozdzielczość obrazu, np. 1024 × 1024 dla ostrzejszego tekstu; jej zwiększenie nie zmienia fontów ani rozmieszczenia kontrolek. Przy różnych proporcjach układ mieści się w całości, z przezroczystymi marginesami. Zachowaj zgodne proporcje powierzchni mesha, UV i tekstury, aby nie rozciągać obrazu.

Własny widget zachowuje ustawienia wyrównania, zawijania, clippingu i stylów. Automatyczna obsługa PlayerNameText aktualizuje tylko nazwę; formatowanie ustaw w Designerze. Domyślny układ C++ nadal ma zabezpieczenia przed długimi nazwami. Renderer zapisuje kolor liniowy, bez dodatkowej korekcji gamma przed materiałem sceny.

Poprzedni M_PlayerFace pozostaje wariantem z oświetleniem i osobnym tłem. Nie został nadpisany przy dodaniu M_PlayerWidgetSurface. Żaden materiał nie zamienia UMG Button w interaktywny przycisk na meshu.

## Alternatywa: prosty avatar i nazwa na oświetlonym materiale

Poniższy wariant opisuje pierwotny materiał generowany przez create_face_material.py. Dla całego WBP_PlayerRepresentation użyj konfiguracji M_PlayerWidgetSurface z pierwszej sekcji. Ręczne zmiany w istniejącym M_PlayerFace mogą zmieniać opisane poniżej zachowanie.

1. Przygotuj w meshu slot materiału **PlayerFace**. Powierzchnia przeznaczona na avatar i nazwę powinna wykorzystywać pełny kwadrat UV0: 0–1. Pozostałe sloty, np. Frame i Background, są dowolne. Dla innej proporcji powierzchni dopasuj również proporcje widgetu i Face Render Size.
2. Do slotu PlayerFace przypisz **/Game/SeaHorse/Board/Materials/M_PlayerFace**. Możesz też przypisać własną instancję tego materiału i ustawić BackgroundColor oraz Roughness.
3. W BP_PlayerRepresentation przypisz swój mesh do komponentu Static Mesh. W Class Defaults → Player Representation → Face ustaw **Player Face Widget Class = SHPlayerFaceWidget**. To uruchamia gotowy, prosty układ C++: kwadratowy avatar, wyśrodkowana nazwa z wielokropkiem, przezroczyste tło.
4. Pozostaw **Face Material Slot = PlayerFace**, **Face Texture Parameter = PlayerFaceTexture**, **Face Render Size = 512 × 512**. Nie musisz tworzyć Render Targetu, Dynamic Material Instance ani połączeń w Tick.
5. Ustaw **Fallback Avatar** do trybu offline i czasu pobierania avatara. **Offline Display Name** to nazwa używana bez PlayerState. Przy aktywnym Steam obraz pobiera się automatycznie.
6. Usuń lub wyłącz starą wizualizację WidgetComponent/TextRender i jej logikę aktualizacji, aby nie wyświetlać nazwy/avatara dwa razy. Nowy widget nie potrzebuje WidgetComponent ani Add to Viewport.
7. Zachowaj kolizję mesha blokującą Visibility. C++ nadal włącza wybieranie reprezentacji tylko w odpowiednim kontekście rozgrywki. Nie dodawaj kolizji dla powierzchni widgetu.

Mechanizm automatycznie wybiera jedyny komponent Static Mesh z podanym slotem. Jeżeli kilka komponentów ma taki slot, w BeginPlay wywołaj **Set Player Face Mesh** z właściwym komponentem. Funkcja zwraca true po podłączeniu materiału. Zwraca false przy niepełnej konfiguracji; szczegóły pojawiają się w Output Log pod LogSHPlayerFace. Komponent musi należeć do tej reprezentacji.

**Face Base Material** jest opcjonalnym nadpisaniem materiału dla slotu PlayerFace. Gdy jest pusty, używany jest materiał już przypisany do mesha. Pozostałe sloty nie są zmieniane.

## Własny Widget Blueprint

Utwórz Widget Blueprint dziedziczący po **SHPlayerFaceWidget** i wybierz go jako Player Face Widget Class. Możesz też zmienić Parent Class istniejącego widgetu, po dostosowaniu jego dotychczasowej logiki.

Jeżeli widget nie ma własnego korzenia w Designerze, baza tworzy prosty układ domyślny. Aby w pełni zmienić wygląd, dodaj własny root i cały układ w Designerze. Układ natywny jest fallbackiem; nie jest edytowalnym drzewem elementów odziedziczonych w Designerze.

Dwa elementy można podłączyć bez grafu:

| Nazwa w Designerze | Typ | Obsługa automatyczna |
| --- | --- | --- |
| PlayerNameText | TextBlock | Nazwa; formatowanie pozostaje takie jak w Designerze |
| AvatarImage | Image | Tekstura avatara; element ukryty przy braku tekstury |

Oba elementy są opcjonalne. Font, kolor, marginesy i położenie ustalasz w Designerze. Aby ograniczyć długie nazwy, wyłącz Auto Wrap Text, ustaw Overflow Policy = Ellipsis i Clipping = Clip To Bounds. **PlayerNameText musi otrzymać ograniczoną szerokość**: np. slot Canvas z ustalonym rozmiarem bez Auto Size, lub SizeBox o ustalonym rozmiarze. Wielokropek nie ograniczy szerokości pola, które samo rośnie wraz z tekstem. Dla obrazu użyj kwadratowego pola; opcjonalnie umieść je w ScaleBox → Scale To Fit.

**Wyśrodkowanie krótkich nazw:** umieść PlayerNameText w SizeBox o stałej szerokości, a na slocie dziecka w SizeBox ustaw Horizontal Alignment = Center. Na samym TextBlock ustaw Justification = Left, ponieważ w UE 5.7 Justification = Center wyłącza wielokropek dla pojedynczej linii. Tak działa układ domyślny: krótka nazwa jest pośrodku, długa wypełnia dostępne pole i kończy się wielokropkiem.

Własne nazwy kontrolek też są obsługiwane: wtedy w **Event On Presentation Updated** odczytaj:

- **Display Name** — pełna nazwa, bez skracania danych gracza;
- **Avatar Texture** — aktualny avatar lub fallback;
- **Selectable** — lokalny stan wybierania reprezentacji;
- **Get Representation** — aktor, z którego możesz odczytać Get Represented Player State / Get Represented Hand.

Przykład: On Presentation Updated → Set Text(Display Name) na własnym TextBlock oraz Set Brush from Texture(Avatar Texture) na własnym Image. Ograniczenie szerokości, clipping i wielokropek ustawiasz wtedy samodzielnie.

Zdarzenie następuje po Construct, tuż przed rysowaniem do tekstury. Nie wywołuj z niego ponownie Refresh Player Face. Widget służy do wyświetlania danych; stan rozgrywki pozostaje w PlayerState/GameState i odpowiednich aktorach.

## Aktualizacja i avatar

- Zmiana reprezentowanego PlayerState, powiadomienie o zmianie nazwy, zmiana Selectable i zakończenie pobierania avatara odświeżają teksturę automatycznie.
- **Refresh Player Face** wywołaj na reprezentacji po zmianie dodatkowych danych wizualnych, np. ikon statusu. Ustawiaj je w On Presentation Updated na podstawie aktualnych danych. Nie ma odświeżania co klatkę, więc sam binding UMG, animacja lub timer wewnątrz widgetu nie zapewni ciągłej zmiany obrazu na meshu.
- **Reload Player Avatar** ponawia żądanie pobrania avatara. Przydaje się np. po późniejszym zalogowaniu do Steam. Błąd pobrania pozostawia fallback; nie uruchamia nieskończonego ponawiania.
- Kolejność wyboru avatara: wynik nadpisanego **Resolve Player Avatar**, avatar pobrany przez Steam, **Fallback Avatar**. Dla własnego dostawcy możesz wyłączyć **Load Steam Avatar** i wywołać Refresh Player Face po zakończeniu ładowania.
- Opóźnione wyniki dotyczące poprzedniego gracza lub poprzedniego żądania są ignorowane. Po odłączeniu gracza następuje powrót do fallbacku.
- **Get Player Face Widget**, **Get Player Face Render Target**, **Get Player Face Material** udostępniają lokalne obiekty prezentacji. Mogą być null przed inicjalizacją lub przy niepełnej konfiguracji.
- Rozdzielczość jest ograniczona do 64–2048 pikseli na oś. Zwykle zacznij od 512 × 512; większą wybierz po ocenie z kamery gry. Większy render target nie zwiększa szczegółowości źródłowego avatara.

Widget i tekstura są tworzone lokalnie, bez replikacji i bez renderowania na dedicated serverze. Render target i materiał są ponownie używane podczas odświeżania. EndPlay odłącza prezentację i przywraca poprzedni materiał, o ile slot nadal używa materiału tego mechanizmu.

## Pierwotny materiał M_PlayerFace i geometria

M_PlayerFace to materiał **Surface / Opaque / Default Lit**, używający UV0 i parametru tekstury **PlayerFaceTexture**. Światło sceny wpływa na jego wygląd. Własna instancja pozwala zmienić BackgroundColor i Roughness; dla wyglądu bez wpływu światła możesz przygotować własny materiał Unlit.

Widget ma przezroczyste tło. Materiał wypełnia je BackgroundColor, dlatego nie potrzebujesz osobnej, nachodzącej płaszczyzny pod avatarem. Jeśli chcesz tam wzór skóry, podmień kolor tła w swoim materiale na teksturę.

Wyjście Slate ma kolor premultiplikowany alfą. Materiał składa je jako `WidgetRGB + BackgroundRGB * (1 - WidgetAlpha)`. Ponowne mnożenie WidgetRGB przez alfa może przyciemnić krawędzie tekstu. Przy własnym materiale przezroczystym dopasuj sposób mieszania do tego formatu.

Ramkę avatara i metalowe ozdoby możesz zrobić w geometrii albo w swoim widgecie. Jeśli wybierasz geometrię, dopasuj położenie obrazu w widgecie do powierzchni/UV mesha. Sam slot materiału nie tworzy mapowania UV.

Dolne ikony mogą być częścią widgetu, jeśli są informacyjne. **Przyciski wyrenderowane do tekstury nie otrzymują automatycznie kliknięć UMG**. Oddzielne akcje ikon wymagają osobnej obsługi trafień/komponentów. Obecny kod obsługuje kliknięcie całej reprezentacji jako celu gracza.

## Zgodność i weryfikacja

Player Face Widget Class pozostaje domyślnie puste, więc istniejące Blueprinty działają jak wcześniej do momentu świadomego przepięcia. Żaden istniejący Blueprint nie jest zmieniany przez tę implementację.

Materiał można odtworzyć przez Tools/PlayerRepresentation/create_face_material.py w Unreal Editor Python; skrypt nie nadpisuje istniejącego materiału i nie edytuje Blueprintów.

Test automatyczny: **SeaHorse.Gameplay.UI.PlayerFacePresentation**. Sprawdza m.in. offline, zmianę nazwy i gracza, ignorowanie spóźnionych avatarów, ponowne użycie tekstury, właściwy slot, sprzątanie i własny układ Widget Blueprint. Z aktywnym RHI odczytuje piksele avatara z Render Targetu i zapisuje Saved/PlayerFacePreview.png do kontroli wizualnej. Test nie wymaga połączenia ze Steam; rzeczywiste pobieranie z kont online należy sprawdzić w zalogowanej sesji gry.
