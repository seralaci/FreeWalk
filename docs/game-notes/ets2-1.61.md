# ETS2 1.61 - a játék belső felépítése

Játékverzió: 1.61.1.1s
eurotrucks2.exe SHA-256: 2014bfcc850a06108f4ca3c233a5bb6d31d7d83346c5d449f94c8e0675e5199a
PE időbélyeg: 0x6AB3C451, képméret: 0x398A000, előnyben részesített báziscím: 0x140000000
A címek alább RVA-k (a modul báziscíméhez képest), a Ghidra-címek a 0x140000000 báziscímmel értendők.

A fájlon Steam DRM-burkoló van (`.bind` szakasz, a belépési pont is ott van), de a `.text` nincs titkosítva: az entrópiája 6,42, és több tízezer szabványos függvénykezdetet tartalmaz.
Az elemzés ezért közvetlenül a fájlon elvégezhető volt.

## A játék típusrendszere

A játék saját osztályainak nincs MSVC RTTI-je (a fájlban csak a beépített könyvtárak `.?AV` nevei szerepelnek).
A játék saját típusrendszert használ:
- minden típusnak van egy `.data`-beli típusbejegyzése, amelynek első mezője a típus nevére mutat (például `"debug_camera"`);
- a `.rdata` egy cellája erre a bejegyzésre mutat;
- a típusnév-lekérdező függvény pontosan `lea rax, [cella]; ret` (`48 8D 05 <disp32> C3`);
- ez a függvény a típus vtable-jének 5. bejegyzése (`+0x28`).
A FreeWalk ezen a láncon jut el a nevéből egy típus vtable-jéig, és a kamerákat a vtable-jük alapján azonosítja.

## Kamerakezelő

- Típusnév: `camera_manager`, vtable: RVA `0x2254760` (típusnév-lekérdező: `FUN_1405014e0`).
- A gyártófüggvény (`FUN_140501080` és `FUN_140501310`) a létrehozott példányt a `0x36AE740` RVA-jú globális változóba menti, a vtable betöltése előtt néhány utasítással.
  A vtable betöltései körüli ±0x80 bájtban ez az egyetlen `.data`-beli tárolás.
- Mezők a gyártófüggvény szerint:
  - `+0x10`, `+0x14`, `+0x18`: int32, mindhárom kezdőértéke 14 (a kamerák száma, azaz "nincs kamera"); a `+0x10` az aktív, a `+0x14` a kért kamera sorszáma (az élő teszt erősíti meg);
  - `+0x30`: egy tömbobjektum, `+0x38` az adatmutatója, `+0x40` a 64 bites hossza (14-re állítva).

## Szabad kamera

- Típusnév: `debug_camera`, vtable: RVA `0x225C598`; a vtable 5. bejegyzése a típusnév-lekérdező (`FUN_14054e860`), a 30. bejegyzése (`FUN_1405522f0`) olvassa a `g_flyspeed`-et.
- Konstruktor: `FUN_14054e8e0`, amely először a kamerák közös ősosztályának konstruktorát hívja (`FUN_1407966a0`).
- Az ősosztály konstruktora szerint:
  - `+0x10`, `+0x14`, `+0x18`: három nullázott float (irány-, dőlés- és billenési szög);
  - `+0x1C = 1.0`, `+0x20 = 60.0` (látószög), `+0x24 = 0.2` és `+0x28 = 1600.0` (vágósíkok);
  - `+0x40..+0x4F`: 16 nullázott bájt: 3 float helyi pozíció és két int16 négyzet-sorszám;
  - `+0x50`: `1.0, 0, 0, 0`, egységkvaternió w, x, y, z sorrendben.

## Konzolváltozók

- Minden konzolváltozót egy statikus inicializáló hoz létre, amely a nevet bemásolja az objektumba: `lea rax, [objektum + 8]; mov edx, 0x20; lea r8, [név szövege]`, majd egy bájtonkénti másoló ciklus (`g_developer`: `FUN_140032a90`, `g_flyspeed`: `FUN_140031e30`).
- Az objektum felépítése (alaposztály vtable: RVA `0x2222800`, mérete 0x140):
  - `+0x00`: vtable;
  - `+0x08`: a név, legfeljebb 0x20 bájt;
  - `+0x28`: a leírás, legfeljebb 0x65 bájt;
  - `+0x90`: a típus (a `g_developer`-nél 1, a `g_flyspeed`-nél 2);
  - `+0x98`: opcionális ellenőrző függvény;
  - `+0xA1`: írásvédettség jelzője;
  - `+0xB1`: az aktuális érték szövegként, legfeljebb 0x65 bájt;
  - `+0x116`: a számgyorsítótár érvényessége, `+0x118`: a gyorsítótárazott szám;
  - `+0x120`: egy másik változóra mutató lánc (felülírás), ha nem nulla.
