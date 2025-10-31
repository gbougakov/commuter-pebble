// AppMessage protocol handler for NMBS Pebble App
var Constants = require('./00-constants.js');
var DataProcessor = require('./03-data-processor.js');
var Storage = require('./01-storage.js');
var API = require('./02-api.js');

// Request ID tracking (for race condition prevention)
var currentRequestId = 0;  // Last received request ID
var currentDetailRequestId = 0;  // Last received detail request ID

  // Process train data from API response and send to watch
function processTrainData(response) {
    console.log('Processing response: ' + JSON.stringify(response).substring(0, 200));

    if (!response.connection || response.connection.length === 0) {
      console.log('No connections found');
      // Send count of 0
      Pebble.sendAppMessage({
        'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_COUNT,
        'DATA_COUNT': 0,
        'REQUEST_ID': currentRequestId
      });
      return;
    }

    var connections = response.connection;
    var count = Math.min(connections.length, Constants.CONFIG.MAX_DEPARTURES);

    console.log('Found ' + count + ' connections');
    console.log('First connection: ' + JSON.stringify(connections[0]));

    // Send count first (with request ID)
    Pebble.sendAppMessage({
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_COUNT,
      'DATA_COUNT': count,
      'REQUEST_ID': currentRequestId
    }, function () {
      console.log('Count sent: ' + count + ' [ID ' + currentRequestId + ']');
      // Start sending departures
      sendDepartures(connections, 0);
    }, function (e) {
      console.log('Failed to send count: ' + e.error.message);
    });
  }

  // Send departures one at a time (recursive with callbacks)
function sendDepartures(connections, index) {
    if (index >= connections.length || index >= Constants.CONFIG.MAX_DEPARTURES) {
      console.log('All departures sent');
      return;
    }

    var conn = connections[index];
    var departure = DataProcessor.processConnection(conn, index);

    // Build message
    var message = {
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_DEPARTURE,
      'DEPARTURE_INDEX': departure.index,
      'DESTINATION': departure.destination,
      'DEPART_TIME': departure.departTime,
      'DEPART_TIMESTAMP': departure.departTimestamp,
      'ARRIVE_TIME': departure.arriveTime,
      'PLATFORM': departure.platform,
      'TRAIN_TYPE': departure.trainType,
      'DURATION': departure.duration,
      'DEPART_DELAY': departure.departDelay,
      'ARRIVE_DELAY': departure.arriveDelay,
      'IS_DIRECT': departure.isDirect,
      'PLATFORM_CHANGED': departure.platformChanged,
      'REQUEST_ID': currentRequestId
    };

    console.log('Sending departure ' + index + ': ' + departure.destination + ' [ID ' + currentRequestId + ']');

    // Send message with callbacks
    Pebble.sendAppMessage(message, function () {
      // Success - send next departure
      console.log('Departure ' + index + ' sent successfully');
      sendDepartures(connections, index + 1);
    }, function (e) {
      console.log('Failed to send departure ' + index + ': ' + e.error.message);
      // Try next one anyway
      sendDepartures(connections, index + 1);
    });
  }

  // Send full connection details to watch (leg-by-leg)
function sendConnectionDetail(conn, departureIndex) {
    var legs = DataProcessor.processConnectionDetail(conn);

    // Send leg count first (with request ID)
    Pebble.sendAppMessage({
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_DETAIL,
      'DEPARTURE_INDEX': departureIndex,
      'LEG_COUNT': legs.length,
      'REQUEST_ID': currentDetailRequestId
    }, function () {
      console.log('Leg count sent: ' + legs.length + ' [ID ' + currentDetailRequestId + ']');
      // Start sending individual legs
      sendLegs(legs, 0);
    }, function (e) {
      console.log('Failed to send leg count: ' + e.error.message);
    });
  }

  // Send legs one at a time (recursive with callbacks)
function sendLegs(legs, index) {
    if (index >= legs.length) {
      console.log('All legs sent');
      return;
    }

    var leg = legs[index];

    var message = {
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_DETAIL,
      'LEG_INDEX': index,
      'LEG_DEPART_STATION': leg.departStation.substring(0, 31),
      'LEG_ARRIVE_STATION': leg.arriveStation.substring(0, 31),
      'LEG_DEPART_TIME': leg.departTime.substring(0, 7),
      'LEG_ARRIVE_TIME': leg.arriveTime.substring(0, 7),
      'LEG_DEPART_PLATFORM': leg.departPlatform.substring(0, 3),
      'LEG_ARRIVE_PLATFORM': leg.arrivePlatform.substring(0, 3),
      'LEG_DEPART_DELAY': leg.departDelay,
      'LEG_ARRIVE_DELAY': leg.arriveDelay,
      'LEG_VEHICLE': leg.vehicle.substring(0, 15),
      'LEG_DIRECTION': leg.direction.substring(0, 31),
      'LEG_STOP_COUNT': leg.stopCount,
      'LEG_DEPART_PLATFORM_CHANGED': leg.departPlatformChanged,
      'LEG_ARRIVE_PLATFORM_CHANGED': leg.arrivePlatformChanged,
      'REQUEST_ID': currentDetailRequestId
    };

    console.log('Sending leg ' + index + ': ' + leg.departStation + ' → ' + leg.arriveStation + ' [ID ' + currentDetailRequestId + ']');

    Pebble.sendAppMessage(message, function () {
      // Success - send next leg
      sendLegs(legs, index + 1);
    }, function (e) {
      console.log('Failed to send leg ' + index + ': ' + e.error.message);
      // Try next one anyway
      sendLegs(legs, index + 1);
    });
  }

  // Send favorite stations to watch
