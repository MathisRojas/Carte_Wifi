// Fonction pour appliquer le mode sombre ou clair
function toggleTheme() {
    const body = document.body;
    const themeToggle = document.getElementById('themeToggle');

    // Basculer la classe 'dark-mode' sur le body
    if (themeToggle.checked) {
        body.classList.add('dark-mode');
        localStorage.setItem('theme', 'dark'); // Sauvegarde dans localStorage
    } else {
        body.classList.remove('dark-mode');
        localStorage.setItem('theme', 'light'); // Sauvegarde dans localStorage
    }
}

// Appliquer le thème sauvegardé lors du chargement de la page
window.onload = function () {
    const savedTheme = localStorage.getItem('theme');
    const themeToggle = document.getElementById('themeToggle');

    if (savedTheme === 'dark') {
        document.body.classList.add('dark-mode');
        themeToggle.checked = true;
    } else {
        document.body.classList.remove('dark-mode');
        themeToggle.checked = false;
    }

    // Ajouter l'événement de changement au slider
    themeToggle.addEventListener('change', toggleTheme);
};
