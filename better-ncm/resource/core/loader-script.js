
if (localStorage.getItem('better-ncm-show-console-when-started') === null) {
    betterncm.utils.showConsole()
}

function replaceLogo() {
    /** @type {HTMLDivElement | null} */
    const logo2 = document.querySelector('.logo2')
    if (logo2) {
        logo2.innerHTML = 'BetterNCM'
        logo2.style.fontSize = '21px'
    }
    const playButton = document.querySelector('#main-player .btnp.f-cp.btnp-play')
    if (playButton) {
        playButton.dispatchEvent(new MouseEvent('click'));
    }
}
window.addEventListener('load', replaceLogo)
window.addEventListener('DOMContentLoaded', replaceLogo)