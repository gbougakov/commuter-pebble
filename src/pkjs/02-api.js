// iRail API communication for NMBS Pebble App
var Constants = require('./00-constants.js');
var Storage = require('./01-storage.js');

// Debounce timer for API requests
var requestDebounceTimer = null;

// Fetch stations from iRail API and cache them
function fetchStations(callback) {
    console.log('Fetching stations from iRail API...');

    // Get language preference for API request
    var lang = Storage.getLanguage();
    var url = Constants.IRAIL_STATIONS_URL + '&lang=' + lang;

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.setRequestHeader('User-Agent', Constants.CONFIG.USER_AGENT);
    xhr.onload = function() {
      if (xhr.readyState === 4 && xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          if (response.station && response.station.length > 0) {
            // Map to simplified structure
            var stations = response.station.map(function(s) {
              return {
                id: s.id,                          // "BE.NMBS.008813003"
                name: s.name                       // "Brussels-Central"
              };
            });

            // Save to storage
            Storage.saveStationCache(stations);
            console.log('Fetched and cached ' + stations.length + ' stations');

            if (callback) {
              callback(stations);
            }
          } else {
            console.log('No stations in API response');
          }
        } catch (e) {
          console.log('Error parsing stations API response: ' + e.message);
        }
      } else {
        console.log('Failed to fetch stations: ' + xhr.status);
      }
    };
    xhr.onerror = function() {
      console.log('Network error fetching stations');
    };
    xhr.send();
  }

  // Fetch train connections from iRail API
function fetchConnections(fromId, toId, callback, errorCallback) {
    // Store current route for detail requests
    Storage.setCurrentFromStation(fromId);
    Storage.setCurrentToStation(toId);
    Storage.clearConnectionIdentifiers();

    // Persist station selection to localStorage
    Storage.savePersistedData();

    if (!fromId || !toId) {
      console.log('Invalid station IDs: ' + fromId + ' -> ' + toId);
      if (errorCallback) {
        errorCallback('Invalid station IDs');
      }
      return;
    }

    // Can't request same station for from/to
    if (fromId === toId) {
      console.log('From and To stations are the same, sending empty result');
      if (errorCallback) {
        errorCallback('Same from/to station');
      }
      return;
    }

    // Build API URL with language preference
    var lang = Storage.getLanguage();
    var url = Constants.IRAIL_API_URL +
        '?from=' + encodeURIComponent(fromId) +
        '&to=' + encodeURIComponent(toId) +
        '&format=json' +
        '&lang=' + lang;

    console.log('Fetching: ' + url);

    // Make HTTP request
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.setRequestHeader('User-Agent', Constants.CONFIG.USER_AGENT);
    xhr.onload = function () {
      if (xhr.readyState === 4) {
        if (xhr.status === 200) {
          console.log('Response received');
          try {
            var response = JSON.parse(xhr.responseText);
            if (callback) {
              callback(response);
            }
          } catch (e) {
            console.log('JSON parse error: ' + e.message);
            if (errorCallback) {
              errorCallback('Parse error');
            }
          }
        } else {
          console.log('Request failed: ' + xhr.status + ' - ' + xhr.responseText);
          if (errorCallback) {
            errorCallback('HTTP ' + xhr.status);
          }
        }
      }
    };
    xhr.onerror = function () {
      console.log('Network error');
      if (errorCallback) {
        errorCallback('Network error');
      }
    };
    xhr.send();
  }

  // Fetch connection details for a specific departure
function fetchConnectionDetails(departureIndex, callback, errorCallback) {
    console.log('Fetching details for departure ' + departureIndex);

    var identifier = Storage.getConnectionIdentifier(departureIndex);
    if (!identifier) {
      console.log('No connection identifier found for index ' + departureIndex);
      if (errorCallback) {
        errorCallback('No identifier');
      }
      return;
    }

    var fromId = Storage.getCurrentFromStation();
    var toId = Storage.getCurrentToStation();

    if (!fromId || !toId) {
      console.log('Invalid stations for detail request');
      if (errorCallback) {
        errorCallback('Invalid stations');
      }
      return;
    }

    // Fetch connections starting 10 minutes before selected train
    // This ensures we get the exact train plus any earlier alternatives
    var lang = Storage.getLanguage();
    var url = Constants.IRAIL_API_URL +
        '?from=' + encodeURIComponent(fromId) +
        '&to=' + encodeURIComponent(toId) +
        '&date=' + formatDate(identifier.departTime * 1000) +
        '&time=' + formatTime(identifier.departTime * 1000 - 10 * 60 * 1000) +
        '&format=json' +
        '&lang=' + lang;

    console.log('Fetching details from: ' + url);

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.setRequestHeader('User-Agent', Constants.CONFIG.USER_AGENT);
    xhr.onload = function () {
      if (xhr.readyState === 4 && xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          console.log(response);
          if (response.connection && response.connection.length > 0) {
            // Find the matching connection by vehicle and departure time
            var matchedConn = null;
            for (var i = 0; i < response.connection.length; i++) {
              var conn = response.connection[i];
              if (conn.departure.vehicle === identifier.vehicle &&
                  parseInt(conn.departure.time) === identifier.departTime) {
                matchedConn = conn;
                break;
              }
            }

            if (matchedConn && callback) {
              console.log('Found matching connection');
              callback(matchedConn, departureIndex);
            } else {
              console.log('Connection not found in fresh data');
              if (errorCallback) {
                errorCallback('Connection not found');
              }
            }
          }
        } catch (e) {
          console.log('Detail fetch parse error: ' + e.message);
          if (errorCallback) {
            errorCallback('Parse error');
          }
        }
      }
    };
    xhr.send();
  }

  // Helper function to format date for API (DDMMYY)
function formatDate(dateIn) {
    var date = new Date(dateIn);
    var day = ('0' + date.getDate()).slice(-2);
    var month = ('0' + (date.getMonth() + 1)).slice(-2);
    var year = ('' + date.getFullYear()).slice(-2);
    return day + month + year;
  }

  // Helper function to format time for API (HHMM)
function formatTime(dateIn) {
    var date = new Date(dateIn);
    var hours = ('0' + date.getHours()).slice(-2);
    var minutes = ('0' + date.getMinutes()).slice(-2);
    return hours + minutes;
  }

  // Debounce a function call
function debounce(func, delay) {
    if (requestDebounceTimer) {
      clearTimeout(requestDebounceTimer);
      requestDebounceTimer = null;
      console.log('Debouncing request...');
    }

    requestDebounceTimer = setTimeout(function() {
      console.log('Executing debounced request');
      func();
      requestDebounceTimer = null;
    }, delay);
  }

module.exports = {
  fetchStations: fetchStations,
  fetchConnections: fetchConnections,
  fetchConnectionDetails: fetchConnectionDetails,
  debounce: debounce
};
