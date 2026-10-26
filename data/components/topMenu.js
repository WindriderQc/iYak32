(() => {
    if (localStorage.getItem('theme') === 'light') document.documentElement.dataset.theme = 'light';
    class Header extends HTMLElement {
        connectedCallback() {
            this.innerHTML = `
            <header><nav class="topnav" aria-label="Navigation principale">
              <button class="nav-toggle" id="navToggle" aria-label="Ouvrir le menu" aria-expanded="false"><span></span><span></span><span></span></button>
              <div class="nav-links" id="navLinks">
                <a href="/index.html">Tableau de bord</a>
                <a href="/io_control.html">I/O</a>
                <a href="/automation.html">Automatismes</a>
                <a href="/alerts.html">Alertes</a>
                <a href="/mqtt.html">MQTT</a>
                <a href="/history.html">Historique</a>
                <a href="/wifi.html">Wi-Fi</a>
                <details class="nav-tools">
                  <summary>Outils</summary><div class="dropdown-content">
                    <a href="/basic_mode/" id="modeLink" hidden>Basic Mode</a>
                    <a href="/setup.html">Réglages</a>
                    <a href="/status.html">État système</a>
                    <a href="/activity.html">Activité I/O</a>
                    <a href="/errors.html">Journal système</a>
                    <a href="/commission.html">Mise en service</a>
                    <a href="/selftest.html">Diagnostics</a>
                    <a href="/tides.html">Marées</a>
                    <a href="/about.html">À propos</a>
                  </div>
                </details>
                <button class="theme-toggle" id="themeToggle" aria-label="Changer de thème"></button>
              </div>
            </nav></header>`;
            const links = this.querySelector('#navLinks'), toggle = this.querySelector('#navToggle');
            toggle.addEventListener('click', () => {
                const open = links.classList.toggle('nav-open'); toggle.classList.toggle('active', open);
                toggle.setAttribute('aria-expanded', String(open));
            });
            const theme = this.querySelector('#themeToggle');
            const updateTheme = () => {
                const light = document.documentElement.dataset.theme === 'light';
                theme.textContent = light ? '☾' : '☀';
                theme.setAttribute('aria-label', light ? 'Passer au thème sombre' : 'Passer au thème clair');
            };
            theme.addEventListener('click', () => {
                const light = document.documentElement.dataset.theme !== 'light';
                if (light) document.documentElement.dataset.theme = 'light'; else delete document.documentElement.dataset.theme;
                localStorage.setItem('theme', light ? 'light' : 'dark'); updateTheme();
            }); updateTheme();
            const path = location.pathname === '/' ? '/index.html' : location.pathname;
            this.querySelectorAll('a').forEach(a => {
                if (a.getAttribute('href') === path) { a.classList.add('active'); a.setAttribute('aria-current', 'page'); }
            });
            fetch('/api/status').then(r => r.ok ? r.json() : null).then(data => {
                const link = this.querySelector('#modeLink');
                const modes = { basic: ['/basic_mode/', 'Basic Mode'], boat: ['/boat/', 'Boat'], hockey: ['/hockey/Scoreboard.html', 'Hockey'] };
                if (data && modes[data.mode]) {
                    [link.href, link.textContent] = modes[data.mode]; link.hidden = false;
                }
            }).catch(() => {});
        }
    }
    customElements.define('header-component', Header);
})();
