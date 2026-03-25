# 🔐 Système de Détection d'Intrusion IoT — Guide Complet

> Un gardien automatique qui surveille un réseau IoT, détecte les attaques et réagit physiquement en temps réel.

---

## 💡 L'idée en une phrase

Tu construis un petit réseau IoT avec un **ESP32**, tu le surveilles depuis un **Raspberry Pi**, et quand quelqu'un attaque — le système **se défend tout seul** et te prévient via une LED rouge et un buzzer.

---

## 🧠 C'est quoi MQTT ?

MQTT est un **système de messagerie** conçu pour les petits appareils connectés. Pense à lui comme un **tableau d'affichage public** avec 3 acteurs :

| Acteur | Rôle |
|--------|------|
| **Publisher** | Envoie un message (ex : ESP32 publie "température = 25°C") |
| **Broker** | Reçoit et redistribue les messages (Mosquitto sur le Raspberry Pi) |
| **Subscriber** | Écoute les messages qui l'intéressent (Raspberry Pi écoute tout) |

### Le concept de "topic"

Un topic c'est comme une **chaîne de télé**. Chaque message est publié sur un sujet précis :

```
maison/temperature    →  "25.3"
maison/humidite       →  "60%"
security/alert        →  "ATTACK_DETECTED"
```

Le Raspberry Pi s'abonne à `#` (le joker) pour recevoir **absolument tout**.

---

## 🔩 Rôle de chaque composant

| Composant | Ce qu'il fait — en simple |
|-----------|---------------------------|
| **ESP32** | Se connecte au WiFi et envoie les données du capteur via MQTT |
| **DHT11** | Mesure la température et l'humidité — c'est le capteur |
| **LED rouge** | S'allume quand une attaque est détectée — actionneur visuel |
| **Buzzer** | Émet un bip sonore lors d'une attaque — actionneur sonore |
| **Mosquitto** | Reçoit tous les messages MQTT et les redistribue — le facteur |
| **Moteur C/C++** | Lit les messages et décide si c'est une attaque ou pas |
| **iptables** | Bloque automatiquement l'adresse IP de l'attaquant |
| **Fichier log** | Note tout ce qui s'est passé pour l'analyse après |

---

## 🔄 Le scénario complet en 6 étapes

```
1. DHT11 mesure 24°C
        ↓
2. ESP32 publie le message MQTT → Mosquitto (broker)
        ↓
3. Un attaquant envoie 1000 messages/seconde → Mosquitto
        ↓
4. Le moteur C/C++ analyse : "1000 msg/sec → ATTAQUE !"
        ↓
5. iptables bloque l'IP + publie "security/alert" via MQTT
        ↓
6. ESP32 reçoit l'alerte → LED clignote + Buzzer sonne
```

---

## 🚨 Les 4 types d'attaques détectées

### 1. Brute Force
L'attaquant essaie des milliers de mots de passe pour se connecter au broker MQTT.
- **Détection** : trop de tentatives de connexion échouées depuis la même IP.

### 2. DoS (Denial of Service)
L'attaquant envoie des milliers de messages pour saturer le réseau.
- **Détection** : fréquence de messages dépasse un seuil (ex : > 100 msg/sec).

### 3. Rogue Device
Un appareil inconnu tente de se connecter au broker.
- **Détection** : l'ID du device n'est pas dans la liste blanche.

### 4. Trafic anormal
Volume de données inhabituellement élevé.
- **Détection** : taille ou fréquence des messages dépasse les limites normales.

---

## ⚡ Réponse automatique aux attaques

Quand une attaque est détectée, le moteur C/C++ fait **3 choses automatiquement** :

```bash
# 1. Blocage de l'IP attaquante
sudo iptables -A INPUT -s <attacker_ip> -j DROP

# 2. Publication d'une alerte MQTT (pour la LED et le buzzer)
mosquitto_publish -t "security/alert" -m "ATTACK_DETECTED"

# 3. Écriture dans le fichier de log
echo "[ALERTE] Attaque détectée depuis <ip>" >> /var/log/iot_security.log
```

---

## 🛠️ Architecture matérielle

```
[DHT11] ──┐
           ├──> [ESP32] ──WiFi──> [Mosquitto Broker]
[PIR]  ──┘                              │
                                        ↓
                              [Moteur C/C++ - Raspberry Pi]
                               /              \
                          [iptables]    [Publication alerte]
                               │                │
                          Bloque IP         [MQTT]
                                               │
                               ┌───────────────┘
                               ↓
                    [ESP32 reçoit l'alerte]
                       /          \
                  [LED rouge]   [Buzzer]
```

