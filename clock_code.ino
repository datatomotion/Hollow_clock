/************************************************************
 * Data To Motion - Horloge pas à pas avec ESP32-C3
 * Description : avance l’aiguille d’1 minute par minute réelle
 * avec correction anti-jeu (dépassement puis retour).
 * Matériel : ESP32-C3 + ULN2003 + stepper 28BYJ-48
 * Câblage : GPIO2, GPIO3, GPIO4, GPIO5 -> IN1..IN4 du ULN2003
 ************************************************************/

// --------- Paramètres moteur et horloge ---------
#define STEPS_PER_MIN 256                // Nombre de pas = 1 minute
#define MIN_PER_USEC (60000500LL)        // Durée corrigée d’une minute
#define SAFETY_MOTION (STEPS_PER_MIN)    // Mouvement de correction

int delaytime = 6;                       // Vitesse moteur (ms)

// --------- Broches ESP32-C3 ---------
int port[4] = {9, 8, 7, 6};              // GPIO2, GPIO3, GPIO4, GPIO5

// --------- Séquence moteur ---------
int seq[4][4] = {
  {  LOW,  LOW, HIGH,  LOW},  // phase 0
  {  LOW,  LOW,  LOW, HIGH},  // phase 1
  { HIGH,  LOW,  LOW,  LOW},  // phase 2
  {  LOW, HIGH,  LOW,  LOW}   // phase 3
};

// --------- Fonction rotation ---------
void rotate(int step) {
  static int phase = 0;
  int i, j;
  int delta = (step > 0) ? 1 : 3;    // sens horaire/antihoraire
  int dt = delaytime * 3;            // rampe d’accélération

  step = abs(step);                  // valeur absolue

  for (j = 0; j < step; j++) {
    phase = (phase + delta) % 4;     // nouvelle phase
    for (i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]); // activer bobine
    }
    delay(dt);
    if (dt > delaytime) dt--;        // accélération progressive
  }

  // couper alimentation après mouvement
  for (i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}

// --------- Setup ---------
void setup() {
  // Configurer les GPIO comme sorties
  for (int i = 0; i < 4; i++) {
    pinMode(port[i], OUTPUT);
    digitalWrite(port[i], LOW);
  }

  // calibration initiale
  rotate(-STEPS_PER_MIN * 2);
}

// --------- Boucle ---------
void loop() {
  static uint64_t prev_cnt = (uint64_t)-1;
  uint64_t cnt, usec;

  // temps écoulé en µs
  usec = (uint64_t)millis() * 1000ULL;

  // minutes écoulées
  cnt = usec / MIN_PER_USEC;

  if (prev_cnt == cnt) return;   // attendre nouvelle minute

  prev_cnt = cnt;

  // avancer d’une minute avec correction
  rotate(STEPS_PER_MIN + SAFETY_MOTION);
  rotate(-SAFETY_MOTION);
}
