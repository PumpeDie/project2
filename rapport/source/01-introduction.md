# Introduction

## Matériel utilisé

### Capteur embarqué

La **LilyGo T-Watch S3** est un microcontrôleur basé sur un **ESP32** équipé d'un écran tactile et d'une connectivité Wi-Fi. Son rôle dans ce projet est de servir comme capteur de vibration portable, capable de mesurer les accélérations via son accéléromètre interne et de transmettre ces données en temps réel.

![Photo de notre LilyGo T-Watch S3](twatch.jpg){height=300px}


### Système de supervision

Un **Raspberry Pi 4** exécute les services de supervision centrale :

- Héberge un broker **MQTT** (Mosquitto) pour la collecte centralisée des données
- Assure le stockage et la journalisation des mesures
- Déclenche les alertes basées sur les seuils de vibration

### Infrastructure d'attaque

Un **PC d'attaque** connecté au même réseau local, exécutant un système d'exploitation Linux, permet de tester les vulnérabilités du système en tant qu'acteur malveillant.

![Photo de notre montage physique](setup.jpg){height=300px}

## Situation et Scénario

### Contexte industriel

Dans le cadre d'une transition vers l'**Industrie 4.0**, une usine déploie une infrastructure intelligente de surveillance des machines. Des objets connectés équipés d'accéléromètres sont fixés directement sur les moteurs, pompes et convoyeurs afin de surveiller les vibrations mécaniques en temps réel. Cette surveillance précoce permet de détecter :

* l'usure des composants (roulements, engrenages)
* les désalignements et déséquilibres
* les risques de pannes imminentes

Chaque anomalie détectée doit déclencher une alerte pour permettre une intervention de maintenance préventive avant la casse matérielle.

### Périmètre et convergence IT/OT

Le projet se déroule dans un **réseau local partagé** où les systèmes informatiques (IT) et opérationnels (OT) convergent. Le capteur (T-Watch), le superviseur (Raspberry Pi) et les stations de travail partagent le même segment réseau `10.42.0.0/24`.

Cette convergence, bien que bénéfique pour l'intégration et la maintenance, introduit des considérations de sécurité spécifiques aux environnements industriels.

### Enjeu principal

L'objectif critique est d'**assurer l'intégrité des données de vibration** afin d'éviter les faux positifs et les faux négatifs. Un système mal sécurisé risque de voir ses données de mesure compromises ou falsifiées, ce qui pourrait avoir des conséquences graves :

* **Masquage d'anomalies** : Un attaquant injecte de fausses données normales pour dissimuler une panne imminente
* **Fausses alertes** : Un attaquant déclenche des alarmes non justifiées pour perturber l'opération
* **Perte de confiance** : L'opérateur perd la certitude que les alertes reflètent l'état réel du système

En parallèle, il est essentiel de permettre une **maintenance facile des capteurs** (reconfiguration des identifiants, mises à jour) sans intervention physique laborieuse. Cette tension entre sécurité et facilité d'usage constitue le cœur des vulnérabilités étudiées dans ce rapport.
