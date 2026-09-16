# Magiczny krąg — UE 5.7

Gotowe assety: `/Game/SeaHorse/VFX/MagicCircle/`.

| Asset | Zastosowanie |
| --- | --- |
| `NS_MagicCircle` | Jednorazowy efekt Niagara, 1,5 s, średnica 32 cm, pozycja +12 cm na osi Z. |
| `M_MagicCircle` | Proceduralny, dwustronny materiał unlit/additive. Bez tekstur i bez oświetlenia sceny. |
| `MI_MagicCircle` | Instancja używana przez Niagara. |
| `MI_MagicCircle_Preview` | Statyczny podgląd w połowie animacji. |
| `SM_MagicCircle_Preview` | Płaszczyzna 100 × 100 cm z materiałem podglądowym. Skala 0,32 daje rozmiar Niagara. |

Krąg jest poziomy, skierowany wzdłuż osi świata Z. Turkusowe znaki i złoty środek
obracają się w przeciwnych kierunkach. Przez pierwsze 12% czasu życia efekt
pojawia się płynnie, a przez ostatnie 28% gaśnie.

## Wywołanie z animacji karty

Efekt jest już podpięty do Aramdila, Crumo/Ursula, Fimarik, Gnushor i Otfried
przez `ActivationVFX` w ich `CardEffectFragment`. Serwer wywołuje dedykowane
zdarzenie multicast po rozpoczęciu obsługi pary. Jeden krąg pojawia się nad CardA
na każdej maszynie; powtórne powiadomienie tej samej pary jest ignorowane.
`ActivationVFXDuration` ustala czas oczekiwania prezentacji (domyślnie 1,5 s).
Zakończenie oczekiwania jest sterowane timerem, niezależnie od renderowania GPU.
Gloria, Olga, Paulus i Wilhelm również mają przypisany `ActivationVFX`.
Ich zadania zwracają `RequiresTargetSelection() = true`, więc krąg nie uruchamia
się na początku wybierania. Wywołują `PlayActivationVFX()` dopiero po ostatnim
zatwierdzonym celu, przed wykonaniem operacji na kartach. Paulus czeka na wybór
zarówno gracza dobierającego, jak i gracza, od którego ma dobrać kartę.
Odrzucony wybór albo zakończenie zadania bez dostępnego celu nie uruchamia kręgu.

Nie dodawaj drugiego wywołania Niagara w BP_Hand dla tych pięciu kart.
Poniższe ręczne wywołanie służy do podglądu lub innych animacji:

W lokalnym zdarzeniu prezentacji, np. `BP_Hand.OnPairEffectActivated`, użyj
`Spawn System at Location` z systemem `NS_MagicCircle`, pozycją `CardA.GetActorLocation`,
zerową rotacją, skalą `(1,1,1)`, `Auto Activate=true` i `Auto Destroy=true`.
Przesunięcie o 12 cm jest już ustawione w emitterze — nie dodawaj go drugi raz.
Dla jednego kręgu nad całą parą użyj środka pomiędzy pozycjami CardA i CardB.

Wywołanie powinno wykonać się raz na każdej maszynie wyświetlającej zdarzenie
prezentacji. Niagara jest lokalnym efektem wizualnym i nie zmienia stanu rozgrywki.
Sam `Spawn System at Location` nie replikuje efektu. Nie wywołuj go co klatkę.
Efekt pozostaje w miejscu wywołania; poruszająca się karta nie przeciąga go za sobą.

## Dostosowanie

W `MI_MagicCircle` zmieniaj `Primary`, `Accent`, `Intensity` i `RotationSpeed`.
`UseParticleAge` pozostaw równe 1; wartość 0 i `PreviewAge` służą wyłącznie do
podglądu materiału. W Niagara `Initialize Particle` steruje Lifetime, Sprite Size
i Initial Position. Zmieniając czas życia, dostosuj też Loop Duration emitera.
Rozmiar samej cząsteczki ustawiaj przez Sprite Size w emitterze.

## Wiek cząsteczki w materiale

Emitter Lightweight nie udostępnia `Particles.NormalizedAge`. Dlatego materiał
czyta `EffectAge` z kanału X `Dynamic Parameter 0`, a moduł `Dynamic Material
Parameters` przekazuje liniową krzywą 0–1 przez czas życia cząsteczki.
Zastąpienie tego wejścia przez `Particle Relative Time` powoduje niewidoczny krąg:
wejście wieku pozostaje zerowe, więc przezroczystość również wynosi zero.

## Weryfikacja i źródła

- `MagicCircle_Preview.png`: render materiału z Unreal, bez bloom/postprocessingu.
- `capture_niagara.py`: renderuje zapisany system w tymczasowej mapie PIE,
  z normalnym upływem czasu. `Niagara_Early.png`, `Niagara_InScene.png`,
  `Niagara_Fading.png` i `Niagara_Finished.png` pokazują kolejne etapy animacji.
  Mały krąg u góry jest statycznym obiektem kontrolnym.
- `verify_niagara.py`: aktywacja i symulacja systemu; aktywny po 0,75 s,
  nieaktywny po 3,75 s. Wynik w `niagara_validation.txt`.
- Materiał wyrenderowany z RHI; Niagara zapisana i zweryfikowana jako poprawna.
- Integracja zmienia przypisania prezentacji w dziewięciu definicjach kart.
  Kopie sprzed integracji są w `Saved/Diagnostics/MagicCircleIntegrationBackup`.
- Test `SeaHorse.Gameplay.Effects.StandardActivationPresentation` weryfikuje
  przypisania w zapisanych BP i właściwy moment uruchomienia dla każdego typu zadania,
  deduplikację i zwolnienie blokady prezentacji.
- `SeaHorse.Gameplay.Effects.TargetedCircleWaitsForAllTargets` sprawdza wybór
  dwóch celów Paulusa, odrzucony cel i powtórzenie końcowego żądania.

`magic_circle.hlsl` oraz `create_material.py` zawierają źródła materiału.
Generator odmawia nadpisania istniejących materiałów.
Pliki commandletu są zachowane jako źródło generatora Niagara i nie są
kompilowane z grą. Do odtworzenia systemu trzeba tymczasowo skompilować je
w module edytora z zależnościami Niagara, NiagaraEditor, NiagaraShader,
RenderCore, RHI, UnrealEd i AssetRegistry oraz ścieżkami Internal modułów
Niagara i NiagaraShader. Commandlet `-run=SHCreateMagicCircle` również odmawia
nadpisania istniejącego systemu. Gotowe assety nie wymagają tego narzędzia.
