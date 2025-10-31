// Data processing and formatting for NMBS Pebble App
var Constants = require('./00-constants.js');
var Storage = require('./01-storage.js');

// Helper function to format Unix timestamp to HH:MM
function formatUnixTime(timestamp) {
    var date = new Date(timestamp * 1000);
    var hours = ('0' + date.getHours()).slice(-2);
    var minutes = ('0' + date.getMinutes()).slice(-2);
    return hours + ':' + minutes;
  }

  // Helper function to calculate duration in minutes
function calculateDuration(departTime, arriveTime) {
    var durationMinutes = Math.floor((arriveTime - departTime) / 60);
    var hours = Math.floor(durationMinutes / 60);
    var minutes = durationMinutes % 60;

    if (hours > 0) {
      return hours + 'h' + (minutes > 0 ? minutes + 'm' : '');
    } else {
      return minutes + 'm';
    }
  }

  // Helper function to check if platform changed
  // iRail API: platforminfo.normal == "0" means changed, "1" means no change
function checkPlatformChanged(departureOrArrival) {
    if (!departureOrArrival.platforminfo) return 0;
    return (departureOrArrival.platforminfo.normal === "0") ? 1 : 0;
  }

  // Process a single connection into departure data structure
function processConnection(conn, index) {
    // Parse connection data
    var departTime = parseInt(conn.departure.time);
    var arriveTime = parseInt(conn.arrival.time);
    var departDelay = parseInt(conn.departure.delay) || 0;
    var arriveDelay = parseInt(conn.arrival.delay) || 0;

    // Extract train type from vehicleinfo
    var trainType = 'IC';  // Default
    if (conn.departure.vehicleinfo && conn.departure.vehicleinfo.type) {
      trainType = conn.departure.vehicleinfo.type;
    }

    // Check if direct connection (vias is an object with 'number' property)
    var viasNumber = 0;
    if (conn.vias && conn.vias.number) {
      viasNumber = parseInt(conn.vias.number) || 0;
    }
    var isDirect = (viasNumber === 0) ? 1 : 0;

    // Get platform
    var platform = conn.departure.platform || '?';

    // Check platform change
    var platformChanged = checkPlatformChanged(conn.departure);
    console.log('Platform changed: ' + platformChanged);

    // Calculate duration from conn.duration (in seconds)
    var durationSeconds = parseInt(conn.duration) || 0;
    var duration = calculateDuration(departTime, departTime + durationSeconds);

    // Get direction (terminus) from departure direction
    var direction = 'Unknown';
    if (conn.departure.direction && conn.departure.direction.name) {
      direction = conn.departure.direction.name;
    } else if (conn.arrival.stationinfo && conn.arrival.stationinfo.name) {
      // Fallback to arrival station if direction not available
      direction = conn.arrival.stationinfo.name;
    }

    // Store connection identifier for detail requests
    Storage.setConnectionIdentifier(index, {
      vehicle: conn.departure.vehicle || '',
      departTime: departTime
    });

    // Persist connection identifiers (will save after each departure)
    Storage.savePersistedData();

    // Build departure object
    return {
      index: index,
      destination: direction.substring(0, 31), // Limit to 31 chars
      departTime: formatUnixTime(departTime),
      departTimestamp: departTime, // Unix timestamp for glance expiration
      arriveTime: formatUnixTime(arriveTime),
      platform: platform.substring(0, 3), // Limit to 3 chars
      trainType: trainType.substring(0, 7), // Limit to 7 chars
      duration: duration.substring(0, 7), // Limit to 7 chars
      departDelay: Math.floor(departDelay / 60),
      arriveDelay: Math.floor(arriveDelay / 60),
      isDirect: isDirect,
      platformChanged: platformChanged
    };
  }

  // Process connection detail into journey legs
