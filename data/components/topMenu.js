class Header extends HTMLElement {
    constructor() {
      super();
    }
  
    connectedCallback() {
      this.innerHTML = `
        <header>
           
                <div class=" topnav  card-4" id="topNav">
                    <a class=" " href="index.html" >Home</a>
                    <a class=" " href="setup.html">Setup</a>
                    <div class="dropdown-hover">
                       <a class=""> Web Apps</a>
                        <div class="dropdown-content ">
                            <a class=" " id="hockey-link" href="hockey/Scoreboard.html">Hockey</a>
                            <a class=" " id="boat-link" href="boat/index.html">Boat</a>
                            <a class=" " id="tides-link" href="tides.html">Tides</a>
                            <a class=" " id="dashio-link" href="dashio.html">DashIO</a>
                        </div>
                    </div>
                    <a  class="" id="mqtt-link" href="mqtt.html">MQTTViewer</a>
                    <a  class="" id="about-link" href="about.html">About</a>

                </div>
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
          link.classList.add('active');console.log(link.href); // Log each link's href
        } else {
          link.classList.remove('active');
        }
      });
  }

  // Call the function when the page loads
  window.addEventListener('load', setActiveLink);

  //setActiveLink();