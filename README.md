# Système de Détection d'Intrusion IoT

> Un gardien automatique qui surveille un réseau IoT, détecte les attaques, allume une LED et envoie une notification au dashboard.

---

## 1. Vue d'ensemble

L'idée est simple : un ESP32 mesure la température et envoie les données via WiFi. Un Raspberry Pi surveille tout le trafic en temps réel. Si quelqu'un attaque le réseau, le système bloque l'attaquant automatiquement, allume la LED rouge et envoie une alerte au dashboard.

Trois domaines sont couverts : l'électronique (capteurs et actionneurs), les réseaux (protocole MQTT) et la cybersécurité (détection d'intrusion automatique).

---

## 2. Architecture des composants

```
[DHT11 - Capteur]
       |
       ▼
[ESP32] ──── WiFi ────▶ [Mosquitto Broker]
[LED rouge]                     |
       ▲                        ▼
       |              [Moteur C/C++ - Détection]
       |                 /              \
       |           [iptables]     [Node-RED Dashboard]
       |           Bloque IP       Notification alerte
       |                \
       └─────────────────▶ MQTT security/alert
```

---

## 3. Rôle de chaque composant

| Composant | Rôle |
|-----------|------|
| **ESP32** | Se connecte au WiFi, lit le DHT11, envoie les données via MQTT, allume la LED si alerte |
| **DHT11** | Capteur de température et humidité — représente le côté capteur du système |
| **LED rouge** | S'allume quand une attaque est détectée — c'est l'actionneur physique |
| **Mosquitto** | Broker MQTT — reçoit tous les messages et les redistribue |
| **Moteur C/C++** | Analyse le trafic MQTT et décide si c'est une attaque ou non |
| **iptables** | Bloque automatiquement l'adresse IP de l'attaquant |
| **Node-RED** | Affiche une notification d'alerte sur un dashboard web |

---

## 4. Fonctionnement

### Situation normale

1. Le DHT11 mesure la température et l'humidité.
2. L'ESP32 publie ces valeurs via MQTT toutes les 5 secondes.
3. Le Raspberry Pi reçoit et surveille les messages.
4. Tout est calme — la LED reste éteinte, le dashboard affiche les données.

### Quand une attaque se produit

1. Un attaquant envoie des centaines de messages (DoS) ou tente de forcer le mot de passe (brute force).
2. Le moteur C/C++ détecte l'anomalie.
3. Il ordonne à iptables de bloquer l'IP de l'attaquant.
4. Il publie une alerte sur le topic MQTT `security/alert`.
5. L'ESP32 reçoit l'alerte → la LED rouge s'allume.
6. Node-RED affiche la notification sur le dashboard.
7. L'événement est enregistré dans un fichier log.

---

## 5. Types d'attaques détectées

| Attaque | Ce qui se passe | Comment c'est détecté |
|---------|-----------------|----------------------|
| **DoS** | Flood de messages pour saturer le réseau | Trop de messages par seconde |
| **Brute Force** | Essai de multiples mots de passe | Trop d'échecs de connexion |
| **Rogue Device** | Appareil inconnu tente de se connecter | ID non reconnu dans la liste blanche |

---

## 6. Matériel nécessaire

- ESP32 Development Board
- Capteur DHT11 (température + humidité)
- LED rouge + résistance 220Ω
- Raspberry Pi (avec Raspberry Pi OS)

---

## 7. Logiciels utilisés

- **Arduino IDE** — pour programmer l'ESP32
- **Mosquitto** — broker MQTT sur le Raspberry Pi
- **C / C++** — moteur de détection
- **iptables** — pare-feu Linux
- **Node-RED** — dashboard de notifications

---

## 8. Résumé

Ce projet est un système IoT complet avec trois aspects essentiels :

- **Physique** — un capteur (DHT11) qui mesure et une LED qui réagit
- **Réseau** — le protocole MQTT qui fait communiquer tous les composants
- **Sécurité** — détection automatique des attaques et réponse immédiate

La particularité du projet : la réponse est **active et automatique** — sans intervention humaine, le système détecte, bloque et alerte en temps réel.
