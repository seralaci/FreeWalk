# FreeWalk - a gyaloglás magja (1. alprojekt)

Dátum: 2026-10-13

## Cél

A FreeWalk egy Euro Truck Simulator 2 plugin, amellyel a játékos kiszállhat a kamionból, és első személyű nézetben gyalogolhat körülötte.
A plugin az SCS nyilvános telemetria-SDK-jára épül, a gyalogláshoz pedig a játék saját fejlesztői szabad kameráját használja.
Ez a dokumentum az első alprojektet írja le: a gyaloglás magját, amire a későbbi funkciók (ütközés, tankolás, pótkocsi, pihenés, képek) épülnek.

## Kiinduló helyzet és döntések

- Célplatform: ETS2 1.61.x, Windows x64.
  Az ATS támogatása egy későbbi alprojekt.
- Fejlesztés macOS-en történik, keresztfordítással.
  A játékot a felhasználó futtatja a saját Windows gépén, és visszaküldi a `game.log.txt`-t.
- A gyaloglás valódi 3D séta, nem 2D overlay.
- A kamerakezelés módja: a játék beépített szabad kamerája (`debug_camera`) lesz az aktív kamera, a saját mozgását a sebesség nullázásával letiltjuk, a forgatást a játék kezeli, a pozíciót és a fejmozgást a plugin írja minden képkockában.
- A kereskedelmi TM Real Walk plugin visszafejtéséből csak a játékra vonatkozó tényeket (struktúra-eltolások, objektumnevek) és általános módszereket használjuk, kiindulási feltevésként.
  A FreeWalk kódja saját, a keresési logikát az `eurotrucks2.exe` saját elemzése alapján írjuk meg, és minden feltevést a felhasználó játékverzióján ellenőrzünk.
- Név: FreeWalk (`FreeWalk.dll`, `FreeWalk.ini`, napló előtag `[FreeWalk]`), így az eredeti plugin mellett is telepíthető.

## Funkciók az első verzióban

### Kiszállás

- Billentyű: `F10` (állítható).
- Előfeltétel: a kamion sebessége 0,5 km/h alatt van, és a kézifék be van húzva.
  Ha nem teljesül, a plugin egy naplósort ír, és nem történik semmi.
- A kezdőpozíció a vezetőoldali ajtó mellett van, a talajon, a kamion irányába nézve.
  Az ajtó helyét az SDK `truck` konfigurációjából számoljuk: a `cabin.position` és a `head.position` attribútum együtt megadja a vezető fejének helyét a kamion saját koordinátáiban.
  A vezető oldala a fej oldalirányú koordinátájának előjeléből adódik.
  A kezdőpont ettől a fejponttól oldalra, a fülkén kívül van, állítható távolságra (alapérték: 1,0 m a fejpont oldalirányú koordinátáján túl).
  Ha a konfiguráció még nem érkezett meg, a kiszállás nem indul, és erről naplósor készül.

### Séta

- `W A S D`: járás, alapérték 1,4 m/s.
- `Shift`: futás, alapérték 4,0 m/s.
- A mozgás iránya a kamera vízszintes nézési iránya (a dőlésszög nem számít).
- Az egérnézést a játék szabad kamerája kezeli.
- Szemmagasság: alapérték 1,7 m a talaj felett.
- Fejbiccentés: mozgás közben enyhe függőleges elmozdulás a megtett úttal arányos fázissal, alapérték 3 cm amplitúdó, kikapcsolható.
- Talaj: sík, a kiszálláskor mért magasságban.
  A kiinduló magasság a kamion telemetriás pozíciójának függőleges koordinátája, mínusz egy állítható eltolás (alapérték: 0 m, mert a kamion origója a kerekek talajpontjainak síkjában van; ezt az élő teszt igazolja vagy pontosítja).

### Visszaszállás