---

## 💻 Code clé — ESP32 (Arduino C++)

### Publication des données capteur

```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHT_PIN    4
#define LED_PIN    2
#define BUZZER_PIN 5
#define DHT_TYPE   DHT11

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  dht.begin();
  connectWifi();
  client.setServer("IP_RASPBERRY", 1883);
  client.setCallback(onMqttMessage);  // écouter les alertes
}

void loop() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // Publier les données
  client.publish("maison/temperature", String(temp).c_str());
  client.publish("maison/humidite",    String(hum).c_str());

  client.loop();
  delay(5000);
}
```

### Réception des alertes (actionneurs)

```cpp
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, "security/alert") == 0) {
    // Actionneur visuel : LED rouge
    digitalWrite(LED_PIN, HIGH);
    // Actionneur sonore : Buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    delay(3000);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}
```

---

## 💻 Code clé — Raspberry Pi (C/C++)

### Détection DoS et blocage automatique

```c
#include <mosquitto.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEUIL_DOS     100   // messages/seconde
#define SEUIL_BRUTEFORCE 5  // tentatives échouées

typedef struct {
    char ip[50];
    int  msg_count;
    int  fail_count;
    time_t last_seen;
} DeviceStats;

void on_message(struct mosquitto *mosq, void *userdata,
                const struct mosquitto_message *msg) {

    DeviceStats *stats = (DeviceStats *)userdata;
    time_t now = time(NULL);

    // Incrémenter le compteur de messages
    stats->msg_count++;

    // Vérifier si DoS : trop de messages en 1 seconde
    if (difftime(now, stats->last_seen) <= 1.0) {
        if (stats->msg_count > SEUIL_DOS) {
            bloquer_ip(stats->ip);
            publier_alerte(mosq, "DoS détecté");
        }
    } else {
        // Réinitialiser le compteur chaque seconde
        stats->msg_count = 0;
        stats->last_seen = now;
    }
}

void bloquer_ip(const char *ip) {
    char commande[200];
    snprintf(commande, sizeof(commande),
             "sudo iptables -A INPUT -s %s -j DROP", ip);
    system(commande);

    // Écrire dans le log
    FILE *log = fopen("/var/log/iot_security.log", "a");
    fprintf(log, "[BLOQUE] IP: %s\n", ip);
    fclose(log);
}

void publier_alerte(struct mosquitto *mosq, const char *message) {
    mosquitto_publish(mosq, NULL, "security/alert",
                      strlen(message), message, 0, false);
}
```

---

## 🧰 Technologies utilisées

### Matériel
- ESP32 Development Board
- Raspberry Pi (avec Raspberry Pi OS)
- Capteur DHT11 (température + humidité)
- Capteur PIR (mouvement) — optionnel
- LED rouge + résistance 220Ω
- Buzzer actif

### Logiciel
- **Arduino IDE** — programmation de l'ESP32
- **Mosquitto** — broker MQTT
- **C / C++** — moteur de détection sur Raspberry Pi
- **libmosquitto** — bibliothèque MQTT pour C
- **iptables** — pare-feu Linux pour bloquer les IP

---

## 📌 Ce qui rend ce projet original

La plupart des systèmes similaires font uniquement de la **surveillance passive** (ils alertent mais n'agissent pas). Ici la réponse est **active et automatique** :

- Dès la détection, le pare-feu bloque l'attaquant sans intervention humaine.
- L'alerte est physique (LED + buzzer), pas seulement un message sur écran.
- Le cycle est complet : **capteur → réseau → détection → actionneur**.

---

## ✅ Checklist du projet

- [ ] ESP32 connecté au WiFi et au broker MQTT
- [ ] DHT11 publie température et humidité toutes les 5 secondes
- [ ] Raspberry Pi fait tourner Mosquitto
- [ ] Moteur C/C++ compilé et abonné à tous les topics (`#`)
- [ ] Détection DoS fonctionnelle (seuil messages/sec)
- [ ] Détection brute force fonctionnelle (seuil tentatives)
- [ ] Détection rogue device fonctionnelle (liste blanche)
- [ ] Blocage automatique iptables testé
- [ ] LED rouge réagit aux alertes MQTT
- [ ] Buzzer réagit aux alertes MQTT
- [ ] Fichier log créé et rempli correctement
