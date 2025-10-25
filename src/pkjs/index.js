// iRail API URLs
var IRAIL_API_URL = 'https://api.irail.be/connections/';
var IRAIL_STATIONS_URL = 'https://api.irail.be/v1/stations?format=json';

// Station cache (fetched from API)
var stationCache = [];

// Default station IDs (fallback if no config and backward compatibility)
var STATION_IDS = {
    'Brussels-Central': 'BE.NMBS.008813003',
    'Antwerp-Central': 'BE.NMBS.008821006',
    'Ghent-Sint-Pieters': 'BE.NMBS.008892007',
    'Liège-Guillemins': 'BE.NMBS.008841004',
    'Leuven': 'BE.NMBS.008833001'
};

// Message type constants
var MSG_REQUEST_DATA = 1;
var MSG_SEND_DEPARTURE = 2;
var MSG_SEND_COUNT = 3;
var MSG_REQUEST_DETAILS = 4;
var MSG_SEND_DETAIL = 5;
var MSG_SEND_STATION_COUNT = 6;
var MSG_SEND_STATION = 7;
var MSG_SET_ACTIVE_ROUTE = 8;

// Debounce timer for API requests
var requestDebounceTimer = null;
var DEBOUNCE_DELAY = 500; // milliseconds

// Store current route and connection identifiers for detail requests
// These are persisted to localStorage for continuity across app restarts
var currentFromStation = '';
var currentToStation = '';
var connectionIdentifiers = []; // Array of {vehicle, departTime} for each departure

// LocalStorage keys
var STORAGE_KEY_FROM_STATION = 'nmbs_from_station';
var STORAGE_KEY_TO_STATION = 'nmbs_to_station';
var STORAGE_KEY_CONNECTIONS = 'nmbs_connections';
var STORAGE_KEY_STATION_CACHE = 'nmbs_station_cache';
var STORAGE_KEY_FAVORITE_STATIONS = 'nmbs_favorite_stations';
var STORAGE_KEY_SMART_SCHEDULES = 'nmbs_smart_schedules';

// Load persisted data from localStorage
function loadPersistedData() {
    try {
        var storedFrom = localStorage.getItem(STORAGE_KEY_FROM_STATION);
        var storedTo = localStorage.getItem(STORAGE_KEY_TO_STATION);
        var storedConnections = localStorage.getItem(STORAGE_KEY_CONNECTIONS);

        if (storedFrom && STATION_IDS[storedFrom]) {
            currentFromStation = storedFrom;
            console.log('Loaded from station: ' + currentFromStation);
        }

        if (storedTo && STATION_IDS[storedTo]) {
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
            localStorage.setItem(STORAGE_KEY_FROM_STATION, currentFromStation);
        }
        if (currentToStation) {
            localStorage.setItem(STORAGE_KEY_TO_STATION, currentToStation);
        }
        localStorage.setItem(STORAGE_KEY_CONNECTIONS, JSON.stringify(connectionIdentifiers));
        console.log('Persisted data saved');
    } catch (e) {
        console.log('Error saving persisted data: ' + e.message);
    }
}

// Fetch train data from iRail API (by iRail IDs)
function fetchTrainDataById(fromId, toId) {
    // Store current route for detail requests
    currentFromStation = fromId;
    currentToStation = toId;
    connectionIdentifiers = [];

    // Persist station selection to localStorage
    savePersistedData();

    if (!fromId || !toId) {
        console.log('Invalid station IDs: ' + fromId + ' -> ' + toId);
        return;
    }

    // Can't request same station for from/to
    if (fromId === toId) {
        console.log('From and To stations are the same, sending empty result');
        Pebble.sendAppMessage({
            'MESSAGE_TYPE': MSG_SEND_COUNT,
            'DATA_COUNT': 0
        });
        return;
    }

    // Build API URL
    var url = IRAIL_API_URL +
        '?from=' + encodeURIComponent(fromId) +
        '&to=' + encodeURIComponent(toId) +
        '&format=json' +
        '&lang=en';

    console.log('Fetching: ' + url);

    // Make HTTP request
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = function () {
        if (xhr.readyState === 4) {
            if (xhr.status === 200) {
                console.log('Response received');
                try {
                    var response = JSON.parse(xhr.responseText);
                    processTrainData(response);
                } catch (e) {
                    console.log('JSON parse error: ' + e.message);
                    // Send empty result on parse error
                    Pebble.sendAppMessage({
                        'MESSAGE_TYPE': MSG_SEND_COUNT,
                        'DATA_COUNT': 0
                    });
                }
            } else {
                console.log('Request failed: ' + xhr.status + ' - ' + xhr.responseText);
                // Send empty result on API error
                Pebble.sendAppMessage({
                    'MESSAGE_TYPE': MSG_SEND_COUNT,
                    'DATA_COUNT': 0
                });
            }
        }
    };
    xhr.onerror = function () {
        console.log('Network error');
    };
    xhr.send();
}

