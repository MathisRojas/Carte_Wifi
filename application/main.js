//main.js
const { app, BrowserWindow } = require('electron');
const path = require('path');
const { exec } = require('child_process'); // Pour lancer `server.js`
const { spawn } = require('child_process');

const serverProcess = spawn('node', ['server.js'], { stdio: 'inherit' });

let mainWindow;

// Fonction pour démarrer le serveur Node.js
function startServer() {
    const serverProcess = exec('node server.js', { cwd: __dirname });

    serverProcess.stdout.on('data', (data) => {
        console.log(`[SERVER]: ${data}`);
    });

    serverProcess.stderr.on('data', (data) => {
        console.error(`[SERVER ERROR]: ${data}`);
    });

    serverProcess.on('close', (code) => {
        console.log(`[SERVER]: Process exited with code ${code}`);
    });
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 800,
        height: 600,
    });

    mainWindow.loadFile('public/home.html');
    mainWindow.loadURL('http://localhost:3000/home.html');

    
    
}

app.whenReady().then(() => {
    startServer(); // Démarre le serveur
    createWindow(); // Crée la fenêtre Electron

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
