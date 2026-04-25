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
