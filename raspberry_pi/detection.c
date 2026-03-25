#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mosquitto.h>

/* ─────────────────────────────────────────
   CONFIGURATION
───────────────────────────────────────── */
#define BROKER_HOST       "localhost"
#define BROKER_PORT       1883
#define LOG_FILE          "/var/log/intrusion.log"

/* Seuils de détection */
#define DOS_SEUIL         10   /* messages par seconde = attaque DoS */
#define DOS_FENETRE       1    /* fenêtre de temps en secondes       */

/* Appareils autorisés (liste blanche) */
const char *appareils_autorises[] = {
    "esp32_capteur",
    "dashboard_nodered",
    NULL   /* marqueur de fin */
};

/* ─────────────────────────────────────────
   VARIABLES GLOBALES
───────────────────────────────────────── */
int    compteur_messages = 0;
time_t debut_fenetre     = 0;

/* ─────────────────────────────────────────
   ÉCRIRE DANS LE FICHIER LOG
───────────────────────────────────────── */
void ecrire_log(const char *type_attaque, const char *ip) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;

    time_t maintenant = time(NULL);
    char horodatage[64];
    strftime(horodatage, sizeof(horodatage), "%Y-%m-%d %H:%M:%S", localtime(&maintenant));

    fprintf(f, "[%s] ATTAQUE DETECTEE | Type: %s | IP: %s\n",
            horodatage, type_attaque, ip);
    fclose(f);
    printf("[LOG] %s | %s | IP: %s\n", horodatage, type_attaque, ip);
}

/* ─────────────────────────────────────────
   BLOQUER L'IP AVEC IPTABLES
───────────────────────────────────────── */
void bloquer_ip(const char *ip) {
    char commande[128];
    snprintf(commande, sizeof(commande),
             "iptables -I INPUT 1 -s %s -j DROP", ip);
    system(commande);
    printf("[FIREWALL] IP bloquee : %s\n", ip);
}

/* ─────────────────────────────────────────
   ENVOYER UNE ALERTE MQTT
───────────────────────────────────────── */
void envoyer_alerte(struct mosquitto *mosq, const char *message) {
    mosquitto_publish(mosq, NULL, "security/alert",
                      strlen(message), message, 0, false);
    printf("[ALERTE] Publiee sur security/alert : %s\n", message);
}

/* ─────────────────────────────────────────
   VÉRIFIER SI L'APPAREIL EST AUTORISÉ
───────────────────────────────────────── */
int appareil_autorise(const char *client_id) {
    for (int i = 0; appareils_autorises[i] != NULL; i++) {
        if (strcmp(client_id, appareils_autorises[i]) == 0)
            return 1; /* autorisé */
    }
    return 0; /* inconnu */
}

/* ─────────────────────────────────────────
   CALLBACK — APPELÉ À CHAQUE MESSAGE REÇU
───────────────────────────────────────── */
void on_message(struct mosquitto *mosq,
                void *userdata,
                const struct mosquitto_message *msg)
{
    /* Afficher le message reçu */
    printf("[MQTT] Topic: %s | Message: %s\n",
           msg->topic, (char *)msg->payload);

    /* ── Détection DoS ──────────────────────────────────────
       On compte combien de messages arrivent par seconde.
       Si > DOS_SEUIL messages en DOS_FENETRE secondes = DoS
    ────────────────────────────────────────────────────────── */
    time_t maintenant = time(NULL);

    if (maintenant - debut_fenetre >= DOS_FENETRE) {
        /* Nouvelle fenêtre de temps — reset du compteur */
        compteur_messages = 0;
        debut_fenetre      = maintenant;
    }

    compteur_messages++;

    if (compteur_messages > DOS_SEUIL) {
        printf("[DETECTION] Attaque DoS detectee ! (%d msg/s)\n",
               compteur_messages);

        /* Dans un vrai projet : récupérer la vraie IP source
           Ici on utilise une IP exemple pour la démonstration */
        const char *ip_attaquant = "10.0.0.66";

        bloquer_ip(ip_attaquant);
        ecrire_log("DoS", ip_attaquant);
        envoyer_alerte(mosq, "ALERTE:DoS detecte");

        /* Reset pour ne pas bloquer en boucle */
        compteur_messages = 0;
    }

    /* ── Détection Rogue Device ─────────────────────────────
       Si le topic contient un ID d'appareil inconnu = Rogue
       Format attendu : "sensor/data/esp32_capteur"
    ────────────────────────────────────────────────────────── */
    if (strncmp(msg->topic, "sensor/data/", 12) == 0) {
        const char *client_id = msg->topic + 12; /* après "sensor/data/" */

        if (!appareil_autorise(client_id)) {
            printf("[DETECTION] Appareil inconnu : %s\n", client_id);

            ecrire_log("Rogue Device", client_id);
            envoyer_alerte(mosq, "ALERTE:Appareil inconnu detecte");
        }
    }
}

/* ─────────────────────────────────────────
   CALLBACK — CONNEXION AU BROKER
───────────────────────────────────────── */
void on_connect(struct mosquitto *mosq, void *userdata, int code) {
    if (code == 0) {
        printf("[OK] Connecte au broker Mosquitto\n");
        /* S'abonner à tous les topics */
        mosquitto_subscribe(mosq, NULL, "#", 0);
        printf("[OK] Abonne a tous les topics (#)\n");
    } else {
        printf("[ERREUR] Connexion echouee (code %d)\n", code);
    }
}

/* ─────────────────────────────────────────
   MAIN
───────────────────────────────────────── */
int main(void) {
    printf("=== Moteur de Detection d'Intrusion IoT ===\n");
    printf("Connexion a %s:%d ...\n", BROKER_HOST, BROKER_PORT);

    /* Initialiser la bibliothèque mosquitto */
    mosquitto_lib_init();

    /* Créer le client */
    struct mosquitto *mosq = mosquitto_new("moteur_detection", true, NULL);
    if (!mosq) {
        fprintf(stderr, "[ERREUR] Impossible de creer le client\n");
        return 1;
    }

    /* Enregistrer les callbacks */
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    /* Se connecter au broker */
    int ret = mosquitto_connect(mosq, BROKER_HOST, BROKER_PORT, 60);
    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "[ERREUR] Connexion impossible : %s\n",
                mosquitto_strerror(ret));
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    /* Boucle principale — tourne indéfiniment */
    printf("[OK] Moteur actif — surveillance en cours...\n\n");
    mosquitto_loop_forever(mosq, -1, 1);

    /* Nettoyage (jamais atteint normalement) */
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
