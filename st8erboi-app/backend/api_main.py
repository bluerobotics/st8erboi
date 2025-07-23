# backend/api_main.py
import asyncio
from fastapi import FastAPI, WebSocket

# Import the application state and background tasks from our new comms module
from . import comms

# Create the FastAPI application
app = FastAPI()

# --- FastAPI Endpoints ---

@app.websocket("/ws/telemetry")
async def websocket_endpoint(websocket: WebSocket):
    """Streams the application state to the frontend."""
    await websocket.accept()
    print("Frontend telemetry client connected.")
    try:
        while True:
            # Send the entire telemetry part of the app_state
            await websocket.send_json(comms.app_state["telemetry"])
            await asyncio.sleep(0.1) # Stream data 10 times per second
    except Exception:
        print("Frontend telemetry client disconnected.")

@app.on_event("startup")
async def startup_event():
    """Start background tasks when the server starts."""
    print("Starting UDP receiver and connection monitor...")
    asyncio.create_task(comms.udp_receiver())
    asyncio.create_task(comms.connection_monitor())