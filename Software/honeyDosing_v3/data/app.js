/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║                  BeeSMART Honey Dosing System - Web Interface                ║
 * ║                                Version 3.1.0                                 ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 * 
 * @file app.js
 * @description Modern, responsive web interface for the BeeSMART honey dosing system.
 * Features HTTP polling architecture for maximum compatibility and reliability.
 * 
 * ARCHITECTURE:
 * =============
 * • HTTP Polling: 200ms intervals for real-time weight updates
 * • RESTful API: GET /api/status, GET /api/settings, POST /api/command
 * • Responsive UI: iOS-inspired design with golden honey theme
 * • Multi-language: Danish, German, English support
 * • Local Storage: Persistent statistics across sessions
 * 
 * KEY FEATURES:
 * =============
 * • Real-time weight monitoring with 200ms polling
 * • Automatic mode for continuous filling
 * • PID parameter tuning for different honey viscosities
 * • Servo calibration and testing
 * • Scale calibration with progress indication
 * • Performance statistics with visualization
 * • Error distribution analytics
 * 
 * @author Mogens Groth Nicolaisen
 * @license Open Source - Please maintain attribution
 * @date November 2025
 */

/**
 * Main application class for BeeSMART honey dosing system
 * @class BeeSMART
 */
class BeeSMART {
    /**
     * Create a BeeSMART application instance
     * @constructor
     */
    constructor() {
        //═══════════════════════════════════════════════════════════════════════
        // NETWORK COMMUNICATION STATE
        //═══════════════════════════════════════════════════════════════════════
        /** @type {?number} Polling interval timer ID */
        this.pollingInterval = null;
        
        /** @type {boolean} Connection status to ESP32 backend */
        this.isConnected = false;
        
        /** @type {boolean} Flag to prevent concurrent status fetches */
        this.isFetchingStatus = false;
        
        /** @type {boolean} Flag to prevent concurrent settings fetches */
        this.isFetchingSettings = false;

        //═══════════════════════════════════════════════════════════════════════
        // SYSTEM CONFIGURATION
        //═══════════════════════════════════════════════════════════════════════
        /** @type {number} Current language index (0: Danish, 1: German, 2: English) */
        this.currentLanguage = 0;
        
        /** @constant {number} Minimum allowed servo angle */
        this.SERVO_MIN_ANGLE = 0;
        
        /** @constant {number} Maximum allowed servo angle */
        this.SERVO_MAX_ANGLE = 180;
        
        /** @constant {number} Default minimum servo position (must match firmware) */
        this.SERVO_DEFAULT_MIN = 0;
        
        /** @constant {number} Default maximum servo position (must match firmware) */
        this.SERVO_DEFAULT_MAX = 90;

        /**
         * System settings synchronized with ESP32 firmware
         * @type {Object}
         * @property {number} desiredAmount - Target honey weight in grams
         * @property {number} kp - PID proportional gain
         * @property {number} ti - PID integral time constant
         * @property {number} kd - PID derivative gain
         * @property {number} servoMin - Minimum servo angle (closed position)
         * @property {number} servoMax - Maximum servo angle (open position)
         * @property {number} stopHysteresis - Distance before target to stop filling (grams)
         * @property {number} minGlassWeight - Minimum weight for glass detection (grams)
         * @property {number} maxWeight - Maximum dispensing amount (grams)
         * @property {number} viscosity - Viscosity preset (0: Custom, 1: Low, 2: Medium, 3: High)
         * @property {number} calWeight - Calibration weight value (grams)
         * @property {boolean} autoState - Automatic mode enabled
         * @property {number} lang - Language preference (synced with firmware)
         */
        this.settings = {
            desiredAmount: 300,
            kp: 8,
            ti: 10,
            kd: 5,
            servoMin: this.SERVO_DEFAULT_MIN,
            servoMax: this.SERVO_DEFAULT_MAX,
            stopHysteresis: 5,
            minGlassWeight: 10,
            maxWeight: 1000,
            viscosity: 2,
            calWeight: 250,
            autoState: false, // Default to off for safety
            lang: 0           // Persisted language (mirror of firmware 'language')
        };
        
        //═══════════════════════════════════════════════════════════════════════
        // STATISTICS TRACKING SYSTEM
        //═══════════════════════════════════════════════════════════════════════
        /**
         * Performance statistics synchronized with backend
         * @type {Object}
         * @property {Array} dispensingHistory - Recent dispensing records from backend (last 10)
         * @property {number} totalDispensed - Cumulative weight dispensed in kg
         * @property {number} cycleCount - Total number of dispensing cycles (persistent)
         * @property {number} averageError - Average dispensing error in grams
         * @property {Object} errorStats - Error distribution percentages
         * @property {number} errorStats.overshoot - Percentage over target
         * @property {number} errorStats.undershoot - Percentage under target
         * @property {number} errorStats.perfect - Percentage within ±2g tolerance
         */
        this.statisticsData = {
            dispensingHistory: [], // Recent dispensing records from backend
            totalDispensed: 0,    // Total weight dispensed (kg) – cumulative if firmware provides persistent stats
            cycleCount: 0,        // Total number of dispensing cycles (persistent)
            averageError: 0,      // Average dispensing error in grams – cumulative if available
            errorStats: {
                overshoot: 0,     // Percentage of cycles with positive error
                undershoot: 0,    // Percentage of cycles with negative error  
                perfect: 0        // Percentage of cycles within ±2g tolerance
            }
        };
        
        //═══════════════════════════════════════════════════════════════════════
        // CHART VISUALIZATION
        //═══════════════════════════════════════════════════════════════════════
        /** @type {?Object} Chart object for accuracy visualization */
        this.accuracyChart = null;
        
        //═══════════════════════════════════════════════════════════════════════
        // MULTI-LANGUAGE SUPPORT SYSTEM
        //═══════════════════════════════════════════════════════════════════════
        /**
         * Localized text strings for all UI elements
         * Each key contains an array with [Danish, German, English] translations
         * @type {Object.<string, string[]>}
         */
        this.languages = {
            // Navigation tabs
            tab1Text: ["Tapning", "Abfüllung", "Filling"],
            tab2Text: ["Indstillinger", "Einstellungen", "Settings"],
            tab3Text: ["Avanceret", "Erweiterte Einstellungen", "Advanced settings"],
            tab4Text: ["Statistik", "Statistiken", "Statistics"],
            
            // PID Control section
            pidHeading: ["PID Kontrol Parametre", "PID-Steuerungsparameter", "PID Control Parameters"],
            viscosityHeading: ["Viskositet", "Viskosität", "Viscosity"],
            viscosityCustom: ["Brugerdefineret", "Benutzerdefiniert", "User Defined"],
            viscosityLow: ["Lav", "Niedrig", "Low"],
            viscosityMedium: ["Medium", "Mittel", "Medium"],
            viscosityHigh: ["Høj", "Hoch", "High"],
            tap3Heading: ["Kontrol Parametre", "Regelparameter", "Control Parameters"],
            pidKpLabel: ["Kp", "Kp", "Kp"],
            pidTiLabel: ["Ti", "Ti", "Ti"],
            pidKdLabel: ["Kd", "Kd", "Kd"],
            
            // Main interface
            autoText: ["Automatisk start af tapning", "Automatischer Start des Abfüllens", "Automatic start of filling"],
            SPText: ["Ønsket Mængde", "Gewünschte Menge", "Desired quantity"],
            PVText: ["Aktuel Honning Vægt (uden glas):", "Aktuelles Honig Gewicht (ohne Glas):", "Current Honey Weight (ex. glass):"],
            tareText: ["Aktuel Vægt:", "Aktuelles Gewicht:", "Current Weight:"],
            
            // Servo controls
            servoText: ["Indstil min og max Servo position", "Min und max Servoposition einstellen", "Set min and max servo position"],
            servoMinButtonText: ["Gå til min position", "Gehe zu min Position", "Go to min position"],
            servoMaxButtonText: ["Gå til max position", "Gehe zu max Position", "Go to max position"],
            servoMinLabel: ["Servo Minimum", "Servo Minimum", "Servo Minimum"],
            servoMaxLabel: ["Servo Maximum", "Servo Maximum", "Servo Maximum"],
            
            // Control buttons
            startText: ["Start", "Start", "Start"],
            stopText: ["Stop", "Stop", "Stop"],
            tareButton: ["Tare", "Tarieren", "Tare"],
            langHeading: ["Sprog", "Sprache", "Language"],
            step1Text: ["Tryk start", "Start drücken", "Press start"],
            
            // Statistics page strings
            performanceOverviewHeading: ["Statistik Oversigt", "Statistik-Übersicht", "Statistics Overview"],
            totalDispensingText: ["Total Tappet", "Gesamt abgefüllt", "Total Dispensed"],
            glassesFilledText: ["Glas Fyldt", "Gläser gefüllt", "Glasses Filled"],
            averageDeviationText: ["Gennemsnitlig Afvigelse", "Durchschnittliche Abweichung", "Average Deviation"],
            accurencyTrendHeading: ["Præcisions Trend", "Genauigkeitstrend", "Accuracy Trend"],
            targetWeightText: ["Ønsket Vægt", "Zielgewicht", "Target Weight"],
            actualWeightText: ["Faktisk Vægt", "Tatsächliches Gewicht", "Actual Weight"],
            errorDistributionHeading: ["Fejl Fordeling", "Fehlerverteilung", "Error Distribution"],
            overshootText: ["Overskydning", "Überschuss", "Overshoot"],
            undershootText: ["Underskydning", "Unterschuss", "Undershoot"],
            perfectText: ["Perfekt", "Perfekt", "Perfect"],
            resetStatisticsButton: ["Nulstil Statistik", "Statistik zurücksetzen", "Reset Statistics"],
            
            // Calibration strings
            calSectionHeading: ["Vægt Kalibrering", "Waagen-Kalibrierung", "Scale Calibration"],
            calStatusLoading: ["Indlæser kalibreringsstatus...", "Lade Kalibrierungsstatus...", "Loading calibration status..."],
            calSelectorText: ["Kalibreringsvægt", "Kalibrierungsgewicht", "Calibration Weight"],
            calButtonText: ["Kalibrer", "Kalibrieren", "Calibrate"],
            
            // Input field labels
            prestopTextHeading: ["Luk hanen når der mangler", "Hahn schließen wenn fehlt", "Close valve when missing"],
            minGlassWeightTextHeading: ["Glasregistrering: Glas vejer mere end", "Glaserkennung: Glas wiegt mehr als", "Glass detection: Glass weighs more than"],
            maxWeightTextHeading: ["Max tappemængde", "Max Abfüllmenge", "Max dispensing amount"],
            tareTextHeading: ["Tare kun med tom vægt !", "Nur mit leerem Gewicht tarieren!", "Tare with empty scale only!"]
        };

        //═══════════════════════════════════════════════════════════════════════
        // APPLICATION INITIALIZATION
        //═══════════════════════════════════════════════════════════════════════
        this.init();
    }

