// Configuration management for NMBS Pebble App
var Constants = require('./00-constants.js');
var Storage = require('./01-storage.js');
var MessageHandler = require('./04-message-handler.js');
var API = require('./02-api.js');

// Evaluate smart schedules and return active route (if any)
function evaluateSchedules() {
    try {
      var schedules = Storage.getSmartSchedules();
      if (!schedules) {
        console.log('No smart schedules configured');
        return null;
      }

      var now = new Date();
      var currentDay = now.getDay();  // 0=Sunday, 1=Monday, ..., 6=Saturday
      var currentTime = ('0' + now.getHours()).slice(-2) + ':' + ('0' + now.getMinutes()).slice(-2);

      console.log('Evaluating schedules for day=' + currentDay + ', time=' + currentTime);

      for (var i = 0; i < schedules.length; i++) {
        var schedule = schedules[i];

        // Skip disabled schedules
        if (!schedule.enabled) {
          continue;
        }

        // Check if current day matches
        var dayMatches = schedule.days.indexOf(currentDay) !== -1;

        if (!dayMatches) {
          continue;
        }

        // Check if current time is within range
        if (currentTime >= schedule.startTime && currentTime <= schedule.endTime) {
          console.log('Matched schedule: ' + schedule.id);
          return {
            fromId: schedule.fromId,
            toId: schedule.toId
          };
        }
      }

      console.log('No matching schedule found');
      return null;
    } catch (e) {
      console.log('Error evaluating schedules: ' + e.message);
      return null;
    }
  }

  // Handle configuration page open
function handleShowConfiguration() {
    console.log('Opening configuration page');
    Pebble.openURL(Constants.CONFIG.CONFIG_URL);
  }

  // Handle configuration page close
function handleWebviewClosed(e) {
    console.log('Configuration closed');

    if (e && e.response) {
      try {
        var config = JSON.parse(decodeURIComponent(e.response));
        console.log('Received config: ' + JSON.stringify(config));

        // Save favorite stations
        if (config.favoriteStations && config.favoriteStations.length > 0) {
          Storage.saveFavoriteStations(config.favoriteStations);

          // Send stations to watch
          MessageHandler.sendStationsToWatch(config.favoriteStations);
        }

        // Save smart schedules
        if (config.smartSchedules) {
          Storage.saveSmartSchedules(config.smartSchedules);

          // Evaluate and set active route
          var activeRoute = evaluateSchedules();
          if (activeRoute) {
            MessageHandler.setActiveRoute(config.favoriteStations, activeRoute.fromId, activeRoute.toId);
          }
        }

      } catch (e) {
        console.log('Error parsing configuration: ' + e.message);
      }
    } else {
      console.log('Configuration cancelled or no data received');
    }
  }

  // Handle PebbleKit JS ready event
function handleReady() {
    console.log('PebbleKit JS ready!');

    // Load cached stations immediately (for offline use)
    Storage.loadCachedStations();

    // Fetch fresh station list in background
    API.fetchStations();

    // Load persisted station selection and connection data
    Storage.loadPersistedData();

    // Load user configuration and send to watch
    var favoriteStations = Storage.getFavoriteStations();
    if (favoriteStations) {
      console.log('Loading saved configuration with ' + favoriteStations.length + ' stations');
      MessageHandler.sendStationsToWatch(favoriteStations);

      // Evaluate schedules and set active route
      var activeRoute = evaluateSchedules();
      if (activeRoute) {
        console.log('Found active schedule');
        MessageHandler.setActiveRoute(favoriteStations, activeRoute.fromId, activeRoute.toId);
      } else {
        console.log('No active schedule, will use default route');
      }
    } else {
      console.log('No saved configuration, watch will use defaults');
    }

    if (Storage.getCurrentFromStation() && Storage.getCurrentToStation()) {
      console.log('Restored session: ' + Storage.getCurrentFromStation() + ' -> ' + Storage.getCurrentToStation());
    }
  }

module.exports = {
  evaluateSchedules: evaluateSchedules
};

// Register event listeners at module level (where Pebble is available)
Pebble.addEventListener('appmessage', MessageHandler.handleAppMessage);
Pebble.addEventListener('showConfiguration', function() {
  console.log('Opening configuration page');
  Pebble.openURL(Constants.CONFIG.CONFIG_URL);
});
Pebble.addEventListener('webviewclosed', function(e) {
  console.log('Configuration closed');

  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      console.log('Received config: ' + JSON.stringify(config));

      // Save favorite stations
      if (config.favoriteStations && config.favoriteStations.length > 0) {
        Storage.saveFavoriteStations(config.favoriteStations);

        // Send stations to watch
        MessageHandler.sendStationsToWatch(config.favoriteStations);
      }

      // Save smart schedules
      if (config.smartSchedules) {
        Storage.saveSmartSchedules(config.smartSchedules);

        // Evaluate and set active route
        var activeRoute = evaluateSchedules();
        if (activeRoute) {
          MessageHandler.setActiveRoute(config.favoriteStations, activeRoute.fromId, activeRoute.toId);
        }
      }

    } catch (e) {
      console.log('Error parsing configuration: ' + e.message);
    }
  } else {
    console.log('Configuration cancelled or no data received');
  }
});
Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');

  // Load cached stations immediately (for offline use)
  Storage.loadCachedStations();

  // Fetch fresh station list in background
  API.fetchStations();

  // Load persisted station selection and connection data
  Storage.loadPersistedData();

  // Load user configuration and send to watch
  var favoriteStations = Storage.getFavoriteStations();
  if (favoriteStations) {
    console.log('Loading saved configuration with ' + favoriteStations.length + ' stations');
    MessageHandler.sendStationsToWatch(favoriteStations);

    // Evaluate schedules and set active route
    var activeRoute = evaluateSchedules();
    if (activeRoute) {
      console.log('Found active schedule');
      MessageHandler.setActiveRoute(favoriteStations, activeRoute.fromId, activeRoute.toId);
    } else {
      console.log('No active schedule, will use default route');
    }
  } else {
    console.log('No saved configuration, watch will use defaults');
  }

  if (Storage.getCurrentFromStation() && Storage.getCurrentToStation()) {
    console.log('Restored session: ' + Storage.getCurrentFromStation() + ' -> ' + Storage.getCurrentToStation());
  }
});
