# FreeWalk - zseblámpa (4. alprojekt)

Dátum: 2026-10-14

## Cél

Séta közben, főleg éjjel, a játékos zseblámpát kapcsolhat be: a gyalogló kezéből a nézés irányába mutató fénykúp, amely a kerítéseket, falakat, a kamiont megvilágítja.
A fénykúp mérete séta közben állítható, be- és kikapcsoláskor elhalványul, és beállítható gyakorisággal "rakoncátlankodik" (villog, kialszik, visszajön).

## Döntések (a felhasználóval egyeztetve)

- Éjjel mindenhol működjön (lakott és lakatlan helyen is); nappal nem követelmény.
- Megközelítés: a TM Real Walk mintájára éjjel a játék egy távoli utcai lámpáját "kölcsönvesszük" és a gyalogló kezébe tesszük; ha nincs kölcsönözhető lámpa (lakatlan hely, nappal), a játék `light_source` osztályából saját fényt hozunk létre.
- A TM Real Walk kódja tényforrás; kódot nem veszünk át, minden játéktényt a saját elemzésünkkel és élő teszttel igazolunk.
- Keret: a játék adatait írjuk (a kölcsönzött lámpa mezőit, utána visszaírjuk) és a saját függvényeit hívjuk (konstruktor, destruktor); a játék kódját és vtable-jeit nem módosítjuk.
- Billentyűk (alapérték, a beállítóablakban állítható): be/ki `F`, a fénykúp szélesebb `NUMPAD+` (0x6B), szűkebb `NUMPAD-` (0x6D); séta közben az egérgörgő is állítja a méretet (fel: szélesebb, le: szűkebb).
- Minden beállítás az INI-ben és a beállítóablakban: a billentyűk a Keys fülön, a fény beállításai egy új Light fülön.
- Nem része ennek az alprojektnek: a képernyőre rajzolt kéz (rajzoló hook kellene), a több rétegű lágy fénykúp (`layers`, `softness`), a fény a fülkében (pihenés).

## Működés

### Kezelés

- A be/ki billentyű csak séta közben és a játékablak fókuszában hat (a beállítóablak fókuszában nem), mint a séta billentyűi.
- A fénykúp mérete 5 fokozatú: a külső szög `angle × {0,5; 0,75; 1; 1,5; 2}`, 5° és 80° közé szorítva; a belső szög ugyanilyen arányban változik. Bekapcsoláskor a középső fokozat. A szélesebb/szűkebb billentyű és a görgő egy fokozatot lép; a szélső fokozatnál nem lép túl.
- Séta közben az egérgörgőt a plugin elnyeli (a játék a görgővel a szabad kamera sebességét állítaná, amelyet séta közben úgyis lefagyasztunk).
- Be- és kikapcsoláskor a fény `fade_time` alatt erősödik, illetve halványul (lineárisan).

### A fény

- Helye: a szem + a kéz eltolása (`hand_right` jobbra, `hand_down` lefelé) a nézés irányához forgatva (yaw és pitch); iránya a nézés iránya.
- Színe: `(r, g, b) × brightness × a halványítás × a villogás szorzója`; hatótáv `range`, külső szög a fokozat szerint, belső szög `inner_angle` a fokozat arányában.
- Villogás: a lámpa használatának óránként átlagosan `flicker_chance` eseménye (Poisson-folyamat, véletlenszerű időpontokban). Egy esemény: 0,6-1,2 s gyors villódzás (a szorzó 0 és 1 között ugrál), 0,5-1 s sötét ("megrázás"), majd 0,3 s alatt visszaerősödés. `flicker_chance = 0`: soha.

### Fény beszerzése

1. Bekapcsoláskor (és ha a fény elveszett): kölcsönzés. A scene fénytömbjéből a bekapcsolt, `light_source` típusú lámpák a jelöltek; a kiválasztás: legalább `borrow_min_distance` (300 m) távolságra a gyaloglótól (hogy a közelben ne aludjon ki egy), ezen belül a legközelebbi a `borrow_max_distance`-en (900 m) belül; ha ott nincs, a legközelebbi távolabbi.
   A kölcsönzött lámpa eredeti mezőit (hely, négyzet-sorszám, forgatás, szín, hatótáv, szögek) elmentjük.