- Újra `F10`: a kamera visszaáll a kiszállás előtti kamerára.
- Automatikus visszaszállás, ha:
  - a kamion telemetriás pozíciója két képkocka között 5 m-nél többet ugrik (például a játék áthelyezi a kamiont),
  - a játékban más kamera lesz az aktív (a játékos kamerát vált),
  - a gyalogló vízszintes távolsága a kamiontól 150 m fölé nő.
- Leálláskor (`scs_telemetry_shutdown`) a plugin mindig visszaállítja az eredeti állapotot.

### Beállítások (`FreeWalk.ini`)

- `[keys]`: kiszállás billentyű, járás billentyűk, futás billentyű (Windows virtuális billentyűkód vagy név).
- `[walker]`: járási és futási sebesség, szemmagasság, fejbiccentés ki/be és amplitúdó, a kiszállási pont oldaltávolsága, talaj eltolás, automatikus visszaszállás távolsága.
- Hiányzó vagy érvénytelen érték esetén az alapérték érvényes, és erről egy naplósor készül.
- Hiányzó INI esetén minden alapértéken működik.

### Napló

- A plugin az SDK naplózó függvényén keresztül a `game.log.txt`-be ír, `[FreeWalk]` előtaggal.
- Naplózza: a betöltést és a verziót, a játék futtatható fájljának azonosítóit, minden megtalált objektum címét (a modulhoz képest), az ellenőrzések eredményét, minden állapotváltást és annak okát, és minden hibát.

### Szándékosan kimarad

Ütközés, domborzat, lépcső, ugrás, guggolás, képek és overlay, elhalványítás, a sofőr modelljének elrejtése, billentyűk elnyelése, a játék konzoljának figyelése, tankolás, pótkocsi, fuvar, pihenés, menü, ATS, VR, licenc, többnyelvűség.

## Felépítés

A kód két rétegből áll.
A játékfüggetlen mag nem függ a Windowstól, ezért macOS-en natívan fordul és tesztelhető.
A játékhoz kötött réteg csak Windowson fordul.

### Játékfüggetlen mag

- `coords`: átváltás a telemetria dupla pontosságú világkoordinátái és a kamera formátuma között.
  A kamera formátuma: X és Z tengelyen 512 m-es négyzet sorszáma (16 bites egész) és a négyzeten belüli helyi pozíció (lebegőpontos), Y tengelyen egyszerű lebegőpontos érték.
  Átváltás: `chunk = floor(x / 512)`, `helyi = x - chunk * 512`.
- `walker`: a gyalogló állapota (pozíció, megtett út a fejbiccentéshez) és egy lépés kiszámítása.
  Bemenet: lenyomott irány- és futásbillentyűk, eltelt idő, a kamera irányszöge, a talaj magassága, a beállítások.
  Kimenet: az új szempozíció világkoordinátában.
- `walk_session`: állapotgép a `vezet`, `kiszállás folyamatban`, `sétál`, `visszaszállás folyamatban` állapotokkal.
  Bemenet: telemetria-pillanatkép (pozíció, sebesség, kézifék, a vezető fejének helye a kamionban), a kiszállás billentyű lenyomásának éle, a játék aktív kamerájának sorszáma, az eltelt idő.
  Kimenet: a végrehajtandó parancs (kiszállás indítása, visszaszállás indítása, pozíció írása, semmi) és az ok szövege a naplóhoz.
- `config`: az INI beolvasása szövegből, alapértékekkel és tartományellenőrzéssel.

### Windows- és játékfüggő réteg

- `plugin`: az SDK belépési pontjai, a telemetriacsatornák és események regisztrálása, a naplózás, a rétegek összekötése.
  Használt csatornák: `truck.world.placement`, `truck.speed`, `truck.brake.parking`.
  Használt események: `frame_end` (és ha a teszt indokolja, `frame_start`), `paused`, `started`, `configuration` (a `truck` konfiguráció `cabin.position` és `head.position` attribútumaihoz).
