# Guide du Test Interactif avec Moteurs Réels

Le test interactif vous permet d'entrer des coordonnées (x, y) via le Serial Monitor et de voir en temps réel :
- ✅ Tous les points d'interpolation
- ✅ Les angles calculés (θ1, θ2) pour chaque point
- ✅ Les moteurs qui bougent réellement
- ✅ La vérification de précision

## 🚀 Activation

### Étape 1 : Configurer les pins des moteurs

Les pins sont déjà configurés dans `Config.h` :
- **Motor 1** : STEP=19, DIR=18, ENABLE=27
- **Motor 2** : STEP=14, DIR=12, ENABLE=13

### Étape 2 : Activer le mode interactif

Ouvrez `src/Config.h` et changez :
```cpp
#define RUN_INTERACTIVE_TEST false
```
en :
```cpp
#define RUN_INTERACTIVE_TEST true
```

Assurez-vous que les autres modes de test sont à `false` :
```cpp
#define RUN_UNIT_TESTS false
#define RUN_VISUAL_TESTS false
```

### Étape 3 : Connecter les moteurs

Connectez vos stepper motors selon la configuration :
- Motor 1 (Base) : STEP=19, DIR=18, ENABLE=27
- Motor 2 (Elbow) : STEP=14, DIR=12, ENABLE=13

### Étape 4 : Compiler et téléverser

```bash
platformio run --target upload
```

### Étape 5 : Ouvrir le Serial Monitor

- Vitesse : **115200 baud**
- **Important** : Assurez-vous que "Both NL & CR" ou "Newline" est sélectionné dans le Serial Monitor

## 📝 Commandes Disponibles

### 1. Déplacer vers une position
```
200,150
```
ou
```
move 200,150
```
Déplace le robot vers la position (200mm, 150mm)

### 2. Retour à la position d'origine
```
home
```
Déplace le robot vers (0, 0)

### 3. Afficher la position actuelle
```
pos
```
ou
```
position
```
Affiche :
- Position actuelle (x, y)
- Angles actuels (θ1, θ2)
- Angles des moteurs
- État de mouvement

### 4. Séquence de test automatique
```
test
```
Exécute une séquence de 4 mouvements prédéfinis pour tester le système

### 5. Aide
```
help
```
Affiche la liste des commandes disponibles

## 📊 Exemple de Sortie

Quand vous entrez `200,150`, vous verrez :

```
═══════════════════════════════════════════════════════════
MOVING TO TARGET
═══════════════════════════════════════════════════════════
From: (150.00, 150.00) mm
To: (200.00, 150.00) mm

📊 Interpolation: 25 points generated

Point# | X (mm)  | Y (mm)  | θ1 (°)  | θ2 (°)  | Status
─────────────────────────────────────────────────────────────
    0 |  150.00 |  150.00 |   45.00 |   30.00 | ✅
    5 |  160.00 |  150.00 |   48.50 |   32.20 | ✅
   10 |  170.00 |  150.00 |   52.00 |   34.40 | ✅
   15 |  180.00 |  150.00 |   55.50 |   36.60 | ✅
   20 |  190.00 |  150.00 |   58.00 |   38.80 | ✅
   24 |  200.00 |  150.00 |   60.00 |   40.00 | ✅
─────────────────────────────────────────────────────────────
Summary: 25 passed ✅, 0 failed ❌

⏳ Waiting for motors to reach target...
✅ Motors reached target position

Final angles: θ1=60.00°, θ2=40.00°

✅ Movement command completed!
═══════════════════════════════════════════════════════════
```

## 🎯 Fonctionnalités

### Affichage en Temps Réel
- Chaque point interpolé est affiché avec ses coordonnées et angles
- Le statut ✅/❌ indique si la vérification round-trip a réussi
- Les moteurs bougent réellement pendant l'affichage

### Vérification Automatique
Pour chaque point interpolé :
1. Calcul de la cinématique inverse (position → angles)
2. Vérification round-trip (angles → position)
3. Calcul de l'erreur
4. Affichage du statut

### Gestion des Erreurs
- Si une position n'est pas atteignable, un message d'erreur clair est affiché
- Les limites de l'espace de travail sont affichées
- Timeout de 30 secondes pour éviter les blocages

