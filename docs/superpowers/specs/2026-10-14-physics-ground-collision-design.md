# FreeWalk - talaj és ütközés a játék PhysX-jelenetéből (3. alprojekt, 1. szakasz)

Dátum: 2026-10-14

## Cél

Séta közben a gyalogló kövesse a domborzatot (lejtő, járdaszegély, lépcső), és ne menjen át falakon, kerítéseken, a kamionon és a pótkocsin, hanem csússzon el mellettük.
Ez az első szakasz a legkisebb működő változat: a játék PhysX-jelenetének megkeresése, a jelenet origójának eltolása, a talaj lefelé mutató sugárral és a vízszintes ütközés kapszula-söpréssel.

## Döntések (a felhasználóval egyeztetve)

- A játék statikusan linkelt PhysX 3.4-ének élő jelenetét kérdezzük le (raycast, sweep), a jelenet vtable-jén keresztül; saját aktort nem adunk hozzá, a játék kódját nem módosítjuk.
- A mechanizmus forrása a TM Real Walk visszafejtett kódja (`docs/game-notes/physx-tmrealwalk.md`); kódot nem veszünk át, minden játéktényt a saját elemzésünkkel és élő teszttel igazolunk.
- Ha a talajsugár nem talál semmit, a láb az utolsó ismert talajmagasságon marad, ütközés nélkül.
- Kiegészítés az élő tesztek után (2026-10-14): a játék a térkép ütközési geometriáját (épületek, kerítések, oszlopok) csak a járművek közelében tölti be a fizikába; séta közben a játék saját régió-lekérdező függvényét hívjuk a gyalogló köré (lásd "A környék betöltése"). Ez a játék függvényének hívása, a kódját nem módosítja (mint a konzolváltozók beállítása).
- A parkoló járművek, pótkocsik és kellékek ütközését a játék csak a játékos kamionjától 35 m-en belül hozza létre; ennek a gyalogló köré terjesztése a játék vtable-jének átírásával járna, ezért ebben a szakaszban nincs benne (ismert korlát, a felhasználó döntése).
- A séta sosem szakad meg a fizika miatt: ha a jelenet nem érhető el vagy hibázik, a gyalogló sík talajon jár, mint eddig.
- Nem része ennek a szakasznak: ugrás, mászás, a mozgó járművek saját dobozütközése, a talaj becslése térképelemekből (`far_ground`), a parkoló járművek és kellékek ütközése a gyalogló körül (a TM `focus_routine` vtable-átírása).

## Működés

### Talaj

- Minden képkockában egy lefelé mutató sugár a `láb + step_height` pontból, 10 m hosszan, statikus és dinamikus testekre; a találat akkor talaj, ha a felület normálisának függőleges összetevője legalább 0,3 (abszolút értékben).
- A `step_height`-nél (0,45 m) alacsonyabb lépcsőfokra a gyalogló fellép, mert a sugár a fok tetejét találja.
- Talajon: ha a láb több mint `step_down_max`-szal (0,6 m) a talaj fölött van, esés indul; különben a láb simítva követi a talajt: `láb += (1 - exp(-dt / ground_follow_time)) * (talaj - láb)`, ha az eltérés 2 cm-nél nagyobb.
- Esés közben: `vy -= 9,81 * dt`, `láb += vy * dt`; leérkezés, amikor a láb eléri a talajt és `vy <= 0`. Esés közben a vízszintes mozgás és az ütközés változatlan.
- Ha nincs találat, a láb az utolsó ismert magasságon marad (esés közben is: az esés ott ér véget).

### Vízszintes ütközés

- A test egy függőleges kapszula: sugara `radius` (0,40 m), alja `láb + step_height`, teteje `láb + szemmagasság + 0,08`.
  A félmagasság `max(0,02; (tető - alj) / 2 - sugár)`.
- A kívánt vízszintes elmozdulást legfeljebb 3 söpréssel tesszük meg, 0,02 m ráhagyással: találatnál `távolság - 0,02`-t haladunk, majd a hátralevő elmozdulást a fal vízszintes normálisára merőleges irányba vetítjük (csúszás a fal mentén).
- Ha a kapszula már a kiindulóponton átfed valamivel (a söprés 0 távolságot és behatolási mélységet ad), a vízszintes normális mentén kitoljuk, legfeljebb `1,5 m/s * dt` és legfeljebb a mélység + 5 mm mértékben; függőleges normálisú (0,3 alatti vízszintes összetevőjű) átfedést figyelmen kívül hagyunk.

### A jelenet megkeresése

1. A játékfájl `.rdata`-jában a `PxScene::release(): Scene is still being simulated` és a `PxScene::shiftOrigin() not allowed` szöveg helye.
2. A `.text`-ben az ezekre mutató RIP-relatív `LEA` utasítások; a befoglaló függvény elejét a `.pdata` függvénytáblája adja: ez a `release` és a `shiftOrigin`.
3. A `.rdata`-ban az a vtable, ahol `slot[i] == release` és `slot[i + 0x73] == shiftOrigin`; pontosan egy találat kell.
4. Háttérszálon, alacsony prioritással: a folyamat `MEM_COMMIT`, `MEM_PRIVATE`, `PAGE_READWRITE` területein azok a 8 bájtra igazított qwordok, amelyek a vtable címét tartalmazzák (legfeljebb 16 jelölt).
   Ha nincs jelölt, 8 s-onként újra, legfeljebb 8-szor.
