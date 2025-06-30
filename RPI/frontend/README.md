# Smart Wine Fridge Web UI

A modern web interface for managing your smart wine fridge.

## Prerequisites

- Node.js (v18 or higher recommended)
- pnpm package manager (recommended)
- npm (alternative)

## Technology Stack

- Build tool: Vite
- UI Components: Swiper
- Package manager: pnpm (recommended) / npm
- Code Formatting: Prettier

## Installation

1. Clone the repository:
```bash
git clone https://github.com/andriyl/smart-wine-fridge_web.git
```

2. Install dependencies:
```bash
pnpm install
# or
npm install
```

## Development

### Running the Development Server

```bash
pnpm dev
# or
npm run dev
```

This will start the development server with hot module replacement (HMR) enabled. The app will be available at `http://localhost:5173`.

### Building for Production

```bash
pnpm build
# or
npm run build
```

This will create a production-ready build in the `dist` directory.

### Previewing Production Build

```bash
pnpm preview
# or
npm run preview
```

This will serve the production build locally for testing.

## Code Style and Formatting

The project uses Prettier for code formatting. You can:

- Check formatting:
```bash
pnpm format:check
# or
npm run format:check
```

- Automatically format code:
```bash
pnpm format
# or
npm run format
```

## Deployment

The application is built using Vite and can be deployed to any static hosting service. After building with `npm run build`, the contents of the `dist` directory can be deployed.

## Project Structure

- `/src` - Source code
- `/content` - Project content and configuration
  - `main.json` - Main configuration file
- `/dist` - Production build output

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
| unauthorized-unload | Handling unauthorized wine removal |
| swap-bottle | Wine swapping functionality |
| drawer | Wine drawer management |
| red-wines | Red wine inventory |
| white-wines | White wine inventory |
| rose-wines | Rosé wine inventory |
| routes | All routes |

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
## 3. Bottle Loading
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

## 2. Suggest

- We show a screen with a list of suggestions (e.g. food or atmosphere preferences):
<img width="404" alt="Screenshot 2025-06-18 at 11 32 17" src="https://github.com/user-attachments/assets/f9b4a6fc-4ef8-44ec-a0b3-b4bf0eb5c0f1" />

- After the user selects an offer, we filter the data from inventory.json and wineCatalog.json based on the selected type and available wines. We then display the list:
<img width="406" alt="Screenshot 2025-06-18 at 11 32 40" src="https://github.com/user-attachments/assets/2c5fabb2-3f66-47f2-b353-8df7a17689bb" />

- After the wine is selected and “Confirm & Take Wine Out” button is clicked, the process continues exactly the same as with "Pick Manually" (see above).

## 2. Home Dashboard & System Overview

Main Menu
- We retrieve data from inventory.json and render it to the interface.
<img width="397" alt="Screenshot 2025-06-23 at 10 40 03" src="https://github.com/user-attachments/assets/88aa0db7-f268-4814-8274-8d04b4d98903" />

- When entering zone settings, there are two possible modes: Automatic and Manual.
<img width="267" alt="Screenshot 2025-06-23 at 10 44 20" src="https://github.com/user-attachments/assets/fa817acc-48a3-4992-bd96-ccc0110edb1e" />  
<img width="270" alt="Screenshot 2025-06-23 at 10 41 41" src="https://github.com/user-attachments/assets/09564cec-d655-4856-ad69-aaad6c93fa99" />

- After filling out the form, we send an MQTT message to the winefridge/status topic with the updated zone settings:

### Web -> RPI
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
    "humidity": "68"
    "zone": "upper"
  }
}
```
### RPI -> Web
- We wait for an update to inventory.json from the backend.
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
- Once confirmed, the modal is closed and the main page is re-rendered accordingly.
