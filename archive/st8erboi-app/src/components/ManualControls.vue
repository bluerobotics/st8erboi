<script setup lang="ts">
import { reactive, computed } from 'vue';

// --- Component State ---
const fh = reactive({
  jog_dist_mm: 10.0, jog_vel_mms: 15.0, jog_accel_mms2: 50.0, jog_torque: 20,
  home_torque: 20, home_distance_mm: 420.0,
});
const ij = reactive({
  jog_dist_mm: 1.0, jog_velocity: 5.0, jog_acceleration: 25.0, jog_torque_percent: 20.0,
  jog_pinch_degrees: 1.0, jog_pinch_velocity_mms: 5.0, jog_pinch_accel_mms2: 25.0, pinch_jog_torque_percent: 20,
  homing_stroke_len: 50.0, homing_rapid_vel: 20.0, homing_touch_vel: 2.0, homing_acceleration: 100.0, homing_retract_dist: 1.0, homing_torque_percent: 25,
  feed_cyl1_dia: 10.0, feed_cyl2_dia: 10.0, feed_ballscrew_pitch: 2.0, feed_acceleration: 5000, feed_torque_percent: 50,
  cartridge_retract_offset_mm: 1.0,
  inject_amount_ml: 0.1, inject_speed_ml_s: 0.1,
  purge_amount_ml: 0.5, purge_speed_ml_s: 0.5,
  heater_setpoint: 50.0, heater_kp: 10, heater_ki: 0.5, heater_kd: 0.1,
});

// --- Calculated Values ---
const MOTOR_STEPS_PER_REV = 800;
const calculated_ml_per_rev = computed(() => {
    const r1 = ij.feed_cyl1_dia / 2;
    const r2 = ij.feed_cyl2_dia / 2;
    const area_mm2 = (Math.PI * r1 * r1) + (Math.PI * r2 * r2);
    const vol_mm3_per_rev = area_mm2 * ij.feed_ballscrew_pitch;
    return vol_mm3_per_rev / 1000;
});
const calculated_steps_per_ml = computed(() => {
    if (calculated_ml_per_rev.value === 0) return 0;
    return MOTOR_STEPS_PER_REV / calculated_ml_per_rev.value;
});

// --- API Communication ---
const API_BASE_URL = 'http://127.0.0.1:8000/api/manual';
async function sendCommand(endpoint: string, body: object | null = null) {
  try {
    const options: RequestInit = {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      ...(body && { body: JSON.stringify(body) })
    };
    const response = await fetch(`${API_BASE_URL}${endpoint}`, options);
    if (!response.ok) {
      const errorData = await response.json();
      alert(`Error: ${errorData.detail || 'Unknown error'}`);
    }
  } catch (error) {
    console.error('API call failed:', error);
    alert('Failed to connect to the backend. Is the Python server running?');
  }
}

// --- Command Functions ---
const sendFillheadJog = (axis: string, direction: number) => sendCommand('/fillhead/jog', { axis, direction, dist: fh.jog_dist_mm, vel: fh.jog_vel_mms, accel: fh.jog_accel_mms2, torque: fh.jog_torque });
const sendFillheadHome = (axis: string) => sendCommand('/fillhead/home', { axis, torque: fh.home_torque, max_dist: fh.home_distance_mm });
const sendInjectorJog = (dist_m0: number, dist_m1: number) => sendCommand('/injector/jog', { dist_m0, dist_m1, vel: ij.jog_velocity, accel: ij.jog_acceleration, torque: ij.jog_torque_percent });
const sendPinchJog = (degrees: number) => sendCommand('/pinch/jog', { degrees, vel: ij.jog_pinch_velocity_mms, accel: ij.jog_pinch_accel_mms2, torque: ij.pinch_jog_torque_percent });
const sendHomingMove = (type: string) => sendCommand('/homing/move', { type, stroke: ij.homing_stroke_len, rapid_vel: ij.homing_rapid_vel, touch_vel: ij.homing_touch_vel, accel: ij.homing_acceleration, retract_dist: ij.homing_retract_dist, torque: ij.homing_torque_percent });
const sendGoToRetract = () => sendCommand('/feed/go-to-cartridge-retract', { offset: ij.cartridge_retract_offset_mm });
const sendInjectPurge = (type: 'INJECT_MOVE' | 'PURGE_MOVE') => {
    const isInject = type === 'INJECT_MOVE';
    sendCommand('/feed/inject-purge', {
        type,
        vol_ml: isInject ? ij.inject_amount_ml : ij.purge_amount_ml,
        speed_ml_s: isInject ? ij.inject_speed_ml_s : ij.purge_speed_ml_s,
        accel: ij.feed_acceleration,
        steps_per_ml: calculated_steps_per_ml.value,
        torque: ij.feed_torque_percent
    });
};
const sendHeaterSetpoint = () => sendCommand('/injector/set-heater-setpoint', { temp: ij.heater_setpoint });
const sendHeaterGains = () => sendCommand('/injector/set-heater-gains', { kp: ij.heater_kp, ki: ij.heater_ki, kd: ij.heater_kd });
const sendSimpleCommand = (path: string) => sendCommand(`/${path}`, null);
</script>

