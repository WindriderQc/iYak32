class Footer extends HTMLElement {
    constructor() {
      super();
    }
  
    connectedCallback() {
      this.innerHTML = `
        <style>
          .footerNav {
            height: 40px;
            display: flex;
            align-items: center;
            justify-content: center;
          }
  
          .footerNav ul {
            padding: 0;
          }
          
          .footerNav a {
            font-weight: 700;
            margin: 0 25px;
            color: #fff;
            text-decoration: none;
          }
          
         .footerNav a:hover {
            padding-bottom: 5px;
            box-shadow: inset 0 -2px 0 0 #fff;
          }
        </style>
        <footer>
        <br><br>
          <div class ="footerNav">
            <a href="https://github.com/WindriderQc"><i class="fab fa-github"></i></a>
            <a href="https://www.facebook.com/yanik.beaulieu/"><i class="fab fa-facebook"></i></a>
            <a href="https://www.linkedin.com/in/yanik-beaulieu-67448a26"><i class="fab fa-linkedin"></i></a>
          </div>
        </footer>
      `;
    }
  }
  
  customElements.define('footer-component', Footer);