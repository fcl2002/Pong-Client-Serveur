# Pong Client-Server Game

## 📌 Présentation du projet
Ce projet consiste à implémenter un **jeu Pong en réseau** basé sur une **architecture client–serveur**, développé sous **Linux**, dans le cadre du *Projet Jeux 2025*.

Le **serveur** maintient l’**état global du jeu**, tandis que les **clients** envoient leurs actions (entrées clavier) au serveur.  
Le projet permet de comparer les communications **TCP et UDP**, de gérer **plusieurs clients simultanément**, et d’analyser les **vulnérabilités réseau** à l’aide de **Wireshark**.

---

## 🎯 Objectifs
- Implémenter un jeu Pong en **architecture client–serveur**
- Utiliser des **sockets TCP** pour une communication fiable
- Utiliser des **sockets UDP** pour une communication non connectée
- Gérer **plusieurs clients simultanément**
- Analyser les échanges réseau avec **Wireshark**
- Identifier et expliquer des **failles de sécurité potentielles**

---

## 🧠 Architecture générale
- **Serveur**
  - Maintient l’état autoritaire du jeu
  - Traite les entrées des joueurs
  - Diffuse l’état du jeu aux clients
- **Client**
  - Capture les entrées utilisateur (clavier)
  - Envoie les actions au serveur
  - Affiche l’état du jeu localement (rendu ASCII)

L’architecture repose sur un **modèle serveur autoritaire**, empêchant les clients de modifier directement l’état du jeu.

---

## 📁 Structure du projet
```text
pong-client-server/
│
├── README.md
├── Makefile
│
├── common/
│   ├── common.h          # Constantes, structures et éléments partagés
│   └── protocol.h        # Définition des messages réseau
│
├── server/
│   ├── server_tcp.c
│   ├── server_udp.c
│   ├── game_logic.c      # Logique centrale du Pong (physique, collisions)
│   └── game_logic.h
│
├── client/
│   ├── client_tcp.c
│   ├── client_udp.c
│   ├── render.c          # Affichage ASCII
│   └── render.h
│
├── tests/
│   └── local_test.c      # Tests locaux sans réseau (optionnel)
│
└── report/
    ├── captures/
    │   ├── wireshark_tcp.png
    │   └── wireshark_udp.png
    └── notes.txt         # Notes d’analyse de sécurité
```

## 🛠️ Technologies utilisées
- **Langage** : C  
- **Système d’exploitation** : Linux  
- **Réseau** : Sockets BSD (TCP / UDP)  
- **Compilation** : Makefile  
- **Analyse réseau** : Wireshark  
- **Gestion de version** : Git / GitHub  

## ▶️ Exécution

### Version TCP
Lancer d’abord le serveur, puis les clients.

```bash
./server_tcp <port>
./client_tcp <adresse_serveur> <port>
```

## 🎮 Commandes de jeu

- **Joueur 1**
  - `W` → Monter la raquette
  - `S` → Descendre la raquette

- **Joueur 2**
  - `↑` → Monter la raquette
  - `↓` → Descendre la raquette

_(Les commandes pourront être ajustées ultérieurement.)_


