// BeeSMART Honey Dosing System - Modern Web Interface
// Author: AI Assistant based on Mogens Groth Nicolaisen's original work

class BeeSMART {
    constructor() {
        this.pollingInterval = null;
        this.currentLanguage = 0; // 0: DA, 1: DE, 2: EN
        this.isConnected = false;
        this.settings = {
            desiredAmount: 300,
            kp: 8,
            ti: 10,
            kd: 5,
            servoMin: 90,
            servoMax: 90,
            stopHysteresis: 5,
            minGlassWeight: 10,
            maxWeight: 1000,
            viscosity: 0,
            calWeight: 250,
            autoState: false
        };
        
        // Language strings matching the Arduino code
        this.languages = {
            tab1Text: ["Tapning", "Abfüllung", "Filling"],
            tab2Text: ["Indstillinger", "Einstellungen", "Settings"],
            tab3Text: ["Avanceret", "Erweiterte Einstellungen", "Advanced settings"],
            tab4Text: ["Kalibrering", "Kalibrierung Wage", "Calibration Scale"],
            autoText: ["Automatisk start af tapning", "Automatischer Start des Abfüllens", "Automatic start of filling"],
            SPText: ["Ønsket Mængde", "Gewünschte Menge", "Desired quantity"],
            PVText: ["Aktuel Honning Vægt (uden glas):", "Aktuelles Honig Gewicht (ohne Glas):", "Current Honey Weight (ex. glass):"],
            tareText: ["Aktuel Vægt:", "Aktuelles Gewicht:", "Current Weight:"],
            servoText: ["Indstil min og max Servo position", "Min und max Servoposition einstellen", "Set min and max servo position"],
            servoMinButtonText: ["Gå til min position", "Gehe zu min Position", "Go to min position"],
            servoMaxButtonText: ["Gå til max position", "Gehe zu max Position", "Go to max position"],
            tap3Heading: ["Kontrol Parametre", "Regelparameter", "Control Parameters"],
            langHeading: ["Sprog", "Sprache", "Language"],
            stopText: ["Stop aktiveret !", "Stop aktiviert !", "Stop activated !"],
            prestopTextHeading: ["Luk hanen når der mangler [g]", "Abfüllen abbrechen wenn fehlt [g]", "Close tap when missing [g]"],
            minGlassWeightTextHeading: ["Glasregistrering: Glas vejer mere end [g]", "Glasregistrierung: Glas wiegt mehr als [g]", "Glass registration: Glass weighs more than [g]"],
            maxWeightTextHeading: ["Max tappemængde [g]", "Maximaler Abfüll Menge [g]", "Maximum fill quantity [g]"],
            tareTextHeading: ["Tare kun med tom vægt !", "Tare nur mit Leergewicht verwenden !", "Tare with empty scale only !"],
            viscosityHeading: ["Viskositet", "Viskosität", "Viscosity"],
            viscosityLow: ["Lav", "Niedrig", "Low"],
            viscosityMedium: ["Medium", "Mittel", "Medium"],
            viscosityHigh: ["Høj", "Hoch", "High"],
            viscosityCustom: ["Brugerdefineret", "Benutzerdefiniert", "Custom"],
            calSelectorText: ["Kalibreringsvægt", "Kalibriergewicht", "Calibration weight"],
            calButtonText: ["Kalibrer", "Kalibrieren", "Calibrate"],
            calStep1Text: ["System kalibreret, tryk Kalibrer for at starte re-kalibrering", "System kalibriert, für Neukalibrierung drücken Sie Kalibrieren", "System calibrated, press Calibrate to start re-calibration"],
            calStep2Text: ["Tøm vægt og tryk Kalibrer", "Gewicht entfernen und Kalibrieren drücken", "Remove any weight from scale and press Calibrate"],
            calStep3Text: ["Kalibrerer...", "Kalibriert...", "Calibrating..."],
            calStep4Text: ["Placer kalibreringsvægt og tryk Kalibrer", "Kalibriergewicht auflegen und Kalibrieren drücken", "Place calibration weight on scale and press calibrate"],
            saveText: ["Gem", "Speichern", "Save"],
            saveTextHeading: ["Gem indstillinger", "Einstellungen speichern", "Save settings"],
            saveStateText: ["Indstillinger gemt", "Einstellungen gespeichert", "Settings saved"],
            step1Text: ["Tryk start", "Start drücken", "Press start"],
            step2Text: ["Placer glas på vægt", "Glas auf die Wage platzieren", "Place glass on scale"],
            step3Text: ["Fylder glas...", "Glas füllen...", "Filling glass..."],
            step4Text: ["Glas fyldt - fjern glas", "Glas gefüllt - Glas entfernen", "Glass filled - remove glass"],
            startText: ["Start", "Start", "Start"],
            stopText: ["Stop", "Stop", "Stop"],
            tareButton: ["Tare", "Tarieren", "Tare"]
        };
        
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.initializePolling();
        this.setupTabNavigation();
        this.setupSliders();
        this.loadSettings();
        this.updateLanguage();
    }

