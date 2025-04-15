//server.js
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const net = require('net');
const path = require('path');
const wifi = require('node-wifi');

// Variables globales
const ESP_HOST = '192.168.4.1'; // IP de la carte Wi-Fi
const ESP_PORT = 8080;          // Port du serveur TCP sur la carte Wi-Fi
const port = 3000;
let global_ssid = {};           // Variable globale pour le SSID

// Initialisation de Node Wi-Fi
wifi.init({ iface: null }); // Utilise l'interface par défaut

// Création du serveur Express et Socket.IO
const app = express();
const server = http.createServer(app);
const io = socketIo(server);


app.get('/page_connection.html', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'page_connection.html'));
});
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'home.html'));
});

// Route pour scanner les réseaux Wi-Fi
app.get('/scan', async (req, res) => {
    try {
        const networks = await wifi.scan(); // Scan Wi-Fi
        res.json(networks); // Retourne les résultats sous forme de JSON
    } catch (error) {
        console.error('Erreur lors du scan Wi-Fi :', error.message);
        res.status(500).json({ error: 'Erreur lors du scan des réseaux Wi-Fi.' });
    }
});




// Gestion des connexions Socket.IO
io.on('connection', (socket) => {                   // Avec IO, quand on a une connection on fait ce qui suit
    console.log('Un client est connecté.');         // Message a la console pour dire que quelqu'un s'est connecté

    // Gestion du SSID envoyé par le client
    socket.on('send-ssid', (ssid) => {                                      // Avec socket, quand on recoit du code client le message 'send-ssid', on fait :
        console.log(`SSID reçu du client : ${ssid}`);                           // Message a la console pour dire qu'on a recu un SSID et donne lequel
        global_ssid.ssid = ssid;                                                // Affecte la valeur du SSID a la variable globale
        socket.emit('receive-ssid', `Vous avez envoyé le SSID : ${ssid}`);      // Renvoi un message de validation au code client
    });

    // Gestion du mot de passe Wi-Fi envoyé par le client et de la connection
    socket.on('getmdp', (mdp) => {                                          // Avec socket, quand on recoit du code client le message 'getmdp', on fait :
        console.log(`Mot de passe reçu du client : ${mdp}`);                    // Message a la console pour dire qu'on a recu un mdp et donne lequel
        wifi.connect({ ssid: global_ssid.ssid, password: mdp }, (err) => {      // Essaye de se connecter au wifi dont on a le mdp et le SSID et récupère l'érreure possible
            if (err) {                                                              // S'il y a une érreure
                console.log("Erreur lors de la connexion Wi-Fi :", err);                // Message a la console avec l'érreure renvoyée
                socket.emit('receive-mdp', "failed");                                   // Renvoi un message d'échec de connection au code client
                return;                                                                 // Sort de la connection
            }
            setTimeout(() => {                                                      // On attends 5 secondes
                wifi.getCurrentConnections((error, currentConnections) => {             // On récupère l'érreure possible et la connection actuelle
                    if (error) {                                                            // S'il y a une érreure
                        console.log("Erreur lors de la vérification de la connexion :", error); // Message a la console avec l'érreure renvoyée
                    socket.emit('receive-mdp', "failed");                                       // Renvoi un message d'échec de connection au code client
                    } else {                                                                // Sinon
                        console.log("Connexions actuelles :", currentConnections);              // Message a la console avec les informations de la connection 
                        const isConnected = currentConnections.some(conn => conn.ssid === 'connect�');// Crée la variable isConnected et stocke la valeur du test "égal a connect�" pour chaque élément con du tableau currentConnections
                        if (isConnected) {                                                      // Si la valeur est True
                            console.log("Connexion réussie au réseau :", global_ssid.ssid);         // Message a la console avec le SSID
                            socket.emit('receive-mdp', "success");                                  // Renvoi un message de réussite de connection au code client
                        } else {                                                                // Sinon
                            console.log("Connexion échouée. Réseau non trouvé :", global_ssid.ssid);// Message a la console avec le SSID
                            socket.emit('receive-mdp', "failed");                                   // Renvoi un message d'échec de connection au code client
                        }
                    }
                });
            }, 5000);                                                               // Les 5 secondes en questions
        });
        socket.emit('receive-mdp', "connection en cours");                      // Renvoi 2 messages, mdp recu et tentative de connection
    });

    // Gestion de la connexion TCP au serveur de la carte Wi-Fi
    socket.on('connect-tcp', () => {                                        // Avec socket, quand on recoit du coté client le message 'connect-tcp', on fait:
        const tcpClient = new net.Socket();                                     // Crée la variable tcpClient qui sera un net de socket.io

        tcpClient.connect(ESP_PORT, ESP_HOST, () => {                           // A la connection avec le port et l'host de l'ESP
            console.log(`Connecté au serveur TCP de la carte Wi-Fi (${ESP_HOST}:${ESP_PORT}).`);// Message a la console avec l'host et le port de l'ESP
            socket.emit('tcp-connected', "success");                                // Renvoi un message de réussite au code client
        });

        tcpClient.on('data', (data) => {                                        // Qaund on recoi des données
            console.log(`Message du serveur TCP : ${data.toString()}`);             // Message a la console avec les données
            socket.emit('message-tcp-reply', data.toString());                      // Renvoi un message avec les données tranformés en string au code client
        });

        tcpClient.on('error', (err) => {                                        // Quand on a une erreur
            console.error(`Erreur lors de la connexion TCP : ${err.message}`);      // Message a la console avec l'erreur
        socket.emit('tcp-connected', "failed");                                     // Renvoie un message d'erreur au code client
        });

        tcpClient.on('close', () => {                                           // Quand on recoit un message de fermeture
            console.log('Connexion TCP fermée.');                                   // Message a la console de fermeture de la connection TCP
        });

        socket.on('message-tcp', (message) => {                                 // Avec socket, quand on recoit le message 'message-tcp', on fait
            console.log(`Message du navigateur : ${message}`);                      // Message a la console avec le message envoyé par le client
            tcpClient.write(message);                                               // Execute la commande write de TCP avec le message du client
        });

        socket.on('disconnect-tcp', () => {                                     // Avec socket,
            if (tcpClient) {
                tcpClient.end();
                console.log('Connexion TCP fermée manuellement par le client.');
                socket.emit('tcp-disconnected', "success");
            } else {
                console.log('Aucune connexion TCP active.');
                socket.emit('tcp-disconnected', "failed");
            }
        });
    });

    // Gestion de la déconnexion Socket.IO
    socket.on('disconnect', () => {
        console.log('Un client s\'est déconnecté.');
    });
});

// Se servir des fichiers statiques (HTML, CSS, JS) qui sont dans le dossier 'public'
app.use(express.static('public'));
app.use('/socket.io', express.static(path.join(__dirname, 'node_modules', 'socket.io', 'client-dist')));
app.use((req, res, next) => {
    res.status(404).send('Page introuvable');
});


// Démarre le serveur
server.listen(port, () => {
    console.log(`Serveur démarré sur http://localhost:${port}`);
});