    //═══════════════════════════════════════════════════════════════════════════
    // INITIALIZATION & SETUP
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Initialize the BeeSMART application
     * Sets up event listeners, starts polling, and loads saved data
     * @returns {void}
     */
    init() {
        this.setupEventListeners();
        this.initializeStatistics();
        this.startPolling();
        this.updateLanguage();
        this.loadStatisticsFromStorage();
    }
    
    //═══════════════════════════════════════════════════════════════════════════
    // EVENT LISTENER SETUP
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Set up all event listeners for user interface interactions
     * Handles tab switching, control buttons, sliders, inputs, and more
     * @returns {void}
     */
    setupEventListeners() {
        // Tab switching
        document.querySelectorAll('.tab-button').forEach(button => {
            button.addEventListener('click', (e) => {
                const tabName = e.currentTarget.getAttribute('data-tab');
                this.switchTab(tabName);
            });
        });
        
        // Control buttons
        document.getElementById('startButton')?.addEventListener('click', () => {
            this.sendCommand('start');
        });
        
        document.getElementById('stopButton')?.addEventListener('click', () => {
            this.sendCommand('stop');
        });
        
        document.getElementById('tareButton')?.addEventListener('click', () => {
            this.sendCommand('tare');
        });
        
        // Auto state toggle
        document.getElementById('autoSwitch')?.addEventListener('change', (e) => {
            this.settings.autoState = e.target.checked;
            this.sendCommand('setAuto', { value: e.target.checked });
        });
        
        // Amount controls
        const amountSlider = document.getElementById('amountSlider');
        const amountInput = document.getElementById('amountInput');
        
        if (amountSlider) {
            amountSlider.addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                this.settings.desiredAmount = value;
                document.getElementById('sliderValue').textContent = value + 'g';
                if (amountInput) amountInput.value = value;
                this.sendCommand('setAmount', { value });
            });
        }
        
        if (amountInput) {
            amountInput.addEventListener('change', (e) => {
                const value = Math.max(50, Math.min(this.settings.maxWeight, parseInt(e.target.value) || 50));
                this.settings.desiredAmount = value;
                e.target.value = value;
                if (amountSlider) {
                    amountSlider.value = value;
                    document.getElementById('sliderValue').textContent = value + 'g';
                }
                this.sendCommand('setAmount', { value });
            });
        }

        // Viscosity selection
        document.querySelectorAll('input[name="viscosity"]').forEach(radio => {
            radio.addEventListener('change', (e) => {
                const viscosity = parseInt(e.target.value);
                this.settings.viscosity = viscosity;
                this.sendCommand('setViscosity', { value: viscosity });
                this.updatePidParameterEditability();
            });
        });

        // PID parameter inputs
        ['kp', 'ti', 'kd'].forEach(param => {
            const input = document.getElementById(param + 'Input');
            if (input) {
                input.addEventListener('change', (e) => {
                    const value = parseFloat(e.target.value);
                    this.settings[param] = value;
                    const pidData = {};
                    pidData[param] = value;
                    this.sendCommand('setPID', pidData);
                });
            }
        });

        // Servo controls
        const servoMinSlider = document.getElementById('servoMinSlider');
        const servoMaxSlider = document.getElementById('servoMaxSlider');

        if (servoMinSlider) {
            servoMinSlider.addEventListener('input', (e) => {
                const value = Math.max(0, Math.min(90, parseInt(e.target.value)));
                this.settings.servoMin = value;
                e.target.value = value;
                document.getElementById('servoMinValue').textContent = value + '°';
                this.sendCommand('setServoMin', { value });
            });
        }

        if (servoMaxSlider) {
            servoMaxSlider.addEventListener('input', (e) => {
                const value = Math.max(90, Math.min(180, parseInt(e.target.value)));
                this.settings.servoMax = value;
                e.target.value = value;
                document.getElementById('servoMaxValue').textContent = value + '°';
                this.sendCommand('setServoMax', { value });
            });
        }

        // Servo test buttons
        document.getElementById('servoMinButton')?.addEventListener('click', () => {
            this.sendCommand('servoTest', { position: 'min' });
        });

        document.getElementById('servoMaxButton')?.addEventListener('click', () => {
            this.sendCommand('servoTest', { position: 'max' });
        });

        // Other input controls
        ['stopHysteresis', 'minGlassWeight', 'maxWeight', 'calWeight'].forEach(param => {
            const input = document.getElementById(param + 'Input');
            if (input) {
                input.addEventListener('change', (e) => {
                    const value = parseInt(e.target.value);
                    this.settings[param.replace('Input', '')] = value;
                    
                    const command = 'set' + param.charAt(0).toUpperCase() + param.slice(1).replace('Input', '');
                    this.sendCommand(command, { value });
                });
            }
        });

        // Language selection
        document.querySelectorAll('input[name="language"]').forEach(radio => {
            radio.addEventListener('change', (e) => {
                const lang = parseInt(e.target.value);
                this.currentLanguage = lang;
                this.sendCommand('setLanguage', { value: lang });
                this.updateLanguage();
            });
        });

        // Calibration
        document.getElementById('calibrateButton')?.addEventListener('click', () => {
            this.sendCommand('calibrate');
        });

        // Calibration weight control
        const calWeightSlider = document.getElementById('calWeightSlider');
        const calWeightInput = document.getElementById('calWeightInput');

        if (calWeightSlider) {
            calWeightSlider.addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                this.settings.calWeight = value;
                document.getElementById('calSliderValue').textContent = value + 'g';
                if (calWeightInput) calWeightInput.value = value;
                this.sendCommand('setCalWeight', { value });
            });
        }

        if (calWeightInput) {
            calWeightInput.addEventListener('change', (e) => {
                const value = Math.max(50, Math.min(1000, parseInt(e.target.value) || 250));
                this.settings.calWeight = value;
                e.target.value = value;
                if (calWeightSlider) {
                    calWeightSlider.value = value;
                    document.getElementById('calSliderValue').textContent = value + 'g';
                }
                this.sendCommand('setCalWeight', { value });
            });
        }

        // Reset statistics button
        document.getElementById('resetStatisticsButton')?.addEventListener('click', () => {
            if (confirm(this.languages.resetStatisticsButton[this.currentLanguage] + '?')) {
                this.sendCommand('resetStatistics');
                this.clearStatistics();
            }
        });
    }

    //═══════════════════════════════════════════════════════════════════════════
    // UI NAVIGATION & TAB MANAGEMENT
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Switch between application tabs (Filling, Settings, Advanced, Statistics)
     * @param {string} tabName - Name of the tab to activate
     * @returns {void}
     */
    switchTab(tabName) {
        // Remove active class from all tabs and buttons
        document.querySelectorAll('.tab-button, .tab-content').forEach(el => {
            el.classList.remove('active');
        });

        // Add active class to selected tab and button
        document.getElementById(tabName)?.classList.add('active');
        document.querySelector(`[data-tab="${tabName}"]`)?.classList.add('active');

        // Initialize chart when switching to statistics tab
        if (tabName === 'statistics') {
            setTimeout(() => {
                this.initializeChart();
                this.updateStatisticsDisplay();
            }, 100);
        }
    }

    /**
     * Update PID parameter input editability based on viscosity selection
     * Only User Defined (viscosity 0) allows manual PID parameter editing
     * @returns {void}
     */
    updatePidParameterEditability() {
        // Only allow editing PID parameters for User Defined (viscosity 0)
        const isUserDefined = (this.settings.viscosity === 0);
        
        ['kpInput', 'tiInput', 'kdInput'].forEach(id => {
            const input = document.getElementById(id);
            if (input) {
                input.disabled = !isUserDefined;
                input.style.opacity = isUserDefined ? '1' : '0.6';
            }
        });
    }

    //═══════════════════════════════════════════════════════════════════════════
    // NETWORK COMMUNICATION & POLLING
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Start HTTP polling for real-time updates
     * Fetches status every 200ms and settings every 2 seconds
     * @async
     * @returns {Promise<void>}
     */
    async startPolling() {
        if (this.pollingInterval) {
            clearInterval(this.pollingInterval);
        }

        // Start immediate fetch
        this.fetchStatus();
        this.fetchSettings();

        // Set up 200ms polling
        this.pollingInterval = setInterval(() => {
            this.fetchStatus();
        }, 200);

        // Fetch settings less frequently (every 2 seconds)
        setInterval(() => {
            this.fetchSettings();
        }, 2000);
    }

    /**
     * Fetch current system status from ESP32 backend
     * Includes weights, state machine status, and calibration progress
     * @async
     * @returns {Promise<void>}
     */
    async fetchStatus() {
        if (this.isFetchingStatus) return;
        this.isFetchingStatus = true;

        try {
            const response = await fetch('/api/status');
            const data = await response.json();
            
            this.isConnected = true;
            this.updateConnectionStatus(true);
            this.updateStatusDisplay(data);
            
            // Update statistics from server data
            if (data.statistics) {
                this.updateStatisticsFromServer(data.statistics);
            }
            
        } catch (error) {
            console.log('Connection error:', error);
            this.isConnected = false;
            this.updateConnectionStatus(false);
        } finally {
            this.isFetchingStatus = false;
        }
    }

    /**
     * Fetch system settings and configuration from ESP32 backend
     * Includes PID parameters, servo limits, and user preferences
     * @async
     * @returns {Promise<void>}
     */
    async fetchSettings() {
        if (this.isFetchingSettings) return;
        this.isFetchingSettings = true;

        try {
            const response = await fetch('/api/settings');
            const data = await response.json();
            this.updateSettingsDisplay(data);
        } catch (error) {
            console.log('Settings fetch error:', error);
        } finally {
            this.isFetchingSettings = false;
        }
    }

    //═══════════════════════════════════════════════════════════════════════════
    // UI UPDATE FUNCTIONS
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Update status display elements with current system data
     * @param {Object} data - Status data from backend API
     * @param {string} data.message - Current status message
     * @param {Object} data.weights - Weight measurements
     * @param {Object} data.calibration - Calibration status and progress
     * @returns {void}
     */
    updateStatusDisplay(data) {
        // Update basic status
        document.getElementById('statusMessage').textContent = data.message || '';
        
        // Update weights
        document.getElementById('honeyWeight').textContent = (data.weights?.honey || 0) + ' g';
        document.getElementById('tareWeight').textContent = (data.weights?.total || 0) + ' g';
        document.getElementById('calCurrentWeight').textContent = (data.weights?.total || 0) + ' g';
        
        // Update calibration status and progress
        const calStatusElement = document.getElementById('calStatusMessage');
        if (calStatusElement && data.calibration) {
            const statusText = calStatusElement.querySelector('.cal-status-text');
            if (statusText) {
                statusText.textContent = data.calibration.message || '';
            }
            
            // Update progress bar
            const progressBar = document.getElementById('calProgressBar');
            if (progressBar && data.calibration.progress) {
                if (data.calibration.progress.active) {
                    const percent = Math.min(100, Math.max(0, data.calibration.progress.percent || 0));
                    progressBar.style.width = percent + '%';
                    calStatusElement.setAttribute('aria-valuenow', percent);
                } else {
                    progressBar.style.width = '0%';
                    calStatusElement.setAttribute('aria-valuenow', 0);
                }
            }
        }
    }

    /**
     * Update settings UI elements with configuration data from backend
     * @param {Object} data - Settings data from backend API
     * @returns {void}
     */
    updateSettingsDisplay(data) {
        // Normalize API field name: firmware sends 'language'
        if (typeof data.language !== 'undefined' && typeof data.lang === 'undefined') {
            data.lang = data.language; // mirror into expected internal field
        }
        // Update settings object
        Object.keys(data).forEach(key => {
            if (this.settings.hasOwnProperty(key)) {
                this.settings[key] = data[key];
            }
        });

        // Apply language from settings if provided and different
        if (typeof this.settings.lang !== 'undefined' && this.settings.lang !== this.currentLanguage) {
            this.currentLanguage = this.settings.lang;
            this.updateLanguage();
            // Re-sync language radio buttons
            document.querySelectorAll('input[name="language"]').forEach(radio => {
                radio.checked = (parseInt(radio.value) === this.currentLanguage);
            });
        }

        // Update UI elements
        this.updateSliders();
        this.updateInputs();
        this.updateRadioButtons();
        this.updatePidParameterEditability();
    }

    /**
     * Update all slider controls with current values
     * @returns {void}
     */
    updateSliders() {
        const amountSlider = document.getElementById('amountSlider');
        if (amountSlider) {
            amountSlider.value = this.settings.desiredAmount;
            amountSlider.max = this.settings.maxWeight;
            document.getElementById('sliderValue').textContent = this.settings.desiredAmount + 'g';
            document.getElementById('maxWeightDisplay').textContent = this.settings.maxWeight + 'g';
        }

        // Update main page desired amount display
        const desiredAmountElement = document.getElementById('desiredAmount');
        if (desiredAmountElement) {
            desiredAmountElement.textContent = this.settings.desiredAmount + ' g';
        }

        const servoMinSlider = document.getElementById('servoMinSlider');
        const servoMaxSlider = document.getElementById('servoMaxSlider');
        
        if (servoMinSlider) {
            servoMinSlider.value = this.settings.servoMin;
            servoMinSlider.min = 0;   // Fixed range: 0-90 degrees
            servoMinSlider.max = 90;
            document.getElementById('servoMinValue').textContent = this.settings.servoMin + '°';
        }
        
        if (servoMaxSlider) {
            servoMaxSlider.value = this.settings.servoMax;
            servoMaxSlider.min = 90;  // Fixed range: 90-180 degrees  
            servoMaxSlider.max = 180;
            document.getElementById('servoMaxValue').textContent = this.settings.servoMax + '°';
        }

        const calWeightSlider = document.getElementById('calWeightSlider');
        if (calWeightSlider) {
            calWeightSlider.value = this.settings.calWeight;
            document.getElementById('calSliderValue').textContent = this.settings.calWeight + 'g';
        }
    }

    /**
     * Update all input fields with current values
     * @returns {void}
     */
    updateInputs() {
        const inputs = {
            'amountInput': this.settings.desiredAmount,
            'kpInput': this.settings.kp,
            'tiInput': this.settings.ti,
            'kdInput': this.settings.kd,
            'stopHysteresisInput': this.settings.stopHysteresis,
            'minGlassWeightInput': this.settings.minGlassWeight,
            'maxWeightInput': this.settings.maxWeight,
            'calWeightInput': this.settings.calWeight
        };

        Object.entries(inputs).forEach(([id, value]) => {
            const element = document.getElementById(id);
            if (element && element.value != value) {
                element.value = value;
            }
        });
    }

    /**
     * Update radio button states based on current settings
     * @returns {void}
     */
    updateRadioButtons() {
        // Update viscosity
        document.querySelectorAll('input[name="viscosity"]').forEach(radio => {
            radio.checked = (parseInt(radio.value) === this.settings.viscosity);
        });

        // Update language
        document.querySelectorAll('input[name="language"]').forEach(radio => {
            radio.checked = (parseInt(radio.value) === this.currentLanguage);
        });

        // Update auto state
        const autoSwitch = document.getElementById('autoSwitch');
        if (autoSwitch) {
            autoSwitch.checked = this.settings.autoState;
        }
    }

    //═══════════════════════════════════════════════════════════════════════════
    // STATISTICS MANAGEMENT
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Update statistics data from server response
     * Merges backend statistics with local tracking
     * @param {Object} serverStats - Statistics object from backend
     * @returns {void}
     */
    updateStatisticsFromServer(serverStats) {
        if (serverStats.totalCycles !== undefined) {
            this.statisticsData.cycleCount = serverStats.totalCycles;
        }
        
        if (serverStats.totalDispensed !== undefined) {
            // Legacy field (derived from recent history in firmware) kept for backward compatibility
            this.statisticsData.totalDispensed = serverStats.totalDispensed;
        }
        
        if (serverStats.averageError !== undefined) {
            this.statisticsData.averageError = serverStats.averageError;
        }

        // Prefer cumulative persistent statistics if present
        if (serverStats.cumulativeDispensed !== undefined) {
            // cumulativeDispensed is in grams (firmware), convert to kg for display consistency
            this.statisticsData.totalDispensed = (serverStats.cumulativeDispensed / 1000);
        }
        if (serverStats.cumulativeAverageError !== undefined) {
            this.statisticsData.averageError = serverStats.cumulativeAverageError; // already grams
        }
        
        // Update recent history from server
        if (serverStats.recentHistory && Array.isArray(serverStats.recentHistory)) {
            this.statisticsData.dispensingHistory = serverStats.recentHistory.map(record => ({
                target: record.target,
                actual: record.actual,
                error: record.error,
                timestamp: Date.now() - Math.random() * 3600000 // Spread over last hour
            }));
            
            this.calculateErrorDistribution();
            this.updateChart();
        }
        
        this.updateStatisticsDisplay();
    }

    /**
     * Calculate error distribution percentages from dispensing history
     * Categorizes each cycle as overshoot, undershoot, or perfect (±2g tolerance)
     * @returns {void}
     */
    calculateErrorDistribution() {
        if (this.statisticsData.dispensingHistory.length === 0) {
            this.statisticsData.errorStats = { overshoot: 0, undershoot: 0, perfect: 0 };
            return;
        }
        
        let overshoot = 0;
        let undershoot = 0;
        let perfect = 0;
        
        this.statisticsData.dispensingHistory.forEach(record => {
            const error = record.error;
            if (Math.abs(error) <= 2) {
                perfect++;
            } else if (error > 0) {
                overshoot++;
            } else {
                undershoot++;
            }
        });
        
        const total = this.statisticsData.dispensingHistory.length;
        this.statisticsData.errorStats.overshoot = Math.round((overshoot / total) * 100);
        this.statisticsData.errorStats.undershoot = Math.round((undershoot / total) * 100);
        this.statisticsData.errorStats.perfect = Math.round((perfect / total) * 100);
    }

    /**
     * Update statistics display elements with current data
     * Updates performance cards and error distribution bars
     * @returns {void}
     */
    updateStatisticsDisplay() {
        // Update performance cards
        const totalDispensedElement = document.getElementById('totalDispensed');
        const cycleCountElement = document.getElementById('cycleCount');
        const deviationElement = document.getElementById('averageDeviation');
        
        if (totalDispensedElement) {
            totalDispensedElement.textContent = this.statisticsData.totalDispensed.toFixed(2) + ' kg';
        }
        
        if (cycleCountElement) {
            cycleCountElement.textContent = this.statisticsData.cycleCount.toString();
        }
        
        if (deviationElement) {
            const avgError = this.statisticsData.averageError || 0;
            const deviationText = (avgError >= 0 ? '+' : '') + avgError.toFixed(1) + 'g';
            deviationElement.textContent = deviationText;
            deviationElement.style.color = Math.abs(avgError) <= 2 ? '#4CAF50' : 
                                          Math.abs(avgError) <= 5 ? '#FF9800' : '#F44336';
        }
        
        // Update error distribution bars
        this.updateErrorBars();
    }

    /**
     * Update error distribution bar chart elements
     * @returns {void}
     */
    updateErrorBars() {
        const overshootBar = document.getElementById('overshootBar');
        const undershootBar = document.getElementById('undershootBar');
        const perfectBar = document.getElementById('perfectBar');
        const overshootValue = document.getElementById('overshootValue');
        const undershootValue = document.getElementById('undershootValue');
        const perfectValue = document.getElementById('perfectValue');
        
        if (overshootBar && this.statisticsData.errorStats) {
            overshootBar.style.width = this.statisticsData.errorStats.overshoot + '%';
            if (overshootValue) overshootValue.textContent = this.statisticsData.errorStats.overshoot + '%';
        }
        
        if (undershootBar && this.statisticsData.errorStats) {
            undershootBar.style.width = this.statisticsData.errorStats.undershoot + '%';
            if (undershootValue) undershootValue.textContent = this.statisticsData.errorStats.undershoot + '%';
        }
        
        if (perfectBar && this.statisticsData.errorStats) {
            perfectBar.style.width = this.statisticsData.errorStats.perfect + '%';
            if (perfectValue) perfectValue.textContent = this.statisticsData.errorStats.perfect + '%';
        }
    }

    /**
     * Initialize statistics system
     * Loads persistent data from local storage
     * @returns {void}
     */
    initializeStatistics() {
        this.loadStatisticsFromStorage();
    }

    //═══════════════════════════════════════════════════════════════════════════
    // CHART VISUALIZATION
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Initialize accuracy chart canvas with high-DPI rendering
     * Sets up chart context and prepares for data visualization
     * @returns {void}
     */
    initializeChart() {
        const canvas = document.getElementById('accuracyChart');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        // Set canvas size with high-DPI scaling for sharpness
        const container = canvas.parentElement;
        const logicalWidth = container.clientWidth - 40;
        const logicalHeight = 300;
        const dpr = window.devicePixelRatio || 1;
        canvas.style.width = logicalWidth + 'px';
        canvas.style.height = logicalHeight + 'px';
        canvas.width = Math.round(logicalWidth * dpr);
        canvas.height = Math.round(logicalHeight * dpr);
        ctx.scale(dpr, dpr);

        this.accuracyChart = {
            canvas,
            ctx,
            data: this.statisticsData.dispensingHistory.slice(-10), // Show last 10 points
            maxDataPoints: 10
        };

        this.drawChart();
    }

    /**
     * Draw accuracy trend chart showing target vs actual weights
     * Renders line chart with grid, axes, and data points
     * @returns {void}
     */
    drawChart() {
        if (!this.accuracyChart || !this.accuracyChart.ctx) return;

        const { ctx, canvas, data } = this.accuracyChart;
    // Use CSS pixel dimensions for layout math (canvas is scaled for DPR)
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
        const padding = 40;

        // Clear canvas
        ctx.clearRect(0, 0, width, height);

        if (data.length === 0) {
            // Localized "No data" message
            const noDataMessages = [
                'Ingen data tilgængelig',        // Danish
                'Keine Daten verfügbar',         // German
                'No data available'              // English
            ];
            const msg = noDataMessages[this.currentLanguage] || noDataMessages[0];
            ctx.fillStyle = '#666';
            ctx.font = '16px Arial';
            ctx.textAlign = 'center';
            ctx.fillText(msg, width / 2, height / 2);
            return;
        }

        // Calculate bounds
        const targets = data.map(d => d.target);
        const actuals = data.map(d => d.actual);
        const allValues = [...targets, ...actuals];
        const minY = Math.min(...allValues) * 0.95;
        const maxY = Math.max(...allValues) * 1.05;
        const range = maxY - minY || 1;

        // Draw axes
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(padding, padding);
        ctx.lineTo(padding, height - padding);
        ctx.lineTo(width - padding, height - padding);
        ctx.stroke();

        // Draw grid lines
        ctx.strokeStyle = '#eee';
        ctx.lineWidth = 0.5;
        for (let i = 1; i <= 4; i++) {
            const y = padding + (i * (height - 2 * padding) / 4);
            ctx.beginPath();
            ctx.moveTo(padding, y);
            ctx.lineTo(width - padding, y);
            ctx.stroke();
        }

        // Function to convert value to y coordinate
        const valueToY = (value) => height - padding - ((value - minY) / range) * (height - 2 * padding);

        // Draw target line
        ctx.strokeStyle = '#007AFF';
        ctx.lineWidth = 2;
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = padding + (i / (data.length - 1 || 1)) * (width - 2 * padding);
            const y = valueToY(data[i].target);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        // Draw actual line
        ctx.strokeStyle = '#4CAF50';
        ctx.lineWidth = 2;
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = padding + (i / (data.length - 1 || 1)) * (width - 2 * padding);
            const y = valueToY(data[i].actual);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        // Draw points
        for (let i = 0; i < data.length; i++) {
            const x = padding + (i / (data.length - 1 || 1)) * (width - 2 * padding);
            
            // Target point
            ctx.fillStyle = '#007AFF';
            ctx.beginPath();
            ctx.arc(x, valueToY(data[i].target), 3, 0, 2 * Math.PI);
            ctx.fill();
            
            // Actual point
            ctx.fillStyle = '#4CAF50';
            ctx.beginPath();
            ctx.arc(x, valueToY(data[i].actual), 3, 0, 2 * Math.PI);
            ctx.fill();
        }

        // Draw Y-axis labels
        ctx.fillStyle = '#666';
        ctx.font = '10px Arial';
        ctx.textAlign = 'right';
        for (let i = 0; i <= 4; i++) {
            const value = minY + (i * range / 4);
            const y = height - padding - (i * (height - 2 * padding) / 4);
            ctx.fillText(value.toFixed(0) + 'g', padding - 5, y + 3);
        }
    }

    /**
     * Update chart with latest data from statistics
     * Refreshes chart visualization with last 10 data points
     * @returns {void}
     */
    updateChart() {
        if (this.accuracyChart) {
            this.accuracyChart.data = this.statisticsData.dispensingHistory.slice(-10);
            this.drawChart();
        }
    }

    /**
     * Clear all statistics data and reset counters
     * Called when user confirms statistics reset
     * @returns {void}
     */
    clearStatistics() {
        this.statisticsData = {
            dispensingHistory: [],
            totalDispensed: 0,
            cycleCount: 0,
            averageError: 0,
            errorStats: { overshoot: 0, undershoot: 0, perfect: 0 }
        };
        this.updateStatisticsDisplay();
        this.updateChart();
        this.saveStatisticsToStorage();
    }

    /**
     * Load statistics from browser local storage
     * Restores persistent statistics across browser sessions
     * @returns {void}
     */
    loadStatisticsFromStorage() {
        try {
            const saved = localStorage.getItem('beesmart_statistics');
            if (saved) {
                const data = JSON.parse(saved);
                this.statisticsData = { ...this.statisticsData, ...data };
            }
        } catch (error) {
            console.log('Failed to load statistics from storage:', error);
        }
    }

    /**
     * Save statistics to browser local storage
     * Persists statistics data across browser sessions
     * @returns {void}
     */
    saveStatisticsToStorage() {
        try {
            localStorage.setItem('beesmart_statistics', JSON.stringify(this.statisticsData));
        } catch (error) {
            console.log('Failed to save statistics to storage:', error);
        }
    }

    //═══════════════════════════════════════════════════════════════════════════
    // UI FEEDBACK & STATUS UPDATES
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Update connection status indicator
     * @param {boolean} connected - True if connected to ESP32, false otherwise
     * @returns {void}
     */
    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connectionStatus');
        const indicator = statusElement?.querySelector('.connection-indicator');
        const text = statusElement?.querySelector('span');
        
        if (connected) {
            statusElement?.classList.add('connected');
            if (text) text.textContent = 'Forbundet';
        } else {
            statusElement?.classList.remove('connected');
            if (text) text.textContent = 'Forbinder...';
        }
    }

    /**
     * Update all UI text elements to current selected language
     * Iterates through all elements with data-key attributes
     * @returns {void}
     */
    updateLanguage() {
        document.querySelectorAll('[data-key]').forEach(element => {
            const key = element.getAttribute('data-key');
            if (this.languages[key]) {
                element.textContent = this.languages[key][this.currentLanguage];
            }
        });
    }

    //═══════════════════════════════════════════════════════════════════════════
    // COMMAND TRANSMISSION TO ESP32
    //═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Send command to ESP32 backend via HTTP POST
     * @async
     * @param {string} command - Command name (e.g., 'start', 'stop', 'tare')
     * @param {Object} [payload={}] - Command parameters
     * @returns {Promise<Object>} Response object with success status
     */
    async sendCommand(command, payload = {}) {
        try {
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    command,
                    payload
                })
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const result = await response.json();
            return result;
        } catch (error) {
            console.error('Command failed:', error);
            return { success: false, error: error.message };
        }
    }
}

//═══════════════════════════════════════════════════════════════════════════════
// APPLICATION ENTRY POINT
//═══════════════════════════════════════════════════════════════════════════════

/**
 * Initialize the BeeSMART application when DOM is fully loaded
 * Creates global instance accessible as window.beesmart
 */
document.addEventListener('DOMContentLoaded', () => {
    window.beesmart = new BeeSMART();
});