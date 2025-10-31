// Constants for NMBS Pebble App

// API URLs
var IRAIL_API_URL = 'https://api.irail.be/connections/';
var IRAIL_STATIONS_URL = 'https://api.irail.be/v1/stations?format=json';

// Default station IDs (fallback if no config and backward compatibility)
var STATION_IDS = {
  'Brussels-Central': 'BE.NMBS.008813003',
  'Antwerp-Central': 'BE.NMBS.008821006',
  'Ghent-Sint-Pieters': 'BE.NMBS.008892007',
  'Liège-Guillemins': 'BE.NMBS.008841004',
  'Leuven': 'BE.NMBS.008833001'
};

// Message type constants
var MESSAGE_TYPES = {
  REQUEST_DATA: 1,
  SEND_DEPARTURE: 2,
  SEND_COUNT: 3,
  REQUEST_DETAILS: 4,
  SEND_DETAIL: 5,
  SEND_STATION_COUNT: 6,
  SEND_STATION: 7,
  SET_ACTIVE_ROUTE: 8,
  REQUEST_ACK: 9
};

// LocalStorage keys
var STORAGE_KEYS = {
  FROM_STATION: 'nmbs_from_station',
  TO_STATION: 'nmbs_to_station',
  CONNECTIONS: 'nmbs_connections',
  STATION_CACHE: 'nmbs_station_cache',
  FAVORITE_STATIONS: 'nmbs_favorite_stations',
  SMART_SCHEDULES: 'nmbs_smart_schedules',
  LANGUAGE: 'nmbs_language'
};

// Configuration limits
var CONFIG = {
  DEBOUNCE_DELAY: 500,           // milliseconds
  MAX_DEPARTURES: 11,            // Limit to 11 departures
  MAX_FAVORITE_STATIONS: 6,      // Maximum favorite stations
  USER_AGENT: 'WerknaamCommuter <https://werknaam.be, commuter@werknaam.be>',
  CONFIG_URL: 'https://assets-eu.gbgk.net/nmbs-pebble/config.html',
  DEFAULT_LANGUAGE: 'en'         // Default language for API requests
};

// Supported languages
var LANGUAGES = [
  { code: 'en', name: 'English' },
  { code: 'nl', name: 'Nederlands' },
  { code: 'fr', name: 'Français' },
  { code: 'de', name: 'Deutsch' }
];

module.exports = {
  IRAIL_API_URL: IRAIL_API_URL,
  IRAIL_STATIONS_URL: IRAIL_STATIONS_URL,
  STATION_IDS: STATION_IDS,
  MESSAGE_TYPES: MESSAGE_TYPES,
  STORAGE_KEYS: STORAGE_KEYS,
  CONFIG: CONFIG,
  LANGUAGES: LANGUAGES
};