function sendStationsToWatch(stationIds) {
    console.log('Sending ' + stationIds.length + ' stations to watch');

    // Send count first
    Pebble.sendAppMessage({
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_STATION_COUNT,
      'CONFIG_STATION_COUNT': stationIds.length
    }, function() {
      console.log('Station count sent');
      // Start sending individual stations
      sendStationSequential(stationIds, 0);
    }, function(e) {
      console.log('Failed to send station count: ' + e.error.message);
    });
  }

  // Send stations one at a time (sequential with callbacks)
function sendStationSequential(stationIds, index) {
    if (index >= stationIds.length) {
      console.log('All stations sent to watch');
      return;
    }

    var stationId = stationIds[index];
    var station = Storage.getStationById(stationId);

    if (!station) {
      console.log('Station not found in cache: ' + stationId);
      // Skip and continue
      sendStationSequential(stationIds, index + 1);
      return;
    }

    var message = {
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_STATION,
      'CONFIG_STATION_INDEX': index,
      'CONFIG_STATION_NAME': station.name.substring(0, 63),
      'CONFIG_STATION_IRAIL_ID': station.id.substring(0, 31)
    };

    console.log('Sending station ' + index + ': ' + station.name);

    Pebble.sendAppMessage(message, function() {
      // Success - send next station
      sendStationSequential(stationIds, index + 1);
    }, function(e) {
      console.log('Failed to send station ' + index + ': ' + e.error.message);
      // Try next one anyway
      sendStationSequential(stationIds, index + 1);
    });
  }

  // Set active route based on schedule
function setActiveRoute(stationIds, fromId, toId) {
    // Find indices in favorite stations array
    var fromIndex = stationIds.indexOf(fromId);
    var toIndex = stationIds.indexOf(toId);

    if (fromIndex === -1 || toIndex === -1) {
      console.log('Active route stations not in favorites');
      return;
    }

    console.log('Setting active route: index ' + fromIndex + ' -> ' + toIndex);

    Pebble.sendAppMessage({
      'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SET_ACTIVE_ROUTE,
      'CONFIG_FROM_INDEX': fromIndex,
      'CONFIG_TO_INDEX': toIndex
    }, function() {
      console.log('Active route set successfully');
    }, function(e) {
      console.log('Failed to set active route: ' + e.error.message);
    });
  }

  // Handle incoming message from watch
function handleAppMessage(e) {
    console.log('Message from watch: ' + JSON.stringify(e.payload));

    var messageType = e.payload.MESSAGE_TYPE;

    if (messageType === Constants.MESSAGE_TYPES.REQUEST_DATA) {
      // Extract request ID (for race condition prevention)
      currentRequestId = e.payload.REQUEST_ID || 0;
      console.log('Train data request [ID ' + currentRequestId + ']');

      // Check if using new iRail ID format or old name format
      var fromId = e.payload.FROM_STATION_ID;
      var toId = e.payload.TO_STATION_ID;

      if (fromId && toId) {
        console.log('Data requested (by ID): ' + fromId + ' -> ' + toId);
        Storage.setCurrentFromStation(fromId);
        Storage.setCurrentToStation(toId);
      } else {
        // Fallback to old format (station names) - convert to IDs
        var fromStation = e.payload.FROM_STATION;
        var toStation = e.payload.TO_STATION;
        console.log('Data requested (by name): ' + fromStation + ' -> ' + toStation);
        fromId = Constants.STATION_IDS[fromStation];
        toId = Constants.STATION_IDS[toStation];
        // Always store IDs, not names
        Storage.setCurrentFromStation(fromId);
        Storage.setCurrentToStation(toId);
      }

      // Send acknowledgment immediately (before debounce)
      Pebble.sendAppMessage({
        'MESSAGE_TYPE': Constants.MESSAGE_TYPES.REQUEST_ACK,
        'REQUEST_ID': currentRequestId
      }, function() {
        console.log('Request acknowledged [ID ' + currentRequestId + ']');
      }, function(e) {
        console.log('Failed to send acknowledgment: ' + e.error.message);
      });

      // Debounce the API request
      API.debounce(function() {
        console.log('Executing debounced request [ID ' + currentRequestId + ']');
        API.fetchConnections(fromId, toId, processTrainData, function(error) {
          // Send empty result on error
          Pebble.sendAppMessage({
            'MESSAGE_TYPE': Constants.MESSAGE_TYPES.SEND_COUNT,
            'DATA_COUNT': 0,
            'REQUEST_ID': currentRequestId
          });
        });
      }, Constants.CONFIG.DEBOUNCE_DELAY);

    } else if (messageType === Constants.MESSAGE_TYPES.REQUEST_DETAILS) {
      // Extract request ID for detail request
      currentDetailRequestId = e.payload.REQUEST_ID || 0;
      var departureIndex = e.payload.DEPARTURE_INDEX;
      console.log('Details requested [ID ' + currentDetailRequestId + '] for departure ' + departureIndex);

      API.fetchConnectionDetails(departureIndex, sendConnectionDetail, function(error) {
        console.log('Failed to fetch connection details: ' + error);
      });
    }
  }

module.exports = {
  handleAppMessage: handleAppMessage,
  sendStationsToWatch: sendStationsToWatch,
  setActiveRoute: setActiveRoute
};
