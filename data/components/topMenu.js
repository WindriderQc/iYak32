class Header extends HTMLElement {
    constructor() {
      super();
    }
  
    connectedCallback() {
      this.innerHTML = `
        <style>
          nav {
            height: 40px;
            display: flex;
            align-items: center;
            justify-content: center;
            background-color:  #0a0a23;
          }
  
          ul {
            padding: 0;
          }
          
          a {
            font-weight: 700;
            margin: 0 25px;
            color: #fff;
            text-decoration: none;
          }
          
          a:hover {
            padding-bottom: 5px;
            box-shadow: inset 0 -2px 0 0 #fff;
          }
        </style>
        <header>
            <nav class="topnav">
                <a id="home-link" href="index.html">Home</a>
                <a id="setup-link" href="setup.html">Setup</a>
                <a id="tides-link" href="tides.html">Tides</a>
                <a id="mqtt-link" href="mqtt.html">MQTTViewer</a>
                <a id="hockey-link" href="hockey/hockey.html">Hockey</a>
                <a id="about-link" href="about.html">About</a>
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