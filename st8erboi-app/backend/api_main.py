# backend/api_main.py
# Corrected version with CORS Middleware to handle browser security checks.

import asyncio
from fastapi import FastAPI, WebSocket, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# Import the application modules
from . import comms
from . import manual_controls

# Create the FastAPI application
app = FastAPI(title="st8erboi API")

# --- CORS Middleware Setup ---
# This is the crucial part. It tells the backend to accept requests
# from your frontend's origin (http://localhost:5173) and to correctly
# handle the preflight "OPTIONS" requests sent by the browser.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allows all origins, perfect for development.
    allow_credentials=True,
    allow_methods=["*"],  # Allows all methods (GET, POST, OPTIONS, etc.).
    allow_headers=["*"],  # Allows all headers.
)

# --- Router Inclusion ---
# This must come *after* the middleware is added.
app.include_router(manual_controls.router, prefix="/api/manual", tags=["Manual Controls"])


# --- Pydantic Model for Scripting Commands ---
class CommandRequest(BaseModel):
    command: str
    target_device: str # "injector" or "fillhead"


# --- HTTP API Endpoint for Scripting ---
@app.post("/api/send-command")
async def send_command(req: CommandRequest):
    """
    Sends a raw command string from the scripting interface to the specified target device via UDP.
    """
    device_name = req.target_device.lower()
    if device_name not in ["injector", "fillhead"]:
        raise HTTPException(status_code=400, detail="Invalid target_device. Must be 'injector' or 'fillhead'.")

    device = comms.app_state["devices"].get(device_name)
    if not device or not device.get("connected"):
        raise HTTPException(status_code=404, detail=f"{device_name.capitalize()} device not connected.")
    
    comms.send_udp_command(device["ip"], comms.CLEARCORE_PORT, req.command)
    return {"message": f"Command '{req.command}' sent to {device_name}."}


# --- WebSocket Endpoint for Telemetry ---
@app.websocket("/ws/telemetry")
async def telemetry_websocket_endpoint(websocket: WebSocket):
    """Streams the application state telemetry to the frontend."""
    await websocket.accept()
    print("Frontend telemetry client connected.")
    try:
        while True:
            if "telemetry" in comms.app_state:
                await websocket.send_json(comms.app_state["telemetry"])
            await asyncio.sleep(0.1)
    except Exception:
        print("Frontend telemetry client disconnected.")


# --- Application Startup Event ---
@app.on_event("startup")
async def startup_event():
    """Starts your UDP and connection monitoring background tasks."""
    print("Starting background tasks (UDP receiver, connection monitor)...")
    asyncio.create_task(comms.udp_receiver())
    asyncio.create_task(comms.connection_monitor())


# --- Root Endpoint ---
@app.get("/")
def read_root():
    """A simple endpoint to confirm that the backend server is running."""
    return {"message": "st8erboi backend is running"}
