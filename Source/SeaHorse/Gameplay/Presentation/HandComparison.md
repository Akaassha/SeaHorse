# Diego – Zwadźca

Definicja karty: `/Game/SeaHorse/Cards/Definitions/Card_Diego`.
Główna talia `/Game/SeaHorse/Deck/DT_Deck` zawiera dwie kopie karty.

Po wybraniu innego, niechronionego gracza serwer porównuje liczbę kart w obu rękach. Jeśli liczby są równe, efekt kończy się od razu. W przeciwnym razie obaj gracze dostają prywatny widok obu rąk: większej u góry i mniejszej u dołu.

Gracz z mniejszą ręką przeciąga z górnego rzędu dokładnie tyle kart, ile wynosiła początkowa różnica. Każdy ruch jest sprawdzany na serwerze. Kartę można przenieść tylko z ustalonej większej ręki do ustalonej mniejszej ręki, więc nie da się jej oddać ani przenieść drugi raz. Po ostatniej karcie widok zamyka się automatycznie.

Widok korzysta z `BP_HandRevealPawn`, tak jak Bodgy – Mistrz Shao Li. Definicje kart są wysyłane wyłącznie prywatnymi RPC do dwóch uczestników; inni gracze ich nie otrzymują.