5. A játék szálán, a `frame_end`-ben jelöltenként egy próba-raycast; az első hibátlan jelölt a jelenet.

A vtable a betöltéskor, a többi kereséssel együtt található meg; a példány keresése akkor indul, amikor a kamerakezelő megjelenik (`camera_ready` hamisról igazra vált), és minden ilyen váltáskor (új világ) újraindul.

### A környék betöltése (régió-lekérdezés)

- Séta közben a játék `box_builder` (RVA `0x46D4C0`) függvényével dobozt építünk a gyalogló köré, és a `region_query` (RVA `0x489E10`) függvénnyel lekérdezzük a scene objektumon (a mutatója az RVA `0x36AE6D8` globálisban); a címek az ellenőrzött buildre érvényesek, betöltéskor a `.pdata` szerint függvénykezdetnek kell lenniük.
- Ütemezés (`FocusSchedule`, a magban, tesztelve): a séta elején 12 lekérdezés 0,25 s-onként, a doboz 30 m-ről 150 m-re nő; utána másodpercenként 150 m.
- A hívás kivételvédett; hibánál vagy 3 egymás utáni, 250 ms-nál hosszabb lekérdezés után kikapcsol (napló). Élő teszt: 0,1-1,4 ms, a statikus testek száma 3 s alatt 15-ről 564-re nőtt, a kerítések, falak, oszlopok a kamiontól távol is megállítják a gyaloglót.

### Koordináták: az origó eltolása

A jelenet origója X-ben és Z-ben 512 m többszörösével el van tolva a világhoz képest, Y-ban nincs; jelenet-koordináta = világ - eltolás.
Az eltolást séta közben 250 ms-onként frissítjük a kamion telemetriás pozíciójából:
- ha van megjegyzett kamionaktor, újraolvassuk a helyét;
- különben, vagy ha ez hibázik, vagy a maradék 6 m fölé nő: a dinamikus aktorok közül azok, amelyek helyének Y-ja 30 m-en belül van a kamionétól; mindegyikre `d = kamion - aktor`, `eltolás = round(d / 512) * 512` tengelyenként, `maradék = |dx - eltolás_x| + |dz - eltolás_z|`; a legkisebb maradékú aktor, ha a maradék 6 m alatti.

Az eltolás változását a napló jelzi. Érvényes eltolás nélkül a gyalogló sík világot kap.

## Felépítés

### Mag (`src/core`, tesztelhető)

- `world.h`: a `World` felület, amelyen át a gyalogló a környezetet kérdezi:
  - `std::optional<GroundHit> ground_below(Vec3 from, double max_distance)`: a talajpont magassága és normálisa;
  - `std::optional<SweepHit> sweep_capsule(const Capsule& capsule, Vec3 from, Vec3 direction, double distance)`: a távolság, a normális, és hogy a kiindulóponton már átfed-e (a behatolási mélységgel).
  - `FlatWorld`: sík talaj egy adott magasságban, ütközés nélkül (a mostani viselkedés és a visszaesés).
- `motion.h/.cpp`: a vízszintes csúszás és kitolás, valamint a függőleges talajkövetés, esés és leérkezés, a `World` felületen át.
- `walker.h/.cpp`: a `step` egy `World&` paramétert kap; a billentyűkből számolt kívánt elmozdulás a `motion`-on át valósul meg; a fejbiccentés a ténylegesen megtett úttal arányos.
- `scene_offset.h/.cpp`: az eltolás becslése a kamion helyéből és az aktorok helyeiből (tiszta függvény).
- `locator`: `find_physx_scene_vtable(memory, image, log)`, a fenti 1-3. lépés; a `PeImage` a `.pdata` olvasásával bővül.
- `game_layout.h`: a PhysX vtable-eltolásai, a 0x73-as slottávolság, az 512 m és a két szöveg, mindegyiknél az ellenőrzés helye.
- `config`/`settings`: új `[collision]` rész.

### Windows-réteg (`src/win`)

- `physx_types.h`: a hívásokhoz szükséges minimális PhysX 3.4 szerkezetek saját írással, a nyilvános fejlécek elrendezése szerint (`PxVec3`, `PxQuat`, `PxTransform`, kapszula-geometria, raycast- és sweep-találat, a virtuális függvényt tartalmazó találati puffer, a lekérdezési szűrőadat és flagek), méret- és eltolás-ellenőrzéssel (`static_assert`).
- `physx_scene.h/.cpp`: a példány keresése, az állapotgép (`Searching`, `Ready`, `Disabled`), az eltolás frissítése, és a `World` valódi megvalósítása: koordináta-átváltás, SEH-védett hívások, hibaszámláló.
- `plugin.cpp`: séta közben a gyalogló a PhysX-világot kapja, ha kész és van érvényes eltolás, különben egy `FlatWorld`-öt az utolsó talajmagassággal.

