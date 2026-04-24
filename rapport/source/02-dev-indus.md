# Développement du système industriel (Côté Ingénierie OT/IT)

L'objectif de cette section est de détailler l'architecture technique du système de surveillance vibratoire et d'expliquer les choix d'implémentation qui ont mené aux vulnérabilités actuelles. Dans un contexte d'Industrie 4.0, la convergence entre les technologies opérationnelles (OT) et informatiques (IT) crée souvent des zones de risques où la facilité de maintenance prime sur la sécurité.

## Le Capteur de Vibration (Firmware T-Watch)

Le capteur embarqué est une montre **LilyGo T-Watch S3**. Sa mission principale est de mesurer les accélérations via son accéléromètre interne et de transmettre ces données sous forme de paquets JSON via le protocole **MQTT** toutes les **5 secondes** vers le superviseur central.

**Fonctionnalité « Maintenance » (La faille introduite par design)**

Pour permettre aux techniciens de reconfigurer l'identifiant des capteurs sans intervention physique (connexion USB), les ingénieurs ont ouvert un **port d'écoute UDP (3333)** sur la montre. L'envoi d'un paquet UDP vers ce port permet de mettre à jour dynamiquement le nom du capteur (`device_id`) en mémoire.

**La vulnérabilité — Buffer Overflow**

La vulnérabilité majeure réside dans la gestion de la mémoire contiguë utilisée pour stocker les données de configuration. Les deux variables sont placées **côte à côte en mémoire** :

```cpp
// --- Disposition mémoire (Vulnérabilité) ---
struct VulnerableMemory {
    char rx_buffer[32];   // ← tampon de réception : 32 octets seulement
    char device_id[128];  // ← identifiant du capteur, juste après en mémoire
};
```

Lors de la réception d'un paquet de configuration, le firmware utilise `udp.read()` en lui passant la taille réelle du paquet réseau (`packetSize`) **sans vérifier** si celle-ci dépasse la capacité du tampon de réception :

```cpp
// 1. Écoute UDP — Le Buffer Overflow
int packetSize = udp.parsePacket();
if (packetSize) {
    // Aucune vérification : packetSize peut largement dépasser 32 octets
    udp.read(memory_block.rx_buffer, packetSize);

    // Mise à jour de l'affichage sur l'écran de la montre
    String idStr = "ID: " + String(memory_block.device_id);
    lv_label_set_text(label_id, idStr.c_str());
}
```

Si un attaquant envoie un paquet de plus de 32 octets, les octets supplémentaires **débordent** dans `device_id` et y écrivent n'importe quelle valeur arbitraire, sans accès physique à la montre.

> **Justification métier :** Cette implémentation a été choisie pour sa légèreté. L'absence de vérification de bornes a été jugée acceptable par l'équipe OT car le réseau local industriel est considéré comme un périmètre de confiance.

---

## Le Superviseur Central (Script Python sur Raspberry Pi)

Le superviseur est un script Python exécuté sur un **Raspberry Pi 4**. Il assure la centralisation des alertes en s'abonnant au topic MQTT `capteurs/vibration`.

**Validation et Journalisation (La faille IT)**

Le système inclut une vérification pour s'assurer que seul l'appareil autorisé (`watch01`) peut soumettre des données. Cependant, suite à des exigences d'audit et de traçabilité, l'équipe IT a ajouté une fonction de journalisation automatique pour enregistrer chaque tentative de connexion anormale.

Le script utilise une **commande système directe** pour écrire dans les logs, créant une vulnérabilité d'**injection de commande (Command Injection)** :

```python
# 1. Validation métier (Le système se croit sécurisé)
if device_id == EXPECTED_DEVICE:
    print(f"[VALIDÉ] Appareil légitime. Vibration: {vibration}")
    with open("usine_data.csv", "a") as f:
        f.write(f"{data.get('timestamp')},{vibration}\n")
else:
    # 2. Rejet et Journalisation (LA VULNÉRABILITÉ)
    print(f"[REJETÉ] ID suspect détecté : {device_id}")
    print("[AUDIT] Écriture dans les logs de sécurité...")

    # Le device_id est concaténé directement dans une commande shell
    # sans aucune désinfection (sanitization)
    commande_log = f"echo 'Tentative rejetée du device : {device_id}' >> audit_rejets.log"

    os.system(commande_log)
```

Si `device_id` contient des caractères spéciaux shell (`;`, `&&`, `` ` ``...), le shell interprétera et exécutera le code injecté avec les **privilèges du script Python**.

> **Justification métier :** L'utilisation de `os.system` a été privilégiée pour sa rapidité d'implémentation par l'équipe IT. L'identifiant de l'appareil a été considéré comme une donnée fiable provenant du matériel, sans anticiper qu'un débordement de tampon côté OT permettrait à un attaquant de modifier cette valeur pour injecter des commandes arbitraires.

---

**Tableau récapitulatif des vulnérabilités de la section 2**

| Critère                    | OT — T-Watch (Buffer Overflow)            | IT — Raspberry Pi (Command Injection)      |
| -------------------------- | ----------------------------------------- | ------------------------------------------ |
| **Type de faille**         | CWE-121 : Stack-based Buffer Overflow     | CWE-78 : OS Command Injection              |
| **Vecteur d'attaque**      | Réseau local, port UDP 3333               | Via les données MQTT reçues                |
| **Hypothèse erronée**      | Le réseau local est une zone de confiance | `device_id` vient du matériel, donc fiable |
| **Justification initiale** | Légèreté d'implémentation                 | Rapidité de développement                  |
| **Impact si exploitée**    | Écriture arbitraire dans `device_id`      | Exécution de code arbitraire sur le Pi     |
