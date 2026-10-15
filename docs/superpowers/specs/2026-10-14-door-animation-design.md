# FreeWalk - élethűbb ki- és beszállás (5. alprojekt)

Dátum: 2026-10-14

## Cél

A ki- és beszállás ne egy ugrás legyen, hanem életszerű mozdulat: kiszálláskor egy pislogás alatt a kép az ajtó küszöbére kerül, onnan a gyalogló lépcsőfokonként lemegy a talajra; beszálláskor az ajtó mellől felmegy a lépcsőn, és egy pislogás után a fülkében van.
Ha az ajtó előtt nincs elég hely, a másik ajtón száll ki.

## Döntések (a felhasználóval egyeztetve)

- Elemek: kamera-útvonal és lépcső, pislogás az ajtónál, a szabad hely ellenőrzése. Az ajtócsapódás rázkódása, a hangok és a fülkén belüli fordulás nem része ennek az alprojektnek.
- Megközelítés: a magban egy tesztelhető animáció-ütemező, a Windows-rétegben egy sötétítő ablak a játék fölött; a kamerát a meglévő módon írjuk.
- A pislogás alatt (teljes sötétségben) történik a kameraváltás, így a kamera nem indul a sofőr modelljéből, és nem megy át a fülke falán.
- Visszaszállás: az ajtó közeléből (alapból 3 m) teljes animáció; messzebbről csak egy rövid pislogás.
- A TM Real Walk a tényforrás az ötlethez (`door_animation`, `door_step_time`, a szabad hely mérése); kódot nem veszünk át.

## Működés

### Az ajtó geometriája

- Az ajtó a vezető fejének oldalán van (a telemetria `head_position`-ja alapján, mint eddig), a másik ajtó a tükörképe a kamion hossztengelyére.
- A küszöbpont: a fej helyétől oldalra `door_out` (0,7 m, a fülkén kívül), a fej magassága alatt `door_sill_below_head`-del (1,0 m).
- A kilépés talajpontja a mostani `exit_point` (a fejtől oldalra `exit_side_distance`), az adott ajtó oldalán; a magassága a talaj (a fizikából, ha elérhető, különben a mostani becslés).
- A lépcsőfokok száma: `ceil((küszöb - talaj) / 0,45)`, legalább 1; ha a talaj nincs a küszöb alatt, 0 (csak kilépés).

### Kiszállás

1. Ajtóválasztás: a fizika kapszula-söprésével a vezetőoldali küszöb előtti szabad hely (a küszöbtől kifelé, 1,5 m-ig) mérése; ha kevesebb, mint `door_min_free` (0,8 m), az utasoldal mérése; ha az is kevesebb, a kiszállás elmarad (a napló mindkét értékkel). Fizika nélkül a vezetőoldal.
2. Sötétedés a fülkenézetben `door_fade_time` (0,25 s) alatt.
3. Teljes sötétségben: a szabad kamera a küszöbpontra kerül, kifelé nézve (az ajtó irányába), utána a játék átvált (a meglévő sorrend és beállítások szerint).
4. Amint a szabad kamera aktív: világosodás `door_fade_time` alatt. Ha a váltás 3 s alatt sem történik meg: világosodás, és a kiszállás elmarad.
5. Lépcsőzés: `door_step_time` (1,3 s) alatt a kamera a küszöbtől a talajpontig megy; a magasság fokonként lépcsős (minden fok idejének első felében ereszkedik, a másodikban áll), a vízszintes hely egyenletesen halad a küszöbtől a talajpontig; a végén az `exit_yaw` szerinti irány (az utolsó fok alatt fordul).
6. Utána indul a séta (a talajra igazítással).

### Beszállás

- Ha a gyalogló az ajtó küszöbpontjától vízszintesen `enter_near_door`-on (3 m) belül van: a kamera 0,4 s alatt a lépcső aljára áll az ajtó felé fordulva, `door_step_time` alatt fokonként felmegy a küszöbre, majd sötétedés, teljes sötétségben a régi kamera vissza, világosodás.
- Messzebbről: sötétedés, a régi kamera vissza, világosodás.
- Az automatikus visszaszállások (távolság, a kamion elindul, hiba) a messziről beszállás szerint mennek; a hibából fakadó visszaszállás azonnali (sötétítés nélkül).

### Az animáció alatt

- A séta billentyűi, a zseblámpa és a ki/beszállás billentyűje nem hat; a zseblámpa beszálláskor az elején kialszik.
- Szünetben az animáció megáll, és a játék folytatásakor folytatódik.
- `door_animation = 0`: a mostani azonnali ki- és beszállás.

## Felépítés

### Mag (`src/core`, tesztelhető)

