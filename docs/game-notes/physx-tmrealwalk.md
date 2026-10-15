# A játék PhysX-jelenete - amit a TM Real Walk kódjából tudunk

Forrás: a TM Real Walk v1.0.0-beta5 visszafejtett kódja (`content/decomp/all.c`), a hiányzó hívási paraméterek a DLL gépi kódjából (`dumpbin /disasm`), a konstansok a DLL-ből kiolvasva.
A kódjából semmit nem veszünk át; ez a jegyzet csak a játékra vonatkozó tényeket és mechanizmusokat rögzíti.
**Minden pont feltevés, amíg a saját elemzésünkkel (a `game/eurotrucks2.exe`-n) és élő teszttel nem igazoltuk.**
Az igazolt tények az `ets2-1.61.md`-be kerülnek.

Már ellenőrizve: a két alább használt PhysX-hibaüzenet szerepel a mi exénkben (`handoff/eurotrucks2-strings.txt`).

## Alapelv

A játék statikusan linkelt PhysX 3.4-ének élő jelenetét kérdezi le (raycast, sweep, overlap); saját aktort nem ad hozzá, nem szimulál.
Minden PhysX-hívás SEH-védett burkolón megy át; a hibák száma nő, 8 hiba után az ütközés a munkamenetre kikapcsol.

## A jelenet megkeresése (háttérszálon)

1. A `.rdata`-ban a `PxScene::release(): Scene is still being simulated` és a `PxScene::shiftOrigin() not allowed` szöveg helye.
2. A `.text`-ben az ezekre mutató RIP-relatív `LEA` (REX.W/REX.WR `8D`, ModRM `(x & 0xC7) == 5`); a befoglaló függvény eleje `RtlLookupFunctionEntry`-vel: ez a `release` és a `shiftOrigin`.
3. A `.rdata`-ban az a vtable, ahol `slot[i] == release` és `slot[i + 0x73] == shiftOrigin`.
   A vtable (az objektum első qwordja) a release előtti slottal kezdődik: a TM is `release slotja - 8`-at keres a memóriában, és a vtable-eltolásai ehhez képest értendők (a játékfájlon igazolva, `ets2-1.61.md`).
4. A folyamat `MEM_COMMIT` + `MEM_PRIVATE` + `PAGE_READWRITE` területein azok a qwordok, amelyek a vtable címét tartalmazzák (legfeljebb 16 jelölt; ha nincs, legfeljebb 8 próba, 8 s-onként).
5. Jelöltenként egy próba-raycast; az első hibátlan a jelenet.
6. Új világ betöltésekor nullázás és újrakeresés.

## Koordináták

A jelenet origója X-ben és Z-ben 512 m többszörösével el van tolva (`shiftOrigin`); Y nincs eltolva.
Meghatározás 250 ms-onként a telemetria kamionpozíciójából:
- a dinamikus aktorok közül azok, amelyek pozíciójának Y-ja 30 m-en belül van a kamionétól;
- `d = telemetria - aktor pozíciója`, `eltolás = round(d / 512) * 512` (X és Z külön);
- a legkisebb `|dx - eltolás_x| + |dz - eltolás_z|` maradékú aktor, ha a maradék 6 m alatt van; ezt az aktort megjegyzi, és legközelebb csak a pozícióját olvassa újra.

Jelenet-koordináta = világ - eltolás.

## Használt PhysX 3.4 felület (a nyilvános fejlécek alapján, ellenőrizendő)

| Objektum | Vtable-eltolás: függvény |
|---|---|
| Jelenet | `0x90` getNbActors, `0x98` getActors, `0x2F0` raycast, `0x2F8` sweep, `0x300` overlap |
| Aktor | `0x28` getType, `0x40` getName, `0x48` getWorldBounds(1.01), `0xA0` getGlobalPose, `0xC8` getNbShapes, `0xD0` getShapes, `0x128` getLinearVelocity |
| Alakzat | `0x30` getGeometryType, `0x48` getBoxGeometry, `0x90` getLocalPose, `0xA0`/`0xB0` szűrőadatok, `0x108` getFlags, `0x120` getName |