// Process iRail API response and send to watch
function processTrainData(response) {
    console.log('Processing response: ' + JSON.stringify(response).substring(0, 200));

    if (!response.connection || response.connection.length === 0) {
        console.log('No connections found');
        // Send count of 0
        Pebble.sendAppMessage({
            'MESSAGE_TYPE': MSG_SEND_COUNT,
            'DATA_COUNT': 0
        });
        return;
    }

    var connections = response.connection;
    var count = Math.min(connections.length, 11); // Limit to 11 departures

    console.log('Found ' + count + ' connections');
    console.log('First connection: ' + JSON.stringify(connections[0]));

    // Send count first
    Pebble.sendAppMessage({
        'MESSAGE_TYPE': MSG_SEND_COUNT,
        'DATA_COUNT': count
    }, function () {
        console.log('Count sent: ' + count);
        // Start sending departures
        sendDeparture(connections, 0);
    }, function (e) {
        console.log('Failed to send count: ' + e.error.message);
    });
}

// Send departures one at a time (recursive with callbacks)
function sendDeparture(connections, index) {
    if (index >= connections.length) {
        console.log('All departures sent');
        return;
    }

    var conn = connections[index];

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
    // iRail API: platforminfo.normal == "0" means changed, "1" means no change
    var platformChanged = 0;
    if (conn.departure.platforminfo) {
        console.log('Platform info: ' + JSON.stringify(conn.departure.platforminfo));
        if (conn.departure.platforminfo.normal === "0") {
            platformChanged = 1;
        }
    }
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
    connectionIdentifiers[index] = {
        vehicle: conn.departure.vehicle || '',
        departTime: departTime
    };

    // Persist connection identifiers (will save after each departure)
    console.log('oy');
    savePersistedData();
    console.log('yo');

    // Build message (DESTINATION field now contains direction/terminus)
    var message = {
        'MESSAGE_TYPE': MSG_SEND_DEPARTURE,
        'DEPARTURE_INDEX': index,
        'DESTINATION': direction.substring(0, 31), // Limit to 31 chars
        'DEPART_TIME': formatUnixTime(departTime),
        'ARRIVE_TIME': formatUnixTime(arriveTime),
        'PLATFORM': platform.substring(0, 3), // Limit to 3 chars
        'TRAIN_TYPE': trainType.substring(0, 7), // Limit to 7 chars
        'DURATION': duration.substring(0, 7), // Limit to 7 chars
        'DEPART_DELAY': Math.floor(departDelay / 60),
        'ARRIVE_DELAY': Math.floor(arriveDelay / 60),
        'IS_DIRECT': isDirect,
        'PLATFORM_CHANGED': platformChanged
    };

    console.log('Sending departure ' + index + ': ' + direction);

    // Send message with callbacks
    Pebble.sendAppMessage(message, function () {
        // Success - send next departure
        console.log('Departure ' + index + ' sent successfully');
        sendDeparture(connections, index + 1);
    }, function (e) {
        console.log('Failed to send departure ' + index + ': ' + e.error.message);
        // Try next one anyway
        sendDeparture(connections, index + 1);
    });
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

// Fetch and send connection details
function fetchConnectionDetails(departureIndex) {
    console.log('Fetching details for departure ' + departureIndex);

    if (!connectionIdentifiers[departureIndex]) {
        console.log('No connection identifier found for index ' + departureIndex);
        return;
    }

    var identifier = connectionIdentifiers[departureIndex];
    // currentFromStation and currentToStation are now iRail IDs
    var fromId = currentFromStation;
    var toId = currentToStation;

    if (!fromId || !toId) {
        console.log('Invalid stations for detail request');
        return;
    }

    var url = IRAIL_API_URL +
        '?from=' + encodeURIComponent(fromId) +
        '&to=' + encodeURIComponent(toId) +
        '&date=' + formatDate(identifier.departTime * 1000) +
        '&time=' + formatTime(identifier.departTime * 1000 - 10 * 60 * 1000) +
        '&format=json' +
        '&lang=en';

    console.log('Fetching details from: ' + url);

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
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

                    if (matchedConn) {
                        console.log('Found matching connection');
                        sendConnectionDetail(matchedConn, departureIndex);
                    } else {
                        console.log('Connection not found in fresh data');
                    }
                }
            } catch (e) {
                console.log('Detail fetch parse error: ' + e.message);
            }
        }
    };
    xhr.send();
}

