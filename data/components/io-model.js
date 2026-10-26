(function (root) {
    const GPIOS = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39];
    const INPUT_ONLY = [34, 35, 36, 39], ADC2 = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27], ADC = [...ADC2, 32, 33, ...INPUT_ONLY];
    const output = pin => pin?.mode === 'OUTPUT' && (pin.type || 'DIGITAL') === 'DIGITAL';
    const duration = ms => Number.isInteger(ms) && ms >= 100 && ms <= 3600000;
    const object = value => value && typeof value === 'object' && !Array.isArray(value);
    const bytes = value => new TextEncoder().encode(value).length;
    const name = (value, limit) => typeof value === 'string' && value.trim().length > 0 && bytes(value) <= limit;
    const ICONS = { '': ['Auto', ''], home: ['Maison', '⌂'], light: ['Éclairage', '☀'], fan: ['Ventilateur', '✺'], water: ['Pompe', '💧'], plant: ['Plante', '🌱'], temperature: ['Température', '🌡'], button: ['Bouton', '●'], sensor: ['Sonde', '◈'], switch: ['Interrupteur', '⏻'], scene: ['Scène', '▶'] };
    const icon = item => ICONS[item?.icon]?.[1] || (output(item) ? '⏻' : '◈');
    const iconOptions = () => Object.entries(ICONS).map(([key, [label, symbol]]) => new Option(`${symbol} ${label}`.trim(), key));
    const presentationError = item => item?.icon != null && (typeof item.icon !== 'string' || !Object.hasOwn(ICONS, item.icon)) ? 'Choisissez une icône disponible.' : '';
    const copyName = value => { let s = Array.from(value); while (bytes(s.join('') + ' · copie') > 64) s.pop(); return s.join('') + ' · copie'; };
    const newId = prefix => `${prefix}_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 6)}`;
    const pinTemplates = {
        light: { label: 'Éclairage', mode: 'OUTPUT', type: 'DIGITAL', icon: 'light', pulse_presets_ms: [60000, 300000, 900000] },
        fan: { label: 'Ventilateur', mode: 'OUTPUT', type: 'DIGITAL', icon: 'fan', pulse_presets_ms: [60000, 300000, 600000] },
        pump: { label: 'Pompe', mode: 'OUTPUT', type: 'DIGITAL', icon: 'water', pulse_presets_ms: [1000, 5000, 10000] },
        button: { label: 'Bouton', mode: 'INPUT_PULLUP', type: 'DIGITAL', icon: 'button' },
        motion: { label: 'Mouvement', mode: 'INPUT', type: 'DIGITAL', icon: 'sensor' },
        soil: { label: 'Humidité du bac', mode: 'INPUT', type: 'ANALOG_INPUT', icon: 'plant', graph: true }
    };
    function explainRule(item, state) {
        if (!state || state.state === 'input_unavailable') return 'Lecture indisponible : aucun déclenchement.';
        if (state.state === 'paused') return 'Les automatismes sont en pause.';
        if (state.state === 'disabled') return 'Cette règle est désactivée.';
        if (state.state === 'outside_window') return `Hors plage ${item.window?.start}–${item.window?.end}. Les franchissements sont ignorés, sans rattrapage à l’ouverture.`;
        if (state.state === 'clock_unavailable') return 'Heure indisponible : aucun déclenchement dans une plage horaire, sans rattrapage au retour de l’heure.';
        if (state.override_remaining_ms) return `Priorité manuelle encore ${Math.ceil(state.override_remaining_ms / 1000)} s. Les franchissements sont ignorés, sans rattrapage.`;
        if (state.state === 'initializing') return 'Initialisation : une condition déjà vraie doit revenir au repos avant de pouvoir déclencher.';
        const lines = [];
        if (state.stable_remaining_ms) lines.push(`Stabilité ${state.candidate ? 'du seuil atteint' : 'du retour au repos'} : encore ${Math.ceil(state.stable_remaining_ms / 1000)} s.`);
        else lines.push(state.condition ? 'Condition active : en attente du retour au repos, puis d’un nouveau franchissement.' : state.condition_now ? 'Seuil atteint : stabilité en cours.' : 'En attente du seuil.');
        if (['above', 'below'].includes(item.operator)) lines.push(`Retour au repos : ${item.operator === 'above' ? '≤' : '≥'} ${item.operator === 'above' ? item.threshold - item.hysteresis : item.threshold + item.hysteresis}.`);
        if (state.cooldown_remaining_ms) lines.push(`Délai entre déclenchements : encore ${Math.ceil(state.cooldown_remaining_ms / 1000)} s. Un franchissement pendant ce délai n’est pas reporté.`);
        return lines.join(' ');
    }
    function signature(value) {
        return value == null ? '' : JSON.stringify(value, (_, entry) => object(entry) ? Object.fromEntries(Object.keys(entry).sort().map(key => [key, entry[key]])) : entry);
    }
    function zoneState(config, readings, automation, zone) {
        const outputs = (config?.io_pins || []).filter(pin => output(pin) && pin.zone === zone);
        const available = outputs.map(pin => readings?.find(r => r.gpio === pin.gpio)).filter(r => typeof r?.value === 'number');
        const held = available.filter(r => r.override_remaining_ms > 0), remaining = Math.max(0, ...held.map(r => r.override_remaining_ms));
        const mode = !readings ? 'unknown' : !outputs.length ? 'readings' : !available.length ? 'unavailable' : held.length ? held.length === available.length ? 'manual' : 'mixed' : !automation ? 'unknown' : automation.paused ? 'paused' : 'auto';
        return { mode, outputs: outputs.length, available: available.length, held: held.length, remaining, paused: automation?.paused === true };
    }
    function relationships(config, gpio) {
        const scenes = (config?.scenes || []).filter(scene => scene.actions.some(action => action.gpio === gpio));
        const target = id => scenes.some(scene => scene.id === id) || id === 'all_off' && output(config?.io_pins?.find(pin => pin.gpio === gpio));
        return { scenes, rules: (config?.automation?.rules || []).filter(rule => rule.gpio === gpio || target(rule.scene_id)), schedules: (config?.automation?.schedules || []).filter(item => target(item.scene_id)) };
    }
    function convert(raw, c) {
        let ratio = (raw - c.raw_min) / (c.raw_max - c.raw_min);
        if (c.clamp !== false) ratio = Math.max(0, Math.min(1, ratio));
        return c.value_min + ratio * (c.value_max - c.value_min);
    }
    function calibrationError(c) {
        if (c == null) return '';
        if (!object(c)) return 'La calibration doit être un objet.';
        if (!['raw_min', 'raw_max'].every(key => Number.isInteger(c[key]) && c[key] >= 0 && c[key] <= 4095)) return 'Les références ADC doivent être des entiers de 0 à 4095.';
        if (c.raw_min === c.raw_max) return 'Les deux références ADC doivent être différentes.';
        if (!['value_min', 'value_max'].every(key => typeof c[key] === 'number' && Number.isFinite(c[key]) && Math.abs(c[key]) <= 1e9)) return 'Les valeurs de calibration doivent être des nombres finis entre -1e9 et 1e9.';
        if (typeof c.unit !== 'string' || bytes(c.unit) > 16) return 'L’unité doit contenir au maximum 16 caractères.';
        if (c.clamp != null && typeof c.clamp !== 'boolean') return 'La limitation de plage doit être un booléen.';
        return '';
    }
    function validate(config) {
        if (!object(config) || !Array.isArray(config.io_pins)) return 'io_pins doit être une liste.';
        if (bytes(JSON.stringify(config)) > 32768) return 'La configuration dépasse 32 Ko.';
        if (homeAssistantError(config.home_assistant)) return homeAssistantError(config.home_assistant);
        if (config.device != null && (!object(config.device) || !name(config.device.name, 64))) return 'Nom du board : de 1 à 64 caractères.';
        if (presentationError(config.device)) return presentationError(config.device);
        const pins = new Map();
        for (const pin of config.io_pins) {
            if (!object(pin) || !Number.isInteger(pin.gpio) || !GPIOS.includes(pin.gpio)) return 'GPIO indisponible sur cet ESP32.';
            if (pins.has(pin.gpio)) return 'Un GPIO ne peut être configuré qu’une fois.';
            pins.set(pin.gpio, pin);
            if (presentationError(pin)) return presentationError(pin);
            if (!name(pin.label, 64)) return `GPIO ${pin.gpio} : donnez un nom de 1 à 64 caractères.`;
            if (pin.zone != null && (typeof pin.zone !== 'string' || bytes(pin.zone) > 64)) return 'Une zone peut contenir jusqu’à 64 caractères.';
            const mode = pin.mode ?? 'INPUT', type = pin.type ?? 'DIGITAL';
            if (!['INPUT', 'INPUT_PULLUP', 'INPUT_PULLDOWN', 'OUTPUT'].includes(mode)) return 'Mode GPIO inconnu.';
            if (INPUT_ONLY.includes(pin.gpio) && mode === 'OUTPUT') return 'GPIO 34–39 : entrée uniquement.';
            if (!['DIGITAL', 'ANALOG_INPUT'].includes(type)) return 'Type GPIO inconnu.';
            if (type === 'ANALOG_INPUT' && (mode === 'OUTPUT' || !ADC.includes(pin.gpio))) return 'L’analogique nécessite une entrée compatible ADC.';
            if (!['', 'LOW', 'HIGH'].includes(pin.initial_state ?? '')) return 'État au démarrage inconnu.';
            if (pin.inverted != null && typeof pin.inverted !== 'boolean') return 'La polarité inversée doit être un booléen.';
            if (pin.pulse_presets_ms != null && (!output(pin) || !Array.isArray(pin.pulse_presets_ms) || pin.pulse_presets_ms.length > 5 || !pin.pulse_presets_ms.every(duration) || new Set(pin.pulse_presets_ms).size !== pin.pulse_presets_ms.length)) return 'Une sortie peut avoir jusqu’à cinq durées différentes, de 0,1 à 3600 secondes.';
            if (pin.calibration != null) {
                if (type !== 'ANALOG_INPUT') return 'La calibration nécessite une entrée analogique.';
                const error = calibrationError(pin.calibration); if (error) return error;
            }
        }
        if (config.scenes != null) {
            if (!Array.isArray(config.scenes) || config.scenes.length > 12) return 'Configurez au maximum douze scènes.';
            const ids = new Set(); let totalActions = 0;
            for (const scene of config.scenes) {
                if (!object(scene) || typeof scene.id !== 'string' || !/^[A-Za-z0-9_-]{1,32}$/.test(scene.id) || scene.id === 'all_off' || ids.has(scene.id)) return 'Identifiant de scène invalide ou dupliqué.';
                ids.add(scene.id);
                if (presentationError(scene)) return presentationError(scene);
                if (!name(scene.name, 64)) return 'Donnez un nom de 1 à 64 caractères à chaque scène.';
                if (scene.zone != null && (typeof scene.zone !== 'string' || bytes(scene.zone) > 64)) return 'Une zone peut contenir jusqu’à 64 caractères.';
                if (!Array.isArray(scene.actions) || !scene.actions.length || scene.actions.length > 26) return `${scene.name} : choisissez au moins une sortie.`;
                totalActions += scene.actions.length; if (totalActions > 64) return 'Les scènes peuvent contenir 64 actions au total.';
                const used = new Set();
                for (const action of scene.actions) {
                    if (!object(action) || !Number.isInteger(action.gpio) || typeof action.active !== 'boolean' || used.has(action.gpio) || !output(pins.get(action.gpio))) return `${scene.name} : chaque action doit cibler une sortie configurée différente. Modifiez aussi la scène lors d’un retrait.`;
                    used.add(action.gpio);
                    if (action.duration_ms != null && (!Number.isInteger(action.duration_ms) || action.duration_ms !== 0 && (!action.active || !duration(action.duration_ms)))) return `${scene.name} : une durée nécessite ON et de 0,1 à 3600 secondes.`;
                }
            }
        }
        return alertError(config) || automationError(config);
    }
    function homeAssistantError(h) {
        if (h == null) return '';
        if (!object(h) || Object.keys(h).some(k => !['url', 'device_id', 'entities'].includes(k))) return 'Liens HA : URL, appareil et entités uniquement. Aucun jeton.';
        try { const u = new URL(h.url); if (typeof h.url !== 'string' || bytes(h.url) > 256 || !['http:', 'https:'].includes(u.protocol) || !u.hostname || u.username || u.password || u.pathname !== '/' || u.search || u.hash) return 'Adresse HA : serveur HTTP/HTTPS sans identifiants, chemin, paramètres ou fragment.'; } catch (_) { return 'Adresse HA : origine HTTP ou HTTPS requise.'; }
        if (h.device_id != null && (typeof h.device_id !== 'string' || h.device_id !== '' && !/^[a-fA-F0-9]{32}$/.test(h.device_id))) return 'Identifiant d’appareil HA : 32 caractères hexadécimaux.';
        if (!object(h.entities) || Object.keys(h.entities).length > 48) return 'Configurez jusqu’à 48 liens d’entités HA.';
        for (const [key, id] of Object.entries(h.entities)) if (!/^[A-Za-z0-9_]{1,48}$/.test(key) || typeof id !== 'string' || bytes(id) > 192 || !/^[a-z0-9_]+\.[a-z0-9_]+$/.test(id)) return 'Entité HA : domaine.nom en minuscules requis.';
        return '';
    }
    function alertError(config) {
        if (config.alerts == null) return '';
        if (!Array.isArray(config.alerts) || config.alerts.length > 12) return 'Configurez au maximum douze alertes.';
        const ids = new Set();
        for (const a of config.alerts) {
            if (!object(a) || typeof a.id !== 'string' || !/^[A-Za-z0-9_-]{1,32}$/.test(a.id || '') || ids.has(a.id) || typeof a.enabled !== 'boolean') return 'Alerte : identifiant unique et état activé requis.';
            ids.add(a.id); if (!name(a.name, 64)) return 'Donnez un nom de 1 à 64 caractères à chaque alerte.';
            if (a.source === 'gpio') { const p = config.io_pins.find(p => p.gpio === a.gpio); if (!Number.isInteger(a.gpio) || !p || p.type !== 'ANALOG_INPUT' || output(p)) return 'L’alerte doit cibler une entrée analogique configurée. Modifiez aussi ses références lors d’un retrait.'; }
            else if (a.source !== 'temperature') return 'Source d’alerte : température ou entrée analogique.';
            if (!['above', 'below'].includes(a.operator)) return 'Alerte : choisissez au-dessus ou sous le seuil.';
            if (!['threshold', 'hysteresis'].every(k => typeof a[k] === 'number' && Number.isFinite(a[k]) && Math.abs(a[k]) <= 1e9)) return 'Seuil et marge d’alerte doivent être des nombres finis.';
            if (a.hysteresis < 0 || !Number.isInteger(a.stable_ms) || a.stable_ms < 500 || a.stable_ms > 60000) return 'Marge positive ou nulle ; stabilité d’alerte de 0,5 à 60 s.';
        } return '';
    }
    const defaultAutomation = () => ({ paused: true, timezone: 'America/Toronto', schedules: [], rules: [] });
    function automationError(config) {
        const a = config.automation; if (a == null) return '';
        if (!object(a) || typeof a.paused !== 'boolean') return 'Les automatismes nécessitent un état pause explicite.';
        if (!['America/Toronto', 'UTC'].includes(a.timezone)) return 'Fuseau disponible : America/Toronto ou UTC.';
        const ids = new Set(), scenes = new Map((config.scenes || []).map(scene => [scene.id, scene])), pins = new Map(config.io_pins.map(pin => [pin.gpio, pin]));
        for (const key of ['schedules', 'rules']) {
            if (!Array.isArray(a[key]) || a[key].length > 12) return 'Configurez au maximum douze horaires et douze règles.';
            for (const item of a[key]) {
                if (!object(item) || !/^[A-Za-z0-9_-]{1,32}$/.test(item.id || '') || ids.has(item.id) || !name(item.name, 64) || typeof item.enabled !== 'boolean') return 'Automatisme : identifiant unique, nom et état activé requis.';
                ids.add(item.id);
                const scene = scenes.get(item.scene_id);
                if (!scene && item.scene_id !== 'all_off') return 'Chaque automatisme doit cibler une scène existante. Modifiez aussi ses références lors d’un retrait.';
                if (key === 'schedules') {
                    if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(item.time || '')) return 'Horaire : heure valide HH:MM requise.';
                    if (!Array.isArray(item.days) || !item.days.length || item.days.length > 7 || item.days.some(day => !Number.isInteger(day) || day < 0 || day > 6) || new Set(item.days).size !== item.days.length) return 'Choisissez des jours distincts de dimanche à samedi.';
                } else {
                    if (item.window != null && (!object(item.window) || ![item.window.start, item.window.end].every(t => typeof t === 'string' && /^([01]\d|2[0-3]):[0-5]\d$/.test(t)) || item.window.start === item.window.end)) return 'Plage de règle : heures HH:MM valides et différentes requises.';
                    const pin = pins.get(item.gpio); if (!Number.isInteger(item.gpio) || !pin || output(pin)) return 'La source d’une règle doit être une entrée configurée.';
                    const analog = pin.type === 'ANALOG_INPUT';
                    if (!(analog ? ['below', 'above'] : ['high', 'low']).includes(item.operator)) return 'Condition incompatible avec le type d’entrée.';
                    if (analog && (!['threshold', 'hysteresis'].every(key => typeof item[key] === 'number' && Number.isFinite(item[key]) && Math.abs(item[key]) <= 1e9) || item.hysteresis < 0)) return 'Seuil et marge doivent être des nombres finis, marge positive ou nulle.';
                    if (!Number.isInteger(item.stable_ms) || item.stable_ms < 500 || item.stable_ms > 60000 || !Number.isInteger(item.cooldown_ms) || item.cooldown_ms < 1000 || item.cooldown_ms > 86400000) return 'Stabilité : 0,5–60 s. Délai entre déclenchements : 1–86400 s.';
                    if (scene?.actions.some(action => action.active && !action.duration_ms)) return 'Une règle nécessite des actions ON avec une durée. Réglez les durées dans la scène.';
                }
            }
        }
        return '';
    }
    const api = { GPIOS, INPUT_ONLY, ADC2, ADC, output, duration, signature, convert, calibrationError, validate, defaultAutomation, automationError, ICONS, icon, iconOptions, presentationError, copyName, newId, pinTemplates, explainRule, zoneState, relationships, alertError, homeAssistantError };
    if (typeof module === 'object' && module.exports) module.exports = api;
    else root.IOModel = api;
})(typeof globalThis === 'object' ? globalThis : this);
