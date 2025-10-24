// iRail API base URL
var IRAIL_API_URL = 'https://api.irail.be/connections/';

// Station ID mapping (iRail uses specific station IDs)
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

// Debounce timer for API requests
var requestDebounceTimer = null;
var DEBOUNCE_DELAY = 500; // milliseconds

// Store current route and connection identifiers for detail requests
var currentFromStation = '';
var currentToStation = '';
var connectionIdentifiers = []; // Array of {vehicle, departTime} for each departure

// Fetch train data from iRail API
function fetchTrainData(fromStation, toStation) {
    // Store current route for detail requests
    currentFromStation = fromStation;
    currentToStation = toStation;
    connectionIdentifiers = [];
    var fromId = STATION_IDS[fromStation];
    var toId = STATION_IDS[toStation];

    if (!fromId || !toId) {
        console.log('Invalid station names: ' + fromStation + ' -> ' + toStation);
        return;
    }

    // Can't request same station for from/to
    if (fromStation === toStation) {
        console.log('From and To stations are the same, sending empty result');
        Pebble.sendAppMessage({
            'MESSAGE_TYPE': MSG_SEND_COUNT,
            'DATA_COUNT': 0
        });
        return;
    }

    // Build API URL with current time
    var now = new Date();
    var dateStr = formatDate(now);
    var timeStr = formatTime(now);

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

    // Check platform change
    var platform = conn.departure.platform || '?';
    var normalPlatform = (conn.departure.platforminfo && conn.departure.platforminfo.normal) ?
        conn.departure.platforminfo.normal : platform;
    var platformChanged = (platform !== normalPlatform) ? 1 : 0;

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
    var fromId = STATION_IDS[currentFromStation];
    var toId = STATION_IDS[currentToStation];

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

// Send full connection details to watch
function sendConnectionDetail(conn, departureIndex) {
    // Get actual destination
    var destination = 'Unknown';
    if (conn.arrival.stationinfo && conn.arrival.stationinfo.name) {
        destination = conn.arrival.stationinfo.name;
    }

    // Get direction
    var direction = 'Unknown';
    if (conn.departure.direction && conn.departure.direction.name) {
        direction = conn.departure.direction.name;
    }

    // Send basic detail info
    Pebble.sendAppMessage({
        'MESSAGE_TYPE': MSG_SEND_DETAIL,
        'DEPARTURE_INDEX': departureIndex,
        'DETAIL_DESTINATION': destination.substring(0, 31),
        'DETAIL_DIRECTION': direction.substring(0, 31),
        'VIA_COUNT': 0  // TODO: implement via stations
    }, function () {
        console.log('Detail sent for departure ' + departureIndex);
    }, function (e) {
        console.log('Failed to send detail: ' + e.error.message);
    });
}

// Listen for messages from watch
Pebble.addEventListener('appmessage', function (e) {
    console.log('Message from watch: ' + JSON.stringify(e.payload));

    var messageType = e.payload.MESSAGE_TYPE;

    if (messageType === MSG_REQUEST_DATA) {
        var fromStation = e.payload.FROM_STATION;
        var toStation = e.payload.TO_STATION;
        console.log('Data requested: ' + fromStation + ' -> ' + toStation);

        // Clear any pending request
        if (requestDebounceTimer) {
            clearTimeout(requestDebounceTimer);
            console.log('Debouncing request...');
        }

        // Debounce the API request
        requestDebounceTimer = setTimeout(function () {
            console.log('Executing debounced request');
            fetchTrainData(fromStation, toStation);
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
});
