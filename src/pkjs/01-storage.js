// LocalStorage management for NMBS Pebble App
var Constants = require('./00-constants.js');

// Current route and connection identifiers for detail requests
var currentFromStation = '';
var currentToStation = '';
var connectionIdentifiers = []; // Array of {vehicle, departTime} for each departure

// Station cache (fetched from API)
var stationCache = [];

// Load persisted data from localStorage
function loadPersistedData() {
  try {
    var storedFrom = localStorage.getItem(Constants.STORAGE_KEYS.FROM_STATION);
    var storedTo = localStorage.getItem(Constants.STORAGE_KEYS.TO_STATION);
    var storedConnections = localStorage.getItem(Constants.STORAGE_KEYS.CONNECTIONS);

    if (storedFrom && Constants.STATION_IDS[storedFrom]) {
      currentFromStation = storedFrom;
      console.log('Loaded from station: ' + currentFromStation);
    }

    if (storedTo && Constants.STATION_IDS[storedTo]) {
      currentToStation = storedTo;
      console.log('Loaded to station: ' + currentToStation);
    }

    if (storedConnections) {
      connectionIdentifiers = JSON.parse(storedConnections);
      console.log('Loaded ' + connectionIdentifiers.length + ' connection identifiers');
    }
  } catch (e) {
    console.log('Error loading persisted data: ' + e.message);
    // Reset to defaults on error
    currentFromStation = '';
    currentToStation = '';
    connectionIdentifiers = [];
  }
}

// Save current data to localStorage
function savePersistedData() {
  try {
    if (currentFromStation) {
      localStorage.setItem(Constants.STORAGE_KEYS.FROM_STATION, currentFromStation);
    }
    if (currentToStation) {
      localStorage.setItem(Constants.STORAGE_KEYS.TO_STATION, currentToStation);
    }
    localStorage.setItem(Constants.STORAGE_KEYS.CONNECTIONS, JSON.stringify(connectionIdentifiers));
    console.log('Persisted data saved');
  } catch (e) {
    console.log('Error saving persisted data: ' + e.message);
  }
}

// Load cached stations from localStorage
function loadCachedStations() {
  try {
    var cached = localStorage.getItem(Constants.STORAGE_KEYS.STATION_CACHE);
    if (cached) {
      stationCache = JSON.parse(cached);
      console.log('Loaded ' + stationCache.length + ' cached stations');
      return true;
    }
  } catch (e) {
    console.log('Error loading cached stations: ' + e.message);
  }
  return false;
}

// Save station cache to localStorage
function saveStationCache(stations) {
  try {
    stationCache = stations;
    localStorage.setItem(Constants.STORAGE_KEYS.STATION_CACHE, JSON.stringify(stationCache));
    console.log('Cached ' + stationCache.length + ' stations');
  } catch (e) {
    console.log('Error saving station cache: ' + e.message);
  }
}

// Get station object by iRail ID
function getStationById(id) {
  for (var i = 0; i < stationCache.length; i++) {
    if (stationCache[i].id === id) {
      return stationCache[i];
    }
  }
  return null;
}

// Get station name by iRail ID (with fallback to ID)
function getStationNameById(id) {
  var station = getStationById(id);
  return station ? station.name : id;
}

// Get favorite stations from localStorage
function getFavoriteStations() {
  try {
    var favoritesJson = localStorage.getItem(Constants.STORAGE_KEYS.FAVORITE_STATIONS);
    if (favoritesJson) {
      return JSON.parse(favoritesJson);
    }
  } catch (e) {
    console.log('Error loading favorite stations: ' + e.message);
  }
  return null;
}

// Save favorite stations to localStorage
function saveFavoriteStations(stations) {
  try {
    localStorage.setItem(Constants.STORAGE_KEYS.FAVORITE_STATIONS, JSON.stringify(stations));
    console.log('Saved ' + stations.length + ' favorite stations');
  } catch (e) {
    console.log('Error saving favorite stations: ' + e.message);
  }
}

// Get smart schedules from localStorage
function getSmartSchedules() {
  try {
    var schedulesJson = localStorage.getItem(Constants.STORAGE_KEYS.SMART_SCHEDULES);
    if (schedulesJson) {
      return JSON.parse(schedulesJson);
    }
  } catch (e) {
    console.log('Error loading smart schedules: ' + e.message);
  }
  return null;
}

// Save smart schedules to localStorage
function saveSmartSchedules(schedules) {
  try {
    localStorage.setItem(Constants.STORAGE_KEYS.SMART_SCHEDULES, JSON.stringify(schedules));
    console.log('Saved ' + schedules.length + ' smart schedules');
  } catch (e) {
    console.log('Error saving smart schedules: ' + e.message);
  }
}

// Current route getters/setters
function getCurrentFromStation() {
  return currentFromStation;
}

function setCurrentFromStation(station) {
  currentFromStation = station;
}

function getCurrentToStation() {
  return currentToStation;
}

function setCurrentToStation(station) {
  currentToStation = station;
}

// Connection identifiers getters/setters
function getConnectionIdentifier(index) {
  return connectionIdentifiers[index];
}

function setConnectionIdentifier(index, identifier) {
  connectionIdentifiers[index] = identifier;
}

function clearConnectionIdentifiers() {
  connectionIdentifiers = [];
}

module.exports = {
  loadPersistedData: loadPersistedData,
  savePersistedData: savePersistedData,
  loadCachedStations: loadCachedStations,
  saveStationCache: saveStationCache,
  getStationById: getStationById,
  getStationNameById: getStationNameById,
  getFavoriteStations: getFavoriteStations,
  saveFavoriteStations: saveFavoriteStations,
  getSmartSchedules: getSmartSchedules,
  saveSmartSchedules: saveSmartSchedules,
  getCurrentFromStation: getCurrentFromStation,
  setCurrentFromStation: setCurrentFromStation,
  getCurrentToStation: getCurrentToStation,
  setCurrentToStation: setCurrentToStation,
  getConnectionIdentifier: getConnectionIdentifier,
  setConnectionIdentifier: setConnectionIdentifier,
  clearConnectionIdentifiers: clearConnectionIdentifiers
};
