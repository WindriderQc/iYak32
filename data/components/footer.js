class Footer extends HTMLElement {
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
          <nav>
            <ul class="social-row">
            <a href="https://github.com/my-github-profile"><i class="fab fa-github"></i></a>
            <a href="https://twitter.com/my-twitter-profile"><i class="fab fa-twitter"></i></a>
            <a href="https://www.linkedin.com/in/my-linkedin-profile"><i class="fab fa-linkedin"></i></a>
            </ul>
          </nav>
        </header>
      `;
    }
  }
  
  customElements.define('footer-component', Footer);