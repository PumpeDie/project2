#!/usr/bin/env python3
import json
import time
from collections import deque
import paho.mqtt.client as mqtt

BROKER_HOST = "localhost"
BROKER_PORT = 1883

TOPIC_RAW = "factory/machine1/vibration/raw"
TOPIC_SPOOFED = "factory/machine1/vibration/spoofed"

DEVICE_ID_EXPECTED = "watch01"

# MODE = "vulnerable"   # accepte la dernière valeur reçue, peu importe la source
MODE = "secure"         # n'accepte que raw + device_id attendu + checks simples

last_raw = None
last_spoofed = None
decision = None

recent_timestamps = deque(maxlen=10)

def classify(vibration: float) -> str:
    if vibration < 1.0:
        return "normal"
    elif vibration < 2.0:
        return "warning"
    return "critical"

def source_is_valid(topic: str, data: dict) -> bool:
    if topic != TOPIC_RAW:
        return False
    if data.get("device_id") != DEVICE_ID_EXPECTED:
        return False
    if "timestamp" not in data or "vibration" not in data:
        return False
    return True

def value_is_consistent(data: dict) -> bool:
    try:
        ts = int(data["timestamp"])
        vib = float(data["vibration"])
    except (ValueError, TypeError):
        return False

    now = int(time.time())

    # timestamp trop ancien ou trop dans le futur
    if ts < now - 60 or ts > now + 10:
        return False

    # valeur absurde
    if vib < 0 or vib > 100:
        return False

    # anti-rejeu simple
    if ts in recent_timestamps:
        return False
    recent_timestamps.append(ts)

    return True

def render():
    print("\n" + "=" * 60)
    print(f"MODE: {MODE}")
    print(f"RAW     : {last_raw}")
    print(f"SPOOFED : {last_spoofed}")
    print(f"DECISION: {decision}")
    print("=" * 60)

def update_decision(topic: str, data: dict):
    global last_raw, last_spoofed, decision

    try:
        vib = float(data["vibration"])
    except (KeyError, ValueError, TypeError):
        print("[!] Message ignoré: vibration invalide")
        return

    state = classify(vib)

    if topic == TOPIC_RAW:
        last_raw = {"vibration": vib, "state": state, "device_id": data.get("device_id"), "timestamp": data.get("timestamp")}
    elif topic == TOPIC_SPOOFED:
        last_spoofed = {"vibration": vib, "state": state, "device_id": data.get("device_id"), "timestamp": data.get("timestamp")}

    if MODE == "vulnerable":
        decision = {
            "source": "last_message",
            "accepted_topic": topic,
            "vibration": vib,
            "state": state,
        }
    else:
        if source_is_valid(topic, data) and value_is_consistent(data):
            decision = {
                "source": "validated_raw",
                "accepted_topic": topic,
                "vibration": vib,
                "state": state,
            }
        else:
            print(f"[REJECTED] topic={topic} payload={data}")

    render()

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"[MQTT] Connecté au broker, code={reason_code}")
    client.subscribe(TOPIC_RAW)
    client.subscribe(TOPIC_SPOOFED)
    print(f"[MQTT] Abonné à {TOPIC_RAW}")
    print(f"[MQTT] Abonné à {TOPIC_SPOOFED}")

def on_message(client, userdata, msg):
    print(f"[RX] {msg.topic} -> {msg.payload.decode(errors='ignore')}")
    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        print("[!] JSON invalide")
        return

    update_decision(msg.topic, data)

def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER_HOST, BROKER_PORT, 60)
    client.loop_forever()

if __name__ == "__main__":
    main()
