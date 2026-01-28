# Guide des Tests Unitaires

Ce document explique comment exécuter les tests unitaires pour vérifier le bon fonctionnement du système SCARA Robot.

## Structure des Tests

Les tests sont organisés dans le répertoire `src/test/` :

```
src/test/
├── TestRunner.h/.cpp       # Framework de test
├── RunTests.h/.cpp         # Point d'entrée principal
├── TestTypes.h/.cpp       # Tests des structures de données
├── TestKinematics.h/.cpp  # Tests de cinématique
├── TestPlanner.h/.cpp     # Tests du planificateur
└── TestStepperMotor.h/.cpp # Tests des moteurs (simulation)
```

## Comment Exécuter les Tests

### Méthode 1 : Via Configuration (Recommandé)

1. Ouvrez `src/Config.h`
2. Changez la ligne :
   ```cpp
   #define RUN_UNIT_TESTS false
   ```
   en :
   ```cpp
   #define RUN_UNIT_TESTS true
   ```
3. Compilez et téléversez le code sur l'ESP32
4. Ouvrez le moniteur série (115200 baud)
5. Les tests s'exécuteront automatiquement au démarrage

### Méthode 2 : Via Serial Monitor

Si vous préférez garder `RUN_UNIT_TESTS false`, vous pouvez ajouter une commande série pour déclencher les tests.

## Tests Disponibles

### 1. Tests Types (`TestTypes`)
- ✅ Constructeurs par défaut
- ✅ Constructeurs paramétrés
- ✅ Opérateurs d'égalité
- ✅ Initialisation des structures

### 2. Tests Kinematics (`TestKinematics`)
- ✅ Cinématique directe (angles → position)
  - Angles à 0°
  - Angles à 90°
  - Angles à 180°
- ✅ Cinématique inverse (position → angles)
  - Position droite (0°)
  - Position verticale (90°)
  - Chemin circulaire
- ✅ Vérification de l'espace de travail
  - Points accessibles
  - Points hors portée
  - Cas limites
- ✅ Tests aller-retour (round-trip)
  - Simple
  - Multiples angles

### 3. Tests Planner (`TestPlanner`)
- ✅ Planification de trajectoire simple
- ✅ Distances courtes et longues
- ✅ Variation de vitesse
- ✅ Intervalle d'interpolation
- ✅ Cas limites (même point, lignes verticales/horizontales)
- ✅ Calcul de distance

### 4. Tests StepperMotor (`TestStepperMotor`)
- ✅ Initialisation
- ✅ Enable/Disable
- ✅ Conversion angle ↔ steps
- ✅ Mouvement vers angle
- ✅ État de mouvement
- ✅ Configuration de vitesse

## Interprétation des Résultats

### Format de Sortie

```
╔══════════════════════════════════════════════════════════╗
║         ESP32 SCARA ROBOT - UNIT TESTS                   ║
╚══════════════════════════════════════════════════════════╝

========================================
TEST SUITE: TYPES
========================================
  [TEST] Point2D: Default constructor... PASS
  [TEST] Point2D: Parameterized constructor... PASS
  ...

========================================
TEST RESULTS
========================================
Total:  25
Passed: 25
Failed: 0

✅ ALL TESTS PASSED!
========================================
```

### Codes de Statut

- **PASS** : Le test a réussi ✅
- **FAIL** : Le test a échoué ❌
  - Les détails de l'échec sont affichés avec les valeurs attendues vs réelles

## Ajouter de Nouveaux Tests

### Exemple : Ajouter un test dans TestKinematics

1. Ajoutez la déclaration dans `TestKinematics.h` :
   ```cpp
   static bool testMonNouveauTest();
   ```

2. Implémentez le test dans `TestKinematics.cpp` :
   ```cpp
   bool TestKinematics::testMonNouveauTest() {
       Kinematics kin(150.0f, 150.0f);
       TestRunner runner(false);
       
       // Votre logique de test ici
       return runner.assertTrue(condition);
   }
   ```

3. Ajoutez l'appel dans `runAllTests()` :
   ```cpp
   runner.runTest("Mon nouveau test", testMonNouveauTest);
   ```

## Débogage des Tests

### Activer les Messages de Debug

Dans `Config.h`, activez les flags de debug :
```cpp
#define DEBUG_KINEMATICS true
#define DEBUG_PLANNER true
#define DEBUG_MOTOR true
```

### Vérifier les Assertions

Si un test échoue, le framework affiche :
- La valeur attendue
- La valeur obtenue
- La différence (pour les nombres flottants)
- Un message optionnel

## Tests avec Matériel Réel

⚠️ **Note importante** : Les tests actuels sont conçus pour fonctionner sans matériel réel. Pour tester avec les moteurs connectés :

1. Connectez les moteurs selon `Config.h`
2. Modifiez les tests pour inclure des délais réels
3. Ajoutez des vérifications de position réelle (encodeurs, etc.)

## Prochaines Étapes

Après avoir exécuté les tests unitaires :

1. ✅ Vérifiez que tous les tests passent
2. 🔧 Corrigez les échecs éventuels
3. 🧪 Testez l'intégration complète (sans mode test)
4. 🤖 Testez avec le matériel réel

## Support

Si vous rencontrez des problèmes :
- Vérifiez que `RUN_UNIT_TESTS` est bien défini
- Vérifiez la vitesse du port série (115200 baud)
- Consultez les messages d'erreur dans le Serial Monitor