- `game_memory`: védett olvasás (érvénytelen címnél hibát ad vissza, nem omlik össze) és írás (előtte a memórialap védelmének ellenőrzése, szükség esetén ideiglenes módosítása és visszaállítása).
- `game_locator`: a játék futtatható fájljának azonosítása (PE időbélyeg, képméret), és név alapján:
  - a kamerakezelő globális példánya (a `"camera_manager"` szövegtől indulva),
  - a szabad kamera objektuma a kamerák tömbjében, RTTI osztálynév (`debug_camera`) alapján,
  - a `g_developer` konzolváltozó,
  - a szabad kamera sebességét tároló konzolváltozó.
  A keresés pontos lépéseit az `eurotrucks2.exe` elemzése határozza meg.
- `game_camera`: a kamera átkapcsolása és visszakapcsolása, az irányszögek olvasása, a pozíció és a forgatás írása, a sebességváltozó mentése, nullázása és visszaállítása.
  Itt van az eltolások egyetlen táblázata.
- `input`: a billentyűk állapotának lekérdezése `GetAsyncKeyState`-tel, csak akkor, ha a játék ablaka van előtérben.

### Eltolások táblázata (kiinduló feltevések, ETS2 1.61.1)

| Objektum | Mező | Eltolás | Állapot |
|---|---|---|---|
| kamerakezelő | aktuális kamera sorszáma | `+0x10` | ellenőrizendő |
| kamerakezelő | kért kamera sorszáma | `+0x14` | ellenőrizendő |
| kamerakezelő | kamerák tömbje / darabszám | `+0x38` / `+0x40` | ellenőrizendő |
| szabad kamera | irányszög / dőlésszög / billenés | `+0x10` / `+0x14` / `+0x18` | ellenőrizendő |
| szabad kamera | helyi pozíció (3 float) | `+0x40` | ellenőrizendő |
| szabad kamera | négyzet sorszám X / Z (int16) | `+0x4C` / `+0x4E` | ellenőrizendő |
| szabad kamera | forgatás kvaternióként | `+0x50` | ellenőrizendő |

A táblázat minden sora mellé az implementáció során odakerül, hogy melyik játékverzión és hogyan ellenőriztük.
A plugin csak akkor használja a táblázatot, ha a futás közbeni ellenőrzések stimmelnek:
- a kamerák darabszáma 1 és 32 között van, és a szabad kamera megtalálható,
- a kamera-sorszámok a darabszámon belül vannak,
- szabadkamera-módban a beolvasott pozíció a kamion telemetriás pozíciójától 1000 m-en belül van.
Ha bármelyik nem teljesül, a plugin nem ír a játék memóriájába, a séta funkció kikapcsol, és a napló megmondja, melyik ellenőrzés bukott el.

## Adatáramlás

### Kiszállás

1. A `walk_session` a kiszállás billentyű élére ellenőrzi az előfeltételeket.
2. A `g_developer` értékét elmentjük, és ha 0, a memóriában 1-re állítjuk.
3. A szabad kamera sebességét elmentjük, és 0-ra állítjuk.
4. Az aktuális kamera sorszámát elmentjük, és a kért sorszámot a szabad kameráéra állítjuk.
5. Legfeljebb 3 másodpercig várunk, amíg az aktuális sorszám a szabad kameráé lesz.
   Ha nem lesz az, mindent visszaállítunk, és a napló a valószínű okot írja (például a `g_developer` nem érvényesült).
6. Kiszámoljuk a kezdőpozíciót, és beírjuk a pozíciót és a forgatást.

### Séta, képkockánként

1. Az SDK frissíti a telemetria-pillanatképet.
2. A `walk_session` eldönti, kell-e állapotot váltani.
3. Ha séta van: az `input` megadja a billentyűket, a `game_camera` az irányszögeket, a `walker` kiszámolja az új pozíciót, a `coords` átváltja, a `game_camera` beírja a pozíciót és az irányszögekből számolt forgatást.

