# backend/manual_controls.py
# Refactored to use FastAPI Dependencies for cleaner, more maintainable code.

from fastapi import APIRouter, HTTPException, Depends
from pydantic import BaseModel
from typing import List
from . import comms

router = APIRouter()

# --- Pydantic Models ---
# (No changes here, these are still the correct data structures)
class FillheadJogBody(BaseModel):
    axis: str; direction: int; dist: float; vel: float; accel: float; torque: int
class FillheadHomeBody(BaseModel):
    axis: str; torque: int; max_dist: float
class InjectorJogBody(BaseModel):
    dist_m0: float; dist_m1: float; vel: float; accel: float; torque: float
class PinchJogBody(BaseModel):
    degrees: float; torque: float; vel: float; accel: float
class HomingMoveBody(BaseModel):
    type: str; stroke: float; rapid_vel: float; touch_vel: float; accel: float; retract_dist: float; torque: float
class CartridgeRetractBody(BaseModel):
    offset: float
class InjectPurgeBody(BaseModel):
    type: str; vol_ml: float; speed_ml_s: float; accel: float; steps_per_ml: float; torque: float
class HeaterSetpointBody(BaseModel):
    temp: float
class HeaterGainsBody(BaseModel):
    kp: float
    ki: float
    kd: float

# --- Dependencies for Device Connections ---
# These functions handle finding and validating a device connection.
# They can be "depended" on by any endpoint that needs to talk to a device.

def get_injector_ip() -> str:
    """Dependency that finds the injector IP, or raises an error if not connected."""
    device = comms.app_state["devices"].get("injector")
    if not device or not device.get("connected"):
        raise HTTPException(status_code=404, detail="Injector device not connected.")
    return device["ip"]

def get_fillhead_ip() -> str:
    """Dependency that finds the fillhead IP, or raises an error if not connected."""
    device = comms.app_state["devices"].get("fillhead")
    if not device or not device.get("connected"):
        raise HTTPException(status_code=404, detail="Fillhead device not connected.")
    return device["ip"]

def get_all_connected_ips() -> List[str]:
    """Dependency for the ABORT command, returns all connected device IPs."""
    ips = []
    for device_name in ["injector", "fillhead"]:
        device = comms.app_state["devices"].get(device_name)
        if device and device.get("connected"):
            ips.append(device["ip"])
    return ips

# --- Endpoints with Request Bodies ---
# Each endpoint now declares which dependency it needs (e.g., `ip: str = Depends(get_fillhead_ip)`)

@router.post("/fillhead/jog")
async def fillhead_jog(body: FillheadJogBody, ip: str = Depends(get_fillhead_ip)):
    cmd = f"MOVE_{body.axis} {body.dist * body.direction} {body.vel} {body.accel} {body.torque}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to fillhead."}

@router.post("/fillhead/home")
async def fillhead_home(body: FillheadHomeBody, ip: str = Depends(get_fillhead_ip)):
    cmd = f"HOME_{body.axis} {body.torque} {body.max_dist}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to fillhead."}

@router.post("/injector/jog")
async def injector_jog(body: InjectorJogBody, ip: str = Depends(get_injector_ip)):
    cmd = f"JOG_MOVE {body.dist_m0} {body.dist_m1} {body.vel} {body.accel} {body.torque}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/pinch/jog")
async def pinch_jog(body: PinchJogBody, ip: str = Depends(get_injector_ip)):
    cmd = f"PINCH_JOG_MOVE {body.degrees} {body.torque} {body.vel} {body.accel}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/homing/move")
async def homing_move(body: HomingMoveBody, ip: str = Depends(get_injector_ip)):
    retract = body.retract_dist if body.type == "MACHINE_HOME_MOVE" else 0
    cmd = f"{body.type} {body.stroke} {body.rapid_vel} {body.touch_vel} {body.accel} {retract} {body.torque}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/feed/go-to-cartridge-retract")