### Eszköz

A `probe` egy ötödik kereséssel bővül: a PhysX-jelenet vtable-je a játékfájlon.

## Szálak és időzítés

- Minden PhysX-hívás a játék szálán, a `frame_end`-ben fut, csak séta közben (képkockánként legfeljebb 4-5 hívás), illetve a próba-raycastok a keresés végén.
- A háttérszál csak memóriát olvas, a jelenetet nem hívja; a jelölteket zárral védett listában adja át.
- A megvalósítás első feladata a játékfájl elemzése: a telemetria `frame_start` és `frame_end` eseménye a jelenet `simulate` és `fetchResults` hívásán kívül fut-e.
  Ha nem, a lekérdezések oda kerülnek, ahol a szimuláció már befejeződött; a döntés `Ruling:` sorként kerül a végrehajtási naplóba.

## Hibakezelés

- Minden PhysX-hívás SEH-védett; hibánál a napló a művelet nevét, a kivétel kódját és címét (`exe+0x...`) írja, az eredmény pedig "nincs találat".
- 8 hiba után a jelenet `Disabled` a következő világ betöltéséig, és ezt a napló egyszer jelzi.
- Jelenet vagy érvényes eltolás nélkül a gyalogló `FlatWorld`-öt kap az utolsó talajmagassággal; a séta nem szakad meg.
- Ha a vtable nem található (más játékverzió), a napló jelzi, és a fizika ebben a munkamenetben ki van kapcsolva.
- `[collision] enabled = 0`: nincs keresés, sík talaj, mint eddig.

## Beállítások

Új `[collision]` rész az INI-ben és a beállítóablak új Collision fülén:

| Kulcs | Alapérték | Jelentés |
|---|---|---|
| `enabled` | 1 | talaj és ütközés a játék fizikájából (a változás újraindítás után érvényes) |
| `radius` | 0,40 | a test sugara (m) |
| `step_height` | 0,45 | ennél alacsonyabb fokra a gyalogló fellép (m) |
| `step_down_max` | 0,6 | ennél nagyobb lépés lefelé esés (m) |
| `ground_follow_time` | 0,12 | a talajkövetés simítása (s) |

## Naplózás

- A jelenet keresése: a vtable címe, a jelöltek száma, a keresés ideje, a használt jelenet; a hiba okai.
- Az eltolás minden változása és a maradék.
- Váltás a PhysX-világ és a sík világ között, az okkal.
- `[debug] discovery_log = 1` esetén másodpercenként: a jelenet állapota, az eltolás, a láb és a talaj magassága, az utolsó söprés találata.

## Tesztelés

### Egységtesztek (a mag, teszt-világgal: síkok, lejtő, dobozok)

- `scene_offset`: kerekítés pozitív és negatív irányban, a legkisebb maradékú aktor, a 30 m-es magasságszűrő, 6 m fölötti maradéknál nincs eredmény.
- `motion`, vízszintes: szabad út; frontális falnál megállás 0,02 m-re; ferde falnál csúszás; sarokban megállás; kitolás átfedésből korlátozott sebességgel.
- `motion`, függőleges: lépcsőfok 0,45 m alatt fel és le simítva; 0,6 m fölötti peremnél esés és leérkezés; lejtő követése; találat nélkül az utolsó magasság marad.
- `walker`: `FlatWorld`-del a mostani viselkedés (a meglévő tesztek zöldek maradnak); a nagy `dt` levágása.
- `locator`: `pe_builder`-rel épített képen megtalálja a vtable-t; hiányzó szövegnél, rossz slottávolságnál, két vtable-jelöltnél nem, és ezt naplózza.
- `settings`: a `[collision]` kulcsok beolvasása, mentése, határértékei.

### Elemzés a játékfájlon

- Ghidrával: a `release`/`shiftOrigin` vtable, a raycast (`0x2F0`), sweep (`0x2F8`), getNbActors/getActors (`0x90`/`0x98`), az aktor getType (`0x28`) és getGlobalPose (`0xA0`) eltolása, és a szálkérdés.
- A `probe` mind az öt keresést megtalálja a `game/eurotrucks2.exe`-n.
- Az igazolt tények az `ets2-1.61.md`-be kerülnek.

### Élő tesztek (verzióléptetéssel, `TESZT.md`-vel)

1. Keresés és eltolás (`discovery_log`): a jelenet megtalálása, a keresés ideje, az eltolás; a kiírt talajmagasság a kamion körül egyezik a látottal.
2. Talaj: lejtő, járdaszegély, lépcső, a pótkocsi platója mellett; leesés peremről; a kb. 35 m-es körzet széle.
3. Ütközés: fal, kerítés, a kamion és a pótkocsi oldala; csúszás a fal mentén, sarok; kiszállás közvetlenül a kamion mellett.
4. Hibatűrés: új mentés betöltése, majd séta (újrakeresés); `enabled = 0`.

A TM Real Walk a tesztek idejére ki van véve a `plugins` mappából.
