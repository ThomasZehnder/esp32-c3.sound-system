async function fetchText(path) {
    const response = await fetch(path);
    if (!response.ok) {
        throw new Error(`HTTP ${response.status} for ${path}`);
    }
    return response.text();
}

function setMainContent(content) {
    const article = document.getElementById('articleId');
    if (article) {
        article.innerHTML = content;
    }
}

function setFooterContent(content) {
    const footer = document.getElementById('pFooter');
    if (footer) {
        footer.innerHTML = content;
    }
}

function renderLoadError(error) {
    setMainContent(`<h2>Load failed</h2><pre>${error.message}</pre>`);
}

const routes = {
    assembly: 'a-home.html',
    sounds: 'a-sounds.html',
    ultrasound: 'a-ultrasound.html',
    config: 'a-config.html',
    sequence: 'a-sequence.html'
};

let soundStatePollTimer = null;
let ultrasoundStatePollTimer = null;
let sequenceEditorConfig = null;

function getCurrentRoute() {
    const route = window.location.hash.replace(/^#/, '');
    return route || 'assembly';
}

function updateActiveNavigation(routeName) {
    const navLinks = document.querySelectorAll('[data-route]');
    navLinks.forEach((link) => {
        const isActive = link.dataset.route === routeName;
        if (isActive) {
            link.setAttribute('aria-current', 'page');
        } else {
            link.removeAttribute('aria-current');
        }
    });
}

function wireRefreshButtons(article) {
    const refreshButtons = article.querySelectorAll('[data-refresh-target]');
    refreshButtons.forEach((button) => {
        button.addEventListener('click', () => {
            const targetId = button.dataset.refreshTarget;
            if (!targetId) {
                return;
            }

            const targetElement = article.querySelector(`#${targetId}`);
            if (!targetElement) {
                return;
            }

            const selectId = button.dataset.jsonSelect;
            if (selectId) {
                const selectElement = article.querySelector(`#${selectId}`);
                const selectedOption = selectElement?.selectedOptions?.[0];
                if (selectedOption) {
                    targetElement.dataset.jsonSource = selectedOption.value;
                    targetElement.dataset.jsonMethod = selectedOption.dataset.method || 'GET';
                    targetElement.dataset.jsonBody = selectedOption.dataset.body || '';
                }
            }

            targetElement.textContent = 'Loading...';
            loadJsonIntoElement(targetElement);
        });
    });
}

async function loadJsonIntoElement(element) {
    const source = element.dataset.jsonSource;
    if (!source) {
        return;
    }

    try {
        const method = element.dataset.jsonMethod || 'GET';
        const body = element.dataset.jsonBody || '';
        const requestOptions = { method };

        if (method !== 'GET' && body) {
            requestOptions.headers = { 'Content-Type': 'application/json' };
            requestOptions.body = body;
        }

        const response = await fetch(source, requestOptions);
        if (!response.ok) {
            const contentType = response.headers.get('content-type') || '';
            if (contentType.includes('application/json')) {
                const errorData = await response.json();
                throw new Error(errorData.error || `HTTP ${response.status} for ${source}`);
            }

            throw new Error(`HTTP ${response.status} for ${source}`);
        }

        const contentType = response.headers.get('content-type') || '';
        const data = contentType.includes('application/json')
            ? await response.json()
            : {
                ok: true,
                status: response.status,
                service: `${method} ${source}`,
                contentType,
                responseText: await response.text()
            };

        element.textContent = JSON.stringify(data, null, 2);
    } catch (error) {
        element.textContent = `Failed to load JSON: ${error.message}`;
    }
}

function createSoundOptions(sounds, selectedTag) {
    return sounds.map((sound) => {
        const selected = sound.tag === selectedTag ? ' selected' : '';
        return `<option value="${sound.tag}"${selected}>${sound.buttonLabel}</option>`;
    }).join('');
}

function toggleSequenceEditorRow(row) {
    const enabled = row.querySelector('.sequence-enabled');
    const kind = row.querySelector('.sequence-kind');
    const tag = row.querySelector('.sequence-tag');
    const duration = row.querySelector('.sequence-duration');
    if (!enabled || !kind || !tag || !duration) {
        return;
    }

    const rowEnabled = enabled.checked;
    kind.disabled = !rowEnabled;
    duration.disabled = !rowEnabled;
    tag.disabled = !rowEnabled || kind.value === 'pause';
}

function renderSequenceEditor(container, config) {
    sequenceEditorConfig = config;
    const sounds = (Array.isArray(config.sounds) ? config.sounds : []).filter((sound) => sound.tag !== 'stop');
    const sequence = Array.isArray(config.sequence) ? config.sequence : [];
    const maxSteps = typeof config.maxSteps === 'number' ? config.maxSteps : 20;
    const sourceHintElement = document.getElementById('sequenceSourceHint');

    if (sourceHintElement) {
        const sourceLabel = config.sequenceSource === 'littlefs' ? 'LittleFS (/sequence.json)' : 'compiled defaults';
        sourceHintElement.textContent = `Sequence source: ${sourceLabel}`;
    }

    const rows = Array.from({ length: maxSteps }, (_, index) => {
        const step = sequence[index];
        const enabled = !!step;
        const kind = step?.kind || 'sound';
        const tag = step?.tag || (sounds[0]?.tag || '');
        const durationMs = step?.durationMs || 1000;

        return `
            <div class="sequence-editor-row" data-step-index="${index}">
                <div>${index + 1}</div>
                <div><input class="sequence-enabled" type="checkbox" ${enabled ? 'checked' : ''}></div>
                <div>
                    <select class="sequence-kind">
                        <option value="sound"${kind === 'sound' ? ' selected' : ''}>sound</option>
                        <option value="pause"${kind === 'pause' ? ' selected' : ''}>pause</option>
                    </select>
                </div>
                <div>
                    <select class="sequence-tag">
                        ${createSoundOptions(sounds, tag)}
                    </select>
                </div>
                <div>
                    <input class="sequence-duration" type="number" min="1" step="100" value="${durationMs}">
                </div>
            </div>
        `;
    }).join('');

    container.innerHTML = `
        <div class="sequence-editor-grid sequence-editor-head">
            <div>#</div>
            <div>Use</div>
            <div>Type</div>
            <div>Sound</div>
            <div>Time ms</div>
        </div>
        ${rows}
    `;

    const rowElements = container.querySelectorAll('.sequence-editor-row');
    rowElements.forEach((row) => {
        row.querySelector('.sequence-enabled')?.addEventListener('change', () => toggleSequenceEditorRow(row));
        row.querySelector('.sequence-kind')?.addEventListener('change', () => toggleSequenceEditorRow(row));
        toggleSequenceEditorRow(row);
    });
}

async function loadSequenceEditorConfig() {
    const container = document.getElementById('sequenceEditor');
    if (!container) {
        return;
    }

    try {
        const response = await fetch('/sequence-config');
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        renderSequenceEditor(container, data);

        const statusElement = document.getElementById('sequenceEditorStatus');
        if (statusElement) {
            statusElement.textContent = 'Configuration loaded.';
        }
    } catch (error) {
        container.innerHTML = `<p>Failed to load sequence editor: ${error.message}</p>`;
    }
}

async function saveSequenceEditorConfig() {
    const container = document.getElementById('sequenceEditor');
    const statusElement = document.getElementById('sequenceEditorStatus');
    if (!container) {
        return;
    }

    const rows = Array.from(container.querySelectorAll('.sequence-editor-row'));
    const sequence = rows
        .filter((row) => row.querySelector('.sequence-enabled')?.checked)
        .map((row) => {
            const kind = row.querySelector('.sequence-kind')?.value || 'sound';
            const tag = kind === 'pause' ? '' : (row.querySelector('.sequence-tag')?.value || '');
            const durationMs = Number(row.querySelector('.sequence-duration')?.value || 0);
            return { kind, tag, durationMs };
        });

    if (!sequence.length) {
        if (statusElement) {
            statusElement.textContent = 'At least one step is required.';
        }
        return;
    }

    if (statusElement) {
        statusElement.textContent = 'Saving sequence ...';
    }

    try {
        const response = await fetch('/sequence-config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ sequence })
        });
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.error || `HTTP ${response.status}`);
        }

        if (statusElement) {
            statusElement.textContent = JSON.stringify(data, null, 2);
        }

        await loadSequenceEditorConfig();
        await reloadSoundConfigViews();
    } catch (error) {
        if (statusElement) {
            statusElement.textContent = `Failed to save sequence: ${error.message}`;
        }
    }
}

