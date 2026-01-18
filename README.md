# Pong Client-Server Game

## Projet Académique - Réseaux Informatiques

**Discipline :** Réseaux
</br> **Titre :** Implémentation du jeu Pong en Architecture Client-Serveur avec Analyse de la Latence  
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

- Implémenter un serveur et des clients sous Linux en utilisant des sockets TCP;
- Implémenter un serveur et des clients sous Linux en utilisant des sockets UDP;
- Modifier le serveur afin qu’il puisse accepter et gérer simultanément plusieurs
clients ;
- Capturer des traces d’exécution du jeu et analyser les échanges réseau à l’aide de
l’outil Wireshark.
- Identifier et analyser des vulnérabilités de sécurité potentielles dans l’implémenta
tion.

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

L'implémentation UDP adopte également une architecture client-serveur autoritaire, mais avec des caractéristiques fondamentalement différentes du TCP. Contrairement au TCP qui établit une connexion persistante, l'UDP utilise un protocole sans connexion (connectionless) où chaque paquet est envoyé de manière indépendante sans garantie de livraison ou d'ordre.

```
┌──────────────┐                    ┌──────────────┐
│   Client 1   │◄───── UDP ────────►│              │
│  (Joueur 1)  │  (datagrams)       │   Serveur    │
└──────────────┘                    │  Autoritaire │
                                    │              │
┌──────────────┐                    │   Port UDP   │
│   Client 2   │◄───── UDP ────────►│    12345     │
│  (Joueur 2)  │  (datagrams)       │              │
└──────────────┘                    └──────────────┘
```

Cette architecture privilégie la **faible latence** et la **réactivité** au détriment de la garantie de livraison. Dans le contexte d'un jeu temps réel comme Pong, perdre occasionnellement un paquet d'état est acceptable puisque le prochain paquet contient l'état le plus récent qui rend l'ancien obsolète.

### 4.2. Composants du Système

#### 4.2.1. Serveur UDP (server_udp.c)

Le serveur UDP maintient l'autorité sur l'état du jeu tout en gérant la communication non connectée avec les clients. Ses responsabilités incluent :

- **Gestion des connexions non persistantes :** Identification des clients par leur adresse IP et port UDP, sans établissement de connexion formelle ;
- **Attribution des rôles :** Assignation des Player IDs (0 et 1) basée sur l'ordre d'arrivée des premiers messages `MSG_CLIENT_CONNECT` ;
- **Détection de timeout :** Surveillance des clients inactifs (pas de messages reçus pendant 5 secondes) et marquage comme déconnectés ;
- **Réception des inputs :** Traitement des messages `MSG_CLIENT_INPUT` contenant les actions des joueurs ;
- **Mise à jour de l'état :** Exécution de `game_step()` à 60 Hz uniquement lorsque les deux joueurs sont connectés ;
- **Broadcast de l'état :** Envoi périodique de l'état complet via des datagrammes UDP à tous les clients actifs.

**Mécanisme de démarrage du jeu :** Le serveur initialise la structure du jeu immédiatement, mais ne commence à exécuter la simulation (`game_step()`) que lorsque les deux joueurs sont connectés. Cette approche évite que le jeu progresse avec un seul joueur.

**Gestion de la déconnexion :** Lorsqu'un joueur se déconnecte (message `MSG_CLIENT_DISCONNECT` ou timeout), le serveur :
1. Marque le client comme inactif ;
2. Arrête la simulation du jeu (`game_started = 0`) ;
3. Continue d'envoyer des broadcasts à 10 Hz pour informer le joueur restant du statut de connexion.

**Structure du message d'état :**
```c
typedef struct {
    uint8_t type;              // MSG_SERVER_STATE
    float ball_x;              // Position X de la balle
    float ball_y;              // Position Y de la balle
    float paddle_left_y;       // Position Y de la raquette gauche
    float paddle_right_y;      // Position Y de la raquette droite
    int score_left;            // Score du joueur gauche
    int score_right;           // Score du joueur droit
    uint32_t tick;             // Numéro du tick actuel
    uint8_t player0_connected; // Statut de connexion du joueur 0
    uint8_t player1_connected; // Statut de connexion du joueur 1
} StateMsg;
```

L'inclusion des flags de connexion (`player0_connected`, `player1_connected`) permet aux clients d'afficher des messages informatifs lorsqu'un joueur se déconnecte.

#### 4.2.2. Client UDP (client_udp.c)

