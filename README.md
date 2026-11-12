# Swell The Percussionist

Instrument de percussions VST3 / AU / Standalone de la suite **Swell** (Swell-audio, by @sevyondatracks). JUCE 8 + CMake.

> **v0.4** : un kit de 16 pads comme une MPC ou un drum rack, et toutes les notes MIDI jouent.
> **v0.4.1** : la fenêtre affiche la note vraiment jouée (la dernière dans le temps), nommée comme dans le piano roll du DAW.
> **v0.4.2** : cliquer un son de la liste l'écoute sans changer le kit ; double-clic ou glisser sur un pad pour le charger.
> **v0.5.0** : des kits prêts à l'emploi (menu Kit), un kit Vide, tes propres kits (Sauver le kit) et un tuto au premier lancement.
> **v0.5.1** : hors des pads 1 à 16, chaque note joue le pad de même nom (tous les F = pad 6) : une octave plus haut, c'est le même son. Dans un DAW, les touches du clavier d'ordi ne jouent plus les pads en double avec le DAW.

## Ce qu'il y a
- **Kits** (menu Kit, en haut) : un clic remplit les 16 pads et règle les effets. Swell Mix (par défaut), Afro Groove,
  Congas Live, Talking & Djembé, Shakers & Claps, Percs & Métal, FX & Textures, et **Vide** pour partir de zéro. Si tu
  as changé des pads, le plugin demande avant de les remplacer. **Sauver le kit** garde ton kit (sons + effets) dans
  `~/Library/Application Support/Swell/The Percussionist/Kits/`, pour tous tes projets. Les kits sont définis dans
  `Source/Kits.h`.
- **Tuto** : au premier lancement, 5 étapes montrent comment s'en servir ; « Passer le tuto » ou Échap pour le fermer,
  le bouton **?** le relance.
- **Kit de 16 pads** (4 x 4, pad 1 en bas à gauche). Chaque pad garde un son de n'importe quel onglet ; le kit est
  sauvegardé avec le projet, donc le piano roll joue toujours les mêmes sons, quel que soit l'onglet affiché.
- **MIDI** : les notes 36 à 51 jouent les pads 1 à 16. Partout ailleurs, chaque note joue le pad de même nom : tous les
  C jouent le pad 1, tous les F le pad 6, etc. Une octave plus haut ou plus bas, c'est donc toujours le même son, et
  aucune note n'est muette. Les pads 13 à 16 ne jouent que depuis leurs propres notes (48 à 51).
  Les noms des notes suivent le DAW : C1 à D#2 dans Ableton, Logic, Cubase, Studio One, Bitwig et Pro Tools (C3 = 60),
  C2 à D#3 dans Reaper (C4 = 60), C3 à D#4 dans FL Studio (C5 = 60).
- **Écouter / charger un son** : choisis un onglet, puis **clique** un son de la liste pour l'écouter : le kit ne change
  pas, donc ce que joue ton piano roll non plus. Pour le mettre dans un pad : **double-clic** (il va dans le pad
  sélectionné) ou **glisse-le sur un pad**. Haut du pad = doux, bas = fort.
- **10 onglets** : Djembé, Congas, Talking, Percs, Toms, Rims, Shakers, Claps, Métal, FX. Chacun a sa photo.
- **Clavier d'ordi** : dans l'app Standalone, `A Z E R T Y U I` et `Q S D F G H J K` jouent les pads 1 à 16. Dans un
  DAW, le plugin laisse les touches au DAW (le clavier de saisie de FL les change en notes, qui jouent les pads).
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
Mac : `./build-mac.sh` (réutilise tout seul le JUCE de `../Swell-Acoustic-Guitar-` s'il est là, et recrée `build/` si
le dossier du projet a changé de place)

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
