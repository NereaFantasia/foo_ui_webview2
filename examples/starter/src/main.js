import fb from 'foo-webview-sdk';
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();

const status = document.querySelector('#status');

function showState(state) {
  status.textContent = state === 'playing' ? 'Playing' : state === 'paused' ? 'Paused' : 'Stopped';
}

fb.player.getState().then((res) => {
  if (res.success) showState(res.state);
});

fb.on('playback:stateChanged', ({ state }) => showState(state));

document.querySelector('#random').addEventListener('click', () => {
  fb.player.random();
});