- A vtable 1. bejegyzése a beállító: `bool set(this, const char* value)` (`FUN_14031d7a0`).
  Követi a `+0x120` láncot, megtagadja az írásvédett változót, meghívja az ellenőrzőt, a szöveget a `+0xB1`-re másolja, és érvényteleníti a gyorsítótárat; sikerre igazat ad.
- Az egész típusú leszármazott (vtable: RVA `0x22227E0`, mérete 0x148) beállítója az alap beállítás után a `+0x140`-re az egész értéket is beírja.
  A `g_developer` ebbe az osztályba tartozik (`+0x3644F70`), a `g_flyspeed` az alaposztályba (`+0x2D41940`).
- A FreeWalk mindkét változót a szöveges értékén át kezeli: kiolvassa a `+0xB1`-ről, a beállítóval írja, és ugyanazzal a szöveggel állítja vissza.

## Eltérések a kiinduló feltevésektől

- A kamerakezelő nem egy név-létrehozó regisztrációs bejegyzésen át található meg, hanem a típusrendszeren át (vtable), és a gyártófüggvény globális tárolásán át.
- A kamerák nem MSVC RTTI-vel, hanem a vtable-jükkel azonosíthatók.
- A kamerák száma 64 bites, nem 32 bites.
- A konzolváltozó a nevét bemásolva tárolja, nem mutatóként, és a regisztráló kód nem `lea rcx`-et, hanem `lea rax`-ot használ.
- A konzolváltozók értékét a FreeWalk szövegként kezeli; típusonkénti értékmezők nem kellenek.
- A szabad kamera sebessége a `g_flyspeed` konzolváltozó.

## Élő teszt eredményei (0.1.1, 2026-10-13)

- A futó játékban mind a négy keresés egyértelmű, összesen 500-530 ms alatt fut le a plugin betöltésekor.
- A `g_developer` értéke `1` (a `config.cfg`-ből), a `g_flyspeed` értéke `100.0`.
- 14 kameraslot van, a szabad kamera a 0., a fülkekamera a 2. (betöltés közben a 7. az aktív).
- A `g_developer 1` és `g_console 0` beállítással a játék saját szabadkamera-gombja nem kapcsolt át.
- A kért mező (`+0x14`) írására a játék a következő képkockákban átvált (`+0x10` a kért értékre változik), majd a kért mezőt visszaállítja 14-re; visszafelé ugyanígy.
  Ezzel igazolt, hogy `+0x10` az aktív, `+0x14` a kért kamera sorszáma.
- Így bekapcsolva a szabad kamera a világ origójában marad (pozíció `0, 0, 0`, a kép fekete), a szögei 0-k, és a `W` a kamionhoz jut (a fordulatszámmérő mozgott).
  A játék tehát ilyenkor nem mozgatja és nem helyezi át a szabad kamerát; a pozíciót a pluginnak kell beírnia.

## Élő teszt eredményei (0.2.0, 2026-10-13)

- A pozíció a `+0x40`-en van: a beírt szempozíció (talaj 1,45 m + 1,7 m = 3,15 m) a játékban a vezetőoldali ajtó mellett, szemmagasságban jelent meg.
- Az irányszög (`+0x10`) radiánban van, ugyanazzal a konvencióval, mint a telemetria headingje: a kezdő irány pontosan a kamion orra felé nézett.
- A dőlésszög (`+0x14`) radiánban, 0..2π tartományban van (lefelé nézve 6,27 körüli érték).
- A játék a szögekből maga számolja a forgatást: `yaw 3,9172` mellett a `+0x50` kvaternió `w -0,378, y 0,926` (cos és sin(θ/2) a függőleges tengely körül); a kvaterniót nem kell írni.
- Az egérrel a játék forgatja a szabad kamerát, a pozíciót nem írja felül.
- A `g_flyspeed` kiszálláskor `0` lett, visszaszálláskor visszaállt `100.0`-ra; a fülkekamera (2.) visszaállt.
- A `W` a kamionhoz jutott (a játék a behúzott kéziféket jelezte): a billentyűket el kell nyelni.
- A két felső sarokban egy-egy fehér téglalap jelent meg ("far" felirattal), képernyőhöz rögzítve, a HUD-tükrök helyén; jelölt a `g_mirrors_keep_hidden` konzolváltozó.
- A játék kilépésekor a kamerakezelő már nem fogadja a visszakérést ("the previous camera could not be requested back"); ennek nincs hatása.

