import App from './App.svelte'
import './app.css'

window.BetterNCM = window.BetterNCM || {}

const configureWindow = document.createElement('div')

const configureStyle = document.createElement('style')

configureStyle.setAttribute('scoped', 'true')

;(async () => {
  const style = await window.betterncm.fs.readFileText("G:/Programs/rust/BetterNCM/better-ncm-framework/dist/style.css")
  configureStyle.innerHTML = style
})();

const configureBody = document.createElement('body')

configureBody.classList.add('betterncm-configurewindow')

configureWindow.style.position = 'fixed'
configureWindow.style.left = '64px'
configureWindow.style.top = '64px'
configureWindow.style.right = '64px'
configureWindow.style.bottom = '64px'
configureWindow.style.borderRadius = '4px'
configureWindow.style.backdropFilter= 'blur(16px)'
configureWindow.style.border = '2px solid #555555'
configureWindow.style.zIndex = '1000'

configureBody.style.background = '#2B2B2BAA'

configureWindow.appendChild(configureStyle)
configureWindow.appendChild(configureBody)

const app = new App({
  target: configureBody,
  props: {
    onClose: () => {
      document.body.removeChild(configureWindow)
    }
  }
})

window.BetterNCM.openConfigureWindow = function() {
  for (const child of document.body.childNodes) {
    if (child === configureWindow) {
      return
    }
  }
  document.body.appendChild(configureWindow)
}


window.addEventListener('keypress', (evt) => {
  if (evt.key === '\u000e' && evt.ctrlKey) {
    window.BetterNCM.openConfigureWindow()
  }
})

console.log('BetterNCM Framework Loaded!')
