/************************************************************
 * Data To Motion - Horloge pas à pas ESP32-C3 (NTP, Asia/Dubai)
 * Principe :
 * - Démarrage : on suppose l’aiguille à 12:00 (réglée à la main).
 * - Connexion WiFi + NTP (UTC+4, pas de DST) -> heure locale.
 * - À chaque seconde : on calcule la position théorique (minutes+secondes)
 *   et on déplace le moteur de l’écart minimal, avec anti-jeu.
 * Matériel : ESP32-C3 + ULN2003 + 28BYJ-48
 ************************************************************/

#include <WiFi.h>
#include <time.h>

// ---------- WiFi ----------
const char* WIFI_SSID = "GSW";
const char* WIFI_PASS = "@geona78";

// Asia/Dubai (UTC+4, no DST)
const long  GMT_OFFSET_SEC = 4 * 3600;
const int   DST_OFFSET_SEC = 0;

// ---------- Moteur / horloge ----------
#define STEPS_PER_MIN   256          // 28BYJ-48 : 2048 * 90 / 12 / 60
#define MIN_PER_USEC    (60000500LL) // si avance/retarde, ajuste finement
#define SAFETY_MOTION   (STEPS_PER_MIN)
int delaytime = 6;

// Broches (conserve ton sens correct). NOTE : sur ESP32-C3, préfère 2,3,4,5.
// Ici on respecte ton montage actuel inversé.
int port[4] = {9, 8, 7, 6};

// Séquence full-step
int seq[4][4] = {
  {  LOW,  LOW, HIGH,  LOW},
  {  LOW,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW,  LOW},
  {  LOW, HIGH,  LOW,  LOW}
};

// État moteur
static int   g_phase = 0;            // phase actuelle
static long  g_steps_now = 0;        // position actuelle en pas depuis 12:00

// ---------- Utilitaires moteur ----------
void motorWritePhase(int phase) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(port[i], seq[phase][i]);
  }
}

void rotateSteps(int steps) {
  if (steps == 0) {
    for (int i = 0; i < 4; i++) digitalWrite(port[i], LOW);
    return;
  }
  int delta = (steps > 0) ? 1 : 3;     // 3 ≡ -1 mod 4
  int count = abs(steps);
  int dt = delaytime * 3;

  for (int j = 0; j < count; j++) {
    g_phase = (g_phase + delta) % 4;
    motorWritePhase(g_phase);
    delay(dt);
    if (dt > delaytime) dt--;
  }
  // coupe le courant
  for (int i = 0; i < 4; i++) digitalWrite(port[i], LOW);

  g_steps_now += steps; // mise à jour position logique
}

// Anti-jeu : aller trop loin puis revenir
void rotateWithBacklash(int steps) {
  if (steps == 0) return;
  if (steps > 0) {
    rotateSteps(steps + SAFETY_MOTION);
    rotateSteps(-SAFETY_MOTION);
  } else {
    rotateSteps(steps - SAFETY_MOTION);
    rotateSteps(+SAFETY_MOTION);
  }
}

// Déplace vers une cible absolue en pas (depuis 12:00), en choisissant
// le chemin direct, avec anti-jeu si l’écart est significatif.
void moveToAbsolute(long target_steps) {
  long diff = target_steps - g_steps_now;
  if (diff == 0) return;

  // Seuil d’anti-jeu : applique la correction si |diff| >= 8 pas
  if (abs(diff) >= 8) rotateWithBacklash(diff);
  else                rotateSteps(diff);
}

// ---------- Temps / NTP ----------
bool waitForTime(unsigned long timeout_ms = 10000) {
  time_t now = 0;
  unsigned long start = millis();
  while ((millis() - start) < timeout_ms) {
    time(&now);
    if (now > 1700000000) return true; // heuristique : temps plausible après 2023
    delay(200);
  }
  return false;
}

// Calcule la cible en pas à partir de l’heure locale
// Aiguille des minutes qui “glisse” grâce aux secondes.
long computeTargetSteps(const tm& t) {
  int minutes_of_day = t.tm_hour * 60 + t.tm_min;  // 0..1439
  // Ajout d’une fraction de minute selon les secondes
  // -> pas “lisses” des secondes : STEPS_PER_MIN * (sec/60)
  long steps = (long)minutes_of_day * STEPS_PER_MIN
             + (long)((STEPS_PER_MIN * t.tm_sec) / 60);
  return steps;
}

// ---------- Setup ----------
void setup() {
  // Broches en sortie
  for (int i = 0; i < 4; i++) { pinMode(port[i], OUTPUT); digitalWrite(port[i], LOW); }

  // Démarrage : on suppose l’aiguille mécaniquement alignée sur 12:00.
  // g_steps_now = 0 par convention.
  g_steps_now = 0;

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);

  // NTP
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.google.com");
  waitForTime(); // on tente, même si le WiFi n’est pas prêt, la montre tournera quand même

  // Premier calage si on a déjà l’heure
  struct tm tinfo;
  if (getLocalTime(&tinfo)) {
    long target = computeTargetSteps(tinfo);
    moveToAbsolute(target);
  }
}

// ---------- Loop ----------
void loop() {
  static unsigned long last_sync_ms = 0;
  static unsigned long last_step_ms = 0;

  // Lecture de l’heure une fois par seconde ~
  if (millis() - last_step_ms >= 1000) {
    last_step_ms = millis();

    struct tm tinfo;
    if (getLocalTime(&tinfo)) {
      long target = computeTargetSteps(tinfo);
      moveToAbsolute(target);
    } else {
      // Pas encore de temps NTP -> fallback “interne” par accumulateur
      // (option : à implémenter si besoin, mais en général NTP arrive vite)
    }
  }

  // Resync NTP périodique (toutes les 6 h par ex.)
  if (millis() - last_sync_ms >= 6UL * 3600UL * 1000UL) {
    last_sync_ms = millis();
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.google.com");
  }
}
