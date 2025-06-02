document.addEventListener('DOMContentLoaded', function() {
    const led1StatusElement = document.getElementById('led1_status');
    const led2StatusElement = document.getElementById('led2_status');
    const led3StatusElement = document.getElementById('led3_status');

    function updateLedStatus(ledElement, isOn) {
        if (ledElement) {
            if (isOn) {
                ledElement.classList.add('led-on');
                ledElement.classList.remove('led-off'); // Optional: if you have a specific led-off class
            } else {
                ledElement.classList.remove('led-on');
                ledElement.classList.add('led-off'); // Optional: if you have a specific led-off class
            }
        }
    }

    function fetchLedStates() {
        fetch('/api/basicmode/status')
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok: ' + response.statusText);
                }
                return response.json();
            })
            .then(data => {
                if (data) {
                    updateLedStatus(led1StatusElement, data.led1);
                    updateLedStatus(led2StatusElement, data.led2);
                    updateLedStatus(led3StatusElement, data.led3);
                }
            })
            .catch(error => {
                console.error('Error fetching LED states:', error);
                // Optionally, indicate error on the page, e.g., grey out LEDs
                if (led1StatusElement) updateLedStatus(led1StatusElement, false); // Default to off on error
                if (led2StatusElement) updateLedStatus(led2StatusElement, false);
                if (led3StatusElement) updateLedStatus(led3StatusElement, false);
            });
    }

    // Fetch initial state
    fetchLedStates();

    // Set interval to fetch states periodically (e.g., every 2 seconds)
    setInterval(fetchLedStates, 2000); // 2000 milliseconds = 2 seconds
});