function wireSequenceEditor(article) {
    const editor = article.querySelector('[data-sequence-editor-source]');
    if (!editor) {
        return;
    }

    article.querySelector('#loadSequenceEditorButton')?.addEventListener('click', () => {
        loadSequenceEditorConfig();
    });

    article.querySelector('#saveSequenceEditorButton')?.addEventListener('click', () => {
        saveSequenceEditorConfig();
    });

    loadSequenceEditorConfig();
}

function renderSoundButtons(container, soundConfig) {
    const sounds = Array.isArray(soundConfig.sounds) ? soundConfig.sounds : [];
    if (!sounds.length) {
        container.innerHTML = '<p>No sounds configured.</p>';
        return;
    }

    container.innerHTML = sounds.map((sound) => (
        `<button type="button" data-sound-tag="${sound.tag}" title="${sound.description}">${sound.buttonLabel}</button>`
    )).join('\n');

    const statusElement = document.getElementById('soundStatus');
    if (statusElement && soundConfig.selectedSound) {
        statusElement.textContent = `Selected sound: ${soundConfig.selectedSound}`;
    }

    const buttons = container.querySelectorAll('[data-sound-tag]');
    buttons.forEach((button) => {
        button.addEventListener('click', () => {
            triggerSound(button.dataset.soundTag);
        });
    });
}

