# Mosquitto — Le broker MQTT

## C'est quoi Mosquitto ?

Imagine une **boîte aux lettres centrale** dans ton réseau.

- L'ESP32 **dépose** un message dedans (température, humidité)
- Le moteur C/C++ **lit** ce qui arrive dans la boîte
- Node-RED **lit** aussi et affiche sur le dashboard

Cette boîte aux lettres, c'est **Mosquitto**.  
Sans lui, personne ne peut parler à personne.

---

## Le vocabulaire MQTT

| Mot | Signification simple |
|-----|----------------------|
| **Broker** | La boîte aux lettres centrale = Mosquitto |
| **Publisher** | Celui qui dépose un message (ex: ESP32) |
| **Subscriber** | Celui qui lit les messages (ex: moteur C, Node-RED) |
| **Topic** | L'adresse du message (ex: `sensor/temperature`) |
| **Port 1883** | La porte d'entrée de Mosquitto (non chiffré) |

---

## Comment ça marche dans ce projet

```
ESP32 publie sur "sensor/data"
        ↓
   [ Mosquitto ]        ← tourne sur le Raspberry Pi
        ↓
  ┌─────┴──────┐
Moteur C    Node-RED
(détection) (dashboard)
```

---

## Installation sur Raspberry Pi

```bash
# 1. Installer Mosquitto
sudo apt update
sudo apt install mosquitto mosquitto-clients -y

# 2. Le démarrer automatiquement au boot
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# 3. Vérifier qu'il tourne
sudo systemctl status mosquitto
```

---

## Configuration minimale

Créer le fichier de config :

```bash
sudo nano /etc/mosquitto/conf.d/projet.conf
```

Contenu du fichier :

```
# Écouter sur le port 1883
listener 1883

# Autoriser les connexions sans mot de passe (pour les tests)
allow_anonymous true
```

Redémarrer après la config :

```bash
sudo systemctl restart mosquitto
```

---

## Tester que ça marche

Ouvre **deux terminaux** sur le Raspberry Pi.

**Terminal 1 — écouter les messages :**
```bash
mosquitto_sub -h localhost -t "sensor/data"
```

**Terminal 2 — envoyer un message test :**
```bash
mosquitto_pub -h localhost -t "sensor/data" -m "temperature:25"
```

Si le Terminal 1 affiche `temperature:25` → Mosquitto fonctionne ✅

---

## Les topics de ce projet

| Topic | Qui publie | Qui écoute |
|-------|-----------|------------|
| `sensor/data` | ESP32 | Moteur C, Node-RED |
| `security/alert` | Moteur C | ESP32 (allume LED), Node-RED |

---

## Résumé

```
Mosquitto = le postier du réseau IoT
Il reçoit les messages et les redistribue
à tous ceux qui sont abonnés au bon topic
```
