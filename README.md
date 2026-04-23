# 🏭 Scénario du projet — Surveillance vibratoire et falsification MQTT

## 🎯 Contexte

Dans un environnement industriel, certaines machines (moteurs, pompes, convoyeurs) doivent être surveillées en continu afin de détecter des anomalies mécaniques.

Une augmentation anormale des vibrations peut indiquer :

* une usure de composants (roulements, engrenages)
* un désalignement
* un déséquilibre
* un risque de panne imminente

Dans ce projet, une montre connectée basée sur ESP32 est utilisée comme **capteur de vibration embarqué**.

---

## ⚙️ Architecture du système

Le système est composé de trois éléments :

### 1. Capteur (montre connectée)

Une montre LilyGo T-Watch S3 :

* lit les données de son accéléromètre
* calcule un niveau de vibration
* détermine un état (normal / warning / critical)
* envoie les données via MQTT

---

### 2. Système de supervision

Un **Raspberry Pi** :

* héberge un broker MQTT (Mosquitto)
* reçoit les données de la montre
* analyse les vibrations
* déclenche des alertes en fonction des seuils

---

### 3. Acteur malveillant simulé

Un client MQTT supplémentaire :

* injecte des messages falsifiés
* simule une attaque sur le système

---

## 🔄 Fonctionnement normal

1. La montre est fixée sur une machine.
2. Elle mesure les accélérations (ax, ay, az).
3. Elle calcule un indicateur de vibration.
4. Elle envoie périodiquement un message MQTT contenant :

   * l’identifiant du device
   * le timestamp
   * la valeur de vibration
   * l’état (normal / warning / critical)
5. Le Raspberry Pi reçoit les données et affiche l’état de la machine.

👉 Le système fonctionne correctement et permet de détecter les anomalies.

---

## ⚠️ Vulnérabilité étudiée

Le système MQTT est volontairement mal sécurisé :

* absence d’authentification forte
* absence de contrôle des publishers
* absence de validation des données reçues
* confiance implicite dans tous les messages

👉 Cela rend possible l’injection de données falsifiées.

---

## 💣 Scénario d’attaque

L’attaque consiste à simuler un acteur malveillant capable d’envoyer de faux messages MQTT.

### Cas étudié : **masquage d’une anomalie**

1. La montre détecte une vibration anormale :

   * état = `critical`
   * valeur élevée

2. Un attaquant envoie simultanément un message falsifié indiquant :

   * une vibration normale
   * état = `normal`

3. Le système de supervision, mal conçu, accepte la mauvaise donnée.

---

## 🚨 Conséquences

* L’anomalie réelle est masquée
* Aucun signal d’alerte n’est déclenché
* La maintenance est retardée
* Le risque de panne ou d’accident augmente

👉 Cela illustre un problème critique d’**intégrité des données** dans les systèmes IoT.

---

## 🔁 Variante possible

Une autre attaque consiste à envoyer de fausses valeurs critiques :

* machine normale → système déclenche une alarme
* arrêt inutile
* perte de productivité