async def go_to_cartridge_retract(body: CartridgeRetractBody, ip: str = Depends(get_injector_ip)):
    cmd = f"MOVE_TO_CARTRIDGE_RETRACT {body.offset}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/feed/inject-purge")
async def inject_or_purge(body: InjectPurgeBody, ip: str = Depends(get_injector_ip)):
    cmd = f"{body.type} {body.vol_ml} {body.speed_ml_s} {body.accel} {body.steps_per_ml} {body.torque}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/injector/set-heater-setpoint")
async def set_heater_setpoint(body: HeaterSetpointBody, ip: str = Depends(get_injector_ip)):
    cmd = f"SET_HEATER_SETPOINT {body.temp}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

@router.post("/injector/set-heater-gains")
async def set_heater_gains(body: HeaterGainsBody, ip: str = Depends(get_injector_ip)):
    cmd = f"SET_HEATER_GAINS {body.kp} {body.ki} {body.kd}"
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, cmd)
    return {"message": f"Command '{cmd}' sent to injector."}

# --- Simple Command Endpoints ---
@router.post("/homing/pinch-home")
async def simple_pinch_home(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "PINCH_HOME_MOVE")
    return {"message": "Command 'PINCH_HOME_MOVE' sent to injector."}

@router.post("/feed/go-to-cartridge-home")
async def simple_go_to_cartridge_home(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "MOVE_TO_CARTRIDGE_HOME")
    return {"message": "Command 'MOVE_TO_CARTRIDGE_HOME' sent to injector."}

@router.post("/feed/pause")
async def simple_pause(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "PAUSE_INJECTION")
    return {"message": "Command 'PAUSE_INJECTION' sent to injector."}

@router.post("/feed/resume")
async def simple_resume(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "RESUME_INJECTION")
    return {"message": "Command 'RESUME_INJECTION' sent to injector."}

@router.post("/feed/cancel")
async def simple_cancel(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "CANCEL_INJECTION")
    return {"message": "Command 'CANCEL_INJECTION' sent to injector."}

@router.post("/injector/enable")
async def simple_enable(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "ENABLE")
    return {"message": "Command 'ENABLE' sent to injector."}

@router.post("/injector/disable")
async def simple_disable(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "DISABLE")
    return {"message": "Command 'DISABLE' sent to injector."}

@router.post("/injector/enable-pinch")
async def simple_enable_pinch(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "ENABLE_PINCH")
    return {"message": "Command 'ENABLE_PINCH' sent to injector."}

@router.post("/injector/disable-pinch")
async def simple_disable_pinch(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "DISABLE_PINCH")
    return {"message": "Command 'DISABLE_PINCH' sent to injector."}

@router.post("/injector/heater-on")
async def simple_heater_on(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "HEATER_PID_ON")
    return {"message": "Command 'HEATER_PID_ON' sent to injector."}

@router.post("/injector/heater-off")
async def simple_heater_off(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "HEATER_PID_OFF")
    return {"message": "Command 'HEATER_PID_OFF' sent to injector."}

@router.post("/injector/vacuum-on")
async def simple_vacuum_on(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "VACUUM_ON")
    return {"message": "Command 'VACUUM_ON' sent to injector."}

@router.post("/injector/vacuum-off")
async def simple_vacuum_off(ip: str = Depends(get_injector_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "VACUUM_OFF")
    return {"message": "Command 'VACUUM_OFF' sent to injector."}

@router.post("/fillhead/start-demo")
async def simple_start_demo(ip: str = Depends(get_fillhead_ip)):
    comms.send_udp_command(ip, comms.CLEARCORE_PORT, "START_DEMO")
    return {"message": "Command 'START_DEMO' sent to fillhead."}

@router.post("/system/abort")
async def simple_abort(ips: List[str] = Depends(get_all_connected_ips)):
    for ip in ips:
        comms.send_udp_command(ip, comms.CLEARCORE_PORT, "ABORT")
    return {"message": "ABORT command sent to all connected devices."}