Les clients UDP fonctionnent comme des terminaux légers qui capturent les entrées et affichent l'état du jeu sans maintenir de connexion persistante. Leurs fonctions sont :

- **Communication sans connexion :** Envoi de datagrammes UDP au serveur sans établissement préalable de connexion ;
- **Identification :** Transmission du Player ID (0 ou 1, défini via argument en ligne de commande) dans chaque message ;
- **Capture d'entrée :** Lecture non-bloquante des commandes clavier (W/S) en mode raw via `termios` ;
- **Envoi des inputs :** Transmission immédiate des actions via `MSG_CLIENT_INPUT` et envoi périodique de keepalive toutes les secondes ;
- **Réception de l'état :** Traitement des datagrammes `MSG_SERVER_STATE` contenant l'état complet du jeu ;
- **Rendu ASCII :** Visualisation du jeu dans le terminal avec représentation des raquettes, de la balle et du score ;
- **Affichage du statut :** Indication visuelle lorsqu'un joueur est déconnecté ou en attente de connexion.

**Gestion de la déconnexion :** Lorsque le joueur appuie sur 'Q', le client :
1. Envoie un message `MSG_CLIENT_DISCONNECT` au serveur ;
2. Ferme le socket UDP ;
3. Restaure le mode normal du terminal ;
4. Termine l'exécution proprement.

**Rendu visuel :** Le client utilise des séquences ANSI pour effacer l'écran et repositionner le curseur, créant l'illusion d'animation. Les éléments sont dessinés dans un buffer de caractères avant d'être affichés d'un coup, évitant le flickering.

**Absence de prédiction côté client :** Contrairement à l'implémentation TCP, la version UDP ne fait pas de client-side prediction. Cette décision est justifiée par :
- La latence naturellement plus faible de l'UDP rend la prédiction moins nécessaire ;
- La simplicité d'implémentation permet de mieux observer les caractéristiques natives du protocole ;
- L'objectif pédagogique de comparer les deux protocoles dans leurs comportements bruts.

### 4.3. Protocole de Communication

Le protocole UDP personnalisé définit quatre types de messages :

**MSG_CLIENT_CONNECT (1) :** Envoyé par le client au démarrage pour s'identifier au serveur.
```c
struct {
    uint8_t type;      // 1
    uint8_t player_id; // 0 ou 1
}
```

**MSG_CLIENT_INPUT (2) :** Envoyé par le client pour communiquer les actions du joueur.
```c
struct {
    uint8_t type;      // 2
    uint8_t player_id; // 0 ou 1
    uint8_t input;     // INPUT_NONE, INPUT_UP, INPUT_DOWN
}
```

**MSG_SERVER_STATE (3) :** Broadcast du serveur contenant l'état complet du jeu (voir structure StateMsg ci-dessus).

**MSG_CLIENT_DISCONNECT (4) :** Envoyé par le client lors de la fermeture propre.
```c
struct {
    uint8_t type; // 4
}
```

**Compromis de design :** Chaque message d'état contient l'état complet du jeu plutôt que des deltas (différences). Bien que cela consomme plus de bande passante (environ 50 octets par paquet), cette approche :
- Garantit que chaque paquet est auto-suffisant ;
- Élimine le besoin de reconstruction d'état en cas de perte de paquets ;
- Simplifie l'implémentation et améliore la robustesse.

Pour un jeu Pong à 60 Hz avec 2 clients, cela représente environ 6 KB/s par client, ce qui est négligeable pour les réseaux modernes.

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

### 6.1. Problème : Perte de Paquets et Ordre Non Garanti

L'utilisation du protocole UDP introduit des défis spécifiques dus à sa nature non fiable :

- **Perte de paquets :** Les datagrammes peuvent être perdus sans notification ni retransmission ;
- **Ordre non garanti :** Les paquets peuvent arriver désordonnés ;
- **Pas de contrôle de flux :** Aucun ajustement automatique du débit.

Ces caractéristiques peuvent causer des sauts visuels ou des mouvements erratiques dans le jeu.

### 6.2. Solution : État Complet et Design Stateless

Pour mitiger ces problèmes, nous utilisons une stratégie de **transmission d'état complet** :

- **Auto-suffisance des paquets :** Chaque `MSG_SERVER_STATE` contient l'état complet du jeu (50 octets) ;
- **Pas de dépendance temporelle :** Un paquet perdu n'affecte pas les suivants ;
- **Numérotation par tick :** Le client ignore les paquets avec un tick inférieur au dernier traité ;
- **Récupération rapide :** L'état correct est restauré dès le prochain paquet.

