/****************************************************
 * Programme de test moteur pas à pas pour montre
 * Auteur : Data To Motion
 ****************************************************/

// Temps d’attente (en millisecondes) entre chaque pas du moteur
#define MOTOR_DELAY 2

// Déclaration des broches utilisées pour les 4 bobines du moteur
// Ici, on utilise les sorties numériques 6, 7, 8 et 9 de l’Arduino UNO
int port[4] = {9, 8, 7, 6};

// Définition de la séquence 4 phases en mode "full step"
// Chaque ligne représente l’état (HIGH/LOW) de chaque bobine
int seq[4][4] = {
  {LOW, LOW, HIGH, LOW},   // Phase 1 : bobine 3 activée
  {LOW, LOW, LOW, HIGH},   // Phase 2 : bobine 4 activée
  {HIGH, LOW, LOW, LOW},   // Phase 3 : bobine 1 activée
  {LOW, HIGH, LOW, LOW}    // Phase 4 : bobine 2 activée
};

// Fonction pour faire tourner le moteur d’un certain nombre de pas
void rotate(int step) {
  static int phase = 0;  // Phase actuelle (mémorisée entre deux appels)
  
  // Détermination du sens de rotation :
  // Si step > 0 → sens horaire (delta = +1)
  // Si step < 0 → sens antihoraire (delta = -1, représenté par 3 car modulo 4)
  int delta = (step > 0) ? 1 : 3;  
  step = abs(step);  // On garde uniquement le nombre de pas (valeur positive)

  // Boucle pour exécuter les pas demandés
  for (int j = 0; j < step; j++) {
    // Avancer d’une phase en tenant compte du sens
    phase = (phase + delta) % 4;

    // Appliquer l’état des bobines correspondant à la phase
    for (int i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]);
    }

    // Petite pause pour laisser le moteur avancer
    delay(MOTOR_DELAY);
  }

  // Après la rotation, on coupe l’alimentation des bobines
  // Cela évite de chauffer le moteur et économise de l’énergie
  for (int i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}

// Fonction d’initialisation exécutée au démarrage
void setup() {
  // Configuration des broches comme sorties
  for (int i = 0; i < 4; i++) {
    pinMode(port[i], OUTPUT);
    digitalWrite(port[i], LOW);  // Mise à zéro initiale
  }
}

// Boucle principale
void loop() {
  // On fait tourner le moteur d’un tour complet (1000 pas environ)
  // Valeur positive → sens horaire
  rotate(1000);   
}
