# Pong Client-Server Game

## Projet Académique - Réseaux Informatiques

**Discipline :** Réseaux
**Titre :** Implémentation du jeu Pong en Architecture Client-Serveur avec Analyse de la Latence  
**Dépôt :** [github.com/fcl2002/Pong-Client-Serveur](https://github.com/fcl2002/Pong-Client-Serveur/tree/main/pong-client-serveur)

**Développeurs :**
- [Fernando COSTA LASMAR](www.linkedin.com/in/fernando-lasmar)
- [Matheus SISTON GALDINO](https://www.linkedin.com/in/matheussistongaldino/)

---

## 1. Introduction

Ce projet académique consiste en l'implémentation d'un jeu Pong multijoueur en réseau, utilisant une architecture client-serveur avec communication TCP et UDP. L'objectif principal est de démontrer les concepts fondamentaux de la programmation réseau, incluant les sockets TCP/UDP, la gestion de connexions multiples concurrentes, la synchronisation d'état distribué et l'analyse des impacts de la latence sur les applications temps réel.

Le projet a été développé en langage C, exécuté en environnement Linux (via WSL sous Windows), et permet l'analyse du trafic réseau à travers des outils comme Wireshark, rendant possible une compréhension pratique des défis impliqués dans la communication d'applications interactives.

---

## 2. Objectifs

- Implémenter un jeu Pong en **architecture client–serveur** ;
- Utiliser des **sockets TCP** pour une communication fiable ;
- Utiliser des **sockets UDP** pour une communication non connectée ;
- Gérer **plusieurs clients simultanément** ;
- Analyser les échanges réseau avec **Wireshark** ;
- Identifier et expliquer des **failles de sécurité potentielles**.

---

## 3. Architecture du Système - TCP

### 3.1. Vue d'Ensemble

Le système adopte une architecture client-serveur classique, où le serveur maintient l'autorité complète sur l'état du jeu. Ce choix architectural est fondamental pour garantir la cohérence et éviter les divergences entre les états perçus par les différents joueurs.

```
┌──────────────┐                    ┌──────────────┐
│   Client 1   │◄───── TCP ────────►│              │
│  (Joueur 1)  │                    │   Serveur    │
└──────────────┘                    │  Autoritaire │
                                    │              │
┌──────────────┐                    │              │
│   Client 2   │◄───── TCP ────────►│              │
│  (Joueur 2)  │                    │              │
└──────────────┘                    └──────────────┘
```

### 3.2. Composants du Système

#### 3.2.1. Core du Jeu (game.c / game.h)

La logique centrale du jeu a été implémentée de manière complètement indépendante de la couche réseau. Ce module est responsable de :

- Gestion des raquettes (position, vélocité, limites de mouvement) ;
- Contrôle de la balle (physique, collisions, détection des points) ;
- Système de score ;
- Logique de collision entre balle et raquettes ;
- Mise à jour discrète de l'état à travers des ticks avec delta time fixe.

**Décision de design :** La séparation entre logique de jeu et communication réseau facilite la maintenance, les tests et d'éventuelles extensions futures du projet. Le serveur est le seul composant ayant accès direct aux fonctions de mise à jour de l'état du jeu via la fonction `game_step()`.

#### 3.2.2. Serveur TCP (server_tcp.c)

Le serveur implémente un modèle autoritaire, où toutes les décisions concernant l'état du jeu sont prises de manière centralisée. Ses responsabilités incluent :

- **Gestion des connexions :** Acceptation de jusqu'à 2 clients TCP simultanés ;
- **Attribution des rôles :** Désignation automatique des joueurs (Joueur 1 = raquette gauche, Joueur 2 = raquette droite) ;
- **Réception des inputs :** Traitement des actions envoyées par les clients (mouvement vers le haut, vers le bas ou immobile) ;
- **Mise à jour de l'état :** Exécution périodique de la fonction `game_step()` avec les inputs collectés ;
- **Broadcast de l'état :** Envoi régulier de l'état complet du jeu à tous les clients connectés.

Le serveur opère dans une boucle principale avec un tick rate fixe (environ 60 Hz dans la version finale), garantissant des mises à jour cohérentes et prévisibles de l'état du jeu.

**Modèle de concurrence :** Le serveur utilise des sockets non-bloquants ou du multiplexage I/O pour gérer plusieurs connexions simultanément, garantissant qu'un client lent n'affecte pas le traitement des autres.

#### 3.2.3. Client TCP (client_tcp.c)

Les clients agissent comme interfaces d'entrée et de visualisation, ne modifiant jamais directement l'état du jeu. Leurs fonctions sont :

- **Capture d'entrée :** Lecture des commandes clavier en mode raw (sans nécessité d'appuyer sur Entrée), utilisant la bibliothèque `termios` disponible sur les systèmes Unix ;
- **Envoi des inputs :** Transmission immédiate des actions du joueur au serveur via TCP ;
- **Réception de l'état :** Traitement des messages d'état envoyés par le serveur ;
- **Rendu :** Présentation visuelle du jeu dans le terminal à travers des caractères ASCII ;
- **Prédiction locale :** Application immédiate du mouvement de sa propre raquette pour améliorer la réactivité perçue.

Chaque client contrôle exclusivement sa propre raquette, utilisant les touches W (monter) et S (descendre), indépendamment du joueur qu'il représente.

---

## 4. Architecture du Système - UDP

### 4.1. Vue d'Ensemble

[Section à compléter par Matheus]

### 4.2. Composants du Système

#### 4.2.1. Serveur UDP (server_udp.c)

[Section à compléter par Matheus]

#### 4.2.2. Client UDP (client_udp.c)

[Section à compléter par Matheus]

---

## 5. Défis de Latence et Solutions Implémentées - TCP

### 5.1. Problème : Latence Perceptible

L'utilisation du protocole TCP, bien qu'elle garantisse une livraison fiable et ordonnée des paquets, introduit une latence inhérente due à :

1. **Latence de propagation :** Temps physique de transmission des données par le réseau ;
2. **Overhead du TCP :** Mécanismes d'acknowledgment, contrôle de flux et retransmission ;
3. **Traitement :** Temps de réception, traitement et réponse au serveur.

Cette latence crée une perception de délai entre le moment où le joueur appuie sur une touche et le moment où il observe la réaction correspondante à l'écran. Dans un jeu d'action comme Pong, ce délai compromet significativement l'expérience utilisateur.

### 5.2. Solution : Client-Side Prediction

Pour atténuer l'impact négatif de la latence sur la jouabilité, nous avons implémenté une technique connue sous le nom de **Client-Side Prediction**. Cette approche est largement utilisée dans les jeux multijoueurs en ligne et consiste en :

1. **Prédiction locale :** Lorsque le joueur appuie sur une touche, le client applique immédiatement le mouvement correspondant à sa propre raquette, sans attendre la confirmation du serveur ;

2. **Autorité du serveur :** Le serveur continue d'être la seule source de vérité, calculant la position réelle de la raquette basée sur les inputs reçus ;

3. **Réconciliation :** Lorsque le client reçoit l'état mis à jour du serveur, il compare la position prédite localement avec la position autoritaire reçue ;

4. **Correction progressive :** En cas de divergence, le client ajuste graduellement sa visualisation pour converger avec l'état du serveur.

### 5.3. Mécanisme de Réconciliation

Pour éviter des corrections brusques qui causeraient des effets visuels indésirables (jittering), nous avons implémenté un système de réconciliation douce :

- **Deadzone :** Une petite zone de tolérance où les petites différences entre prédiction et état réel sont ignorées ;
- **Interpolation :** Lorsque la divergence dépasse la deadzone, la position est ajustée graduellement sur plusieurs frames, au lieu d'être corrigée instantanément.

```
Si |position_prédite - position_serveur| < DEADZONE:
    Maintenir position prédite
Sinon:
    position_client = lerp(position_prédite, position_serveur, facteur_lissage)
```

Cette approche résulte en une expérience plus fluide, où le joueur sent que sa raquette répond immédiatement aux commandes, tandis que le serveur maintient l'autorité sur l'état réel du jeu.

---

## 6. Défis de Latence et Solutions Implémentées - UDP

[Section à compléter par Matheus]

---

## 7. Analyse des Protocoles

### 7.1. Protocole TCP

**Caractéristiques du TCP :**
- **Connexion orientée :** Établissement d'une connexion avant l'échange de données (three-way handshake) ;
- **Garantie de livraison :** Les paquets perdus sont retransmis automatiquement ;
- **Ordonnancement :** Les données arrivent dans l'ordre d'envoi ;
- **Contrôle de flux :** Ajustement automatique du débit pour éviter la saturation du récepteur ;
- **Contrôle de congestion :** Adaptation du débit en fonction de l'état du réseau.

**Avantages pour le jeu Pong :**
- Simplicité d'implémentation grâce aux garanties du protocole ;
- Pas besoin d'implémenter de mécanismes de fiabilité personnalisés ;
- Communication fiable pour les messages critiques (connexion initiale, attribution des joueurs).

**Inconvénients pour les jeux temps réel :**
- Latence additionnelle due aux mécanismes d'ACK et de retransmission ;
- Head-of-line blocking : si un paquet est perdu, tous les paquets suivants sont bloqués jusqu'à sa retransmission, même si leur contenu est déjà obsolète ;
- Overhead du protocole : les mécanismes de contrôle ajoutent des délais non négligeables ;
- Non adapté aux applications où les données anciennes sont non pertinentes (dans un jeu, seul l'état le plus récent importe).

**Impact sur la jouabilité :**
Le délai introduit par le TCP rend le contrôle de la raquette moins réactif. Sans mécanismes de compensation (client-side prediction), l'expérience utilisateur est dégradée, particulièrement en conditions de latence élevée ou de perte de paquets.

### 7.2. Protocole UDP

[Section à compléter par Matheus]

**Caractéristiques de l'UDP :**
[À compléter]

**Avantages pour le jeu Pong :**
[À compléter]

**Inconvénients pour les jeux temps réel :**
[À compléter]

**Impact sur la jouabilité :**
[À compléter]

---

## 8. Choix Techniques et Justifications

### 8.1. Serveur Autoritaire

**Décision :** Toute la logique du jeu est exécutée exclusivement sur le serveur.

**Justification :**
- **Sécurité :** Prévient la triche (cheating) où les clients modifieraient leur état local ;
- **Cohérence :** Garantit que tous les joueurs visualisent le même état de jeu ;
- **Simplicité :** Centralise la logique complexe en un seul point.

**Compromis :** Introduit une latence additionnelle, mais garantit une unique source de vérité.

### 8.2. Mode Raw du Terminal

**Décision :** Utilisation de `termios` pour la capture clavier sans nécessité d'appuyer sur Entrée.

**Justification :**
- **Réactivité :** Permet que les actions du joueur soient capturées immédiatement ;
- **Expérience utilisateur :** Crée une sensation plus naturelle de contrôle en temps réel ;
- **Disponibilité :** Fonctionnalité native sur les systèmes Unix/Linux.

---

## 9. Implémentation et Structure du Code

### 9.1. Structure des Fichiers

```
pong-client-server/
│
├── README.md
├── Makefile
│
├── server/
│   ├── server_tcp.c
│   ├── server_udp.c
│   ├── game.c            # Logique centrale du Pong (physique, collisions)
│   └── game.h
│
├── client/
│   ├── client_tcp.c
│   └── client_udp.c
│
└── tests/
    └── test-game.c       # Tests locaux sans réseau pour valider la logique
```

### 9.2. Flux d'Exécution - TCP

#### Serveur

1. Initialisation du socket TCP et bind sur le port configuré ;
2. Listen pour les connexions entrantes ;
3. Acceptation de jusqu'à 2 connexions clients ;
4. Attribution des Player IDs (1 et 2) et envoi de MSG_HELLO ;
5. Boucle principale :
   - Réception des inputs des clients (MSG_INPUT) ;
   - Mise à jour de l'état via `game_step()` ;
   - Envoi de l'état complet à tous les clients (MSG_STATE) ;
   - Sleep pour maintenir un tick rate de 60 Hz.

#### Client

1. Connexion au serveur via TCP ;
2. Réception du Player ID (MSG_HELLO) ;
3. Configuration du terminal en mode raw ;
4. Boucle principale :
   - Capture de l'input clavier (non-bloquant) ;
   - Application locale du mouvement (client-side prediction) ;
   - Envoi de l'input au serveur (MSG_INPUT) ;
   - Réception de l'état du serveur (MSG_STATE) ;
   - Réconciliation entre état prédit et état réel ;
   - Rendu du jeu dans le terminal ;
5. Restauration du mode normal du terminal à la fermeture.

### 9.3. Flux d'Exécution - UDP

[Section à compléter par Matheus]

### 9.4. Compilation et Exécution

#### Prérequis
- Environnement Linux (ou **WSL** sous Windows)
- `gcc` et `make` installés

Toutes les commandes suivantes doivent être exécutées depuis la racine du projet (`pong-client-serveur/`).

**Compilation :**
```bash
make
```

**Compilation du serveur TCP :**
```bash
make server_tcp
```

**Compilation des clients TCP :**
```bash
make client_tcp
```

Les exécutables sont générés dans le répertoire `\bin`.

**Exécution du serveur TCP :**
```bash
make run_server_tcp
```

**Exécution des clients TCP (dans des terminaux séparés) :**
```bash
make run_client_tcp
```

Par défaut, les clients se connectent au serveur à l’adresse `127.0.0.1:8080`.

**Supression des exécutables générés :**
```bash
make clean
```

**Nottoyage puis recompilation de tout :**
```bash
make re
```

---

## 10. Limitations du Projet

Bien que le projet réponde aux exigences proposées, certaines limitations ont été consciemment acceptées dans le cadre académique :

**Protocole TCP :** Le TCP n'est pas idéal pour les jeux temps réel en raison de la latence additionnelle et du head-of-line blocking. Ces limitations sont inhérentes au protocole et impactent la réactivité du jeu.

**Artefacts visuels :** De petites corrections de position peuvent occasionnellement être perceptibles. La réconciliation entre prédiction et état réel peut générer du micro-stuttering dans des conditions de haute latence.

**Synchronisation temporelle :** Il n'y a pas d'implémentation d'interpolation complète entre états. Le rendu est couplé à la fréquence de réception des paquets du serveur.

**Scalabilité :** Support limité à seulement 2 joueurs. Il n'y a pas de système de matchmaking ou de lobby.

Ces limitations sont assumées dans le contexte pédagogique du projet et pourraient être abordées dans des itérations futures avec des techniques plus avancées comme l'interpolation temporelle, le server reconciliation complet, ou l'optimisation de la bande passante.

---

## 11. Annexes

### A. Commandes de Compilation

```bash
# Compiler tous les composants
make

# Compiler uniquement le serveur TCP
gcc -o server_tcp server_tcp.c game.c -Wall -Wextra

# Compiler uniquement le client TCP
gcc -o client_tcp client_tcp.c -Wall -Wextra

# Compiler uniquement le serveur UDP
gcc -o server_udp server_udp.c game.c -Wall -Wextra

# Compiler uniquement le client UDP
gcc -o client_udp client_udp.c -Wall -Wextra

# Nettoyer les binaires
make clean
```

### B. Exemple de Session de Jeu - TCP

```bash
# Terminal 1 - Serveur
$ ./server_tcp
Server listening on port 8080...
Player 1 connected
Player 2 connected
Game starting...

# Terminal 2 - Client 1 (Joueur 1)
$ ./client_tcp
Connected to server as Player 1
[Jeu rendu en ASCII]
Contrôles: W (haut) / S (bas)

# Terminal 3 - Client 2 (Joueur 2)
$ ./client_tcp
Connected to server as Player 2
[Jeu rendu en ASCII]
Contrôles: W (haut) / S (bas)
```

### C. Exemple de Session de Jeu - UDP

[Section à compléter par Matheus]

---

**Fin du Document**