function processConnectionDetail(conn) {
    console.log('Processing connection details...');

    // Build leg array
    var legs = [];

    // Check if this is a direct connection or has vias
    var viasNumber = 0;
    if (conn.vias && conn.vias.number) {
      viasNumber = parseInt(conn.vias.number) || 0;
    }

    if (viasNumber === 0) {
      // Direct connection - single leg
      var leg = {
        departStation: conn.departure.stationinfo.name,
        arriveStation: conn.arrival.stationinfo.name,
        departTime: formatUnixTime(parseInt(conn.departure.time)),
        arriveTime: formatUnixTime(parseInt(conn.arrival.time)),
        departPlatform: conn.departure.platform || '?',
        arrivePlatform: conn.arrival.platform || '?',
        departDelay: Math.floor(parseInt(conn.departure.delay || 0) / 60),
        arriveDelay: Math.floor(parseInt(conn.arrival.delay || 0) / 60),
        vehicle: (conn.departure.vehicleinfo && conn.departure.vehicleinfo.shortname) || 'Unknown',
        direction: (conn.departure.direction && conn.departure.direction.name) || 'Unknown',
        stopCount: (conn.departure.stops && parseInt(conn.departure.stops.number)) || 0,
        departPlatformChanged: checkPlatformChanged(conn.departure),
        arrivePlatformChanged: checkPlatformChanged(conn.arrival)
      };
      legs.push(leg);
    } else {
      // Multi-leg connection with transfers
      // First leg: departure → first via arrival
      var firstVia = conn.vias.via[0];
      var firstLeg = {
        departStation: conn.departure.stationinfo.name,
        arriveStation: firstVia.arrival.stationinfo.name,
        departTime: formatUnixTime(parseInt(conn.departure.time)),
        arriveTime: formatUnixTime(parseInt(firstVia.arrival.time)),
        departPlatform: conn.departure.platform || '?',
        arrivePlatform: firstVia.arrival.platform || '?',
        departDelay: Math.floor(parseInt(conn.departure.delay || 0) / 60),
        arriveDelay: Math.floor(parseInt(firstVia.arrival.delay || 0) / 60),
        vehicle: (conn.departure.vehicleinfo && conn.departure.vehicleinfo.shortname) || 'Unknown',
        direction: (conn.departure.direction && conn.departure.direction.name) || 'Unknown',
        stopCount: (conn.departure.stops && parseInt(conn.departure.stops.number)) || 0,
        departPlatformChanged: checkPlatformChanged(conn.departure),
        arrivePlatformChanged: checkPlatformChanged(firstVia.arrival)
      };
      legs.push(firstLeg);

      // Middle legs: via departure → next via arrival (or final arrival if last)
      for (var i = 0; i < conn.vias.via.length; i++) {
        var via = conn.vias.via[i];
        var nextStation, nextTime, nextPlatform, nextDelay, nextPlatformChanged;

        if (i < conn.vias.via.length - 1) {
          // Next via exists
          var nextVia = conn.vias.via[i + 1];
          nextStation = nextVia.arrival.stationinfo.name;
          nextTime = formatUnixTime(parseInt(nextVia.arrival.time));
          nextPlatform = nextVia.arrival.platform || '?';
          nextDelay = Math.floor(parseInt(nextVia.arrival.delay || 0) / 60);
          nextPlatformChanged = checkPlatformChanged(nextVia.arrival);
        } else {
          // Last leg - goes to final arrival
          nextStation = conn.arrival.stationinfo.name;
          nextTime = formatUnixTime(parseInt(conn.arrival.time));
          nextPlatform = conn.arrival.platform || '?';
          nextDelay = Math.floor(parseInt(conn.arrival.delay || 0) / 60);
          nextPlatformChanged = checkPlatformChanged(conn.arrival);
        }

        var leg = {
          departStation: via.departure.stationinfo.name,
          arriveStation: nextStation,
          departTime: formatUnixTime(parseInt(via.departure.time)),
          arriveTime: nextTime,
          departPlatform: via.departure.platform || '?',
          arrivePlatform: nextPlatform,
          departDelay: Math.floor(parseInt(via.departure.delay || 0) / 60),
          arriveDelay: nextDelay,
          vehicle: (via.departure.vehicleinfo && via.departure.vehicleinfo.shortname) || 'Unknown',
          direction: (via.departure.direction && via.departure.direction.name) || 'Unknown',
          stopCount: (via.departure.stops && parseInt(via.departure.stops.number)) || 0,
          departPlatformChanged: checkPlatformChanged(via.departure),
          arrivePlatformChanged: nextPlatformChanged
        };
        legs.push(leg);
      }
    }

    console.log('Built ' + legs.length + ' legs');
    return legs;
  }

module.exports = {
  formatUnixTime: formatUnixTime,
  calculateDuration: calculateDuration,
  checkPlatformChanged: checkPlatformChanged,
  processConnection: processConnection,
  processConnectionDetail: processConnectionDetail
};