## Élő teszt eredményei (0.3.0, 2026-10-14)

- A `g_mirrors_keep_hidden = 1` eltünteti a két fehér téglalapot: azok a szabad kamerában érvénytelen képpel megjelenő HUD-tükrök voltak.
- A séta iránya helyes (W előre, A balra, D jobbra, Shift futás).
- Az alacsony szintű billentyűzet-hook nem elég: a játék a billentyűzetet DirectInput-on át olvassa (`[di8] Initializing device '@@keyboard@@'`), így a W felpörgette a motort, az A/D tekerte a kormányt.
- A szabad kamera egérforgatása (`+0x1C` szorzó, alapérték 1,0) a fülkekameráénál jóval érzékenyebb.

## Amit csak az élő teszt dönthet el

- Hogy a DirectInput-szűrő elrejti-e a séta billentyűit a kamion elől, és hogy a `+0x1C` tényleg az egérforgatás szorzója-e.
- Hogy a pozíció írása a `frame_end` vagy a `frame_start` eseményben ad remegésmentes képet.
- Hogy a kezdeti keresés mennyi ideig tart a játék indulásakor.

## Élő teszt eredményei (0.3.1 és 0.3.2, 2026-10-14)

- A 0.3.1 DirectInput-szűrője nem hatott: a W felpörgette a motort, az A/D tekerte a kormányt.
- A 0.3.2 naplója szerint a `DirectInput8Create`-tel létrehozott ideiglenes billentyűzet-eszköz vtable-je nem a `dinput8.dll`-ben, hanem a Steam overlay `gameoverlayrenderer64.dll`-jében van (burkoló objektum), az olvasó függvényeit pedig a játék mappájában lévő ReShade 6.8 (`dxgi.dll`) irányítja át; a W és az A változat ugyanazt a burkoló vtable-t kapja.
- Séta közben a játék egyszer sem olvasott ezen a burkoló vtable-en át (0 hívás kétmásodpercenként), tehát a játék billentyűzet-eszköze más vtable-t használ.
- A ReShade a betöltés után egyszer felülírta a burkoló vtable javított bejegyzéseit.
- Ezért a szűrő a `dinput8.dll` saját objektumát a COM osztálygyárán át (`DllGetClassObject`) hozza létre, és csak a `dinput8.dll`-ben lévő vtable-t javítja; a burkolók végül ezen át olvasnak (élő teszt igazolja).

## Élő teszt eredményei (0.3.3, 2026-10-14)

- A `dinput8.dll` saját vtable-jén át a játék olvas (kb. 120 `GetDeviceData` hívás másodpercenként), de egyik sem billentyűzet-eszközön, és `GetDeviceState` nincs; a billentyűzet útja ezzel nem derült ki.
- A játék saját szabad kamerájában (`g_developer 1`, `g_console 1`, az amerikai kiosztás 0 gombja, magyar billentyűzeten az Ö) is a kamionhoz jut a W (motor) és az A/D (kormány): a játék maga sem tereli el a billentyűket a kamiontól.
- Megoldás a felhasználónál: a játék vezérlőkiosztásában a gáz, fék, kormányzás másodlagos W/S/A/D hozzárendelése törölve (az elsődleges a nyilakon marad); így a séta billentyűi a kamionhoz nem jutnak el.
- Átkapcsoláskor a játék a szabad kamera akkori helyén tölti be a térképet ("Forcing sector data!" ... "Sector force finished."): az első kiszálláskor ez az érintetlen origó volt, és 2,2 s-ig tartott; a későbbi kiszállások 20 ms körüliek.

## PhysX-jelenet

A játék statikusan linkelt PhysX 3.4-et tartalmaz (a forrásútvonalak között `...\Source\PhysX\src\NpScene.cpp`).
Elemzés: `freewalk_probe`, `dumpbin /disasm /range` és rövid Python-szkriptek a játékfájlon.

- A `PxScene::release(): Scene is still being simulated! PxScene::fetchResults() is called implicitly.` és a `PxScene::shiftOrigin() not allowed while simulation is running. Call will be ignored.` szöveg a `.rdata`-ban van, és mindkettőre pontosan egy függvény hivatkozik `lea`-val (a probe szerint).
- A két függvény egy vtable-ben 0x73 slot távolságra áll; a vtable a release előtti slottal kezdődik: a jelenet konstruktora (RVA `0x1ADE073` és `0x1ADE797`) a `0x2500CD8` RVA-t írja az objektum első qwordjába (`lea rax, [vtable]; mov [rdi], rax`). A 0. slot tehát a destruktor, a release az 1., a shiftOrigin a 0x74.
  A jelenet vtable-je: RVA `0x2500CD8`.