- `door_path.h/.cpp`: az ajtó geometriája (küszöbpont, talajpont, lépcsőfokok, az ajtó felé néző irány) a kamion helyéből, irányából, a fej helyéből, az ajtó oldalából és a beállításokból.
- `door_animation.h/.cpp`: időben lépő ütemező a kiszállás, a közeli és a messzi beszállás sorozataira; képkockánként: kamera helye, yaw, pitch, sötétítés (0-1), "most lehet váltani", kész, meghiúsult. A hívó jelzi, ha a váltás megtörtént.
- `door_choice.h/.cpp`: a kiszálló ajtó a két oldal mért szabad helyéből.
- `walk_session`: `Exiting` és `Entering` szakasz; a parancsok kiegészülnek (a váltás kérése a sötétség pillanatában).
- `config`/`settings`: az új beállítások.

### Windows-réteg (`src/win`)

- `fade_window.h/.cpp`: fekete, réteges (`WS_EX_LAYERED`), kattintást átengedő (`WS_EX_TRANSPARENT`), fókuszt nem vevő (`WS_EX_NOACTIVATE`), legfelső, eszköztár-ablak (`WS_EX_TOOLWINDOW`) a játékablak ügyfélterületén, saját üzenetszálon; az átlátszóság képkockánként állítható, 0-nál rejtett. Teljes képernyős (exkluzív) módban nem látszik.
- `plugin`: a szabad hely mérése a fizika söprésével; az animáció kimenete a kamerába (hely, yaw, pitch) és a sötétítő ablakba; a váltások a sötétség pillanatában.

## Hibakezelés

- A kamera olvasásának vagy írásának hibájakor azonnali visszaszállás a régi kamerára, a sötétítő ablak eltűnik; a sötétítés soha nem maradhat bent (minden kilépési úton 0-ra áll).
- Ha a sötétítő ablak nem jön létre: a lépcsőzés megmarad, a pislogás elmarad (egyszer naplózva).
- A plugin kirakásakor a sötétítő ablak megszűnik, a szál leáll.

## Beállítások

`[walker]` (Leaving the truck fül):

| Kulcs | Alapérték | Tartomány | Jelentés |
|---|---|---|---|
| `door_animation` | 1 | 0/1 | 1 = animált ki- és beszállás, 0 = azonnali |
| `door_step_time` | 1.3 | 0.3-4 | a lépcsőzés ideje (s) |
| `door_fade_time` | 0.25 | 0-1 | a pislogás elsötétülése és kivilágosodása (s) |
| `door_out` | 0.7 | 0.3-1.5 | a küszöbpont a fejtől oldalra (m) |
| `door_sill_below_head` | 1.0 | 0.3-2 | a küszöb a fej magassága alatt (m) |
| `door_min_free` | 0.8 | 0-2 | ennyi szabad hely kell az ajtó előtt (m) |
| `enter_near_door` | 3 | 0-10 | ezen belül a teljes beszállás-animáció (m) |

## Naplózás

- Kiszálláskor: a választott ajtó, a két oldal szabad helye, a küszöb magassága és a lépcsőfokok száma; ha elmarad, miért.
- Beszálláskor: közeli vagy messzi.
- A sötétítő ablak létrehozásának hibája egyszer.

## Tesztelés

### Egységtesztek

- `door_path`: a küszöbpontok a kamion irányának megfelelő oldalon (több irányszögre); a küszöb magassága; a lépcsőfokok száma (1,2 m → 3, 0,3 m → 1, a talaj a küszöb fölött → 0).
- `door_animation`: a kiszállás sorrendje és időzítése (a sötétítés 0 → 1 a `door_fade_time` alatt, a "most lehet váltani" csak teljes sötétségben, a világosodás csak a váltás után); a lépcsőzés alatt a magasság monoton csökken és lépcsős, a vége pontosan a talajpont és az `exit_yaw`; a beszállás közelről fordítva, messziről csak sötétedés, váltás, világosodás; a váltás határidejének túllépése: világosodás és meghiúsult; 0 sötétedési idő azonnali; megszakításkor a sötétítés 0.
- `door_choice`: a vezetőoldal, ha elég a hely; az utasoldal, ha a vezetőoldal szorul; egyik sem, ha mindkettő szorul; mérés nélkül a vezetőoldal.
- `walk_session`: az új szakaszok alatt a billentyű nem indít új váltást; `door_animation = 0` mellett a mostani viselkedés (a meglévő tesztek zöldek).
- `settings`/`config`: az új kulcsok olvasása, mentése, határértékei; a csomagolt INI egyezik a formázóval.

### Élő tesztek (verzióléptetéssel, `TESZT.md`-vel)

1. Kiszállás parkolóban (ablakos vagy keret nélküli módban): sötétedés, a kép az ajtóban, lépcsőzés le, fordulás.
2. Kiszállás szorosan egy fal vagy kamion mellett: az utasoldalon; ha mindkét oldal szorul, nem száll ki (a napló okkal).
3. Visszaszállás az ajtó mellől (teljes animáció) és messziről (pislogás).
4. Hibatűrés: szünet az animáció közben; `door_animation = 0`; teljes képernyős módban a pislogás elmarad, a lépcsőzés megmarad.
