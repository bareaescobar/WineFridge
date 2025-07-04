# Smart Wine Fridge Web UI

A modern web interface for managing your smart wine fridge.

## Prerequisites

- Node.js (v18 or higher recommended)
- npm package manager

## Technology Stack

- Build tool: Vite
- UI Components: Swiper
- Package manager: npm
- Code Formatting: Prettier

## Installation

1. Clone the repository:
```bash
git clone https://github.com/andriyl/smart-wine-fridge_web.git
```

2. Install dependencies:
```bash
npm install
```

## Development

### Running the Development Server

```bash
npm run dev
```

This will start the development server with hot module replacement (HMR) enabled. The app will be available at `http://localhost:5173`.

### Building for Production

```bash
npm run build
```

This will create a production-ready build in the `public` directory.

### Previewing Production Build

```bash
npm run preview
```

This will serve the production build locally for testing.

## Code Style and Formatting

The project uses Prettier for code formatting. You can:

- Check formatting:
```bash
npm run format:check
```

- Automatically format code:
```bash
npm run format
```

## Deployment

The application is built using Vite and can be deployed to any static hosting service. After building with `npm run build`, the contents of the `public` directory can be deployed.

## Project Structure

- `/backend` - Source code
  - `db-routes.cjs`
  - `server.cjs`
- `/content` - Project content and configuration
  - `main.json` - Main configuration file
- `/public` - Production build output

## Pages

| File Name | Description |
|-----------|-------------|
| auth | User authentication |
| forgot-password | Password reset functionality |
| connect-fridge | Initial fridge connection setup |
| setup-fridge | Fridge configuration |
| index | Main dashboard |
| account | User profile management |
| notifications | Notification center |
| load-bottle | Add new wines |
| unload-bottle | Remove wines |
| swap-bottle | Wine swapping functionality |
| drawers | Wine drawer management |

# Mosquitto MQTT broker (TCP + WebSocket)
Managing comunication (pub/sub) between Rasbery PI, web, ESP32

## Installing Mosquitto via Homebrew (macOS)
```bash
brew install mosquitto
brew services start mosquitto
```
By default, the broker only listens on localhost. You need to manually create a config file.

### Creating the Configuration File
> The broker should have WebSocket setup in configuration file.

Create a mosquitto.conf file, for example: ./mosquitto/mosquitto.conf

Add these lines to the file:

```txt
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

### Starting Mosquitto with This Config

```bash
mosquitto -c ./mosquitto/mosquitto.conf -v
```

The -v parameter allows to see the logs.

# Features (Web + MQTT) - FRIDGE ONLY

## 2. Home Dashboard & System Overview

This page is the central part of the user interface and dynamically displays the current state of the fridge based on data from `inventory.json`.

<img width="300" alt="Screenshot 2025-07-04 at 13 22 29" src="https://github.com/user-attachments/assets/b0bae324-2ebb-419e-9e57-2cfe06b5c42f" />

### Data Source

The `inventory.json` file is used for:
- Retrieving the current wine stock in each drawer
- Calculating the number of occupied and free slots
- Calculating the fridge occupancy percentage
- Displaying the temperature, humidity, and mode of each zone

### Zones

The fridge is logically divided into three zones:
- `Upper` — drawer_1, drawer_2, drawer_3
- `Middle` — drawer_4, drawer_5, drawer_6
- `Lower` — drawer_7, drawer_8, drawer_9

For each zone, the following parameters are calculated based on the JSON data:
- Temperature
- Humidity
- Zone mode (red, white, sparkling, etc.)
- Zone name and its visual styling

### Interface Synchronization via MQTT

The homepage interface dynamically updates in response to events received from the device (RPI).

In particular, when `inventory.json` changes (e.g., after adding or removing a bottle, or after updating zone settings), the RPI sends an MQTT message with the action `inventory_updated`.

#### RPI -> Web: inventory updated
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "settings_updated",
  "source": "rpi",
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

### Zone Settings

Each zone has a button (arrow icon) in the upper-right corner. Clicking it opens a modal window where you can set the target temperature and humidity.

Zone supports two modes: **Automatic** and **Manual**.

<img width="300" alt="Screenshot 2025-07-04 at 13 23 23" src="https://github.com/user-attachments/assets/09f55a70-5b69-40d1-876d-e5d3ba044b28" />
<img width="300" alt="Screenshot 2025-07-04 at 13 23 27" src="https://github.com/user-attachments/assets/28501f09-a162-40e3-8141-7816fce9c1ee" />

- After filling out the form, an MQTT message is sent to the `winefridge/status` topic with updated zone parameters:

#### Web → RPI

```json
{
  "timestamp": "2025-06-10T12:00:15Z",
  "source": "web_client",
  "target": "rpi_server",
  "message_type": "command",
  "data": {
    "action": "update_setting",
    "mode": "manually",
    "target": "17",
    "humidity": "68",
    "zone": "upper"
  }
}
```

#### RPI → Web

- The system waits for confirmation (e.g., backend updates `inventory.json` and emits a message):

```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "settings_updated",
  "source": "rpi",
  "data": {
    "success": true,
    "mode": "manually",
    "target": "17",
    "humidity": "68",
    "zone": "upper"
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

- Once received, the settings modal is closed, and the main dashboard is re-rendered with updated values.

## 3. Load Bottle Workflow
This module implements the complete cycle of adding a wine bottle to the fridge:  
from barcode scanning to physical bottle placement in the drawer.

## MQTT Topics

| Name      | Topic                       | Direction |
| --------- | --------------------------- | --------- |
| RPI → Web | `winefridge/status`  | subscribe |

## Step-by-Step Instructions

### 1. Go to the load bottle page
- Click the “Load” button on the main page to go to the Load Bottle screen.

<img width="386" alt="Screenshot 2025-06-24 at 12 30 32" src="https://github.com/user-attachments/assets/ebd200af-c70a-4515-8d0c-0801a5b43f28" />

### 2. Bottle Scanning

- Person is scanning the **barcode** of the **bottle**.
#### RPI -> Web
#### Barcode scanned emulation command:
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "barcode_scanned",
  "source": "barcode_scanner",
  "data": {
    "barcode": "8410415520016"
  },
  "timestamp":"2025-06-10T12:00:15Z"
}'
``` 
- Search by barcode in the collection: `wineCatalog.json`

- If not found, shown page with 'Not found' error:
<img width="353" alt="image" src="https://github.com/user-attachments/assets/5dea3ea7-8840-446b-99ab-3dd7014cb649" />

- If found, shown page with wine data:
<img width="361" alt="image" src="https://github.com/user-attachments/assets/bd55d052-3e71-4765-a590-15d157199b1a" />

### 3. Loading Confirmation

- User clicks **"Confirm & Load Bottle"**
- MQTT command `start_load` is sent:
#### Web → RPI:
```json
{
    "timestamp": "2025-06-10T12:00:00Z",
    "source": "web",
    "data": {
        "action": "start_load",
        "barcode": "8410415520628",
        "name": "Señorío de los Llanos"
    }
}
```

---
### Returning a Previously Extracted Bottle

If the user scans a **barcode that matches one stored in `extracted.json`**, the system interprets this as an attempt to **return the bottle to the fridge**.

#### Web → RPI: Return Bottle

In response, a corresponding **MQTT message** is sent to inform the RPI about the return attempt:

```json
{
  "timestamp": "2025-07-04T11:17:47.956Z",
  "source": "web",
  "data": {
    "action": "start_return",
    "barcode": "8410337311168",
    "locations": [
      {
        "drawer": "drawer_5",
        "position": 1,
        "timestamp": "2025-07-04T11:09:41.221Z"
      }
    ]
  }
}
```
---

### 4. Receiving Response from RPI
Expected that bottle loaded at a specific position.
#### RPI → Web
#### Bottle loading emulation command: 
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "expect_bottle",
  "source": "rpi",
  "data": {
  "position": 3,
  "drawer": 1,
  "barcode": "8410415520628",
    "name": "Señorío de los Llanos"
  },
  "timeout_ms": 30000,
  "timestamp": "2025-06-10T12:00:05Z"
}'
```
- As a result the next page is shown:
<img width="260" alt="image" src="https://github.com/user-attachments/assets/057b5e93-4b3a-42d4-b4cf-621673a8c1c8" />