- RTTI ehhez nem használható: az `NpScene` típusleírója megvan (`.?AVNpScene@physx@@`), de Complete Object Locator nem hivatkozik rá, és a vtable-ök szorosan egymás után állnak (a vtable előtti qwordok is kódcímek).
- A slotok (a vtable elejétől):
  - `0x90/8` (`getNbActors`): a `word ptr [rdx]` flagekből az 1-es (statikus) és a 2-es (dinamikus) bitet vizsgálja, és a jelenet aktorlistáját számolja; a flag mutatóként érkezik.
  - `0x98/8` (`getActors`): flagek `rdx`-ben (mutató), puffer `r8`, méret `r9d`, kezdőindex a veremről.
  - `0x2F0/8` (`raycast`): a távolság az `xmm3`-ban, az origót (`rdx`) és az irányt (`r8`) előtölti, `PX_SIMD_GUARD` (MXCSR `0x9FC0`).
  - `0x2F8/8` (`sweep`): a 7. argumentumot (a hit flagek mutatója) olvassa, és az `ePRECISE_SWEEP` (0x100) és `eMTD` (0x200) együttes használatát vizsgálja.
- A `PxHitBuffer<T>` virtuális függvényeinek sorrendje (a TM Real Walk DLL RTTI-je szerint, amely a PhysX 3.4 fejléceivel fordult): 0. `processTouches` (`xor al, al; ret`), 1. `finalizeQuery` (üres), 2. törlő destruktor; a `PxHitCallback` 0. slotja `_purecall`.
- A dinamikus aktor `getGlobalPose`-a (`0xA0/8`) statikusan nincs ellenőrizve (a `NpRigidDynamic` vtable-je RTTI nélkül nem azonosítható egyszerűen): élő teszt.
- Szálkezelés: a lekérdezéseket a telemetria `frame_end` eseményéből hívjuk, mint a TM Real Walk; a PhysX 3.4-ben a jelenet-lekérdezés a szimuláció alatt is a legutóbbi `fetchResults` állapotát látja. Élő teszt igazolja a stabilitást.

## Régió-lekérdezés (az ütközési geometria betöltése)

- Élő teszt (0.5.2-0.5.3): a játék az épületek, kerítések, oszlopok ütközését csak a járművek közelében tölti be a PhysX-jelenetbe. Mielőtt a kamion odament, a jelenetben 15 statikus test volt, és a gyalogló közeli oszlop, kerítés, fal hiányzott belőle; miután a kamion nekiment, 27, majd 34, és a söprések megfogták a gyaloglót.
- A térképelem-lista: a scene objektum mutatója az RVA `0x36AE6D8` globálisban; az objektum `+0x18` az elemmutatók tömbje, `+0x20` a darabszám (600 451 elem a teszten). Elem: `+0` vtable, `+0x0C` a befoglaló doboz minimuma, `+0x20` a maximuma (világkoordináták, float). A telephelyek oszlopai, kerítései, falai nem önálló elemek (a prefab dobozán belül vannak).
- `box_builder` (RVA `0x46D4C0`) és `region_query` (RVA `0x489E10`): a TM Real Walk bájtmintái a játékfájlon egyetlen találattal, mindkettő a `.pdata` szerint függvénykezdet. A `gate_model_item` (RVA `0x6E31A0`) `+0x8D` utasítása `mov r8, [0x1436AE6D8]`, ez adja a scene globálist (ugyanaz, amelyet a térképelem-lista felderítése talált).
- `box_builder(void*, box* out, const position*, const float size[3])`: az első argumentumot nem használja; a pozíció három float (a négyzeten belül) és két int16 négyzet-sorszám (`+0x0C`, `+0x0E`, szorozva 512-vel); a méretből `clamp((max(x, z) + 1) * 0,5, 0,01, 200)` félméretet számol; az `out`-ba 0x28 bájtot ír, és azt adja vissza.
- `region_query(scene*, const box*, bool)`: `rcx` a scene objektum (a globális tartalma), `rdx` a doboz, `r8b` egy jelző (a TM 0-t ad).

## Elemkapuk (parkoló járművek, kellékek)

