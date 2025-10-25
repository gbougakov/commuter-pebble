# 🚆 Commuter

![](misc/banner2x.png)

A real-time Belgian railway (NMBS/SNCB) train schedule viewer for Pebble smartwatches. Check departures, delays, and platform changes directly from your wrist.

## Features

- **Real-time Train Data**: Live departure times, delays, and platform information via the [iRail API](https://api.irail.be/)
- **Smart Schedules**: Auto-switch routes based on time of day (e.g., morning commute vs. evening return)
- **Customizable Favorites**: Select up to 6 favorite Belgian stations
- **Detailed Connection Info**: View full journey details including transfers and stops
- **Visual Indicators**: At-a-glance icons for connections, airport trains, and platform changes

## Installation

### Build & Install

```bash
# Build the app
pebble build

# Install to emulator
pebble install --emulator aplite

# Or install to your watch
pebble install --phone <your-phone-ip>
```

## Configuration

### Setting Up Favorite Stations

1. Open the **Pebble app** on your phone
2. Navigate to **Settings → Commuter**
3. Search and select up to **6 favorite stations**
4. Tap **Save**

### Creating Smart Schedules

Smart schedules automatically switch your route based on the day/time:

1. Tap **Add Schedule**
2. Configure from/to stations, days of week, and time range
3. Toggle **Enabled/Disabled** as needed

## Usage

### Main Screen
- **Top Row**: Station selector (tap to cycle through favorites)
- **Train Rows**: Departures with time, destination, platform, and delay
- **Tap Train**: View detailed connection information

### Visual Indicators
| Icon | Meaning |
|------|---------|
| 🔄 | Connection required (transfer) |
| ✈️ | Airport train |
| **Filled-in Platform** | Normal platform |
| **Outline-only Platform** | Platform changed |
| **+X min** | Delay in minutes |

## Acknowledgments

- **[iRail](https://irail.be/)**: For providing the open-source Belgian railway API
- **NMBS/SNCB**: For original train data

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Made in Leuven by Werknaam**