## 🔧 Personnalisation

### Changer la vitesse d'interpolation

Dans `TestInteractive.cpp`, modifiez :
```cpp
Planner planner(DEFAULT_SPEED, ACCELERATION);
```
ou dans `Config.h` :
```cpp
#define DEFAULT_SPEED 50.0f  // mm/s
```

### Changer la position de départ

Dans `TestInteractive.cpp`, modifiez :
```cpp
Point2D currentPos(150.0f, 150.0f);  // Votre position de départ
```

### Afficher plus/moins de détails

Dans `TestInteractive.cpp`, dans la fonction `executeMove()`, modifiez :
```cpp
if (showDetails && (index % 5 == 0 || index == 0 || index == numPoints - 1)) {
    // Changez 5 en 1 pour tout afficher, ou 10 pour moins de détails
}
```

## ⚠️ Notes Importantes

1. **Les moteurs doivent être connectés** - Le test utilise les vrais moteurs
2. **Vitesse du Serial Monitor** - Doit être à 115200 baud
3. **Format des commandes** - Utilisez `x,y` ou `move x,y` (pas d'espaces autour de la virgule)
4. **Attente du mouvement** - Le système attend que les moteurs atteignent la position avant d'accepter la prochaine commande
5. **Timeout** - Si un mouvement prend plus de 30 secondes, un avertissement est affiché

## 🐛 Dépannage

### Les moteurs ne bougent pas
- Vérifiez les connexions (STEP, DIR, ENABLE)
- Vérifiez que les pins dans `Config.h` correspondent à votre câblage
- Vérifiez que les moteurs sont alimentés

### Erreur "Target not reachable"
- Vérifiez que les coordonnées sont dans l'espace de travail
- Espace de travail : distance de l'origine entre `minReach` et `maxReach`
- Pour un robot avec L1=150mm, L2=150mm : max reach = 300mm

### Le Serial Monitor ne répond pas
- Vérifiez la vitesse (115200 baud)
- Vérifiez que "Newline" est sélectionné
- Réinitialisez l'ESP32

### Les angles semblent incorrects
- Vérifiez les longueurs de bras dans `Config.h` (ARM_LENGTH_1, ARM_LENGTH_2)
- Vérifiez que les moteurs sont correctement montés (sens de rotation)

## 📈 Exemple de Session Complète

```
╔══════════════════════════════════════════════════════════╗
║      INTERACTIVE INTEGRATION TEST                       ║
║      With Real Stepper Motors                           ║
╚══════════════════════════════════════════════════════════╝

Motors initialized and enabled
Arm lengths: L1=150.0 mm, L2=150.0 mm
Max reach: 300.0 mm

Initial position set
Current position: (150.00, 150.00) mm
Current angles: θ1=45.00°, θ2=30.00°

Commands:
  x,y          - Move to position (e.g., '200,150')
  move x,y     - Same as above
  home         - Move to home position (0,0)
  pos          - Show current position and angles
  test         - Run test sequence
  help         - Show this help

═══════════════════════════════════════════════════════════
Ready for commands. Enter coordinates or 'help' for commands.
═══════════════════════════════════════════════════════════

> 200,150
═══════════════════════════════════════════════════════════
MOVING TO TARGET
═══════════════════════════════════════════════════════════
From: (150.00, 150.00) mm
To: (200.00, 150.00) mm

📊 Interpolation: 25 points generated
...
✅ Movement command completed!

> pos
Current position: (200.00, 150.00) mm
Current angles: θ1=60.00°, θ2=40.00°
Motor 1 angle: 60.00°
Motor 2 angle: 40.00°
Motor 1 moving: NO
Motor 2 moving: NO

> home
...
✅ Movement command completed!
```

## 🎓 Comprendre les Résultats

- **Point#** : Numéro du point dans la séquence d'interpolation
- **X, Y** : Coordonnées cartésiennes en millimètres
- **θ1, θ2** : Angles des joints en degrés
- **Status** : ✅ = vérification round-trip réussie, ❌ = échec

Le système vérifie automatiquement que chaque point peut être atteint avec les angles calculés !
