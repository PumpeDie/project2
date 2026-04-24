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
    // TICK GRAPHIC
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
        
        // On affiche le nouvel état de `device_id` sur l'écran
        String idStr = "ID: " + String(memory_block.device_id);
        lv_label_set_text(label_id, idStr.c_str());
    }

    // 2. Publication MQTT toutes les 5 secondes
    unsigned long now = millis();
    if (now - last_publish >= 5000) {
        last_publish = now;

        char payload[300]; 
        snprintf(payload, sizeof(payload), "{\"device_id\": \"%s\", \"vibration\": 42.0}", memory_block.device_id);

        if (mqtt_client.connected()) {
            mqtt_client.publish("capteurs/vibration", payload);
        }
    }
    
    delay(2);
}
