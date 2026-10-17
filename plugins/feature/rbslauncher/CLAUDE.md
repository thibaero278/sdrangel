# RBS Launcher — règles techniques

Plugin feature SDRangel qui ouvre la fenêtre RBS (interface simplifiée façon SDR++) et pilote SDRangel
derrière. Contexte projet, état et à-faire : `C:\RBS\PASSATION.md`. Audit et architecture cible :
`C:\RBS\docs\AUDIT-ARCHITECTURE-2026-10-17.md`.

## Architecture

- `RBSController` (`rbscontroller.*`) : pilote SDRangel via la WebAPI interne (device set, réglages du RSP1B,
  canal décodeur, canal d'écoute). File d'étapes asynchrones, tout dans le thread GUI. **Aucune logique propre à
  un mode ici** : ce qui dépend du mode vient de la fiche JSON.
- `RBSMode` (`rbsmode.*`) + `modes/*.json` : une « fiche de mode » = tout ce qu'il faut pour régler le récepteur.
  Fiches intégrées dans `rbslauncher.qrc`, surchargeables par l'utilisateur (même `id`).
- `RBSWindow` (`rbswindow.*`) : la fenêtre. Aujourd'hui elle contient encore la logique des 4 décodeurs
  (FT8, APRS, ADS-B, CW) : c'est la dette à résorber, voir ci-dessous.
- Widgets génériques : `rbsfrequencydial`, `rbsbandpopup`, `rbsmodepopup`, `rbsmeters`, `rbssection`,
  `rbsdelegates`, `rbsdecodemodel` (table + filtre), `rbsspectrumtee`.
- Décodeur Morse RBS : `rbscwdecoder` (DSP pur C++, sans Qt) + `rbscwtap` (branche le pipe audio d'un canal).
  C'est le modèle à suivre pour tout DSP maison : isolé et testable seul.

### Sources des décodes (une par famille SDRangel)

| Mode | Canal SDRangel | Lecture côté RBS |
|---|---|---|
| FT8 | `FT8Demod` | `RBSFT8Log` suit le log journalier du canal (tail -f) |
| APRS | `PacketDemod` | `MCPDataFeed::getPackets` (source empruntée à `plugins/feature/mcpserver`) |
| ADS-B | `ADSBDemod` | `MCPDataFeed::getMapItems`, toutes les secondes |
| CW | `SSBDemod` | `RBSCWTap` + `RBSCWDecoder` |

## Règles pour ajouter ou modifier un décodeur

1. **Un décodeur = un dossier** `decoders/<id>/`, une classe qui implémente l'interface `RBSDecoder`
   (`decoders/rbsdecoder.h`) et une ligne dans le registre. Tant que le refactor n'est pas fait, le premier
   décodeur ajouté ou migré crée l'interface (esquisse dans l'audit, §2).
2. **Interdit d'ajouter un `if (view == "...")`** dans `RBSWindow` ou `RBSController`. Si la fenêtre a besoin de
   savoir quelque chose du décodeur, c'est une méthode virtuelle de `RBSDecoder` ; si le contrôleur en a besoin,
   c'est une clé de la fiche JSON.
3. Un décodeur ne connaît **ni la fenêtre ni les autres décodeurs** : il émet des signaux (ligne de table,
   ligne de texte, station entendue, événement). Pas d'état partagé entre décodeurs.
4. Les options propres à un décodeur (cases « CQ seulement », filtre CW…) sont créées par le décodeur
   (`createOptions`), pas déclarées en membres de `RBSWindow`.
5. Le DSP maison va dans une classe sans Qt (comme `RBSCWDecoder`) avec un banc de test dans `test/`.
6. Nouvelle fiche JSON : l'ajouter à `rbslauncher.qrc` et, si nouvelle clé, la documenter dans `rbsmode.h`
   et `readme.md`.
7. Nouveaux fichiers source : les ajouter à `CMakeLists.txt` (section `if(NOT SERVER_MODE)`).

Le skill `/nouveau-decodeur` génère ce squelette.

## Cœur SDRangel (hors de ce dossier)

- Le moins possible. Tout changement hors `plugins/feature/rbslauncher/` est commenté `// RBS:` et listé
  dans `PASSATION.md` : les fusions avec `upstream` (`f4exb/sdrangel`) en dépendent.
- Ne jamais supprimer de fichiers upstream (logos, plugins…) : on les exclut du build par option CMake.
- Patchs actuels : spectre/waterfall (`sdrgui/gui/glspectrumview*`, `glspectrum.h`, `sdrbase/util/colormap.cpp`),
  wisdom FFTW (`sdrbase/dsp/fftwengine.*`, `sdrgui/mainwindow.cpp`). Les patchs `morsedecoder.cpp` et
  `mcpdatafeed.cpp` (branche MorseDecoder) sont morts : à retirer.

## Style du code

- Comme SDRangel : en-tête GPL `Copyright (C) 2026 Thibaud (RBS)` + `Some code by Claude`, membres `m_`,
  accolades sur leur propre ligne pour les fonctions et blocs multi-lignes, commentaires en anglais.
- Textes de l'interface en français (c'est l'app de Thibaud), avec les accents.
- Qt 6.9, C++17. Pas de nouvelle dépendance externe sans en parler.

## Compiler, lancer, tester

- Compiler : skill `/build-rbs` (ou `C:\RBS\scripts\build-rbs.cmd`, `clean` pour repartir de zéro).
  Toujours rediriger la sortie vers `C:\RBS\logs\build-NN.log` (numéro suivant).
- Lancer : `C:\RBS\scripts\lancer-rbs.cmd`. Un seul logiciel à la fois sur le RSP1B (fermer SDR++, SDRuno,
  SatDump) et le RSP1B ne doit pas être attaché à WSL (`usbipd list`).
- Test Morse : `rbscwdecoder` est sans Qt, il se compile seul dans un environnement `vcvars64` :
  `cl /nologo /O2 /EHsc /std:c++17 test\rbscwtest.cpp rbscwdecoder.cpp /Fe:<scratch>\rbscwtest.exe`
- Les décodages se valident **en réception réelle** (Thibaud teste) : dire clairement ce qui n'a pas été testé.
- Les sorties de `build/` (DLL, PDB) ne se commitent jamais.

## Git

- Dépôt `RBS-app/`, branche `rbs/launcher` (`master` alignée dessus, indexée par graphify).
- Identité : `git -c user.name=thibaero278 -c user.email=thibaudg3@gmail.com commit …`
- Préfixe des messages : `RBS: `. Un commit par étape compilée. Pousser seulement quand Thibaud le demande.
- Ne pas commiter le pointeur du sous-module `external/windows` (modifié localement par le build).
