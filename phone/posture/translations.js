// UI strings for the posture phone plugin. Locale follows the system
// language (navigator.language) — French is the default. Supported:
// fr, en, es, it, de, zh-CN.

const STRINGS = {
  fr: {
    appTitle: 'Rappel de posture',
    statusStarting: 'Démarrage…',
    statusConnected: 'Lunettes connectées',
    statusDisconnected: 'Lunettes déconnectées',
    enableTracking: 'Activer le suivi',
    thresholdLabel: 'Angle de détection :',
    durationLabel: 'Durée avant rappel :',
    unitSeconds: 's',
    calibrate: 'Calibrer la posture',
    calibrateHint: 'Tiens-toi droit au moment de calibrer. Sur les lunettes, tu peux aussi appuyer sur le bouton.',
    sectionReminders: 'Rappels',
    statToday: 'aujourd’hui',
    statWeek: '7 derniers jours',
    noReminder: 'Aucun rappel pour le moment.',
    calibrateRequested: 'Calibration demandée…',
    calibrated: (baseline) => `Posture de référence calibrée (${baseline}°).`,
    lastAlert: (time, pitch) => `Dernier rappel à ${time} (angle ${pitch}°).`,
  },
  en: {
    appTitle: 'Posture reminder',
    statusStarting: 'Starting…',
    statusConnected: 'Glasses connected',
    statusDisconnected: 'Glasses disconnected',
    enableTracking: 'Enable tracking',
    thresholdLabel: 'Detection angle:',
    durationLabel: 'Delay before reminder:',
    unitSeconds: 's',
    calibrate: 'Calibrate posture',
    calibrateHint: 'Sit up straight when calibrating. On the glasses you can also press the button.',
    sectionReminders: 'Reminders',
    statToday: 'today',
    statWeek: 'last 7 days',
    noReminder: 'No reminder yet.',
    calibrateRequested: 'Calibration requested…',
    calibrated: (baseline) => `Reference posture calibrated (${baseline}°).`,
    lastAlert: (time, pitch) => `Last reminder at ${time} (angle ${pitch}°).`,
  },
  es: {
    appTitle: 'Aviso de postura',
    statusStarting: 'Iniciando…',
    statusConnected: 'Gafas conectadas',
    statusDisconnected: 'Gafas desconectadas',
    enableTracking: 'Activar el seguimiento',
    thresholdLabel: 'Ángulo de detección:',
    durationLabel: 'Tiempo antes del aviso:',
    unitSeconds: 's',
    calibrate: 'Calibrar la postura',
    calibrateHint: 'Siéntate derecho al calibrar. En las gafas también puedes pulsar el botón.',
    sectionReminders: 'Avisos',
    statToday: 'hoy',
    statWeek: 'últimos 7 días',
    noReminder: 'Aún no hay avisos.',
    calibrateRequested: 'Calibración solicitada…',
    calibrated: (baseline) => `Postura de referencia calibrada (${baseline}°).`,
    lastAlert: (time, pitch) => `Último aviso a las ${time} (ángulo ${pitch}°).`,
  },
  it: {
    appTitle: 'Promemoria postura',
    statusStarting: 'Avvio…',
    statusConnected: 'Occhiali connessi',
    statusDisconnected: 'Occhiali disconnessi',
    enableTracking: 'Attiva il monitoraggio',
    thresholdLabel: 'Angolo di rilevamento:',
    durationLabel: 'Tempo prima dell’avviso:',
    unitSeconds: 's',
    calibrate: 'Calibra la postura',
    calibrateHint: 'Siediti dritto durante la calibrazione. Sugli occhiali puoi anche premere il pulsante.',
    sectionReminders: 'Avvisi',
    statToday: 'oggi',
    statWeek: 'ultimi 7 giorni',
    noReminder: 'Nessun avviso per ora.',
    calibrateRequested: 'Calibrazione richiesta…',
    calibrated: (baseline) => `Postura di riferimento calibrata (${baseline}°).`,
    lastAlert: (time, pitch) => `Ultimo avviso alle ${time} (angolo ${pitch}°).`,
  },
  de: {
    appTitle: 'Haltungserinnerung',
    statusStarting: 'Startet…',
    statusConnected: 'Brille verbunden',
    statusDisconnected: 'Brille getrennt',
    enableTracking: 'Überwachung aktivieren',
    thresholdLabel: 'Erkennungswinkel:',
    durationLabel: 'Zeit bis zur Erinnerung:',
    unitSeconds: 's',
    calibrate: 'Haltung kalibrieren',
    calibrateHint: 'Sitz beim Kalibrieren gerade. Auf der Brille kannst du auch die Taste drücken.',
    sectionReminders: 'Erinnerungen',
    statToday: 'heute',
    statWeek: 'letzte 7 Tage',
    noReminder: 'Noch keine Erinnerung.',
    calibrateRequested: 'Kalibrierung angefordert…',
    calibrated: (baseline) => `Referenzhaltung kalibriert (${baseline}°).`,
    lastAlert: (time, pitch) => `Letzte Erinnerung um ${time} (Winkel ${pitch}°).`,
  },
  zh: {
    appTitle: '姿势提醒',
    statusStarting: '启动中…',
    statusConnected: '眼镜已连接',
    statusDisconnected: '眼镜未连接',
    enableTracking: '启用追踪',
    thresholdLabel: '检测角度：',
    durationLabel: '提醒延迟：',
    unitSeconds: '秒',
    calibrate: '校准姿势',
    calibrateHint: '校准请坐直。在眼镜上也可以按按钮。',
    sectionReminders: '提醒',
    statToday: '今天',
    statWeek: '最近 7 天',
    noReminder: '暂无提醒。',
    calibrateRequested: '已请求校准…',
    calibrated: (baseline) => `参考姿势已校准（${baseline}°）。`,
    lastAlert: (time, pitch) => `上次提醒于 ${time}（角度 ${pitch}°）。`,
  },
};

export function detectLocale() {
  const tag = (navigator.language || 'fr').toLowerCase();
  if (tag.startsWith('en')) return 'en';
  if (tag.startsWith('es')) return 'es';
  if (tag.startsWith('it')) return 'it';
  if (tag.startsWith('de')) return 'de';
  if (tag.startsWith('zh')) return 'zh';
  return 'fr';
}

export function getStrings(locale) {
  return STRINGS[locale] || STRINGS.fr;
}
