//cilent.js
const socket = io(); // Connexion au serveur Socket.IO
let ssidSelectionne = false;

// Envoyer un SSID au serveur
function envoyerssid() {
    const ssid = document.getElementById("ssidInput").value;
    if (ssid) {
        socket.emit('send-ssid', ssid);
        ssidSelectionne = true;
        document.getElementById('resultat').textContent = `Réseau sélectionné : ${ssid}`;
        afficherBoutonSuivant();
    } else {
        alert("Veuillez entrer un SSID valide.");
    }
}

// Scanner les réseaux Wi-Fi
function scannerWifi() {
    fetch('/scan')
        .then(response => response.json())
        .then(networks => {
            const wifiList = document.getElementById('wifiList');
            wifiList.innerHTML = '';
            if (networks.length === 0) {
                wifiList.innerHTML = '<p>Aucun réseau Wi-Fi trouvé.</p>';
                return;
            }

            networks.forEach(network => {
                const item = document.createElement('div');
                item.className = 'wifi-item';
                item.innerHTML = `<span>${network.ssid || 'Réseau masqué'}</span>`;
                item.addEventListener('click', () => {
                    const ssid = network.ssid || 'Réseau masqué';
                    document.getElementById('resultat').textContent = `Réseau sélectionné : ${ssid}`;
                    socket.emit('send-ssid', ssid);
                    ssidSelectionne = true;
                    afficherBoutonSuivant();
                });
                wifiList.appendChild(item);
            });
        })
        .catch(error => {
            console.error('Erreur lors du scan des réseaux Wi-Fi :', error);
            document.getElementById('wifiList').innerHTML = '<p>Erreur lors du scan des réseaux.</p>';
        });
}

// Gérer la connexion TCP
function connecterTCP() {
    afficherOverlay("Connexion au serveur TCP en cours...");
    socket.emit('connect-tcp');
    socket.on('tcp-connected', (status) => {
        if (status === "success") {
            afficherOverlay("Connecté au serveur TCP !");
            document.getElementById('tcpMessage').disabled = false;
            document.getElementById('btnEnvoyer').disabled = false;
            setTimeout(cacherOverlay, 2000);
        } else {
            afficherOverlay("Échec de la connexion au serveur TCP.");
            setTimeout(cacherOverlay, 2000);
        }
    });
}

// Envoyer un message via TCP
function envoyerMessage() {
    const message = document.getElementById('tcpMessage').value;
    if (!message) {
        alert("Veuillez entrer un message.");
        return;
    }
    socket.emit('message-tcp', message);
}

// Réception des données TCP
socket.on('message-tcp-reply', (reply) => {
    const mesures = reply.split("*");
    if (mesures.length === 2) {
        document.getElementById('messages_temp_TCP').textContent = `Température : ${mesures[0].trim()}`;
        document.getElementById('messages_hum_TCP').textContent = `\tHumidité : ${mesures[1].trim()}%`;
    } else {
        console.error("Format de données incorrect :", reply);
    }
});

// Déconnexion TCP
function deconnecterTCP() {
    afficherOverlay("Déconnexion en cours...");
    socket.emit('disconnect-tcp');
    socket.on('tcp-disconnected', (status) => {
        if (status === "success") {
            afficherOverlay("Déconnecté !");
            document.getElementById('tcpMessage').disabled = true;
            document.getElementById('btnEnvoyer').disabled = true;
            setTimeout(cacherOverlay, 2000);
        } else {
            afficherOverlay("Échec de la déconnexion.");
            setTimeout(cacherOverlay, 2000);
        }
    });
}

//---Affichage conditionnel du bouton/lien---
function afficherBoutonSuivant() {
    if (ssidSelectionne) {
        document.getElementById('nextPageButton').style.display = 'inline-block'; // Affiche le bouton/lien
    }
}



//---Mot de passe---
function connection() {
    const mdp = document.getElementById("mdp").value; // Récupère le mot de passe saisi
    if (!mdp) {
        alert("Veuillez entrer un mot de passe.");
        return;
    }

    afficherOverlay("Connexion en cours..."); // Afficher l'overlay avec un message initial

    // Envoi du mot de passe au serveur
    socket.emit('getmdp', mdp);

    // Écoute la réponse du serveur
    socket.on('receive-mdp', (message) => {
        if (message === "connection en cours") {
            afficherOverlay("Connexion en cours...");
        } else if (message === "success") {
            afficherOverlay("Connexion réussie ! Redirection...");
            setTimeout(() => {
                window.location.href = "page_ui_carte.html"; // Redirige vers l'interface principale
            }, 2000); // Attendre 2 secondes avant la redirection
        } else if (message === "failed") {
            afficherOverlay("Connexion échouée. Veuillez réessayer.");
            setTimeout(() => {
                cacherOverlay(); // Masquer l'overlay après un échec
            }, 2000);
        }
    });
}



// Fonction pour afficher l'overlay avec un message
function afficherOverlay(message) {
    const overlay = document.getElementById("overlay");
    const overlayMessage = document.getElementById("overlay-message");

    overlayMessage.textContent = message;
    overlay.style.display = "flex"; // Affiche l'overlay
}

// Fonction pour masquer l'overlay
function cacherOverlay() {
    const overlay = document.getElementById("overlay");
    overlay.style.display = "none"; // Masque l'overlay
}

// Fonction pour retourner a la page principale
function retour() {
    window.location.href = "home.html"; // Redirige vers la page d'accueil
}


// Fonction pour ouvrir le relai
function ouvrir_relai() {
    socket.emit('message-tcp', '00');
}

// Fonction pour fermer le relai
function fermer_relai() {
    socket.emit('message-tcp', '01');
}