    setupEventListeners() {
        // Tab navigation
        document.querySelectorAll('.tab-button').forEach(button => {
            button.addEventListener('click', (e) => {
                this.switchTab(e.target.closest('.tab-button').dataset.tab);
            });
        });

        // Start/Stop buttons
        document.getElementById('startButton').addEventListener('click', () => {
            this.sendCommand('start');
        });

        document.getElementById('stopButton').addEventListener('click', () => {
            this.sendCommand('stop');
        });

        // Auto switch
        document.getElementById('autoSwitch').addEventListener('change', (e) => {
            this.settings.autoState = e.target.checked;
            this.sendCommand('setAuto', { value: e.target.checked });
        });

        // Tare button
        document.getElementById('tareButton').addEventListener('click', () => {
            this.sendCommand('tare');
        });



        // Calibrate button
        document.getElementById('calibrateButton').addEventListener('click', () => {
            this.sendCommand('calibrate');
        });

        // Servo test buttons
        document.getElementById('servoMinButton').addEventListener('click', () => {
            this.sendCommand('servoTest', { position: 'min' });
        });

        document.getElementById('servoMaxButton').addEventListener('click', () => {
            this.sendCommand('servoTest', { position: 'max' });
        });

        // Viscosity selection
        document.querySelectorAll('input[name="viscosity"]').forEach(radio => {
            radio.addEventListener('change', (e) => {
                this.settings.viscosity = parseInt(e.target.value);
                
                // If switching to User Defined, fetch current settings from backend
                // to get the saved user-defined PID values
                if (this.settings.viscosity === 0) {
                    this.sendCommand('setViscosity', { value: this.settings.viscosity });
                    // Fetch fresh settings to get saved user-defined PID values
                    setTimeout(() => this.fetchSettings(), 200);
                } else {
                    this.updatePIDPresets();
                    this.sendCommand('setViscosity', { value: this.settings.viscosity });
                }
            });
        });

        // Language selection
        document.querySelectorAll('input[name="language"]').forEach(radio => {
            radio.addEventListener('change', (e) => {
                this.currentLanguage = parseInt(e.target.value);
                this.updateLanguage();
                this.sendCommand('setLanguage', { value: this.currentLanguage });
            });
        });

        // Number inputs
        this.setupNumberInputs();
    }

    setupNumberInputs() {
        const inputs = [
            { id: 'amountInput', setting: 'desiredAmount', command: 'setAmount' },
            { id: 'kpInput', setting: 'kp', command: 'setPID' },
            { id: 'tiInput', setting: 'ti', command: 'setPID' },
            { id: 'kdInput', setting: 'kd', command: 'setPID' },
            { id: 'stopHysteresisInput', setting: 'stopHysteresis', command: 'setStopHysteresis' },
            { id: 'minGlassWeightInput', setting: 'minGlassWeight', command: 'setMinGlassWeight' },
            { id: 'maxWeightInput', setting: 'maxWeight', command: 'setMaxWeight' },
            { id: 'calWeightInput', setting: 'calWeight', command: 'setCalWeight' }
        ];

        inputs.forEach(input => {
            const element = document.getElementById(input.id);
            element.addEventListener('change', (e) => {
                const value = parseFloat(e.target.value);
                this.settings[input.setting] = value;
                
                // Special handling for PID parameters
                if (input.command === 'setPID') {
                    const pidPayload = {};
                    pidPayload[input.setting] = value;  // Use parameter name as key
                    this.sendCommand(input.command, pidPayload);
                } else {
                    this.sendCommand(input.command, { value: value });
                }
                
                this.updateRelatedElements(input.setting, value);
            });
        });
    }