### Visszaszállás

A kért kamera sorszámát visszaállítjuk az elmentettre, majd visszaállítjuk a sebességet és a `g_developer` eredeti értékét.

## Hibakezelés

- Ha a `game_locator` valamit nem talál, vagy egy ellenőrzés elbukik, a séta funkció kikapcsol, a napló megmondja az okot, a játék zavartalanul fut tovább.
- Minden memóriaolvasás védett; sikertelen olvasásnál a művelet elmarad.
- Ha séta közben egy írás vagy olvasás sikertelen, a plugin visszaszállást kezdeményez.
- Leálláskor minden módosítást visszaállítunk, akkor is, ha séta közben áll le a játék.
- Az első verzió nem módosítja a játék kódját (nincs hook), csak adatot ír.

## Fordítás

- CMake projekt, macOS-en MinGW-w64 keresztfordítóval (`x86_64-w64-mingw32`), Ninja generátorral.
- A kimenet egyetlen `FreeWalk.dll`, statikusan linkelt C++ futtatókönyvtárral.
- A projekt Windowson MSVC-vel is fordítható marad.
- Az SCS telemetria-SDK 1.14 nyilvános fejlécfájljai a repóban vannak, a licencszövegükkel együtt.
- A játékfüggetlen mag és a tesztjei macOS-en natívan, a rendszer clang fordítójával fordulnak.

## Tesztelés

### Automatikus tesztek (macOS, minden változtatás után)

Egy egyszerű tesztprogram, külső keretrendszer nélkül:
- `coords`: oda-vissza átváltás, négyzethatárokon és negatív koordinátákon is.
- `walker`: járási és futási sebesség, az irány a kamera irányszögéből, átlós mozgás nem gyorsabb, mint az egyenes, a fejbiccentés csak mozgás közben van.
- `walk_session`: nincs kiszállás mozgó kamionnál, kézifék nélkül vagy a kamion konfigurációja nélkül, a kezdőpont a vezető oldalán, a fülkén kívül van, a kiszállás 3 másodperc után visszavonódik, ha a kamera nem vált át, és visszaszállás következik a kamion elmozdulására, a kameraváltásra és a távolságra.
- `config`: alapértékek hiányzó és érvénytelen értékeknél.

### Élő teszt a felhasználó gépén

1. Felderítő verzió: csak olvas.
   Naplózza a futtatható fájl azonosítóit, a megtalált objektumokat és az eltolások ellenőrzését.
   A felhasználó kézzel bekapcsolja és mozgatja a szabad kamerát, a napló pedig mutatja, hogy a feltételezett mezők a pozíciót és az irányt tartalmazzák-e.
2. Kamera-átkapcsolás: az `F10` átvált a szabad kamerára és vissza, mozgás nélkül.
3. Teljes séta: járás, futás, fejbiccentés, automatikus visszaszállás.
   Itt dől el, hogy a pozíció írása a `frame_end` vagy a `frame_start` eseményben ad remegésmentes képet.

Minden lépés után a felhasználó visszaküldi a `game.log.txt`-t, és ha valami furcsa, képernyőképet vagy videót.

## Szükséges a felhasználótól

- Az `eurotrucks2.exe` másolata (`...\Euro Truck Simulator 2\bin\win_x64\eurotrucks2.exe`) a Ghidrával végzett elemzéshez.
- Élő tesztek a fenti sorrendben, a napló visszaküldésével.

## Későbbi alprojektek (nem része ennek a tervnek)

- Ütközés és domborzat a játék PhysX jelenetéből.
- Billentyűk elnyelése és a játék konzoljának figyelése.
- A sofőr modelljének elrejtése.
- Overlay saját képekkel (D3D11), elhalványítás kiszálláskor.
- Tankolás, pótkocsi, fuvar, pihenés.
- ATS támogatás.
