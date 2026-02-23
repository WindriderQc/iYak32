document.addEventListener('DOMContentLoaded', function() {
    const led1StatusElement = document.getElementById('led1_status');
    const led2StatusElement = document.getElementById('led2_status');
    const led3StatusElement = document.getElementById('led3_status');
    const analog1ValueElement = document.getElementById('analog1_value');
    const analog1ThresholdElement = document.getElementById('analog1_threshold');
    const analog2ValueElement = document.getElementById('analog2_value');
    const analog2ThresholdElement = document.getElementById('analog2_threshold');

    function updateLedStatus(ledElement, isOn) {
        if (ledElement) {
            if (isOn) {
                ledElement.classList.add('led-on');
                ledElement.classList.remove('led-off');
            } else {
                ledElement.classList.remove('led-on');
                ledElement.classList.add('led-off');
            }
        }
    }

    function fetchLedStates() {
        fetch('/api/basicmode/status')
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok');
                return response.json();
            })
            .then(data => {
                if (data) {
                    updateLedStatus(led1StatusElement, data.led1);
                    updateLedStatus(led2StatusElement, data.led2);
                    updateLedStatus(led3StatusElement, data.led3);
                    // Update sound toggle state
                    const soundToggle = document.getElementById('soundToggle');
                    if (soundToggle && data.sound !== undefined) {
                        soundToggle.checked = data.sound;
                    }
                }
            })
            .catch(error => {
                console.error('Error fetching LED states:', error);
                if (led1StatusElement) updateLedStatus(led1StatusElement, false);
                if (led2StatusElement) updateLedStatus(led2StatusElement, false);
                if (led3StatusElement) updateLedStatus(led3StatusElement, false);
            });
    }

    function fetchAnalogStates() {
        fetch('/api/basicmode/analogstatus')
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok');
                return response.json();
            })
            .then(data => {
                if (data) {
                    analog1ValueElement.textContent = data.analog1_value;
                    analog1ThresholdElement.textContent = data.analog1_threshold;
                    analog2ValueElement.textContent = data.analog2_value;
                    analog2ThresholdElement.textContent = data.analog2_threshold;
                    // Sync slider positions
                    const t1 = document.getElementById('thresh1');
                    const t2 = document.getElementById('thresh2');
                    if (t1) t1.value = data.analog1_threshold;
                    if (t2) t2.value = data.analog2_threshold;
                }
            })
            .catch(error => {
                console.error('Error fetching analog states:', error);
                analog1ValueElement.textContent = 'Error';
                analog1ThresholdElement.textContent = 'Error';
                analog2ValueElement.textContent = 'Error';
                analog2ThresholdElement.textContent = 'Error';
            });
    }

    // Fetch initial state
    fetchLedStates();
    fetchAnalogStates();

    // Periodic polling
    setInterval(fetchLedStates, 5000);
    setInterval(fetchAnalogStates, 5000);
});

// Sound toggle (global scope for onclick)
function toggleSound() {
    const enabled = document.getElementById('soundToggle').checked;
    fetch('/api/basicmode/sound', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    }).catch(e => console.error('Sound toggle error:', e));
}

// Threshold slider (global scope for onchange)
function setThreshold(channel, value) {
    document.getElementById('analog' + channel + '_threshold').textContent = value;
    fetch('/api/basicmode/threshold', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ channel, value: parseInt(value) })
    }).catch(e => console.error('Threshold set error:', e));
}