- A `gate_model_item` (RVA `0x6E31A0`) és a `gate_area_item` (RVA `0x7F8E30`) a scene vtable `0x178`-as slotját hívja (`call qword ptr [rax+0x178]`, RVA `0x6E32D7` és `0x7F8F64`; a visszatérési címek `0x6E32DD` és `0x7F8F6A`). A slot eredetije a `0x48B970` függvény: `{bool érvényes; pozíció a +4-en}` szerkezetet ad vissza (a `+0x31B0` objektum helye, vagyis a játékos járműve).
- A kapu a fókusz és az elem távolságnégyzetét az RVA `0x251D25C` küszöbbel (1225 = 35^2) veti össze; azon belül hozza létre az elem ütközését. Ezért van a parkoló járműveknek és a kellékeknek csak a kamion 35 m-es körzetében ütközésük.
- Élő teszt (0.5.4): egy telephelyen parkoló kamionon a kamiontól 53 m-re át lehetett menni; a fizikában nem volt benne.
- A slot átírását (a TM `focus_routine` megoldása) a FreeWalk nem alkalmazza: a játék vtable-jének módosításával járna (2026-10-14, a felhasználó döntése).
- Élő teszt (0.8.0, 2026-10-15): a slot átírása működött (a kapuk a gyalogló helyét kapták volna), de a parkolóban álló autókon továbbra is át lehetett menni; a változat kikerült.

## Parkoló autók (felderítés, 2026-10-15)

- A térképelem-lista: a scene objektum `+0x18` a mutatótömb, `+0x20` a darabszám (egy białystoki mentésben 585 502 elem); az elem befoglaló doboza világ-floatként: minimum `+0xC/+0x10/+0x14`, maximum `+0x20/+0x24/+0x28`.
- A leggyakoribb közeli típus vtable-je `+0x22B4870` (valószínűleg a `model`); egy ferdén parkoló autó-modell doboza 4,79 x 1,73 x 4,28 m volt, és `+0x70`-en egy szögnek látszó float (5,70 rad) van. A típusnév-lekérdező ezeknél nem a szokásos "lea rax, [cella]; ret" (a név nem jött ki).
- A kamion melletti parkolóhelyeken álló forgalmi autók (prefab parkolóhelyek) sem térképelemként, sem a PhysX-jelenetben (dinamikus vagy statikus testként) nem szerepelnek: nincs ütközésük, a gyalogló átmegy rajtuk. A messzi, mozgó forgalmi autók dinamikus testek (1 alakzat, autóméret), ezekkel a söprés ütközik.
- A TM Real Walk sem olvassa a forgalom listáját. A parkoló forgalmi autók ütközéséhez a játék forgalmi járműlistáját kellene megtalálni (hely, irány, méret) és saját ütközést számolni; félretéve.

## Fények

- A `light_source` típus vtable-je RVA `0x229AC80` (a típuskeresővel). Az osztályleíró a `.rdata` RVA `0x1E17F90`-on: első qwordja a típus nevét tartó `.data`-bejegyzésre (RVA `0x2D726B0`) mutat, `+8` a létrehozó függvény (RVA `0x6EAF80`: a játék foglalójával foglal, majd a konstruktorra ugrik), `+0x30` az objektum mérete (`0x230`). A konstruktor (RVA `0x6EB290`) `rcx`-ben kapja az objektumot (a második argumentumot nem használja), az ős konstruktora után beírja a `light_source` vtable-t, és nullázza a színt (`+0x120..+0x138`), a hatótávot (`+0x168`) és a hely-forgatás blokkot (`+0x1B0`-tól).
- A scene fénytömbje (élő teszt, 0.6.1): a scene objektum `+0x3278` a fénymutatók tömbje, `+0x3280` a darabszám; betöltés után telik meg (egy telephelyen 118 elem, ebből 96 `light_source`).
- Egy utcai lámpa mezői (élő teszt): hely-blokk `+0x1B0` (x, y, z a négyzeten belül, két int16 négyzet-sorszám), forgatás `+0x1C0` (w, x, y, z); szín `+0x124` (kétszer RGB, kb. 1,2/1,1/0,8; kikapcsolt lámpánál 0); `+0x108` 1,0; hatótáv `+0x168` kb. 30 m; belső szög `+0x174` 1°, külső szög `+0x178` 90° (másik fajta: 5°/130°); `+0x1A8`/`+0x1AC` kb. 11,2/11,6. A lefelé világító lámpa forgatása -90° az X tengely körül: a fény a saját -Z tengelyén világít, mint a kamera.
- A `g_light_distance_factor` és a `g_light_span_factor` értéke a teszten 1 (a dinamikus fények látszanak).
