# Conséquences et Contre-mesures (Post-Exploitation & Remédiation)

## Atteinte à l'Intégrité des Données (Le Malware)

* **Points à rédiger :**
    * Expliquer l'action post-exploitation de l'attaquant : une fois l'accès `root/rpi` obtenu silencieusement en arrière-plan, il cherche à maintenir la persistance et à manipuler le système métier.
    * Décrire le fonctionnement du script furtif (le malware) : il surveille le fichier `usine_data.csv` en temps réel et écrase systématiquement les valeurs de vibrations critiques (> 1.0) par des valeurs normales (0.80).
* **Éléments à inclure :**
    * **Commande de déploiement (Reverse Shell) :** L'extrait de code montrant comment le malware est injecté sans fichier externe via `cat << 'EOF' > /tmp/falsifier.py` et lancé de manière détachée avec `nohup`.
    * **Capture d'écran (Preuve) :** Un visuel du fichier `usine_data.csv` montrant des lignes modifiées (le "0.80" qui masque le problème).

## Conséquences Industrielles (L'Impact Métier)

* **Points à rédiger :**
    * Sortir de la technique pure pour parler des conséquences physiques (*Cyber-Physical Impact*).
    * L'analyseur central, qui lit le CSV falsifié, conclut que la machine fonctionne parfaitement.
    * **Résultat :** Les alertes de maintenance ne se déclenchent pas. La machine subit des dégâts matériels critiques (casse moteur, arrêt de chaîne de production, danger pour les opérateurs physiques). Le système de supervision devient un "faux sentiment de sécurité".
* **Éléments à inclure :** Aucun (Analyse métier).

## Contre-mesures Logicielles (Correction des Failles)

* **Points à rédiger :**
    * **Côté OT (La T-Watch) :** Expliquer qu'il ne faut **jamais** faire confiance à la taille d'un paquet réseau pour dimensionner une écriture en mémoire.
    * **Côté IT (Le Raspberry Pi) :** Expliquer qu'il ne faut **jamais** concaténer des entrées utilisateurs dans des commandes shell (`os.system`).
* **Éléments à inclure :**
    * **Code C++ (Correction) :** Montrer comment corriger le firmware en limitant la lecture UDP à la taille stricte du buffer :
      ```cpp
      // Remplacer udp.read(..., packetSize) par :
      int bytesToRead = (packetSize < sizeof(memory_block.rx_buffer)) ? packetSize : (sizeof(memory_block.rx_buffer) - 1);
      udp.read(memory_block.rx_buffer, bytesToRead);
      memory_block.rx_buffer[bytesToRead] = '\0'; // Sécurité supplémentaire
      ```
    * **Code Python (Correction) :** Montrer comment sécuriser le log système en utilisant la bibliothèque standard (et sécurisée) `logging` au lieu du shell :
      ```python
      import logging
      logging.basicConfig(filename='audit_rejets.log', level=logging.WARNING)
      # [...]
      logging.warning("Tentative rejetée du device : %s", device_id)
      ```

## Contre-mesures Architecturales (Défense en Profondeur)

* **Points à rédiger :**
    * Proposer des solutions globales pour éviter que l'attaque ne se reproduise, même si le code contient des failles.
    * **Authentification & Chiffrement :** Imposer des identifiants et le TLS (MQTTS) sur le broker Mosquitto.
    * **Segmentation Réseau :** Isoler le réseau des capteurs industriels (VLAN OT) du réseau des utilisateurs (VLAN IT/Invités) pour empêcher le mouvement latéral direct (l'attaquant n'aurait pas pu atteindre le port UDP).
* **Éléments à inclure :** (Optionnel) Un mini-schéma d'une architecture réseau sécurisée avec un pare-feu entre le Wi-Fi et les capteurs.
