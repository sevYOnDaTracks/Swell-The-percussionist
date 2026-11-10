# Swell The Percussionist

Instrument de percussions VST3 / AU / Standalone de la suite **Swell** (Swell-audio, by @sevyondatracks). JUCE 8 + CMake.

> **v0.4** : un kit de 16 pads comme une MPC ou un drum rack, et toutes les notes MIDI jouent.

## Ce qu'il y a
- **Kit de 16 pads** (4 x 4, pad 1 en bas à gauche). Chaque pad garde un son de n'importe quel onglet ; le kit est
  sauvegardé avec le projet, donc le piano roll joue toujours les mêmes sons, quel que soit l'onglet affiché.
- **MIDI** : C1 à D#2 (notes 36 à 51 ; C3 à D#4 dans FL Studio) jouent les pads 1 à 16, et les 16 pads se répètent
  sur tout le clavier : aucune note n'est muette.
- **Charger un son** : clique un pad (il joue et se sélectionne), choisis un onglet, puis clique un son de la liste :
  il est chargé dans le pad sélectionné. Haut du pad = doux, bas = fort.
- **10 onglets** : Djembé, Congas, Talking, Percs, Toms, Rims, Shakers, Claps, Métal, FX. Chacun a sa photo.
- **Clavier** : `A Z E R T Y U I` et `Q S D F G H J K` jouent les pads 1 à 16.
- **8 boutons** : Tune, Decay, Saturation, Chorus, Flangus, Delay, Reverb, Volume. Un effet à zéro est coupé. Le delay suit le tempo du DAW (croche pointée).
- **7 presets** d'effets. Par défaut : **Soft and Clean** (Saturation 17 %, Reverb 2 %, le reste coupé).
- **Favoris** : l'étoile d'un son dans la liste le garde ; **★ Favoris** n'affiche que les favoris de l'onglet. Ils sont
  gardés pour tous les projets, dans `~/Library/Application Support/Swell/The Percussionist/favoris.txt`.

## Les sons
`assets/samples/A-TRIER/` est rangé comme le plugin : un dossier par onglet (Djembé, Congas, Talking, Percs, Toms, Rims,
Shakers, Claps, Métal, FX), chaque fichier nommé comme son pad (`Toms/Bombo Grave 2.wav`). À côté : `Loops/` et `Doublons/`
(non embarqués) et `Autres/` (fichiers qui ne sont pas des sons). `_journal.csv` garde le nom et l'emplacement d'origine de
chaque fichier ; `python3 -I Tools/organize_samples.py assets/samples/A-TRIER --undo` remet tout comme avant.

Le pack embarqué est dans `assets/samples/pack/` (FLAC 16 bits, silences coupés, volume ramené à -1 dBFS), sa liste dans
`Source/SampleIndex.h`, et le détail fichier par fichier dans `docs/classement.csv`.

**Corriger le tri** : glisser le fichier dans le bon dossier d'onglet. **Ajouter des sons** : les déposer à la racine de
`A-TRIER`, ils sont classés d'après leur nom ou leur son. Puis :
```
python3 -I Tools/analyze_samples.py assets/samples/A-TRIER /tmp/perc/analysis.jsonl
python3 -I Tools/classify_samples.py /tmp/perc
python3 -I Tools/pack_samples.py /tmp/perc/classified.json assets/samples/A-TRIER
```
et recompiler. Le pack renomme et range les fichiers pour que le dossier corresponde toujours au plugin.

## Pas encore fait
- Build Windows.
- Les loops (27 fichiers) ne sont pas dans le plugin.

## Icône
`Resources/icon.png` (un djembé), dessinée par `python3 -I Tools/make_icon.py Resources/icon.png`.

## Build
Mac : `./build-mac.sh`

Pour réutiliser le JUCE déjà téléchargé par un autre projet :
`cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=../Swell-Acoustic-Guitar-/build/_deps/juce-src`

## Crédits photos (provisoires)
Wikimedia Commons, passées en noir et blanc par le plugin. **À remplacer avant la vente** : les licences CC BY-SA imposent de partager les œuvres dérivées sous la même licence. Vérifier aussi les logos de marque visibles sur certains instruments.

| Catégorie | Fichier Commons | Auteur | Licence |
|---|---|---|---|
| Djembé | Djembe_Oblique_3rd_20231001.jpg | Niranjan Arminius | CC BY-SA 4.0 |
| Percs | Bongos_-_Vladimir_Morozov.jpg | Vladimir Morozov | CC BY 2.0 |
| Congas | Congas_(4594963734).jpg | Daniele Febei | CC BY-SA 2.0 |
| Shaker | Two_egg_shakers.jpg | Paju | CC BY-SA 3.0 |
| Métal | LP_Tambourine_by_MusicTales.jpg | Serg Childed | CC BY-SA 4.0 |
| Talking | Gangan.jpg | EmmanuelOEmmanuel | CC BY-SA 4.0 |
| Toms | Les_Toms_Alto_et_Médium.jpg | Modjou | CC BY-SA 4.0 |
| Rims | Snare_drum_-_Vladimir_Morozov.jpg | Vladimir Morozov | CC BY 2.0 |
| Claps | Clapping_hand.jpg | Mona Hassan Abo-Abda | CC BY-SA 4.0 |
| FX | Shure_520DX_Green_Bullet (micro) | Ky | CC BY 2.0 |