### 6.3. Gestion de la Connectivité

Sans mécanisme de connexion TCP, nous avons implémenté :

**Keepalive Client :** Envoi de `MSG_CLIENT_INPUT` minimum toutes les 1000ms.

**Timeout Serveur :** Déconnexion après 5000ms sans message. Le serveur continue à broadcaster à 10 Hz pour informer l'autre joueur.

**Reconnexion Transparente :** Un client peut se reconnecter automatiquement en envoyant `MSG_CLIENT_CONNECT`.

### 6.4. Bande Passante

**Consommation :**
- Jeu actif : ~6 KB/s total (2 clients × 50 bytes × 60 Hz)
- Attente : ~1 KB/s total (broadcast à 10 Hz)

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

**Caractéristiques de l'UDP :**
- **Sans connexion :** Pas d'établissement de connexion (pas de handshake) ;
- **Sans garantie de livraison :** Les paquets perdus ne sont pas retransmis ;
- **Sans ordre garanti :** Les datagrammes peuvent arriver dans un ordre différent ;
- **Pas de contrôle de flux :** Aucun mécanisme automatique de régulation du débit ;
- **Overhead minimal :** En-tête de seulement 8 octets (vs 20+ pour TCP).

**Avantages pour le jeu Pong :**
- **Latence minimale :** Pas de délai dû aux ACK, handshakes ou retransmissions ;
- **Pas de head-of-line blocking :** Un paquet perdu n'empêche pas le traitement des suivants ;
- **Simplicité du protocole :** Communication directe sans gestion d'état de connexion ;
- **Performance prévisible :** Pas de variations dues aux mécanismes de contrôle de congestion ;
- **Fraîcheur des données :** Seul l'état le plus récent importe, les anciens paquets perdus sont sans conséquence.

**Inconvénients pour les jeux temps réel :**
- **Perte de paquets visible :** Sauts visuels si plusieurs paquets consécutifs sont perdus ;
- **Complexité d'implémentation :** Nécessité d'implémenter ses propres mécanismes (keepalive, timeout, numérotation) ;
- **Pas de garanties :** Le développeur doit gérer tous les cas de défaillance réseau ;
- **Détection de déconnexion manuelle :** Obligation d'implémenter un système de timeout personnalisé.

**Impact sur la jouabilité :**
L'UDP offre une expérience nettement plus réactive que le TCP grâce à sa latence minimale. La perte occasionnelle de paquets (généralement < 1% sur réseaux locaux) est imperceptible car le prochain paquet contient l'état complet à jour. Le contrôle des raquettes est instantané et fluide, sans le délai perceptible observé avec TCP. Les rares artefacts visuels dus à la perte de paquets sont largement compensés par la réactivité globale supérieure.

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
pong-client-serveur/
│
├── bin/                        # Binaires compilés
│
├── client/
│   ├── client_tcp.c            # Implémentation client TCP
│   └── client_udp.c            # Implémentation client UDP
│
├── server/
│   ├── server_tcp.c            # Implémentation serveur TCP
│   ├── server_udp.c            # Implémentation serveur UDP
│   ├── game.c                  # Logique centrale du jeu
│   └── game.h                  # Interface de la logique de jeu
│
├── tests/
│   ├── test-game.c             # Tests unitaires de la logique
│   ├── attack_control.py       # Script de test d'attaque
│   └── attack_disconnect.py    # Script de test DoS
│
├── wireshark/                  # Captures réseau pour analyse
│
├── report/                     # Documentation et rapports
│
├── Makefile                    # Compilation automatisée
├── README.md                   # Documentation générale
├── LICENSE                     # Licence du projet
└── .gitignore                  # Fichiers ignorés par git
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

#### Serveur