- Hit flagek: raycast `0x83` (ePOSITION, eNORMAL, eMESH_BOTH_SIDES), sweep `0x283` (ugyanez + eMTD, kezdeti átfedésnél a behatolás mélysége), sweep inflation 0.
- Lekérdezési szűrő: a szűrőadat négy nulla szó; a flag 1 = statikus, 2 = dinamikus, 3 = mindkettő; szűrő-callback és cache nincs.
- A `PxFlags` és a `PxQueryFilterData` MSVC-n hivatkozással adódik át.
- `PxRaycastHit` a találati pufferben: actor, shape, faceIndex, flags, position (12), normal (12), distance, u, v; utána a `hasBlock`.

## Talaj (minden képkockában)

- Fő sugár: lefelé a `láb + step_height` (0,45 m) pontból, 10 m, csak statikus; elfogadva, ha `|n.y| >= 0,3`.
- Második sugár: csak dinamikus, `step_height + 0,35` m; elfogadva, ha a távolság legalább 0,02 m és `|n.y| >= 0,5`.
- A talaj a két találat közül a magasabb.
- Talajon: ha `láb - talaj > step_down_max` (0,6 m), esés; különben `láb += (1 - exp(-dt / 0,12)) * (talaj - láb)`, ha az eltérés 2 cm-nél nagyobb.
- Levegőben: `vy -= g * dt`, `láb += vy * dt`; leérkezés, ha a láb eléri a talajt és `vy <= 0`. Ugrás: `vy = jump_speed`.

## Vízszintes ütközés: kapszula-sweep csúszással

- Kapszula: sugár 0,40 m; alja `láb + step_height` (a lépcsőfokokat a talajsugár kezeli), teteje `láb + testmagasság + 0,08`; félmagasság `max(0,02; (tető - alj) / 2 - sugár)`; függőlegesre forgatva (90° a Z körül).
- Legfeljebb 3 lépés: söprés a hátralevő elmozdulással + 0,02 m ráhagyással; találatnál `távolság - 0,02` előre, majd az irány vetítése a fal vízszintes normálisára merőleges síkra.
- Kezdeti átfedés: kitolás a vízszintes normális mentén `min(1,5 + 1,2 * a test sebessége; 30) * dt` sebességgel, legfeljebb a mélység + 5 mm; ha 200 ms-on belül 4-szer egyre mélyebb, a tolás iránya megfordul.

## Későbbi szakaszokhoz

- Mozgó járművek: 150 ms-onként a 10 m-en (függőlegesen 15 m-en) belüli dinamikus testek doboz-alakzatai és konvex burkai (legfeljebb 64), képkockánként az aktuális pozícióval transzformálva, saját 2D kitolás; ezeken állni is lehet.
- Mászás: 0,7 m-es sweep előre; falnál (`|n.y| <= 0,7`) 3 lefelé mutató sugár a falon túl `sugár + {0,12; 0,35; 0,65}` m-re, `láb + max_climb_height + 0,05` magasságból; a tető `láb + step_height / 2` és `láb + max_climb_height` között; végül overlap-teszt 0,1 m-es lépésekben.
- A játék csak a kamion körül (kb. 35 m) tart ütközési geometriát:
  - `second_focus`: a játék `region_query` és `box_builder` rutinja a sétáló köré (a doboz 30 m-ről 150 m-re nő); a jelenet vtable-jének `+0x178` (`focus_routine`) slotját is átírják, hogy az elemenkénti távolságkapuk (`gate_model_item`, `gate_area_item`) a sétálót kövessék. Ez a játék kódmutatójának átírása, a keretünkkel egyeztetni kell.
  - `far_ground`: geometria híján a közeli térképelemek befoglaló dobozainak magasságából súlyozott átlag (csak a lábtól ±1 m-en belüli elemek).

## Nyitott kérdések

- Melyik szálról és mikor biztonságos lekérdezni: a TM a telemetria képkocka-eseményéből hív, zárolás nélkül.
- A vtable-mintázat és az eltolások a 1.61.1.1s-en (saját keresés a `probe`-ban).
- Az 512 m-es eltolás élőben.
