# FreeWalk - beállítóablak (2. alprojekt)

Dátum: 2026-10-14

## Cél

A játékos a `FreeWalk.ini` kézi szerkesztése helyett egy áttekinthető ablakban állíthatja be a plugin minden paraméterét, játék közben is.
A változások azonnal érvényesek, a Save gomb elmenti őket az INI-be.

## Döntések (a felhasználóval egyeztetve)

- Önálló Windows-ablak, amelyet maga a plugin (a DLL) nyit meg billentyűre; a játék képébe rajzolt menü nem része ennek az alprojektnek.
- A felület nyelve angol (a naplóhoz igazodva); más nyelv később kerülhet bele.
- A beállítások azonnal életbe lépnek, ahogy a játékos módosítja őket; a Save az INI-be írja, a Cancel visszaállítja a megnyitáskori értékeket.
- A kereskedelmi TM Real Walk beállítóablakáról csak a felhasználói leírásából ismert működést (önálló ablak, mentés az INI-be, fókusz visszaadása) vettük figyelembe; kódot nem veszünk át.

## Funkciók

### Megnyitás és bezárás

- Billentyű: `Ctrl+F10` (állítható a `[keys] settings` értékkel, `0` = kikapcsolva).
  A kiszállás billentyűje (`F10`) nem reagál, amíg a `Ctrl` le van nyomva, így a kettő nem zavarja egymást.
- Az ablak a játékablak fölött, középen nyílik meg, és megkapja a fókuszt; a játék közben fut tovább.
  Amíg az ablaké a fókusz, a séta billentyűi nem mozgatják a gyaloglót (a meglévő fókuszfeltétel miatt).
- Újabb `Ctrl+F10` vagy a bezárás (Save, Cancel, a címsor X-e, `Esc`) bezárja; bezáráskor a fókusz visszakerül a játékablakra.
  Az X és az `Esc` a Cancel-lel azonos.
- Egyszerre legfeljebb egy ablak lehet nyitva.

### Tartalom

Fülek, a mostani INI-szakaszok szerint; minden mező mellett egy rövid magyarázó felirat (angolul), a mértékegységgel:

- Keys: kiszállás, beállítóablak, előre, hátra, balra, jobbra, futás.
  Minden billentyűnél egy gomb mutatja az aktuális billentyű nevét; rákattintva "Press a key..." felirat jelenik meg, és a következő leütött billentyű lesz az új érték (`Esc` megszakítja).
  Ha két funkció ugyanazt a billentyűt kapná, a mező pirosan jelzi, és a Save nem engedélyezett, amíg az ütközés fennáll.
  Egy figyelmeztető sor emlékeztet, hogy a séta billentyűit a játék saját kiosztásában fel kell szabadítani (a README szövege szerint).
- Walking: járás és futás sebessége, szemmagasság, fejbiccentés (be/ki és mértéke), egérérzékenység, legnagyobb távolság a kamiontól.
- Leaving the truck: oldaltávolság, az ajtó felé nézés (be/ki), a fordulás szöge, talajeltolás.
- Debug: discovery_log, write_on_frame_start, directinput_filter, switch_test; egy felirat jelzi, hogy ezek hibakereséshez valók.

A számértékek csúszkával és mellette szerkeszthető számmezővel állíthatók; a tartomány és a lépésköz ugyanaz, amit az INI beolvasása elfogad.
Alul: Defaults (minden mező az alapértékre, még mentés nélkül), Cancel, Save.

### Életbe lépés

- A számok és kapcsolók a következő képkockától érvényesek; ha éppen séta van, az egérérzékenység is azonnal átíródik a szabad kamerában.
- A billentyűk új értéke vezetés közben azonnal érvényes; séta közben a következő kiszállástól, hogy a gyalogló billentyűi menet közben ne változzanak meg.
  A DirectInput-szűrő (ha be van kapcsolva) a billentyűket csak a játék újraindítása után veszi át; a Debug fülön egy felirat ezt jelzi.
- A `directinput_filter` és a `switch_test` átkapcsolása a játék újraindítása után érvényes (a felirat jelzi).

### Mentés