    setupSliders() {
        // Amount slider
        const amountSlider = document.getElementById('amountSlider');
        const amountInput = document.getElementById('amountInput');
        
        amountSlider.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.settings.desiredAmount = value;
            amountInput.value = value;
            document.getElementById('sliderValue').textContent = value + 'g';
            document.getElementById('desiredAmount').textContent = value + ' g';
        });

        amountSlider.addEventListener('change', (e) => {
            this.sendCommand('setAmount', { value: parseInt(e.target.value) });
        });

        // Servo sliders
        const servoMinSlider = document.getElementById('servoMinSlider');
        const servoMaxSlider = document.getElementById('servoMaxSlider');

        servoMinSlider.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.settings.servoMin = value;
            document.getElementById('servoMinValue').textContent = value + '°';
        });

        servoMinSlider.addEventListener('change', (e) => {
            this.sendCommand('setServoMin', { value: parseInt(e.target.value) });
        });

        servoMaxSlider.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.settings.servoMax = value;
            document.getElementById('servoMaxValue').textContent = value + '°';
        });

        servoMaxSlider.addEventListener('change', (e) => {
            this.sendCommand('setServoMax', { value: parseInt(e.target.value) });
        });

        // Calibration weight slider
        const calWeightSlider = document.getElementById('calWeightSlider');
        const calWeightInput = document.getElementById('calWeightInput');

        calWeightSlider.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            this.settings.calWeight = value;
            calWeightInput.value = value;
            document.getElementById('calSliderValue').textContent = value + 'g';
        });

        calWeightSlider.addEventListener('change', (e) => {
            this.sendCommand('setCalWeight', { value: parseInt(e.target.value) });
        });
    }

    setupTabNavigation() {
        // Initialize first tab as active
        this.switchTab('filling');
    }

    switchTab(tabName) {
        // Remove active class from all tabs and content
        document.querySelectorAll('.tab-button').forEach(btn => btn.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));

        // Add active class to selected tab and content
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
        document.getElementById(tabName).classList.add('active');
    }

    initializePolling() {
        // Start HTTP polling
        this.isConnected = true;
        this.updateConnectionStatus();
        
        // Initial data fetch
        this.fetchStatus();
        this.fetchSettings();
        
        // Start periodic polling
        this.pollingInterval = setInterval(() => {
            this.fetchStatus();
        }, 200); // Poll every 200ms for responsive weight updates
        
        console.log('HTTP polling initialized');
    }

    async fetchStatus() {
        try {
            const response = await fetch('/api/status');
            if (response.ok) {
                const data = await response.json();
                this.handleStatusUpdate(data);
                this.isConnected = true;
            } else {
                throw new Error('HTTP error: ' + response.status);
            }
        } catch (error) {
            console.error('Status fetch failed:', error);
            this.isConnected = false;
        }
        this.updateConnectionStatus();
    }

    async fetchSettings() {
        try {
            const response = await fetch('/api/settings');
            if (response.ok) {
                const data = await response.json();
                this.updateSettings(data);
            }
        } catch (error) {
            console.error('Settings fetch failed:', error);
        }
    }

    handleStatusUpdate(data) {
        // Handle status
        this.updateStatus({
            running: data.running,
            message: data.message
        });
        
        // Handle weights
        if (data.weights) {
            this.updateWeights(data.weights);
        }
        
        // Handle calibration
        if (data.calibration) {
            this.updateCalibrationStatus(data.calibration);
        }
        

    }

    updateStatus(status) {
        // Update status message
        const statusElement = document.getElementById('statusMessage');
        statusElement.textContent = status.message;
        
        // Update button states
        const startButton = document.getElementById('startButton');
        if (status.running) {
            startButton.style.backgroundColor = '#9BC268';
        } else {
            startButton.style.backgroundColor = '';
        }
    }

    updateWeights(weights) {
        document.getElementById('honeyWeight').textContent = weights.honey + ' g';
        document.getElementById('tareWeight').textContent = weights.total + ' g';
        document.getElementById('calCurrentWeight').textContent = weights.total + ' g';
    }

    updateSettings(newSettings) {
        Object.assign(this.settings, newSettings);
        
        // Handle language setting separately
        if (newSettings.hasOwnProperty('language')) {
            this.currentLanguage = newSettings.language;
            this.updateLanguage();
        }
        
        this.applySettingsToUI();
    }

    updateCalibrationStatus(calStatus) {
        document.getElementById('calStatusMessage').textContent = calStatus.message;
    }

    applySettingsToUI() {
        // Update all UI elements with current settings
        document.getElementById('amountSlider').value = this.settings.desiredAmount;
        document.getElementById('amountInput').value = this.settings.desiredAmount;
        document.getElementById('sliderValue').textContent = this.settings.desiredAmount + 'g';
        document.getElementById('desiredAmount').textContent = this.settings.desiredAmount + ' g';

        document.getElementById('kpInput').value = this.settings.kp;
        document.getElementById('tiInput').value = this.settings.ti;
        document.getElementById('kdInput').value = this.settings.kd;

        document.getElementById('servoMinSlider').value = this.settings.servoMin;
        document.getElementById('servoMaxSlider').value = this.settings.servoMax;
        document.getElementById('servoMinValue').textContent = this.settings.servoMin + '°';
        document.getElementById('servoMaxValue').textContent = this.settings.servoMax + '°';

        document.getElementById('stopHysteresisInput').value = this.settings.stopHysteresis;
        document.getElementById('minGlassWeightInput').value = this.settings.minGlassWeight;
        document.getElementById('maxWeightInput').value = this.settings.maxWeight;

        document.getElementById('calWeightSlider').value = this.settings.calWeight;
        document.getElementById('calWeightInput').value = this.settings.calWeight;
        document.getElementById('calSliderValue').textContent = this.settings.calWeight + 'g';

        document.getElementById('autoSwitch').checked = this.settings.autoState;

        // Update viscosity radio buttons
        document.querySelectorAll('input[name="viscosity"]').forEach((radio, index) => {
            radio.checked = (index === this.settings.viscosity);
        });
        
        // Enable/disable PID inputs based on viscosity selection
        const isUserDefined = (this.settings.viscosity === 0);
        document.getElementById('kpInput').disabled = !isUserDefined;
        document.getElementById('tiInput').disabled = !isUserDefined;  
        document.getElementById('kdInput').disabled = !isUserDefined;

        // Update max weight display
        document.getElementById('maxWeightDisplay').textContent = this.settings.maxWeight + 'g';
        document.getElementById('amountSlider').max = this.settings.maxWeight;
        document.getElementById('amountInput').max = this.settings.maxWeight;
    }

    updatePIDPresets() {
        const presets = [
            null,  // Custom - don't override, use current values
            { kp: 8, ti: 10, kd: 5 },    // Low  
            { kp: 8, ti: 7, kd: 2 },     // Medium
            { kp: 10, ti: 0, kd: 1 }     // High
        ];

        const isUserDefined = (this.settings.viscosity === 0);
        
        // Enable/disable PID inputs
        document.getElementById('kpInput').disabled = !isUserDefined;
        document.getElementById('tiInput').disabled = !isUserDefined;
        document.getElementById('kdInput').disabled = !isUserDefined;

        // Only update preset values for non-user-defined viscosity
        if (!isUserDefined) {
            const preset = presets[this.settings.viscosity];
            this.settings.kp = preset.kp;
            this.settings.ti = preset.ti; 
            this.settings.kd = preset.kd;

            document.getElementById('kpInput').value = preset.kp;
            document.getElementById('tiInput').value = preset.ti;
            document.getElementById('kdInput').value = preset.kd;
        }
        // For user-defined (viscosity 0), keep current values in UI
    }

    updateRelatedElements(setting, value) {
        if (setting === 'desiredAmount') {
            document.getElementById('amountSlider').value = value;
            document.getElementById('sliderValue').textContent = value + 'g';
            document.getElementById('desiredAmount').textContent = value + ' g';
        } else if (setting === 'calWeight') {
            document.getElementById('calWeightSlider').value = value;
            document.getElementById('calSliderValue').textContent = value + 'g';
        } else if (setting === 'maxWeight') {
            document.getElementById('maxWeightDisplay').textContent = value + 'g';
            document.getElementById('amountSlider').max = value;
            document.getElementById('amountInput').max = value;
        }
    }

    updateLanguage() {
        // Update all elements with language-specific text
        Object.keys(this.languages).forEach(key => {
            const elements = document.querySelectorAll(`[data-key="${key}"]`);
            elements.forEach(element => {
                const text = this.languages[key][this.currentLanguage];
                if (element.tagName === 'INPUT' && element.type === 'button') {
                    element.value = text;
                } else {
                    element.textContent = text;
                }
            });
        });

        // Update language radio buttons
        document.querySelectorAll('input[name="language"]').forEach((radio, index) => {
            radio.checked = (index === this.currentLanguage);
        });
    }

    async sendCommand(command, payload = {}) {
        try {
            const message = {
                command: command,
                payload: payload
            };
            
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(message)
            });
            
            if (!response.ok) {
                throw new Error('HTTP error: ' + response.status);
            }
            
            // Fetch updated status after command
            setTimeout(() => this.fetchStatus(), 100);
            
        } catch (error) {
            console.error('Command send failed:', command, error);
        }
    }



    loadSettings() {
        // Load settings from localStorage as fallback
        const saved = localStorage.getItem('beesmart-settings');
        if (saved) {
            try {
                const settings = JSON.parse(saved);
                Object.assign(this.settings, settings);
                this.applySettingsToUI();
            } catch (error) {
                console.error('Error loading saved settings:', error);
            }
        }
    }

    saveSettingsToStorage() {
        localStorage.setItem('beesmart-settings', JSON.stringify(this.settings));
    }

    // No reconnection needed for HTTP polling

    updateConnectionStatus() {
        const statusElement = document.getElementById('connectionStatus');
        const indicator = statusElement.querySelector('.connection-indicator');
        const text = statusElement.querySelector('span');

        if (this.isConnected) {
            statusElement.classList.add('connected');
            text.textContent = ['Forbundet', 'Verbunden', 'Connected'][this.currentLanguage];
        } else {
            statusElement.classList.remove('connected');
            text.textContent = ['Forbinder...', 'Verbinde...', 'Connecting...'][this.currentLanguage];
        }
    }


}

// Initialize the application when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.beeSMART = new BeeSMART();
});

// Handle page visibility changes for polling management
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        // Page is hidden, can reduce update frequency
        console.log('Page hidden, reducing polling frequency');
        if (window.beeSMART && window.beeSMART.pollingInterval) {
            clearInterval(window.beeSMART.pollingInterval);
            window.beeSMART.pollingInterval = setInterval(() => {
                window.beeSMART.fetchStatus();
            }, 5000); // Slower polling when hidden
        }
    } else {
        // Page is visible again, resume normal polling
        console.log('Page visible, resuming normal polling');
        if (window.beeSMART && window.beeSMART.pollingInterval) {
            clearInterval(window.beeSMART.pollingInterval);
            window.beeSMART.pollingInterval = setInterval(() => {
                window.beeSMART.fetchStatus();
            }, 200); // Fast polling frequency for responsive updates
        }
    }
});

// Handle window beforeunload for cleanup
window.addEventListener('beforeunload', () => {
    if (window.beeSMART) {
        window.beeSMART.saveSettingsToStorage();
        if (window.beeSMART.pollingInterval) {
            clearInterval(window.beeSMART.pollingInterval);
        }
    }
});