function renderVolumeControl(container, soundConfig) {
    if (typeof soundConfig.volume !== 'number') {
        container.innerHTML = '<p>Volume control unavailable.</p>';
        return;
    }

    container.innerHTML = `
        <label for="volumeSlider">Volume: <strong id="volumeValue">${soundConfig.volume}</strong></label>
        <input id="volumeSlider" type="range" min="${soundConfig.volumeMin}" max="${soundConfig.volumeMax}" value="${soundConfig.volume}">
    `;

    const slider = container.querySelector('#volumeSlider');
    const valueLabel = container.querySelector('#volumeValue');
    if (!slider || !valueLabel) {
        return;
    }

    slider.addEventListener('input', () => {
        valueLabel.textContent = slider.value;
    });

    slider.addEventListener('change', async () => {
        const statusElement = document.getElementById('soundStatus');
        if (statusElement) {
            statusElement.textContent = `Setting volume: ${slider.value} ...`;
        }

        try {
            const response = await fetch(`/volume?value=${encodeURIComponent(slider.value)}`);
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            valueLabel.textContent = data.volume;
            slider.value = data.volume;
            if (statusElement) {
                statusElement.textContent = JSON.stringify(data, null, 2);
            }
        } catch (error) {
            if (statusElement) {
                statusElement.textContent = `Failed to set volume: ${error.message}`;
            }
        }
    });
}

