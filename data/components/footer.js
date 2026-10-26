class Footer extends HTMLElement {
    connectedCallback() {
        this.innerHTML = '<footer class="local-footer"><span>iYak32 · Interface locale</span><a href="/selftest.html">Diagnostics</a><a href="/setup.html">Réglages</a></footer>';
    }
}
customElements.define('footer-component', Footer);
