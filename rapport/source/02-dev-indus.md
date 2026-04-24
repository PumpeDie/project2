# Développement du système industriel (Côté Ingénierie OT/IT)

L'objectif ici est d'expliquer "pourquoi" le système a été codé ainsi. On justifie les failles par des "besoins métiers" (facilité de maintenance, traçabilité).

## Le Capteur de Vibration (Firmware T-Watch)

- Points à rédiger :
  - La boucle principale : Lecture des vibrations et envoi d'un JSON via MQTT toutes les 5 secondes vers le serveur central.
  - La fonctionnalité "Maintenance" (La faille introduite par design) : Expliquer que pour éviter de rebrancher les montres en USB à chaque changement de machine, les ingénieurs ont ajouté un port d'écoute UDP (3333). Si on envoie un paquet UDP, cela met à jour le nom du capteur (device_id) en mémoire.
- Éléments à inclure :
  - Code C++ : La structure mémoire contiguë struct VulnerableMemory { char rx_buffer[32]; char device_id[128]; }; et la ligne fatale udp.read(memory_block.rx_buffer, packetSize);.
  - Justification métier : "L'utilisation de udp.read() sans contrôle de taille a été jugée sans risque car le réseau local est considéré de confiance".

## Le Superviseur Central (Script Python sur Raspberry Pi)

- Points à rédiger :
  - La collecte : Le script Python s'abonne au topic MQTT, vérifie que le JSON est valide, et écrit les données de vibration dans un fichier d'analyse métier (usine_data.csv).
  - La sécurité "robuste" : Le script implémente une vérification stricte. Si l'identifiant (device_id) n'est pas dans la liste blanche (ex: "watch01"), la donnée est rejetée.
  - L'audit de sécurité (La faille IT) : Pour des raisons de traçabilité, les ingénieurs IT exigent que chaque tentative de connexion anormale soit enregistrée dans un log système.
- Éléments à inclure :
  - Code Python : L'extrait montrant la validation (if device_id == EXPECTED_DEVICE:) et le log d'erreur avec os.system(...).
  - Justification métier : "L'utilisation d'une commande système directe pour logger a été choisie pour sa rapidité d'implémentation par l'équipe IT."