- A Save a teljes `FreeWalk.ini`-t újraírja a mostani formában: szakaszok, minden kulcs, egy-egy magyar nyelvű megjegyzés (ahogy a csomagban lévő INI-ben).
  Az INI-ben lévő, a plugin által nem ismert kulcsok elvesznek; erről a mentés előtt egy figyelmeztető sor szól, ha volt ilyen a beolvasott fájlban.
- A fájlt előbb egy ideiglenes fájlba írja, majd lecseréli vele a régit, így egy félbeszakadt mentés nem hagy hibás INI-t.
- Ha a mentés nem sikerül (például írásvédett mappa), az ablak hibaüzenetet mutat, a napló is rögzíti, és az ablak nyitva marad.

## Felépítés

- `src/core/settings_schema.h/.cpp`: a beállítások egyetlen leírótáblája: szakasz, kulcs, felirat, típus (billentyű, szám, kapcsoló), alapérték, tartomány, lépésköz, megjegyzés, és hogy mikor lép életbe.
  Az INI beolvasása, az INI írása és az ablak is ebből dolgozik, így egy új beállítás egy helyen vehető fel.
- `src/core/config.cpp`: a mostani `parse_config` a táblára épül; új `format_config(const Config&)` az INI szövegét állítja elő.
- `src/core/keys.cpp`: új `key_name(int)` a `parse_key` párja (a mentéshez és a gombfeliratokhoz), és a `CTRL+` előtag kezelése.
- `src/win/settings_window.h/.cpp`: a Win32-ablak egy saját szálon, saját üzenetkezelővel; a táblából építi fel a füleket és a vezérlőket.
  A Windows saját vezérlőit használja a rendszer mai megjelenésével (Common Controls 6 aktivációs környezettel, mert a DLL-nek nincs saját manifestje), és a rendszer betűtípusát; méretezés a képernyő DPI-je szerint.
- `src/win/plugin.cpp`: a billentyű figyelése, az ablak nyitása és bezárása, és a módosított beállítások átvétele a képkocka végén.

## Adatáramlás

1. A játék szála `Ctrl+F10`-re elindítja az ablak szálát a pillanatnyi `Config` másolatával.
2. Az ablak minden módosításnál egy teljes új `Config`-ot ad át egy zárral védett "függő beállítás" helyen.
3. A játék szála a `frame_end` elején átveszi a függő beállítást, és alkalmazza (a billentyűknél a fenti szabály szerint).
4. Save: az ablak szála az INI-t írja (a játék szála ebben nem vesz részt); Cancel: a megnyitáskori `Config` kerül a függő helyre.
5. A plugin leállításakor (`scs_telemetry_shutdown`) az ablak bezárul, és a leállítás megvárja a szálát (legfeljebb 2 s).

## Hibakezelés

- Ha az ablak nem hozható létre, egy naplósor rögzíti, a séta zavartalanul működik tovább.
- Érvénytelen kézi beírás (például betű a számmezőben, tartományon kívüli érték) nem kerül át; a mező pirosan jelzi, a csúszka az utolsó jó értéken marad.
- A naplósorok angolok, ASCII-k, `[FreeWalk] ` előtaggal, mint eddig.

## Tesztelés

- Mag (automatikus tesztek):
  - a leírótábla és a `Config` összhangja (minden mezőnek van bejegyzése, az alapértékek egyeznek);
  - `format_config` majd `parse_config` visszaadja ugyanazt a beállítást, figyelmeztetés nélkül;
  - `key_name` és `parse_key` egymás inverzei minden támogatott billentyűre, a `CTRL+` előtaggal együtt;
  - billentyűütközés felismerése;
  - a függő beállítás alkalmazásának szabálya (billentyűk séta közben nem változnak).
- Élő teszt (a felhasználó): megnyitás és bezárás, csúszka húzása séta közben (sebesség, érzékenység), billentyű átállítása és kipróbálása, Save után újraindítás és az értékek megmaradása, Cancel, írásvédett INI esetén a hibaüzenet.

## Nem része ennek az alprojektnek

- A játék képébe rajzolt menü, más nyelvek, a HUD elrejtése, a fizika (talaj és ütközés).