function renderSequenceControl(container, soundConfig) {
    const sequence = Array.isArray(soundConfig.sequence) ? soundConfig.sequence : [];
    if (!sequence.length) {
        container.innerHTML = '<p>No sequence configured.</p>';
        return;
    }

    const sequenceItems = sequence.map((step, index) => {
        const label = step.kind === 'pause'
            ? `Pause ${step.durationMs} ms`
            : `${step.tag} for ${step.durationMs} ms`;
        return `<li>${index + 1}. ${label}</li>`;
    }).join('');

    container.innerHTML = `
        <button type="button" id="sequenceStartButton">${soundConfig.sequenceRunning ? 'Sequence running...' : 'Start endless sequence'}</button>
        <button type="button" id="sequenceStopButton">Stop sequence</button>
        <ul>${sequenceItems}</ul>
    `;

    const button = container.querySelector('#sequenceStartButton');
    const stopButton = container.querySelector('#sequenceStopButton');
    if (!button || !stopButton) {
        return;
    }

    button.disabled = !!soundConfig.sequenceRunning;
    button.addEventListener('click', async () => {
        const statusElement = document.getElementById('soundStatus');
        if (statusElement) {
            statusElement.textContent = 'Starting sequence ...';
        }

        try {
            const response = await fetch('/sequence?action=start');
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            if (statusElement) {
                statusElement.textContent = JSON.stringify(data, null, 2);
            }

            await reloadSoundConfigViews();
        } catch (error) {
            if (statusElement) {
                statusElement.textContent = `Failed to start sequence: ${error.message}`;
            }
        }
    });

    stopButton.disabled = !soundConfig.sequenceRunning;
    stopButton.addEventListener('click', async () => {
        const statusElement = document.getElementById('soundStatus');
        if (statusElement) {
            statusElement.textContent = 'Stopping sequence ...';
        }

        try {
            const response = await fetch('/sequence?action=stop');
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            if (statusElement) {
                statusElement.textContent = JSON.stringify(data, null, 2);
            }

            await reloadSoundConfigViews();
            await refreshSequenceState();
        } catch (error) {
            if (statusElement) {
                statusElement.textContent = `Failed to stop sequence: ${error.message}`;
            }
        }
    });
}

function renderSequenceState(element, state) {
    if (!state.sequenceRunning) {
        element.textContent = 'Sequence idle.';
        return;
    }

    const step = state.currentStep;
    if (!step) {
        element.textContent = 'Sequence running, waiting for first step.';
        return;
    }

    const stepLabel = step.kind === 'pause'
        ? `Pause for ${step.durationMs} ms`
        : `Playing ${step.tag} for ${step.durationMs} ms`;

    element.textContent = `Sequence running\nStep ${state.currentIndex + 1}\n${stepLabel}\nElapsed: ${state.elapsedMs} ms\nRemaining: ${state.remainingMs} ms`;
}

async function refreshSequenceState() {
    const stateElement = document.getElementById('sequenceState');
    if (!stateElement) {
        return;
    }

    try {
        const response = await fetch('/sequence-state');
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        renderSequenceState(stateElement, data);
    } catch (error) {
        stateElement.textContent = `Failed to load sequence state: ${error.message}`;
    }
}

function stopSoundStatePolling() {
    if (soundStatePollTimer !== null) {
        clearInterval(soundStatePollTimer);
        soundStatePollTimer = null;
    }
}

function stopUltrasoundStatePolling() {
    if (ultrasoundStatePollTimer !== null) {
        clearInterval(ultrasoundStatePollTimer);
        ultrasoundStatePollTimer = null;
    }
}

function startSoundStatePolling() {
    stopSoundStatePolling();
    refreshSequenceState();
    soundStatePollTimer = setInterval(() => {
        refreshSequenceState();
    }, 500);
}

function renderUltrasoundState(element, state) {
    if (!state.playing) {
        element.textContent = JSON.stringify({
            ...state,
            message: 'Ultrasound idle.'
        }, null, 2);
        return;
    }

    element.textContent = JSON.stringify(state, null, 2);
}

async function refreshUltrasoundState() {
    const statusElement = document.getElementById('ultrasoundStatus');
    if (!statusElement) {
        return;
    }

    try {
        const response = await fetch('/ultrasound-state');
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        renderUltrasoundState(statusElement, data);
    } catch (error) {
        statusElement.textContent = `Failed to load ultrasound state: ${error.message}`;
    }
}

