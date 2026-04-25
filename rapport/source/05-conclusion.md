# Conclusion

## Bilan de la Démonstration

Ce projet a démontré avec succès la faisabilité d'une attaque en chaîne (Chained Exploit) dans un environnement industriel simulé, de bout en bout. L'exploitation d'une seule faille mémoire bas niveau sur le capteur (buffer overflow dans le firmware de la T-Watch) a servi de tremplin furtif pour compromettre l'intégrité du serveur d'analyse central (Raspberry Pi).

La progression de l'attaque peut être résumée ainsi :

1. Un attaquant connecté au réseau local découvre le port UDP d'administration du capteur
2. L'envoi d'un paquet malveillant surdimensionné exploite un débordement de buffer
3. L'identifiant du capteur est corrompu, permettant au firmware de charger du code malveillant
4. Une tentative de maintien de persistance via injection shell est interceptée
5. Le malware déployé modifie systématiquement les données de vibration pour masquer les anomalies

Cette démonstration confirme qu'une simple faille de gestion mémoire dans un composant OT isolé peut servir de point d'entrée pour compromettre l'ensemble de l'infrastructure informatique superviseur.

## La Leçon Principale : Rejet de la « Confiance Implicite »

La vulnérabilité fondamentale de ce système réside dans le paradigme de la **confiance implicite**. À tous les niveaux :

- Le firmware de la T-Watch fait confiance à la taille signalée par le protocole réseau sans la valider
- Le script Python superviseur fait confiance aux données en provenance du capteur sans authentification
- Le système d'exploitation Raspberry Pi fait confiance aux appels système générés sans les valider d'abord

Cette confiance était autrefois acceptable dans les réseaux industriels isolés et fermés. Aujourd'hui, avec la convergence de l'IT et de l'OT, la frontière entre un "capteur interne de confiance" et un "attaquant connecté" n'existe plus. Une faille sur l'un compromet immanquablement l'autre.

L'enseignement critique est le suivant : **chaque couche doit valider les données qu'elle reçoit**, indépendamment de la source supposée. Pas de raccourci, pas de dérogation.

## Vers une Sécurité Intégrée : Secure by Design

Ces vulnérabilités auraient pu être évitées à peu de frais si la sécurité avait été intégrée dès la conception (Secure by Design) :

- Limiter la taille des lectures UDP à la capacité réelle du buffer
- Utiliser des bibliothèques sécurisées pour l'exécution de commandes (pas de shell interprété)
- Mettre en place une authentification et un chiffrement sur le protocole MQTT dès le départ
- Valider et filtrer toutes les entrées externes

Or, corriger ces failles a posteriori impose un cycle de redéploiement complet, des interruptions de service et des coûts exponentiels de maintenance.

## Perspectives : Industrie 4.0 et la Sécurité Opérationnelle

Avec la multiplication des objets connectés (IoT) dans les usines intelligentes, le statu quo n'est plus tenable. La sécurité physique des installations dépend désormais de la sécurité informatique des capteurs, des protocoles et des analyseurs.

Les prérequis absolus pour les déploiements futurs deviennent :

- **Segmentation réseau stricte** : Isoler les VLANs des capteurs (OT) des réseaux informatiques standard (IT)
- **Authentification et chiffrement obligatoires** : Aucun protocole en clair, aucun accès non authentifié
- **Audit et surveillance continus** : Détecter les anomalies et les comportements suspects en temps réel
- **Conformité de code** : Révisions de sécurité régulières des firmwares et des scripts superviseurs
- **Mise à jour maîtrisée** : Planifier les correctifs sans interrompre la production (Hot Swapping, Load Balancing)

Ce projet illustre que l'absence d'une seule de ces mesures suffit à compromettre l'ensemble du système. La sécurité de l'Industrie 4.0 est une chaîne dont chaque maillon est critique.


# Annexes {-}

## Code LilyGo T-Watch S3

