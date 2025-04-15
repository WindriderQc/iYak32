class Header extends HTMLElement {
    constructor() {
      super();
    }
  
    connectedCallback() {
      this.innerHTML = `
        <header>
            <nav class="topnav">
                 <ul>
                    <li><a id="home-link" href="index.html">Home</a></li>
                    <li><a id="setup-link" href="setup.html">Setup</a></li>
                    <li>
                        <a id="apps-link" href="#">Web Apps</a>
                        <div class="dropdown">
                            <a id="hockey-link" href="hockey/Scoreboard.html">Hockey</a>
                            <a id="boat-link" href="boat/index.html">Boat</a>
                            <a id="tides-link" href="tides.html">Tides</a>
                            <a id="dashio-link" href="dashio.html">DashIO</a>
                        </div>
                    </li>
                    <li><a id="mqtt-link" href="mqtt.html">MQTTViewer</a></li>
                    <li><a id="about-link" href="about.html">About</a></li>
                </ul>
            </nav>

        </header>
      `;
    }
}
  
customElements.define('header-component', Header);



// since the header is a component, we use this at every load to check which is the active page and set the appropriate active link in the menu
function setActiveLink() {
    const currentUrl = window.location.href;
    const links = document.querySelectorAll('.topnav a');

    links.forEach(link => {

        if (link.href === currentUrl) {
          link.classList.add('active');
        } else {
          link.classList.remove('active');
        }
      });
  }

  // Call the function when the page loads
  window.addEventListener('load', setActiveLink);

  setActiveLink();