1. Initialisation du socket UDP et bind sur le port 12345 ;
2. Configuration du socket en mode non-bloquant (timeout de 1ms) ;
3. Initialisation de la structure du jeu via `game_init()` ;
4. Boucle principale :
   - Réception des messages des clients (MSG_CLIENT_CONNECT, MSG_CLIENT_INPUT, MSG_CLIENT_DISCONNECT) ;
   - Attribution des Player IDs (0 et 1) lors de la première connexion ;
   - Démarrage de la simulation quand les deux joueurs sont connectés ;
   - Mise à jour de l'état via `game_step()` à 60 Hz (si jeu démarré) ;
   - Broadcast de l'état complet à tous les clients actifs (MSG_SERVER_STATE) ;
   - Vérification des timeouts (5 secondes d'inactivité) ;
   - Si jeu en pause : broadcast à 10 Hz pour informer du statut de connexion ;
   - Sleep de 1ms pour éviter la surconsommation CPU.

#### Client

1. Création du socket UDP ;
2. Configuration du socket en mode non-bloquant (timeout de 1ms) ;
3. Configuration de l'adresse du serveur (IP + port 12345) ;
4. Configuration du terminal en mode raw ;
5. Envoi du message initial MSG_CLIENT_CONNECT avec Player ID ;
6. Boucle principale :
   - Capture de l'input clavier (non-bloquant) ;
   - Détection de la touche Q pour quitter ;
   - Envoi immédiat de MSG_CLIENT_INPUT si l'input change ;
   - Envoi périodique de keepalive (toutes les 1000ms minimum) ;
   - Réception des messages MSG_SERVER_STATE du serveur ;
   - Mise à jour de l'état local avec les données reçues ;
   - Rendu du jeu dans le terminal (avec affichage du statut de connexion) ;
   - Sleep de 16ms (~60 FPS de rendu) ;
7. En cas de sortie (Q pressé) :
   - Envoi de MSG_CLIENT_DISCONNECT ;
   - Fermeture du socket ;
   - Restauration du mode normal du terminal.

### 9.4. Compilation et Exécution

#### Prérequis
- Environnement Linux (ou **WSL** sous Windows)
- `gcc` et `make` installés

Toutes les commandes suivantes doivent être exécutées depuis la racine du projet (`pong-client-serveur/`).

#### Compilation

**Compiler tout (TCP + UDP) :**
```bash
make all
# ou simplement
make
```

**Compiler uniquement TCP :**
```bash
make tcp
```

**Compiler uniquement UDP :**
```bash
make udp
```

**Compiler des composants individuels :**
```bash
make server_tcp    # Serveur TCP uniquement
make client_tcp    # Client TCP uniquement
make server_udp    # Serveur UDP uniquement
make client_udp    # Client UDP uniquement
```

Les exécutables sont générés dans le répertoire `bin/`.

#### Exécution - TCP

**Serveur TCP (Terminal 1) :**
```bash
make run_server_tcp
# ou directement
./bin/server_tcp 8080
```

**Clients TCP (Terminaux 2 et 3) :**
```bash
make run_client_tcp
# ou directement
./bin/client_tcp 127.0.0.1 8080
```

Par défaut, les clients se connectent au serveur à l'adresse `127.0.0.1:8080`.

#### Exécution - UDP

**Serveur UDP (Terminal 1) :**
```bash
make run_server_udp
# ou directement
./bin/server_udp
```
Le serveur écoute par défaut sur le port 12345.

**Client UDP - Joueur 1 (Terminal 2) :**
```bash
make run_client_udp
# ou directement
./bin/client_udp 127.0.0.1 0
```

**Client UDP - Joueur 2 (Terminal 3) :**
```bash
make run_client_udp_p2
# ou directement
./bin/client_udp 127.0.0.1 1
```

**Note :** Le Player ID (0 ou 1) doit être spécifié en ligne de commande pour l'UDP.

#### Nettoyage

**Supprimer les exécutables générés :**
```bash
make clean
```

**Nettoyage puis recompilation complète :**
```bash
make re
```

#### Structure des Binaires Générés

```
bin/
├── server_tcp     # Serveur TCP
├── client_tcp     # Client TCP
├── server_udp     # Serveur UDP
└── client_udp     # Client UDP
```
---

## 10. Limitations du Projet

Bien que le projet réponde aux exigences proposées, certaines limitations ont été consciemment acceptées dans le cadre académique :

### Implémentation TCP

**Protocole TCP :** Le TCP n'est pas idéal pour les jeux temps réel en raison de la latence additionnelle et du head-of-line blocking. Ces limitations sont inhérentes au protocole et impactent la réactivité du jeu.

**Artefacts visuels :** De petites corrections de position peuvent occasionnellement être perceptibles. La réconciliation entre prédiction et état réel peut générer du micro-stuttering dans des conditions de haute latence.

**Synchronisation temporelle :** Il n'y a pas d'implémentation d'interpolation complète entre états. Le rendu est couplé à la fréquence de réception des paquets du serveur.

### Implémentation UDP

**Perte de paquets visible :** En cas de perte de plusieurs paquets consécutifs (conditions réseau dégradées), des sauts visuels peuvent apparaître. L'absence d'interpolation temporelle rend ces artefacts plus visibles.

**Pas de prédiction côté client :** Contrairement à l'implémentation TCP, la version UDP n'utilise pas de client-side prediction. Bien que la latence de l'UDP soit naturellement plus faible, l'ajout de prédiction améliorerait encore la réactivité perçue.

**Gestion simplifiée des erreurs :** Le système de keepalive et timeout est basique. Une implémentation production inclurait :
- Détection plus fine des variations de latence (jitter) ;
- Adaptation dynamique de la fréquence de broadcast selon les conditions réseau ;
- Métriques de qualité de connexion affichées aux joueurs.

**Bande passante non optimisée :** Bien que raisonnable (~6 KB/s), l'envoi de l'état complet à 60 Hz pourrait être optimisé via :
- Compression des données (quantification des positions) ;
- Delta compression (envoi uniquement des changements) ;
- Priorisation des updates (balle vs raquettes).

### Limitations Communes (TCP & UDP)

**Scalabilité :** Support limité à seulement 2 joueurs. Il n'y a pas de système de matchmaking ou de lobby.

**Sécurité :** Aucune authentification des joueurs. Un client malveillant pourrait :
- Usurper l'identité d'un autre joueur (spoofing d'adresse en UDP) ;
- Envoyer des inputs invalides pour perturber le jeu ;
- Se reconnecter avec différents Player IDs.

**Robustesse réseau :** Pas de gestion de :
- Reconnexion automatique après déconnexion involontaire (TCP) ;
- Migration entre réseaux (changement d'IP) ;
- NAT traversal pour jeu sur Internet.

Ces limitations sont assumées dans le contexte pédagogique du projet et pourraient être abordées dans des itérations futures avec des techniques plus avancées comme l'interpolation temporelle, le lag compensation, le delta compression, ou des mécanismes de sécurité (authentification, encryption, anti-cheat).
---

## 11. Annexes

### A. Commandes de Compilation

```bash
# Compiler tous les composants (TCP + UDP)
make all
# ou simplement
make

# Compiler uniquement TCP
make tcp

# Compiler uniquement UDP
make udp

# Compiler des composants individuels
make server_tcp
make client_tcp
make server_udp
make client_udp

# Compilation manuelle (si nécessaire)

# Serveur TCP
gcc -o bin/server_tcp server/server_tcp.c server/game.c -Wall -Wextra -std=c11 -lm

# Client TCP
gcc -o bin/client_tcp client/client_tcp.c -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -lm

# Serveur UDP
gcc -o bin/server_udp server/server_udp.c server/game.c -Wall -Wextra -std=c11 -lm

# Client UDP
gcc -o bin/client_udp client/client_udp.c -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -lm

# Nettoyer les binaires
make clean

# Nettoyer et recompiler
make re
```

### B. Exemple de Session de Jeu - TCP

```bash
# Terminal 1 - Serveur
$ make run_server_tcp
Server listening on port 8080...
Player 1 connected
Player 2 connected
Game starting...

# Terminal 2 - Client 1 (Joueur 1)
$ make run_client_tcp
Connected to server as Player 1
[Jeu rendu en ASCII]
Contrôles: W (haut) / S (bas)

# Terminal 3 - Client 2 (Joueur 2)
$ make run_client_tcp
Connected to server as Player 2
[Jeu rendu en ASCII]
Contrôles: W (haut) / S (bas)
```

### C. Exemple de Session de Jeu - UDP

```bash
# Terminal 1 - Serveur
$ make run_server_udp
Pong server started on port 12345
Waiting for players...
Player 0 connected: 127.0.0.1:54321
Player 1 connected: 127.0.0.1:54322
Both players connected! Game starting...

# Terminal 2 - Client 1 (Joueur 1 / Player 0)
$ make run_client_udp
Connecting to server 127.0.0.1:12345 as Player 1...
PONG - Player 1
Score: 0 - 0

[Jeu rendu en ASCII]
Controls: W/S to move | Q to quit

# Terminal 3 - Client 2 (Joueur 2 / Player 1)
$ make run_client_udp_p2
Connecting to server 127.0.0.1:12345 as Player 2...
PONG - Player 2
Score: 0 - 0

[Jeu rendu en ASCII]
Controls: W/S to move | Q to quit

# Simulation de déconnexion (Player 2 appuie sur Q)
# Terminal 2 affiche:
[Player 2 disconnected - Waiting for reconnection...]

# Terminal 1 (Serveur) affiche:
Player 1 disconnected
```

---

**Fin du Document**