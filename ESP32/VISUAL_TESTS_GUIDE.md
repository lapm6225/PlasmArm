# Guide des Tests Visuels

Les tests visuels affichent des informations détaillées pour comprendre et vérifier le fonctionnement du système d'interpolation et de cinématique.

## 🎯 Tests Visuels Disponibles

### Test 1: Interpolation Visualization
Affiche tous les points d'interpolation générés entre deux positions.

**Ce que vous verrez :**
- Point de départ et d'arrivée
- Distance totale
- Tous les points interpolés avec leur numéro
- Distance depuis le départ et pourcentage de progression

### Test 2: Angle → Position (Forward Kinematics)
Vérifie que les angles calculent correctement les positions.

**Ce que vous verrez :**
- Différentes combinaisons d'angles (θ1, θ2)
- Position résultante pour chaque combinaison
- Distance depuis l'origine
- Vérification si la position est dans l'espace de travail

### Test 3: Position → Angles (Inverse Kinematics)
Vérifie que les positions calculent correctement les angles.

**Ce que vous verrez :**
- Différentes positions cibles (x, y)
- Angles calculés (θ1, θ2)
- Vérification aller-retour (round-trip)
- Erreur de précision (Δx, Δy, distance)

### Test 4: Full Path Visualization ⭐ **LE PLUS IMPORTANT**
Test complet qui combine interpolation + cinématique.

**Ce que vous verrez :**
- Tableau avec tous les points interpolés
- Pour chaque point :
  - Coordonnées (x, y)
  - Angles calculés (θ1, θ2)
  - Erreur de vérification
  - Statut ✅ ou ❌
- Résumé final avec nombre de points passés/échoués

## 🚀 Comment Activer les Tests Visuels

### Méthode 1 : Via Config.h (Recommandé)

1. Ouvrez `src/Config.h`
2. Trouvez la section "Test Mode Configuration"
3. Changez :
   ```cpp
   #define RUN_VISUAL_TESTS false
   ```
   en :
   ```cpp
   #define RUN_VISUAL_TESTS true
   ```
4. Assurez-vous que `RUN_UNIT_TESTS` est à `false`
5. Compilez et téléversez :
   ```bash
   platformio run --target upload
   ```
6. Ouvrez le Serial Monitor (115200 baud)

### Méthode 2 : Activer les deux modes

Vous pouvez activer les deux pour voir d'abord les tests unitaires, puis les tests visuels :
```cpp
#define RUN_UNIT_TESTS true
#define RUN_VISUAL_TESTS true  // Sera exécuté après les tests unitaires
```

## 📊 Exemple de Sortie

```
═══════════════════════════════════════════════════════════
TEST 4: FULL PATH VISUALIZATION
(Interpolation + Kinematics Verification)
═══════════════════════════════════════════════════════════

Full path test:
Start: (150.00, 100.00) mm
End: (200.00, 200.00) mm

Start reachable: YES ✅
End reachable: YES ✅

Start and end angles:
Start: θ1=45.00°, θ2=30.00°
End: θ1=60.00°, θ2=45.00°

Interpolated path (25 points):
─────────────────────────────────────────────────────────────
Point# | X (mm)  | Y (mm)  | θ1 (°)  | θ2 (°)  | Error (mm) | Status
─────────────────────────────────────────────────────────────
    0 |  150.00 |  100.00 |   45.00 |   30.00 |     0.0000 | ✅
    5 |  160.00 |  120.00 |   48.50 |   35.20 |     0.0023 | ✅
   10 |  170.00 |  140.00 |   52.00 |   40.40 |     0.0018 | ✅
   15 |  180.00 |  160.00 |   55.50 |   42.60 |     0.0031 | ✅
   20 |  190.00 |  180.00 |   58.00 |   43.80 |     0.0027 | ✅
   24 |  200.00 |  200.00 |   60.00 |   45.00 |     0.0000 | ✅
─────────────────────────────────────────────────────────────

Summary:
  Total points: 25
  Passed: 25 ✅
  Failed: 0 ❌
  Max error: 0.0031 mm

✅ All interpolation points verified successfully!
```

## 🔍 Interprétation des Résultats

### ✅ Points Passés
- L'erreur est < 0.1 mm
- Les angles calculés permettent d'atteindre la position cible avec précision

### ❌ Points Échoués
- L'erreur est > 0.1 mm
- Possible problème dans :
  - Le calcul de cinématique inverse
  - La précision des calculs
  - Les limites de l'espace de travail

### Erreur de Vérification
L'erreur montre la différence entre :
- La position cible (point interpolé)
- La position calculée en utilisant les angles (round-trip)

**Formule :**
```
Erreur = distance(position_cible, position_verification)
```

## 🛠️ Personnaliser les Tests

Vous pouvez modifier les points de test dans `src/test/TestVisual.cpp` :

### Changer les points de départ/arrivée
Dans `testFullPathVisual()`, modifiez :
```cpp
Point2D start(150.0f, 100.0f);  // Votre point de départ
Point2D end(200.0f, 200.0f);    // Votre point d'arrivée
```

### Changer la vitesse d'interpolation
```cpp
Planner planner(50.0f, 200.0f);  // Vitesse (mm/s), Accélération (mm/s²)
```

### Afficher plus/moins de points
Dans la boucle d'affichage, modifiez :
```cpp
if (index % 5 == 0 || index == 0 || index == numPoints - 1) {
    // Affiche tous les 5 points
    // Changez 5 en 1 pour tout afficher, ou 10 pour moins de détails
}
```

## 📝 Notes Importantes

1. **Les tests visuels ne nécessitent pas de matériel** - ils fonctionnent sans moteurs connectés
2. **Les valeurs sont en millimètres** - vérifiez vos longueurs de bras dans `Config.h`
3. **Les angles sont en degrés** - 0° = droite, 90° = haut, 180° = gauche
4. **L'erreur acceptable** - généralement < 0.1 mm est excellent pour un robot SCARA

## 🐛 Dépannage

### Si tous les tests échouent :
- Vérifiez les longueurs de bras dans `Config.h` (ARM_LENGTH_1, ARM_LENGTH_2)
- Vérifiez que les points sont dans l'espace de travail

### Si l'erreur est élevée :
- Vérifiez la précision des calculs de cinématique
- Vérifiez les arrondis dans les calculs

### Si le Serial Monitor ne montre rien :
- Vérifiez que `RUN_VISUAL_TESTS` est bien à `true`
- Vérifiez la vitesse du port série (115200 baud)
- Attendez quelques secondes après le reset

## 🎓 Comprendre les Résultats

Le **Test 4 (Full Path)** est le plus important car il vérifie :
1. ✅ Que l'interpolation génère des points réguliers
2. ✅ Que chaque point peut être atteint (cinématique inverse fonctionne)
3. ✅ Que les angles calculés permettent vraiment d'atteindre la position (round-trip)

Si ce test passe, votre système d'interpolation et de cinématique fonctionne correctement ! 🎉