// Helper function to check if platform changed
// iRail API: platforminfo.normal == "0" means changed, "1" means no change
function checkPlatformChanged(departureOrArrival) {
    if (!departureOrArrival.platforminfo) return 0;
    return (departureOrArrival.platforminfo.normal === "0") ? 1 : 0;
}

// Fetch station list from iRail API and cache it
function fetchStations() {
    console.log('Fetching stations from iRail API...');
    var xhr = new XMLHttpRequest();
    xhr.open('GET', IRAIL_STATIONS_URL, true);
    xhr.onload = function() {
        if (xhr.readyState === 4 && xhr.status === 200) {
            try {
                var response = JSON.parse(xhr.responseText);
                if (response.station && response.station.length > 0) {
                    // Map to simplified structure
                    stationCache = response.station.map(function(s) {
                        return {
                            id: s.id,                          // "BE.NMBS.008813003"
                            name: s.name,                      // "Brussels-Central"
                            standardName: s.standardname       // "Brussel-Centraal"
                        };
                    });

                    // Save to localStorage
                    localStorage.setItem(STORAGE_KEY_STATION_CACHE, JSON.stringify(stationCache));
                    console.log('Fetched and cached ' + stationCache.length + ' stations');
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

// Load cached stations from localStorage
function loadCachedStations() {
    try {
        var cached = localStorage.getItem(STORAGE_KEY_STATION_CACHE);
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

// Send full connection details to watch (leg-by-leg)
function sendConnectionDetail(conn, departureIndex) {
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

    // Send leg count first
    Pebble.sendAppMessage({
        'MESSAGE_TYPE': MSG_SEND_DETAIL,
        'DEPARTURE_INDEX': departureIndex,
        'LEG_COUNT': legs.length
    }, function () {
        console.log('Leg count sent: ' + legs.length);
        // Start sending individual legs
        sendLeg(legs, 0);
    }, function (e) {
        console.log('Failed to send leg count: ' + e.error.message);
    });
}

// Send legs one at a time (recursive with callbacks)
function sendLeg(legs, index) {
    if (index >= legs.length) {
        console.log('All legs sent');
        return;
    }

    var leg = legs[index];

    var message = {
        'MESSAGE_TYPE': MSG_SEND_DETAIL,
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
        'LEG_ARRIVE_PLATFORM_CHANGED': leg.arrivePlatformChanged
    };

    console.log('Sending leg ' + index + ': ' + leg.departStation + ' → ' + leg.arriveStation);

    Pebble.sendAppMessage(message, function () {
        // Success - send next leg
        sendLeg(legs, index + 1);
    }, function (e) {
        console.log('Failed to send leg ' + index + ': ' + e.error.message);
        // Try next one anyway
        sendLeg(legs, index + 1);
    });
}

// Listen for messages from watch
Pebble.addEventListener('appmessage', function (e) {
    console.log('Message from watch: ' + JSON.stringify(e.payload));

    var messageType = e.payload.MESSAGE_TYPE;

    if (messageType === MSG_REQUEST_DATA) {
        // Check if using new iRail ID format or old name format
        var fromId = e.payload.FROM_STATION_ID;
        var toId = e.payload.TO_STATION_ID;

        if (fromId && toId) {
            console.log('Data requested (by ID): ' + fromId + ' -> ' + toId);
            currentFromStation = fromId;
            currentToStation = toId;
        } else {
            // Fallback to old format (station names)
            var fromStation = e.payload.FROM_STATION;
            var toStation = e.payload.TO_STATION;
            console.log('Data requested (by name): ' + fromStation + ' -> ' + toStation);
            fromId = STATION_IDS[fromStation];
            toId = STATION_IDS[toStation];
            currentFromStation = fromStation;
            currentToStation = toStation;
        }

        // Clear any pending request
        if (requestDebounceTimer) {
            clearTimeout(requestDebounceTimer);
            console.log('Debouncing request...');
        }

        // Debounce the API request
        requestDebounceTimer = setTimeout(function () {
            console.log('Executing debounced request');
            fetchTrainDataById(fromId, toId);
            requestDebounceTimer = null;
        }, DEBOUNCE_DELAY);
    } else if (messageType === MSG_REQUEST_DETAILS) {
        var departureIndex = e.payload.DEPARTURE_INDEX;
        console.log('Details requested for departure ' + departureIndex);
        fetchConnectionDetails(departureIndex);
    }
});

Pebble.addEventListener('ready', function () {
    console.log('PebbleKit JS ready!');

    // Load cached stations immediately (for offline use)
    loadCachedStations();

    // Fetch fresh station list in background
    fetchStations();

    // Load persisted station selection and connection data
    loadPersistedData();

    // Load user configuration and send to watch
    var favoritesJson = localStorage.getItem(STORAGE_KEY_FAVORITE_STATIONS);
    if (favoritesJson) {
        try {
            var favoriteStations = JSON.parse(favoritesJson);
            console.log('Loading saved configuration with ' + favoriteStations.length + ' stations');
            sendStationsToWatch(favoriteStations);

            // Evaluate schedules and set active route
            var activeRoute = evaluateSchedules();
            if (activeRoute) {
                console.log('Found active schedule');
                setActiveRoute(favoriteStations, activeRoute.fromId, activeRoute.toId);
            } else {
                console.log('No active schedule, will use default route');
            }
        } catch (e) {
            console.log('Error loading configuration: ' + e.message);
        }
    } else {
        console.log('No saved configuration, watch will use defaults');
    }

    if (currentFromStation && currentToStation) {
        console.log('Restored session: ' + currentFromStation + ' -> ' + currentToStation);
    }
});

// Evaluate smart schedules and return active route (if any)
function evaluateSchedules() {
    try {
        var schedulesJson = localStorage.getItem(STORAGE_KEY_SMART_SCHEDULES);
        if (!schedulesJson) {
            console.log('No smart schedules configured');
            return null;
        }

        var schedules = JSON.parse(schedulesJson);
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
            var dayMatches = false;
            for (var j = 0; j < schedule.days.length; j++) {
                if (schedule.days[j] === currentDay) {
                    dayMatches = true;
                    break;
                }
            }

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

// Configuration page handlers
Pebble.addEventListener('showConfiguration', function() {
    console.log('Opening configuration page');
    var configUrl = 'https://assets-eu.gbgk.net/nmbs-pebble/config.html';
    Pebble.openURL(configUrl);
});

Pebble.addEventListener('webviewclosed', function(e) {
    console.log('Configuration closed');

    if (e && e.response) {
        try {
            var config = JSON.parse(decodeURIComponent(e.response));
            console.log('Received config: ' + JSON.stringify(config));

            // Save favorite stations
            if (config.favoriteStations && config.favoriteStations.length > 0) {
                localStorage.setItem(STORAGE_KEY_FAVORITE_STATIONS, JSON.stringify(config.favoriteStations));
                console.log('Saved ' + config.favoriteStations.length + ' favorite stations');

                // Send stations to watch
                sendStationsToWatch(config.favoriteStations);
            }

            // Save smart schedules
            if (config.smartSchedules) {
                localStorage.setItem(STORAGE_KEY_SMART_SCHEDULES, JSON.stringify(config.smartSchedules));
                console.log('Saved ' + config.smartSchedules.length + ' smart schedules');

                // Evaluate and set active route
                var activeRoute = evaluateSchedules();
                if (activeRoute) {
                    setActiveRoute(config.favoriteStations, activeRoute.fromId, activeRoute.toId);
                }
            }

        } catch (e) {
            console.log('Error parsing configuration: ' + e.message);
        }
    } else {
        console.log('Configuration cancelled or no data received');
    }
});

// Send favorite stations to watch
function sendStationsToWatch(stationIds) {
    console.log('Sending ' + stationIds.length + ' stations to watch');

    // Send count first
    Pebble.sendAppMessage({
        'MESSAGE_TYPE': MSG_SEND_STATION_COUNT,
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
    var station = getStationById(stationId);

    if (!station) {
        console.log('Station not found in cache: ' + stationId);
        // Skip and continue
        sendStationSequential(stationIds, index + 1);
        return;
    }

    var message = {
        'MESSAGE_TYPE': MSG_SEND_STATION,
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
        'MESSAGE_TYPE': MSG_SET_ACTIVE_ROUTE,
        'CONFIG_FROM_INDEX': fromIndex,
        'CONFIG_TO_INDEX': toIndex
    }, function() {
        console.log('Active route set successfully');
    }, function(e) {
        console.log('Failed to set active route: ' + e.error.message);
    });
}