function startUltrasoundStatePolling() {
    stopUltrasoundStatePolling();
    refreshUltrasoundState();
    ultrasoundStatePollTimer = setInterval(() => {
        refreshUltrasoundState();
    }, 250);
}

function wireUltrasoundControls(article) {
    const statusElement = article.querySelector('#ultrasoundStatus');
    const frequencyInput = article.querySelector('#ultrasoundFrequency');
    const minFrequencyInput = article.querySelector('#ultrasoundMinFrequency');
    const maxFrequencyInput = article.querySelector('#ultrasoundMaxFrequency');
    const volumeInput = article.querySelector('#ultrasoundVolume');
    const durationInput = article.querySelector('#ultrasoundDuration');
    const startButton = article.querySelector('#ultrasoundStartButton');
    const randomStartButton = article.querySelector('#ultrasoundRandomStartButton');
    const stopButton = article.querySelector('#ultrasoundStopButton');
    const refreshButton = article.querySelector('#ultrasoundRefreshButton');

    if (!statusElement || !frequencyInput || !minFrequencyInput || !maxFrequencyInput || !volumeInput || !durationInput || !startButton || !randomStartButton || !stopButton || !refreshButton) {
        return;
    }

    startButton.addEventListener('click', async () => {
        statusElement.textContent = 'Starting ultrasound ...';

        try {
            const frequency = Number(frequencyInput.value);
            const volume = Number(volumeInput.value);
            const duration = Number(durationInput.value);
            const response = await fetch(`/ultrasound?action=start&frequency=${encodeURIComponent(frequency)}&volume=${encodeURIComponent(volume)}&duration=${encodeURIComponent(duration)}`);
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            renderUltrasoundState(statusElement, data);
            startUltrasoundStatePolling();
        } catch (error) {
            statusElement.textContent = `Failed to start ultrasound: ${error.message}`;
        }
    });

    randomStartButton.addEventListener('click', async () => {
        statusElement.textContent = 'Starting random ultrasound ...';

        try {
            const minFrequency = Number(minFrequencyInput.value);
            const maxFrequency = Number(maxFrequencyInput.value);
            const volume = Number(volumeInput.value);
            const duration = Number(durationInput.value);
            const response = await fetch(`/ultrasound?action=random&minFrequency=${encodeURIComponent(minFrequency)}&maxFrequency=${encodeURIComponent(maxFrequency)}&volume=${encodeURIComponent(volume)}&duration=${encodeURIComponent(duration)}`);
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            renderUltrasoundState(statusElement, data);
            startUltrasoundStatePolling();
        } catch (error) {
            statusElement.textContent = `Failed to start random ultrasound: ${error.message}`;
        }
    });

    stopButton.addEventListener('click', async () => {
        statusElement.textContent = 'Stopping ultrasound ...';

        try {
            const response = await fetch('/ultrasound?action=stop');
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || `HTTP ${response.status}`);
            }

            renderUltrasoundState(statusElement, data);
            await refreshUltrasoundState();
        } catch (error) {
            statusElement.textContent = `Failed to stop ultrasound: ${error.message}`;
        }
    });

    refreshButton.addEventListener('click', () => {
        refreshUltrasoundState();
    });

    startUltrasoundStatePolling();
}