```{.cpp .numberLines}
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <string.h>

// --- Configuration Réseau ---
const char* ssid = "t-450";
const char* password = "qw3rtyu1";
const char* mqtt_server = "10.42.0.33";
const int udp_port = 3333;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
WiFiUDP udp;

// --- Interface Graphique (LVGL) ---
lv_obj_t *label_title;
lv_obj_t *label_ip;
lv_obj_t *label_mqtt;
lv_obj_t *label_id;

// --- Memory Layout (Vulnérabilité) ---
struct VulnerableMemory {
    char rx_buffer[32];
    char device_id[128];
};

VulnerableMemory memory_block;
unsigned long last_publish = 0;
// --- État de la machine (Simulation) ---
bool is_critical = false;

// Fonction appelée quand on touche le bouton sur l'écran
static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        is_critical = !is_critical; // On inverse l'état
        
        lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
        lv_obj_t * target = (lv_obj_t *)lv_event_get_target(e);
        
        if(is_critical) {
            lv_label_set_text(label, "SIMU: DANGER");
            lv_obj_set_style_bg_color(target, lv_palette_main(LV_PALETTE_RED), 0);
        } else {
            lv_label_set_text(label, "SIMU: NORMAL");
            lv_obj_set_style_bg_color(target, lv_palette_main(LV_PALETTE_BLUE), 0);
        }
    }
}

void setup_wifi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        lv_timer_handler(); // Maintient l'interface active pendant la connexion
        delay(100);
    }
    
    // Mise à jour de l'IP sur l'écran
    String ipStr = "IP: " + WiFi.localIP().toString();
    lv_label_set_text(label_ip, ipStr.c_str());
}

void reconnect_mqtt() {
    if (!mqtt_client.connected()) {
        lv_label_set_text(label_mqtt, "MQTT: Déconnecté");
        if (mqtt_client.connect("WatchVulnerableClient")) {
            lv_label_set_text(label_mqtt, "MQTT: Connecté");
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialisation matérielle et graphique
    instance.begin();
    beginLvglHelper(instance);
    instance.setBrightness(128);

    // Construction de l'interface
    label_title = lv_label_create(lv_screen_active());
    lv_label_set_text(label_title, "Capteur Vibration");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

    label_ip = lv_label_create(lv_screen_active());
    lv_label_set_text(label_ip, "IP: Connexion...");
    lv_obj_align(label_ip, LV_ALIGN_TOP_MID, 0, 40);

    label_mqtt = lv_label_create(lv_screen_active());
    lv_label_set_text(label_mqtt, "MQTT: En attente");
    lv_obj_align(label_mqtt, LV_ALIGN_TOP_MID, 0, 70);

    label_id = lv_label_create(lv_screen_active());
    lv_obj_set_width(label_id, 220); // Limite la largeur pour forcer le retour à la ligne
    lv_label_set_long_mode(label_id, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label_id, "ID: Initialisation...");
    lv_obj_align(label_id, LV_ALIGN_TOP_MID, 0, 100);

    // --- Bouton tactile de simulation d'alerte ---
    lv_obj_t * btn = lv_btn_create(lv_screen_active());
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20); // Placé en bas de l'écran
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "SIMU: NORMAL");
    lv_obj_center(btn_label);

    // Lier l'événement tactile à notre fonction
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, btn_label);

    // Initialisation de la mémoire vulnérable
    memset(&memory_block, 0, sizeof(VulnerableMemory));
    strcpy(memory_block.device_id, "watch01");
    
    // Affichage de l'ID initial
    String idStr = "ID: " + String(memory_block.device_id);
    lv_label_set_text(label_id, idStr.c_str());

    setup_wifi();
    mqtt_client.setServer(mqtt_server, 1883);
    udp.begin(udp_port);
}

void loop() {
    // TICK GRAPHIC (Indispensable pour rafraîchir l'écran)
    lv_timer_handler();

    if (WiFi.status() == WL_CONNECTED) {
        reconnect_mqtt();
    }
    mqtt_client.loop();

    // 1. Écoute UDP et Buffer Overflow
    int packetSize = udp.parsePacket();
    if (packetSize) {
        // Le Buffer Overflow : lecture sans limite de taille
        udp.read(memory_block.rx_buffer, packetSize);
        
        // On affiche immédiatement le nouvel état de device_id sur l'écran
        String idStr = "ID: " + String(memory_block.device_id);
        lv_label_set_text(label_id, idStr.c_str());
    }

    // 2. Publication MQTT (Toutes les 5 secondes)
    unsigned long now = millis();
    if (now - last_publish >= 5000) {
        last_publish = now;

        char payload[300]; 
        float vibration_val;
        if (is_critical) {
            // Machine en surrégime : vibrations fortes (entre 1.30 et 1.70)
            vibration_val = 1.50 + (random(-20, 21) / 100.0); 
        } else {
            // Machine saine : vibrations normales (entre 0.70 et 0.90)
            vibration_val = 0.80 + (random(-10, 11) / 100.0);
        }
        
        snprintf(payload, sizeof(payload), "{\"device_id\": \"%s\", \"vibration\": %.2f}", memory_block.device_id, vibration_val);
        if (mqtt_client.connected()) {
            mqtt_client.publish("capteurs/vibration", payload);
        }

    }
    
    delay(2);
}
```

## Code du superviseur sur la Raspberry Pi

```{.python .numberLines}
#!/usr/bin/env python3
import json
import os
import datetime
import paho.mqtt.client as mqtt

# --- Configuration ---
BROKER_HOST = "localhost"
BROKER_PORT = 1883
TOPIC = "capteurs/vibration"
EXPECTED_DEVICE = "watch01"

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"[MQTT] Connecté au broker, abonnement au topic: {TOPIC}")
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    payload_str = msg.payload.decode(errors='ignore')
    print(f"\n[RX] {msg.topic} -> {payload_str}")
    
    try:
        data = json.loads(payload_str)
    except json.JSONDecodeError:
        print("[!] JSON invalide, message ignoré.")
        return

    device_id = data.get("device_id", "inconnu")
    vibration = data.get("vibration", 0.0)

    # 1. Validation métier (Le système se croit sécurisé)
    if device_id == EXPECTED_DEVICE:
        print(f"[VALIDÉ] Appareil légitime. Vibration: {vibration}")
        
        # Récupération du timestamp
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        with open("usine_data.csv", "a") as f:
            f.write(f"{timestamp},{vibration}\n")
        # Traitement normal des données de l'usine...
    else:
        # 2. Rejet et Journalisation (LA VULNÉRABILITÉ)
        print(f"[REJETÉ] ID suspect détecté : {device_id}")
        print("[AUDIT] Écriture dans les logs de sécurité...")
        
        # Le script utilise os.system en concaténant directement l'entrée utilisateur
        # sans aucune désinfection (sanitization). C'est ici que le reverse shell s'active.
        commande_log = f"echo 'Tentative rejetée du device : {device_id}' >> audit_rejets.log"
        
        os.system(commande_log)

def main():
    # Utilisation de l'API v2 requise par les versions récentes de paho-mqtt
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    print("=== Superviseur Industriel Démarré ===")
    print(f"En attente des données de {EXPECTED_DEVICE}...\n")
    
    try:
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nArrêt du superviseur.")
        client.disconnect()

if __name__ == "__main__":
    main()
```
