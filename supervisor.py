#!/usr/bin/env python3
import json
import os
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
        with open("usine_data.csv", "a") as f:
            f.write(f"{data.get('timestamp')},{vibration}\n")
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