async function loadSoundConfigIntoElement(element) {
    const source = element.dataset.soundConfigSource;
    if (!source) {
        return;
    }

    try {
        const response = await fetch(source);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status} for ${source}`);
        }

        const data = await response.json();
        if (element.id === 'soundButtons') {
            renderSoundButtons(element, data);
        } else if (element.id === 'volumeControl') {
            renderVolumeControl(element, data);
        } else if (element.id === 'sequenceControl') {
            renderSequenceControl(element, data);
        }
    } catch (error) {
        element.innerHTML = `<p>Failed to load sound config: ${error.message}</p>`;
    }
}

async function reloadSoundConfigViews() {
    const article = document.getElementById('articleId');
    if (!article) {
        return;
    }

    const soundConfigTargets = article.querySelectorAll('[data-sound-config-source]');
    soundConfigTargets.forEach((element) => {
        loadSoundConfigIntoElement(element);
    });
}

function renderAdminSoundConfig(element, soundConfig) {
    const sounds = Array.isArray(soundConfig.sounds) ? soundConfig.sounds : [];
    if (!sounds.length) {
        element.innerHTML = '<p>No sounds configured.</p>';
        return;
    }

    element.innerHTML = sounds.map((sound) => `
        <div class="sound-admin-card">
            <div class="sound-admin-header">${sound.buttonLabel}</div>
            <div><strong>Tag:</strong> ${sound.tag}</div>
            <div><strong>Description:</strong> ${sound.description}</div>
            <div><strong>Folder:</strong> ${sound.folder}</div>
            <div><strong>Track:</strong> ${sound.track}</div>
        </div>
    `).join('');
}

async function loadAdminSoundConfigIntoElement(element) {
    const source = element.dataset.adminSoundConfigSource;
    if (!source) {
        return;
    }

    try {
        const response = await fetch(source);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status} for ${source}`);
        }

        const data = await response.json();
        renderAdminSoundConfig(element, data);
    } catch (error) {
        element.innerHTML = `<p>Failed to load admin sound config: ${error.message}</p>`;
    }
}

function loadEmbeddedData(article) {
    const jsonTargets = article.querySelectorAll('[data-json-source]');
    jsonTargets.forEach((element) => {
        if (element.dataset.jsonAutoload === 'false') {
            return;
        }

        loadJsonIntoElement(element);
    });

    const soundConfigTargets = article.querySelectorAll('[data-sound-config-source]');
    soundConfigTargets.forEach((element) => {
        loadSoundConfigIntoElement(element);
    });

    const adminSoundConfigTargets = article.querySelectorAll('[data-admin-sound-config-source]');
    adminSoundConfigTargets.forEach((element) => {
        loadAdminSoundConfigIntoElement(element);
    });

    wireSequenceEditor(article);
    wireUltrasoundControls(article);

    wireRefreshButtons(article);
}

async function loadArticle(articlePath) {
    try {
        const articleHtml = await fetchText(articlePath);
        setMainContent(articleHtml);
        const article = document.getElementById('articleId');
        if (article) {
            loadEmbeddedData(article);
        }
    } catch (error) {
        renderLoadError(error);
    }
}

async function loadCurrentRoute() {
    stopSoundStatePolling();
    stopUltrasoundStatePolling();

    const routeName = getCurrentRoute();
    const articlePath = routes[routeName] || routes.assembly;

    updateActiveNavigation(routeName in routes ? routeName : 'assembly');
    await loadArticle(articlePath);

    if ((routeName in routes ? routeName : 'assembly') === 'sounds') {
        startSoundStatePolling();
    }

    if ((routeName in routes ? routeName : 'assembly') === 'ultrasound') {
        startUltrasoundStatePolling();
    }
}

function initializeApp() {
    if (!window.location.hash) {
        window.location.hash = '#assembly';
        return;
    }

    loadCurrentRoute();
}

window.addEventListener('hashchange', loadCurrentRoute);
window.addEventListener('DOMContentLoaded', initializeApp);

async function triggerSound(soundName) {
    const statusElement = document.getElementById('soundStatus');
    if (statusElement) {
        statusElement.textContent = `Sending sound: ${soundName} ...`;
    }

    try {
        const response = await fetch(`/sound?name=${encodeURIComponent(soundName)}`);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        if (statusElement) {
            statusElement.textContent = JSON.stringify(data, null, 2);
        }
        await reloadSoundConfigViews();
    } catch (error) {
        if (statusElement) {
            statusElement.textContent = `Failed to send sound: ${error.message}`;
        }
    }
}