### 5. Waiting for Drawer Events
After bottle placement, the system can receive several possible events:

#### Successful Placement emulation command:
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_placed",
  "source": "rpi",
  "data": {
    "success": true,
    "drawer": 1,
    "position": 3,
    "barcode": "8410415520628",
    "name": "Señorío de los Llanos"
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

- The page is shown:
<img width="251" alt="image" src="https://github.com/user-attachments/assets/31315586-b9db-4126-9548-ae53366a8d0f" />

#### Wrong place emulation command:
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_placed",
  "source": "rpi",
  "data": {
    "success": false,
    "drawer": 1",
    "position": 3,
    "barcode": "8410415520628",
    "name": "Señorío de los Llanos",
    "error": "wrong place"
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

- The page is shown:
<img width="255" alt="image" src="https://github.com/user-attachments/assets/e1bb3fd1-540d-42b9-9004-1000b5144d40" />
<br><br><br><br><br><br><br><br><br>

## 4. Unload Bottle Workflow

### There are 2 possible cases: 
<img width="408" alt="Screenshot 2025-06-18 at 11 26 50" src="https://github.com/user-attachments/assets/6c2b01ed-3b2d-41b5-8738-74ccb685548a" />

### 1.  Pick Manually

- We filter the catalog (wineCatalog.json) and show only the wines that are in the fridge (inventory.json).
<img width="404" alt="Screenshot 2025-06-18 at 11 30 02" src="https://github.com/user-attachments/assets/2d7db090-6f3d-4ad3-900a-c4f941d027fc" />

- After selecting a specific wine, we display its full information using data from wineCatalog.json and inventory.json.
<img width="407" alt="Screenshot 2025-06-18 at 11 30 25" src="https://github.com/user-attachments/assets/b9d17651-873b-4d4b-9bb5-09f67b824e81" />

- When the “Confirm & Take Wine Out” button is clicked, we send an MQTT message with the event details:
### Web -> RPI
```json
{
  "timestamp": "2025-06-10T12:00:05Z",
  "source": "web",
  "data": {
    "action": "start_unload",
    "barcode": "8410415520628",
    "name": "Señorío de los Llanos",
    "drawer": 2,
    "position": 4
  }
}
```
- The page is shown:
<img width="404" alt="Screenshot 2025-06-18 at 11 31 11" src="https://github.com/user-attachments/assets/4febacae-7ac3-4d10-bd5d-97879d9ba713" />

- We wait for a message confirming that the bottle was successfully unloaded:
### RPI -> Web
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_unloaded",
  "source": "rpi",
  "data": {
    "success": true,
    "drawer": 2,
    "position": 4,
    "barcode": "8410415520628",
    "name": "Señorío de los Llanos"
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

- The page is shown:
<img width="405" alt="Screenshot 2025-06-18 at 11 31 36" src="https://github.com/user-attachments/assets/7eb45082-0393-4d6e-952f-b78638efa33e" />

### 2. Suggest

- We show a screen with a list of suggestions (e.g. food or atmosphere preferences):
<img width="404" alt="Screenshot 2025-06-18 at 11 32 17" src="https://github.com/user-attachments/assets/f9b4a6fc-4ef8-44ec-a0b3-b4bf0eb5c0f1" />

- After the user selects an offer, we filter the data from inventory.json and wineCatalog.json based on the selected type and available wines. We then display the list:
<img width="406" alt="Screenshot 2025-06-18 at 11 32 40" src="https://github.com/user-attachments/assets/2c5fabb2-3f66-47f2-b353-8df7a17689bb" />

- After the wine is selected and “Confirm & Take Wine Out” button is clicked, the process continues exactly the same as with "Pick Manually" (see above).

## 9. Zone Lighting Control System

The interface allows you to configure lighting for both individual zones and the entire fridge.
To access the lighting settings menu, click the settings icon located on the main dashboard.

<img width="300" alt="Screenshot 2025-07-04 at 13 16 09" src="https://github.com/user-attachments/assets/062799e2-71a8-4ca3-ba2f-5409150536d9" />

#### Per-Zone Lighting
<img width="300" alt="Screenshot 2025-07-04 at 13 16 37" src="https://github.com/user-attachments/assets/1813c42e-e419-4295-94fe-d66f12b91c1a" />

#### Fridge-Wide Lighting
<img width="300" alt="Screenshot 2025-07-04 at 13 16 28" src="https://github.com/user-attachments/assets/f4e284a3-960c-4eb8-9f8b-25cf4546bdab" />

- When settings are saved, an appropriate MQTT message is sent.

#### Web → RPI: Set Zone Brightness
```json
{
  "timestamp": "2025-07-04T09:42:59.010Z",
  "source": "web_client",
  "target": "rpi_server",
  "message_type": "command",
  "data": {
    "action": "set_brightness",
    "zone": "upper",
    "value": "45",
    "color_temp": "neutral_white"
  }
}
```

#### Web → RPI: Set Fridge Lighting Mode
```json
{
  "timestamp": "2025-07-04T09:44:59.492Z",
  "source": "web_client",
  "target": "rpi_server",
  "message_type": "command",
  "data": {
    "action": "set_lighting_mode",
    "energy_saving": true,
    "night_mode": false
  }
}
```

## 6. Swap Bottle Positions

During a bottle swap process, we listen for a series of `bottle_event` messages from the RPI that indicate when two bottles are removed and placed back into the fridge. When the sequence completes correctly, a UI update and a success modal are triggered.

#### RPI -> Web: take first bottle
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_event",
  "source": "rpi_server",
  "data": {
    "event": "removed",
    "drawer": "drawer_5",
    "position": 2
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```
#### RPI -> Web: take second bottle
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_event",
  "source": "rpi_server",
  "data": {
    "event": "removed",
    "drawer": "drawer_5",
    "position": 1
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```
#### RPI -> Web: return second bottle
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_event",
  "source": "rpi_server",
  "data": {
    "event": "placed",
    "drawer": "drawer_5",
    "position": 1
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```
#### RPI -> Web: take first bottle
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_event",
  "source": "rpi_server",
  "data": {
    "event": "placed",
    "drawer": "drawer_5",
    "position": 2
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

Visual Feedback in UI

- While the swap process is ongoing, bottle details are dynamically rendered into placeholder slots:
<img width="300" alt="Screenshot 2025-07-04 at 13 45 42" src="https://github.com/user-attachments/assets/2c6fa381-b2c0-4ced-aacc-dd8957c3e941" />

Swap Completion

- Once both bottles are placed correctly, a request is sent to the backend to confirm the swap, and a success modal appears:
<img width="300" alt="Screenshot 2025-07-04 at 13 46 16" src="https://github.com/user-attachments/assets/5ebffac0-1b63-4395-82bf-d4fa1343b293" />

## 7. Automatic Bottle Removal

While on the homepage, the client listens for MQTT events signaling that a bottle has been removed from the fridge. When such an event arrives, the corresponding bottle is **removed from `inventory.json`**.

#### RPI -> Web: remove bottle
```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "bottle_event",
  "source": "rpi_server",
  "data": {
    "event": "removed",
    "drawer": "drawer_5",
    "position": 2
  },
  "timestamp": "2025-06-10T12:00:05Z"
}'
```
Information about extracted bottles is stored in extracted.json. Each key is a barcode, and the value contains an array of locations where and when the bottle was removed.

```json
{
  "8410337311168": {
    "locations": [
      {
        "drawer": "drawer_5",
        "position": 1,
        "timestamp": "2025-07-04T10:54:47.604Z"
      }
    ]
  },
  "8410337311175": {
    "locations": [
      {
        "drawer": "drawer_5",
        "position": 2,
        "timestamp": "2025-07-04T10:56:08.442Z"
      }
    ]
  }
}
```

UI Feedback and Carousel

When a bottle is removed:
-A modal with a carousel is shown containing the recently extracted bottles.
-The display order reflects the extraction order.
-The bottle data is sourced directly from extracted.json.
 
<img width="300" alt="Screenshot 2025-07-04 at 14 09 45" src="https://github.com/user-attachments/assets/fe134753-3e10-4515-9d14-061ce4124f44" />

After a predefined timeout:
-The modal is shown notifying the user.
-The bottle is automatically removed from inventory.json.

<img width="300" alt="Screenshot 2025-07-04 at 14 09 48" src="https://github.com/user-attachments/assets/6bfd302f-292c-4400-bbb4-2609cc5632bf" />

## 8. Used Bottles Return

If the user scans a **barcode that matches one stored in `extracted.json`**, the system interprets this as an attempt to **return the bottle to the fridge**.

#### Web → RPI: Return Bottle

In response, a corresponding **MQTT message** is sent to inform the RPI about the return attempt:

```json
{
  "timestamp": "2025-07-04T11:17:47.956Z",
  "source": "web",
  "data": {
    "action": "start_return",
    "barcode": "8410337311168",
    "locations": [
      {
        "drawer": "drawer_5",
        "position": 1,
        "timestamp": "2025-07-04T11:09:41.221Z"
      }
    ]
  }
}
```

After that, the bottle loading flow proceeds as with a new bottle. In other words, we wait for an MQTT message:

```bash
mosquitto_pub -h localhost -p 1883 -t 'winefridge/status' -m '{
  "action": "expect_bottle",
  "source": "rpi",
  "data": {
    "position": 3,
    "drawer": 1,
    "barcode": "8410415520628",
    "name": "Señorío de los Llanos"
  },
  "timeout_ms": 30000,
  "timestamp": "2025-06-10T12:00:05Z"
}'
```

## 10. Drawer Detail Views

When clicking on a specific drawer on the main dashboard, the interface navigates to the Drawer Detail View page. This page provides extended information about the selected drawer’s contents and allows the user to view wine details or initiate bottle removal.

All information is based on a combination of two data sources: inventory.json and wine-catalog.json.

#### wine-catalog example
```json
{
  "version": "1.0",
  "wines": {
    "8410337311038": {
      "name": "Castaño Rosado Golosa Frescura",
      "winery": "Bodegas Castaño",
      "about_winery": "Bodegas Castaño is a family-run winery with a passion for Monastrell, combining tradition and modernity to create vibrant, character-driven wine.",
      "type": "Rose",
      "grape": "Monastrell",
      "region": "Yecla, Murcia",
      "country": "Spain",
      "vintage": "2023",
      "alcohol": 13,
      "volume": "750ml",
      "serving_temp": {
        "min": 16,
        "max": 18
      },
      "description": "This expressive rosé delights with its juicy freshness and vibrant character. Bursting with notes of ripe red berries, hints of watermelon, and a whisper of floral undertones, it offers a smooth, well-rounded texture. With a crisp, lingering finish, it’s ideal for those who seek charm, fruitiness, and elegance in every sip.",
      "flavours": "Juicy red berries such as strawberry and cherry, with floral hints and a rounded, refreshing finish.",
      "meal_type": ["Fish & Seafood", "Cheese"],
      "atmosphere": ["Picnic"],
      "avg_rating": 3.4,
      "price": "5-8 €",
      "img": "/img/wine/Rose/Castano-Rosado-Golosa-Frescura-Monastrell.png",
      "image_url": "/img/wine/Rose/Castano-Rosado-Golosa-Frescura-Monastrell-info-banner-2x.png"
    }
  }
}
```

<img width="300" alt="Screenshot 2025-07-04 at 17 07 22" src="https://github.com/user-attachments/assets/57765711-7862-46c4-8ab8-a6de0cd8f64f" />

Features

- Visual overview:
Displays all slots in the selected drawer, indicating which ones are occupied and which are free.
- Slot-specific wine data:
When clicking on a bottle in a slot:
- The selected slot is highlighted
- A short wine summary appears in the top section
- Displays name, wine type, volume, alcohol percentage, food pairing, country/region, and image
- Detailed info modal:
Clicking the info button opens a modal with full metadata about the selected wine and slot location.

<img width="300" alt="Screenshot 2025-07-04 at 17 07 37" src="https://github.com/user-attachments/assets/064e9a34-1014-43e2-9266-bdcf1488a4e2" />

- Initiate bottle removal:
Clicking the “Take wine out” button opens a modal window. After confirmation, an MQTT message with the start_unload action is sent for the selected bottle:

#### Web -> RPI
```json
{
  "timestamp": "2025-07-04T13:22:00Z",
  "source": "web",
  "data": {
    "action": "start_unload",
    "barcode": "8410337311274",
    "name": "Finca del Mar Cabernet Sauvignon Crianza"
  }
}
```

# Temporary API Endpoints

To enable testing and visualization of changes in the interface before the full MQTT logic is implemented on the RPI side, we created a set of temporary REST API endpoints. These endpoints allow manual manipulation of the inventory state and simulate key user actions.

They are implemented in the dbRoutes.js file and work with local JSON files: **inventory.json** , **extracted.json**.

### Endpoints:
| Method| Endpoint                              | Description                                                                                                              |
|-------|---------------------------------------|--------------------------------------------------------------------------------------------------------------------------|
| POST  | /update-inventory                   | Updates temperature, humidity, and mode for a given zone.                                                                |
| GET   | /inventory                          | Retrieves the current inventory state.                                                                                    |
| POST  | /swap-bottles                       | Swaps bottles between two positions. Used in the Slot-to-Slot Bottle Transfer functionality.                       |
| POST  | /remove-bottle                      | Removes a bottle from a slot based on the barcode.                                                                            |
| POST  | /add-extracted-bottle               | Adds a bottle to the list of recently extracted bottles(extracted.json)(timestamp, drawer, and position).Entries are automatically cleared after 3 hours.|
| DELETE| /remove-extracted-bottle/:barcode   | Deletes an extracted bottle entry by barcode (extracted.json).                                                        |
| GET   | /extracted                          | Retrieves all recently extracted bottles (within the last 3 hours).                                                             |

### Using in UI:

|Page / Feature            | Method | Endpoint                          | Description                                                                 |
|--------------------------|--------|-----------------------------------|-----------------------------------------------------------------------------|
| **Home / Setting Modal** | POST   | /update-inventory                 | Updates temperature, humidity.                                              |
| **All pages**            | GET    | /inventory                        | Automatically refreshes the UI to display the latest inventory state.       |
| **Swap Bottles**         | POST   | /swap-bottles                     | Swap bottle positions between slots.                                        |
| Unauthorired-unload Modal| POST   | /remove-bottle                    | Removes a bottle from a slot based on its barcode after extraction.         |
| Automatic bottle remove  | POST   | /add-extracted-bottle             | adding recently extracted bottles - extended.json                           |
| Automatic bottle remove  | DELETE | /remove-extracted-bottle/:barcode | removing recently extracted bottles - extended.json                         |
