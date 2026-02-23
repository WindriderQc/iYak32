// Theme management - must run early to prevent flash
(function() {
    const saved = localStorage.getItem('theme');
    if (saved === 'light') document.documentElement.dataset.theme = 'light';
})();

class Header extends HTMLElement {
    constructor() {
      super();
    }

    connectedCallback() {
      const isLight = document.documentElement.dataset.theme === 'light';
      this.innerHTML = `
        <header>
          <div class="topnav card-4" id="topNav">
            <button class="nav-toggle" id="navToggle" aria-label="Toggle navigation">
              <span></span><span></span><span></span>
            </button>
            <div class="nav-links" id="navLinks">
              <a href="/index.html">Home</a>
              <a href="/setup.html">Setup</a>
              <a href="/io_control.html">I/O Control</a>
              <div class="dropdown-hover">
                <a class="dropdown-trigger">Web Apps &#9662;</a>
                <div class="dropdown-content">
                  <a href="/hockey/Scoreboard.html">Hockey</a>
                  <a href="/boat/index.html">Boat</a>
                  <a href="/tides.html">Tides</a>
                  <a href="/basic_mode/">Basic Mode</a>
                </div>
              </div>
              <div class="dropdown-hover">
                <a class="dropdown-trigger">System &#9662;</a>
                <div class="dropdown-content">
                  <a href="/status.html">Status</a>
                  <a href="/history.html">History</a>
                  <a href="/errors.html">Errors</a>
                  <a href="/wifi.html">WiFi</a>
                  <a href="/selftest.html">Diagnostics</a>
                </div>
              </div>
              <a href="/mqtt.html">MQTT</a>
              <a href="/about.html">About</a>
              <button class="theme-toggle" id="themeToggle" title="Toggle theme">${isLight ? '\u{1F319}' : '\u{2600}'}</button>
            </div>
          </div>
        </header>
      `;

      // Hamburger toggle
      const toggle = this.querySelector('#navToggle');
      const links = this.querySelector('#navLinks');
      if (toggle && links) {
        toggle.addEventListener('click', () => {
          links.classList.toggle('nav-open');
          toggle.classList.toggle('active');
        });
      }

      // Theme toggle
      const themeBtn = this.querySelector('#themeToggle');
      if (themeBtn) {
        themeBtn.addEventListener('click', () => {
          const current = document.documentElement.dataset.theme;
          if (current === 'light') {
            delete document.documentElement.dataset.theme;
            localStorage.setItem('theme', 'dark');
            themeBtn.textContent = '\u{2600}';
          } else {
            document.documentElement.dataset.theme = 'light';
            localStorage.setItem('theme', 'light');
            themeBtn.textContent = '\u{1F319}';
          }
        });
      }
    }
}

customElements.define('header-component', Header);

// Set active link based on current URL
function setActiveLink() {
    const currentUrl = window.location.href;
    const links = document.querySelectorAll('.topnav a');
    links.forEach(link => {
        if (link.href && link.href === currentUrl) {
            link.classList.add('active');
        } else {
            link.classList.remove('active');
        }
    });
}

window.addEventListener('load', setActiveLink);