2. Ha nincs jelölt és `own_light = 1`: saját fény a `light_source` típusbejegyzésének létrehozó függvényén át megtalált konstruktorral, a saját memóriánkban, a scene fénylistájába felvéve.
3. Ha egyik sem sikerül: a lámpa nem kapcsol be, és a napló megmondja, miért.

Kikapcsoláskor a halványulás végén a kölcsönzött lámpa visszakapja az eredeti mezőit; a saját fény a játék destruktorával lebomlik (és kikerül a fénylistából).

### Kialvás

A lámpa kialszik, és a fényt visszaadjuk: visszaszálláskor, szünetben és a játék menüjében (a játék folytatásakor újra bekapcsol, ha előtte égett), a plugin kirakásakor.
Új világ betöltésekor a régi mutatók érvénytelenek: a kölcsönzött és a saját fényt "elfelejtjük" (nem írunk a régi címekre).

## Felépítés

### Mag (`src/core`, tesztelhető)

- `flashlight.h/.cpp`: a lámpa állapota (ki, erősödik, ég, halványul), a fokozatok, a halványítás, és képkockánként a fény kimenete (hely, irány, szögek, hatótáv, szín, "kell-e fény").
- `flicker.h/.cpp`: a villogás ütemezője befecskendezett véletlenszám-generátorral (a tesztekben rögzített maggal); kimenete a fényerő-szorzó.
- `light_borrow.h/.cpp`: a kölcsönözhető lámpa kiválasztása a jelöltek (hely, bekapcsolt-e) közül.
- `config`/`settings`: `[light]` rész és három új billentyű a `[keys]`-ben.

### Windows-réteg (`src/win`)

- `game_lights.h/.cpp`: a scene fénytömbjének elérése (a scene objektum az RVA `0x36AE6D8` globálisból), a `light_source` típus azonosítása a típuskeresővel, egy fény mezőinek írása és olvasása, a kölcsönzés mentéssel és visszaírással, a saját fény létrehozása és lebontása.
  Minden objektumon át hívás vagy írás előtt vtable-ellenőrzés (`checked_vtable`), minden játékbeli hívás kivételvédett (`guarded_call`).
- `input`: alacsony szintű egér-hook (`WH_MOUSE_LL`) a görgő lépéseihez; séta közben elnyeli a görgőt.
- `plugin`: séta közben képkockánként a bemenet a `flashlight` magba, a kimenet a `game_lights`-ba; a kialvás eseményei.

### A játék fényeinek tényei (a TM Real Walk alapján, a megvalósítás első feladatai igazolják)

- A `light_source` típus a játék típusrendszerében: a vtable RVA `0x229AC80` (a meglévő típuskeresővel megtalálva); a típusbejegyzés `+8` a létrehozó függvény (első `jmp`-je a konstruktor), `+0x30` az objektum mérete.
- A fény mezői: szín `+0x124..+0x138` (6 float), hatótáv `+0x168`, `+0x174`, `+0x178`, `+0x1A8`, `+0x1AC` (szögek és méret), a saját fénynél beállított `+8` jelzőbit (0x1000), `+0x198`, `+0x104`; a hely, a négyzet-sorszám és a forgatás eltolása az elemzésből.
- A scene fénytömbje a scene objektumban (eltolás az elemzésből).
- A játék fénytávolság-beállításai (konzolváltozók): ha 0-n állnak, a dinamikus fények nem látszanak; induláskor ellenőrizzük és naplózzuk. Hogy séta közben ideiglenesen átállítsuk-e (és visszaállítsuk, mint a HUD-tükröket), azt a felderítés után döntjük el.

## Hibakezelés

- Ha a kölcsönzött lámpa eltűnik a tömbből vagy a vtable-je megváltozik: elengedjük (nem írunk rá), és új fényt keresünk; közben egy rövid villogás.
- Ha 10 másodpercen belül háromszor elvész a fény: a lámpa kikapcsol, a napló jelzi (újra bekapcsolva újrapróbál).
- Ha a konstruktor, a destruktor vagy a fény frissítése hibát jelez: a saját fény a munkamenetre kikapcsol, a kölcsönzés marad.
- Ha a fénytömb vagy a `light_source` típus nem található: a lámpa nem érhető el, a séta zavartalan; a napló jelzi.

## Beállítások

`[keys]`:

| Kulcs | Alapérték | Jelentés |
|---|---|---|
| `flashlight` | F | a zseblámpa be/ki (NONE = kikapcsolva) |
| `flashlight_wider` | 0x6B (NUMPAD+) | a fénykúp szélesebb (NONE = csak a görgő) |
| `flashlight_narrower` | 0x6D (NUMPAD-) | a fénykúp szűkebb (NONE = csak a görgő) |

`[light]` (Light fül; a változás azonnal érvényes, égő lámpánál a következő képkockától):

| Kulcs | Alapérték | Tartomány | Jelentés |
|---|---|---|---|
| `angle` | 28 | 5-80 | a fénykúp külső szöge a középső fokozatban (fok) |
| `inner_angle` | 12 | 0-80 | a belső, éles rész szöge (fok) |
| `range` | 40 | 5-200 | hatótáv (m) |
| `brightness` | 0.3 | 0-3 | a szín szorzója |
| `r`, `g`, `b` | 1.0, 0.96, 0.88 | 0-3 | szín |
| `fade_time` | 0.18 | 0-2 | be- és kikapcsolási halványulás (s) |
| `hand_right` | 0.5 | -1-1 | a kéz a szemtől jobbra (m) |
| `hand_down` | 0.21 | -1-1 | a kéz a szem alatt (m) |
| `flicker_chance` | 0 | 0-60 | óránként átlagosan ennyiszer rakoncátlankodik |
| `own_light` | 1 | 0/1 | ha nincs kölcsönözhető lámpa, saját fény |
| `borrow_min_distance` | 300 | 0-2000 | ennél közelebbi utcai lámpát nem veszünk el (m) |
| `borrow_max_distance` | 900 | 100-5000 | ezen belül keresünk elsőként (m) |

## Naplózás

- Indításkor: a `light_source` típus, a fénytömb, a konstruktor; a fénytávolság-beállítások értéke.
- Bekapcsoláskor: a fény forrása (kölcsönzött lámpa távolsága és eredeti értékei, vagy saját fény), illetve hogy miért nem sikerült.
- Kikapcsoláskor: a visszaadás eredménye.
- `[debug] discovery_log = 1`: másodpercenként a fény forrása, címe, mezői és a fénytömb mérete; a felderítő teszthez a fénytömb aktív lámpáinak listája.

## Tesztelés

### Egységtesztek

- `flashlight`: be/ki él és halványítás (0 → 1 és 1 → 0 a `fade_time` alatt, "kell-e fény" csak a halványulás végén hamis); a fokozatok a billentyűkkel és a görgővel, a széleken nincs túllépés; a fény helye és iránya jellemző yaw/pitch értékekre; a szín szorzói.
- `flicker`: rögzített maggal reprodukálható; 0 gyakoriságnál nincs esemény; a szorzó 0 és 1 között, az esemény végén 1; hosszú szimulációban az események átlagos száma a beállított körül.
- `light_borrow`: a minimális távolságon belülit nem választja; a maximálison belül a legközelebbit; ha csak távolabbi van, a legközelebbi távolit; jelölt nélkül nincs választás; kikapcsolt lámpát nem választ.
- `settings`/`config`: az új kulcsok olvasása, mentése, határértékei, a billentyűütközés az új billentyűkkel, a csomagolt INI egyezik a formázóval.

### Elemzés a játékfájlon

A `light_source` típusbejegyzés, a konstruktor és a destruktor, a scene fénytömbjének helye, a fény mezőinek eltolásai (ahol lehet, a játék kódjából), a fénytávolság-beállítások; az igazolt tények az `ets2-1.61.md`-be és a `game_layout.h`-ba kerülnek.

### Élő tesztek (verzióléptetéssel, `TESZT.md`-vel)

1. Felderítés (`discovery_log`): éjjel a fénytömb aktív lámpái a mezőikkel; a hely egyezik egy látható utcai lámpával.
2. Kölcsönzés: éjjel be, a fénykúp a nézést követi, a méret billentyűvel és görgővel változik, a halványulás látszik; kikapcsolás után a távoli lámpa visszaáll.
3. Saját fény: lakatlan helyen éjjel, és nappal árnyékban.
4. Villogás: `flicker_chance` magasra állítva.
5. Hibatűrés: ki- és beszállás égő lámpával, szünet, mentés betöltése égő lámpával; újraindítás után a beállítások megmaradnak.