<template>
  <div class="p-4 sm:p-6 space-y-6">

    <!-- System-wide ABORT button -->
    <div class="flex justify-center">
        <button @click="sendSimpleCommand('system/abort')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-red-700 hover:bg-red-600 text-base py-3 px-6">🛑 ABORT ALL</button>
    </div>

    <!-- Main two-column layout -->
    <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      
      <!-- Left Column -->
      <div class="space-y-6">
        
        <!-- Jog Controls Group -->
        <div class="bg-slate-800/60 border border-slate-700 rounded-xl shadow-lg flex flex-col">
          <h3 class="text-lg font-bold p-3 border-b border-slate-700 text-gray-300">Jog Controls</h3>
          <div class="p-4 space-y-4">
            <!-- Injector Jog -->
            <div class="p-3 border border-slate-700 rounded-lg">
                <h4 class="font-bold text-green-400 mb-2">Injector</h4>
                <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Dist (mm)</label>
                      <input v-model.number="ij.jog_dist_mm" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Vel (mm/s)</label>
                      <input v-model.number="ij.jog_velocity" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Accel (mm/s²)</label>
                      <input v-model.number="ij.jog_acceleration" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Torque (%)</label>
                      <input v-model.number="ij.jog_torque_percent" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                </div>
                <div class="grid grid-cols-3 gap-2 mt-3">
                    <button @click="sendInjectorJog(ij.jog_dist_mm, 0)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▲ Up (M0)</button>
                    <button @click="sendInjectorJog(0, ij.jog_dist_mm)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▲ Up (M1)</button>
                    <button @click="sendInjectorJog(ij.jog_dist_mm, ij.jog_dist_mm)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▲ Up (Both)</button>
                    <button @click="sendInjectorJog(-ij.jog_dist_mm, 0)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▼ Down (M0)</button>
                    <button @click="sendInjectorJog(0, -ij.jog_dist_mm)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▼ Down (M1)</button>
                    <button @click="sendInjectorJog(-ij.jog_dist_mm, -ij.jog_dist_mm)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">▼ Down (Both)</button>
                </div>
            </div>
            <!-- Pinch Jog -->
            <div class="p-3 border border-slate-700 rounded-lg">
                <h4 class="font-bold text-fuchsia-400 mb-2">Pinch</h4>
                <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Dist (mm)</label>
                      <input v-model.number="ij.jog_pinch_degrees" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Vel (mm/s)</label>
                      <input v-model.number="ij.jog_pinch_velocity_mms" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Accel (mm/s²)</label>
                      <input v-model.number="ij.jog_pinch_accel_mms2" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Torque (%)</label>
                      <input v-model.number="ij.pinch_jog_torque_percent" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                </div>
                <div class="grid grid-cols-2 gap-2 mt-3">
                    <button @click="sendPinchJog(ij.jog_pinch_degrees)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Pinch Close</button>
                    <button @click="sendPinchJog(-ij.jog_pinch_degrees)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Pinch Open</button>
                </div>
            </div>
            <!-- Fillhead Jog -->
            <div class="p-3 border border-slate-700 rounded-lg">
                <h4 class="font-bold text-sky-400 mb-2">Fillhead</h4>
                 <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Dist (mm)</label>
                      <input v-model.number="fh.jog_dist_mm" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Speed (mm/s)</label>
                      <input v-model.number="fh.jog_vel_mms" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Accel (mm/s²)</label>
                      <input v-model.number="fh.jog_accel_mms2" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Torque (%)</label>
                      <input v-model.number="fh.jog_torque" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                </div>
                <div class="grid grid-cols-3 gap-2 mt-3">
                    <button @click="sendFillheadJog('X', 1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">X+</button>
                    <button @click="sendFillheadJog('Y', 1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Y+</button>
                    <button @click="sendFillheadJog('Z', 1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Z+</button>
                    <button @click="sendFillheadJog('X', -1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">X-</button>
                    <button @click="sendFillheadJog('Y', -1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Y-</button>
                    <button @click="sendFillheadJog('Z', -1)" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-sky-600 hover:bg-sky-500">Z-</button>
                </div>
            </div>
          </div>
        </div>

        <!-- Motor Power & State Group -->
        <div class="bg-slate-800/60 border border-slate-700 rounded-xl shadow-lg flex flex-col">
            <h3 class="text-lg font-bold p-3 border-b border-slate-700 text-amber-400">Motor Power & State</h3>
            <div class="p-4 space-y-4">
                <div class="grid grid-cols-2 gap-4">
                    <div class="space-y-2">
                        <h4 class="font-bold text-gray-300">Injector</h4>
                        <button @click="sendSimpleCommand('injector/enable')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-green-600 hover:bg-green-500 w-full">Enable</button>
                        <button @click="sendSimpleCommand('injector/disable')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-red-600 hover:bg-red-500 w-full">Disable</button>
                    </div>
                     <div class="space-y-2">
                        <h4 class="font-bold text-gray-300">Pinch</h4>
                        <button @click="sendSimpleCommand('injector/enable-pinch')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-green-600 hover:bg-green-500 w-full">Enable</button>
                        <button @click="sendSimpleCommand('injector/disable-pinch')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-red-600 hover:bg-red-500 w-full">Disable</button>
                    </div>
                    <div class="space-y-2">
                        <h4 class="font-bold text-gray-300">Heater</h4>
                        <button @click="sendSimpleCommand('injector/heater-on')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-green-600 hover:bg-green-500 w-full">ON</button>
                        <button @click="sendSimpleCommand('injector/heater-off')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-red-600 hover:bg-red-500 w-full">OFF</button>
                    </div>
                     <div class="space-y-2">
                        <h4 class="font-bold text-gray-300">Vacuum</h4>
                        <button @click="sendSimpleCommand('injector/vacuum-on')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-green-600 hover:bg-green-500 w-full">ON</button>
                        <button @click="sendSimpleCommand('injector/vacuum-off')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-red-600 hover:bg-red-500 w-full">OFF</button>
                    </div>
                </div>
                 <div class="p-3 border border-slate-700 rounded-lg">
                    <h4 class="font-bold text-gray-300 mb-2">Heater Gains</h4>
                    <div class="grid grid-cols-3 gap-x-2">
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Kp</label>
                          <input v-model.number="ij.heater_kp" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Ki</label>
                          <input v-model.number="ij.heater_ki" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Kd</label>
                          <input v-model.number="ij.heater_kd" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-2 mt-3">
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Setpoint (°C)</label>
                          <input v-model.number="ij.heater_setpoint" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <button @click="sendHeaterSetpoint" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-slate-600 hover:bg-slate-500 self-end">Set Temp</button>
                    </div>
                     <button @click="sendHeaterGains" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-slate-600 hover:bg-slate-500 w-full mt-3">Set Gains</button>
                </div>
            </div>
        </div>

      </div>

      <!-- Right Column -->
      <div class="space-y-6">
        
        <!-- Homing Controls Group -->
        <div class="bg-slate-800/60 border border-slate-700 rounded-xl shadow-lg flex flex-col">
            <h3 class="text-lg font-bold p-3 border-b border-slate-700 text-violet-400">Homing Controls</h3>
            <div class="p-4 space-y-4">
                <!-- Injector Homing -->
                <div class="p-3 border border-slate-700 rounded-lg">
                    <h4 class="font-bold text-green-400 mb-2">Injector Homing</h4>
                    <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Stroke (mm)</label>
                          <input v-model.number="ij.homing_stroke_len" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Rapid Vel (mm/s)</label>
                          <input v-model.number="ij.homing_rapid_vel" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Touch Vel (mm/s)</label>
                          <input v-model.number="ij.homing_touch_vel" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Accel (mm/s²)</label>
                          <input v-model.number="ij.homing_acceleration" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">M.Retract (mm)</label>
                          <input v-model.number="ij.homing_retract_dist" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Torque (%)</label>
                          <input v-model.number="ij.homing_torque_percent" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                    </div>
                    <div class="grid grid-cols-3 gap-2 mt-3">
                        <button @click="sendHomingMove('MACHINE_HOME_MOVE')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Machine Home</button>
                        <button @click="sendHomingMove('CARTRIDGE_HOME_MOVE')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Cartridge Home</button>
                        <button @click="sendSimpleCommand('homing/pinch-home')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Pinch Home</button>
                    </div>
                </div>
                <!-- Fillhead Homing -->
                <div class="p-3 border border-slate-700 rounded-lg">
                    <h4 class="font-bold text-sky-400 mb-2">Fillhead Homing</h4>
                     <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Torque (%)</label>
                          <input v-model.number="fh.home_torque" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                        <div>
                          <label class="block text-xs font-medium text-gray-400 mb-1">Max Dist (mm)</label>
                          <input v-model.number="fh.home_distance_mm" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                        </div>
                    </div>
                    <div class="grid grid-cols-3 gap-2 mt-3">
                        <button @click="sendFillheadHome('X')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Home X</button>
                        <button @click="sendFillheadHome('Y')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Home Y</button>
                        <button @click="sendFillheadHome('Z')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-violet-700 hover:bg-violet-600">Home Z</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- Feed Controls Group -->
        <div class="bg-slate-800/60 border border-slate-700 rounded-xl shadow-lg flex flex-col">
            <h3 class="text-lg font-bold p-3 border-b border-slate-700 text-teal-400">Feed Controls (Injector)</h3>
            <div class="p-4 space-y-4">
                <!-- Params -->
                <div class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Cyl 1 Dia (mm)</label>
                      <input v-model.number="ij.feed_cyl1_dia" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Cyl 2 Dia (mm)</label>
                      <input v-model.number="ij.feed_cyl2_dia" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Pitch (mm/rev)</label>
                      <input v-model.number="ij.feed_ballscrew_pitch" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Feed Accel (sps²)</label>
                      <input v-model.number="ij.feed_acceleration" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Torque Limit (%)</label>
                      <input v-model.number="ij.feed_torque_percent" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                    <div>
                      <label class="block text-xs font-medium text-gray-400 mb-1">Retract Offset (mm)</label>
                      <input v-model.number="ij.cartridge_retract_offset_mm" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                    </div>
                </div>
                 <div class="text-xs text-gray-300 p-2 bg-slate-900/80 rounded grid grid-cols-2 gap-x-4">
                    <div>Calc ml/rev: <span class="font-mono text-cyan-400">{{ calculated_ml_per_rev.toFixed(4) }}</span></div>
                    <div>Calc Steps/ml: <span class="font-mono text-cyan-400">{{ calculated_steps_per_ml.toFixed(2) }}</span></div>
                </div>
                <div class="grid grid-cols-2 gap-2">
                     <button @click="sendSimpleCommand('feed/go-to-cartridge-home')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-slate-600 hover:bg-slate-500">Go to Cartridge Home</button>
                     <button @click="sendGoToRetract" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-slate-600 hover:bg-slate-500">Go to Cartridge Retract</button>
                </div>
                <hr class="border-slate-700">
                <!-- Inject/Purge -->
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                    <!-- Inject -->
                    <div>
                        <h4 class="font-medium text-gray-300 mb-2">Inject</h4>
                        <div class="space-y-2 text-sm">
                            <div>
                              <label class="block text-xs font-medium text-gray-400 mb-1">Inject Vol (ml)</label>
                              <input v-model.number="ij.inject_amount_ml" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                            </div>
                            <div>
                              <label class="block text-xs font-medium text-gray-400 mb-1">Inject Vel (ml/s)</label>
                              <input v-model.number="ij.inject_speed_ml_s" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                            </div>
                        </div>
                        <button @click="sendInjectPurge('INJECT_MOVE')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-emerald-600 hover:bg-emerald-500 w-full mt-3">Start Inject</button>
                    </div>
                    <!-- Purge -->
                    <div>
                        <h4 class="font-medium text-gray-300 mb-2">Purge</h4>
                         <div class="space-y-2 text-sm">
                            <div>
                              <label class="block text-xs font-medium text-gray-400 mb-1">Purge Vol (ml)</label>
                              <input v-model.number="ij.purge_amount_ml" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                            </div>
                            <div>
                              <label class="block text-xs font-medium text-gray-400 mb-1">Purge Vel (ml/s)</label>
                              <input v-model.number="ij.purge_speed_ml_s" type="number" class="p-2 bg-slate-900 border border-slate-600 rounded-md w-full text-sm focus:ring-2 focus:ring-sky-500 focus:border-sky-500" />
                            </div>
                        </div>
                        <button @click="sendInjectPurge('PURGE_MOVE')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-emerald-600 hover:bg-emerald-500 w-full mt-3">Start Purge</button>
                    </div>
                </div>
                <div class="grid grid-cols-3 gap-2">
                    <button @click="sendSimpleCommand('feed/pause')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-amber-600 hover:bg-amber-500">Pause</button>
                    <button @click="sendSimpleCommand('feed/resume')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-cyan-600 hover:bg-cyan-500">Resume</button>
                    <button @click="sendSimpleCommand('feed/cancel')" class="p-2 rounded-md text-sm font-bold text-white transition-colors shadow-md border border-black/20 bg-rose-600 hover:bg-rose-500">Cancel</button>
                </div>
            </div>
        </div>
      </div>
    </div>
  </div>
</template>
