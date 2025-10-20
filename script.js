const repo = '3xecutablefile/APEX';

let port;
let esptool;

document.getElementById('connect-btn').addEventListener('click', async () => {
    if (!navigator.serial) {
        document.getElementById('status').textContent = 'Status: Web Serial not supported. Use Chrome with Web Serial enabled.';
        return;
    }
    try {
        port = await navigator.serial.requestPort();
        esptool = new ESPTool(port);
        await esptool.connect();
        document.getElementById('status').textContent = 'Status: Connected';
        document.getElementById('flash-btn').disabled = false;
    } catch (e) {
        document.getElementById('status').textContent = 'Status: Connection failed - ' + e.message;
    }
});

document.getElementById('flash-btn').addEventListener('click', async () => {
    const device = document.getElementById('device').value;
    document.getElementById('status').textContent = 'Status: Fetching latest release...';
    try {
        const releaseResponse = await fetch(`https://api.github.com/repos/${repo}/releases/latest`);
        const release = await releaseResponse.json();
        const asset = release.assets.find(a => a.name.includes(device) && a.name.endsWith('.bin'));
        if (!asset) {
            document.getElementById('status').textContent = 'Status: No firmware found for selected device in latest release';
            return;
        }
        const binUrl = asset.browser_download_url;
        document.getElementById('status').textContent = 'Status: Downloading firmware...';
        const response = await fetch(binUrl);
        const bin = await response.arrayBuffer();
        document.getElementById('status').textContent = 'Status: Flashing...';
        document.getElementById('progress').style.display = 'block';
        await esptool.flash(bin, 0x0, (progress) => {
            document.querySelector('progress').value = progress;
            document.getElementById('progress-text').textContent = Math.round(progress * 100) + '%';
        });
        document.getElementById('status').textContent = 'Status: Flashed successfully!';
    } catch (e) {
        document.getElementById('status').textContent = 'Status: Flash failed - ' + e.message;
    }
});