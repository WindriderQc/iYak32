# Profils et minuteries quotidiennes

Les profils gardent les cinq champs de SBQC : `io`, `mode`, `lbl`, `isA` et
`name`. Leur tableau reste une mise à jour des broches mentionnées. Les réglages
locaux supplémentaires et les autres broches sont conservés. L’appareil valide
le tableau entier, enregistre un document complet, puis applique les broches.
Une erreur garde la configuration précédente. Les sorties nouvellement créées
commencent à OFF; une sortie existante garde son niveau lors d’un changement de
nom. Les broches réservées au buzzer, à I²C ou à la console série et les broches
incompatibles sont refusées. Les mises à jour passent par la même sauvegarde et
les mêmes commandes logiques protégées que l’interface locale. Elles conservent
la polarité, les autres réglages natifs et les impulsions indépendantes.

Le contrat MQTT est décrit une seule fois dans
[le README de Data](https://github.com/WindriderQc/AgentX/blob/main/data/README.md#profiles-and-daily-output-timers).
Il précise les messages historiques `configIOs`, `sunrise`, `nightfall`, la
désactivation d’une minuterie et la déclaration `state` de l’appareil. La
réponse automatique de Data exige une affectation expressément autorisée dans
Toolbox; l’appareil demande un profil seulement si `isConfigFromServer` est
activé dans sa configuration locale. Une reconnexion MQTT redéclare son état.

Les minuteries sont enregistrées dans SPIFFS. Une paire début/fin doit arriver
entière en moins de vingt secondes avant tout enregistrement ou changement de
sortie. La sauvegarde écrit et vérifie un fichier temporaire, conserve la
version précédente en `.bak`, puis remplace le fichier principal. Une lecture
peut reprendre sur cette sauvegarde après une interruption. Une demi-paire ne
modifie pas la minuterie en cours. La désactivation enregistre l’arrêt et met
la sortie à OFF. Le chargement des horaires précède l’application des niveaux
de démarrage, sans réécrire les valeurs initiales enregistrées par l’utilisateur.

Le fichier de minuteries garde une racine JSON de type tableau. Il utilise
explicitement ce type dans le stockage atomique; les configurations système et
I/O restent des objets JSON. Le test natif du gestionnaire de minuteries passe
par ce même stockage, y compris pour la sauvegarde et la reprise sur `.bak`.

Les horaires utilisent l’heure locale de l’horloge SNTP existante, avec les
réglages GMT/DST de l’appareil. Ils peuvent passer minuit. Aucun second moteur
d’horloge TimeLib n’est installé. L’algorithme suit les dates locales plutôt
qu’un intervalle fixe de 24 heures. Une connexion MQTT n’est pas nécessaire à
l’exécution d’horaires déjà enregistrés tant que l’horloge reste valide.
Un démarrage sans heure valide attend sa synchronisation.

Après un redémarrage, les sorties qui ont une minuterie activée commencent à
OFF, même si leur ancien état initial était HIGH. Le firmware attend le
prochain début de créneau; il ne rattrape pas un créneau déjà commencé.
L’alarme quotidienne de fin reste indépendante : elle remet aussi à OFF une
sortie activée manuellement entre-temps, comme le faisait SBQC. La pause générale
des automatismes et la priorité manuelle de la sortie bloquent les allumages
quotidiens. Un début bloqué est consommé : libérer la pause ou attendre la fin
de la priorité ne rattrape pas ce début. Le prochain début reste le suivant
prévu par l’horaire. L’arrêt quotidien, la désactivation et les commandes
manuelles restent possibles. Une absence de réglage de pause vaut une pause,
comme pour les autres automatismes natifs. Un refus apparaît dans `lastError`.
La vérification des protections et la commande partagent le verrou GPIO natif.
Une perte d’heure ou un recul d’horloge arrête une activation détenue par la
minuterie et recalcule le prochain début. La déclaration MQTT montre les
horaires, leur prochain début, l’heure locale et les niveaux logiques lus.
Elle ne confirme pas le mouvement d’un moteur ou l’état physique d’un relais.

## Vérification locale

`pio run -e esp32dev` compile le firmware sans téléversement.
Après l’installation des dépendances PlatformIO, `tests/run-native.sh` lance
les tests CPU avec les contrôles d’accès mémoire et de comportement indéfini.
Ils couvrent les horaires de SBQC, minuit, le redémarrage, les pertes d’heure,
la pause, la priorité manuelle, leur libération sans rattrapage, l’arrêt maintenu,
les journées de 23/25 heures et les interruptions de sauvegarde.
Ces essais ne qualifient ni SPIFFS sur un appareil réel ni les sorties physiques.
Un téléversement et chaque essai sur un appareil installé demandent un accord
précis de son propriétaire.
