# MemoMind — Rappel de posture (posture)

Application **appairée** MemoMind : détecte la tête baissée via l'IMU des lunettes et rappelle de se redresser. Réglages et statistiques sur le téléphone.

## Composition

| Côté | Dossier | Artefact |
| --- | --- | --- |
| Lunettes (plugin natif C) | `glass/posture/` | `builds/posture.gmp` |
| Téléphone (plugin Web) | `phone/posture/` | `builds/posture-1.0.0.mmpkg` |

## Fonctionnement

- Les lunettes lisent le tangage (pitch) de l'IMU en mode brut, le lissent, et comparent à une posture de référence calibrée.
- Alerte si l'écart dépasse un seuil (5–45°, défaut 15°) pendant une durée continue (2–30 s, défaut 5 s), avec un cooldown anti-harcèlement.
- Calibration via le bouton des lunettes ou depuis le téléphone.
- Le téléphone règle seuil/durabilité et affiche les stats (rappels aujourd'hui, 7 derniers jours, dernière alerte).

## Build

Nécessite le SDK officiel MemoMind (`memomind-open/plugin-open-platform`). Déposer `glass/posture/` dans `GlassSDK/examples/` et `phone/posture/` dans `PhoneSDK/examples/`, puis lancer `python3 build.py`.

- Lunettes : `python3 build.py glass --force` → `.gmp`
- Téléphone : `python3 build.py web --force` → `.mmpkg`

Bilingue FR/EN.
