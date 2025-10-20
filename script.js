const repo = '3xecutablefile/APEX';

let port;
let esptool;

document.addEventListener('DOMContentLoaded', () => {
    const statusEl = document.getElementById('status');
    const progressContainer = document.getElementById('progress');
    const progressBar = progressContainer.querySelector('progress');
    const progressText = document.getElementById('progress-text');
    const flashButton = document.getElementById('flash-btn');
    const connectButton = document.getElementById('connect-btn');
    const deviceSelect = document.getElementById('device');

    const setStatus = (message) => {
        statusEl.textContent = `Status: ${message}`;
    };

    const resetProgress = () => {
        progressBar.value = 0;
        progressText.textContent = '0%';
    };

    connectButton.addEventListener('click', async () => {
    if (!navigator.serial) {
        setStatus('Web Serial not supported. Use Chrome with Web Serial enabled.');
        return;
    }

    connectButton.disabled = true;
    setStatus('Requesting device access...');

    try {
        port = await navigator.serial.requestPort();
        esptool = new ESPTool(port);
        await esptool.connect();
        setStatus('Connected');
        flashButton.disabled = false;
    } catch (error) {
        setStatus(`Connection failed - ${error.message}`);
    } finally {
        connectButton.disabled = false;
    }
});

flashButton.addEventListener('click', async () => {
    if (!esptool) {
        setStatus('Connect a device before flashing.');
        return;
    }

    flashButton.disabled = true;
    resetProgress();
    progressContainer.style.display = 'none';

    const device = deviceSelect.value;
    let flashSucceeded = false;

    try {
        setStatus('Fetching latest release...');
        const releaseResponse = await fetch(`https://api.github.com/repos/${repo}/releases/latest`);
        if (!releaseResponse.ok) {
            throw new Error(`GitHub API responded with ${releaseResponse.status}`);
        }

        const release = await releaseResponse.json();
        const assets = Array.isArray(release.assets) ? release.assets : [];
        const asset = assets.find((a) => a.name.includes(device) && a.name.endsWith('.bin'));

        if (!asset) {
            throw new Error('No firmware found for selected device in latest release');
        }

        setStatus('Downloading firmware...');
        const firmwareResponse = await fetch(asset.browser_download_url);
        if (!firmwareResponse.ok) {
            throw new Error(`Download failed with status ${firmwareResponse.status}`);
        }

        const firmwareBinary = await firmwareResponse.arrayBuffer();
        setStatus('Flashing...');
        progressContainer.style.display = 'block';

        await esptool.flash(firmwareBinary, 0x0, (progress) => {
            const percent = Math.round(progress * 100);
            progressBar.value = percent;
            progressText.textContent = `${percent}%`;
        });

        flashSucceeded = true;
        setStatus('Flashed successfully!');
    } catch (error) {
        setStatus(`Flash failed - ${error.message}`);
    } finally {
        progressContainer.style.display = flashSucceeded ? 'block' : 'none';
        flashButton.disabled = false;
    }
    });
});
