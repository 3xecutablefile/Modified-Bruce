#ifndef ASSETS_HTML_CONFIG_MENU_H
#define ASSETS_HTML_CONFIG_MENU_H

const char CONFIG_MENU_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Config | APEX Headless</title>
    <link rel="stylesheet" href="config_style.css">
</head>
<body>
    <div class="noise"></div>
    
    <nav class="nav">
        <div class="nav-logo">APEX</div>
        <div class="nav-status">
            <div class="status-item">
                <div class="status-dot"></div>
                <span>CONFIG</span>
            </div>
            <div class="status-item">
                <div class="status-dot" style="background-color: var(--neon-blue);"></div>
                <span>3 SETTINGS</span>
            </div>
        </div>
    </nav>

    <div class="background">
        <div class="grid"></div>
    </div>

    <a href="/" class="back-button">← Back to Main</a>

    <div class="main">
        <div class="page-title">
            <h1>Config</h1>
        </div>
        
        <div class="config-grid">
            <div class="config-card" onclick="window.location.href='/config/ir'">
                <span class="card-emoji">💡</span>
                <h3 class="card-title">IR Config</h3>
            </div>

            <div class="config-card" onclick="window.location.href='/config/rf'">
                <span class="card-emoji">📡</span>
                <h3 class="card-title">RF Config</h3>
            </div>

            <div class="config-card" onclick="window.location.href='/config/wifi'">
                <span class="card-emoji">📶</span>
                <h3 class="card-title">WiFi Config</h3>
            </div>
        </div>
    </div>
    
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const cards = document.querySelectorAll('.config-card');
            cards.forEach((card, index) => {
                card.style.opacity = '0';
                card.style.transform = 'translateY(30px)';
                
                setTimeout(() => {
                    card.style.transition = 'all 0.6s ease';
                    card.style.opacity = '1';
                    card.style.transform = 'translateY(0)';
                }, index * 100);
            });
        });
    </script>
</body>
</html>)rawliteral";

#endif