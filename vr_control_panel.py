#!/usr/bin/env python3
"""
MuteMotionVR Control Panel — Standalone Dev GUI
Manages SteamVR lifecycle + MuteMotionVR C++ daemon via QProcess.
Settings persist across sessions via JSON file.
"""
import sys
import os
import subprocess
import json
import requests
import webbrowser
import uuid
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QComboBox, QGroupBox, QTextEdit, QSlider, QCheckBox
)
from PySide6.QtCore import Qt, QProcess, QTimer, QProcessEnvironment, QEvent
from PySide6.QtGui import QFont, QColor, QTextCursor

SERVER_URL = "https://mutemotion.n0t.space"
STATE_FILE = os.path.expanduser("~/.mutemotion/vr_state.txt")
SETTINGS_FILE = os.path.expanduser("~/.mutemotion/vr_settings.json")
STEAMVR_URI = "steam://run/250820//-nohome"  # sneaky steamvr launch argument to bypass home

# defaults in case the user's setup is chalked
DEFAULT_SETTINGS = {
    "preset": 3,
    "intensity": 1.0,
    "opacity": 0.8,
    "invert_axis": True,
    "show_latency": False,
    "telemetry": False,
    "nyan_cat": False,
}

class VRControlPanel(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MuteMotionVR Control Panel")
        self.setFixedSize(520, 880)

        os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)

        self.is_active = False
        self.cpp_process = None
        self.steamvr_ready = False
        self.daemon_running = False

        # load settings from the vault
        self.settings = self._load_settings()
        self.current_preset = self.settings["preset"]

        self._init_user_id()
        self.unlocked_skins = []
        self._purchase_poll_timer = QTimer(self)
        self._purchase_poll_timer.timeout.connect(self._poll_purchase)
        self._purchase_poll_elapsed = 0

        self.init_ui()
        self._check_unlocks()
        self.write_state()

    # json settings stuff so we don't forget what they chose
    def _load_settings(self):
        """Load settings from JSON file, falling back to defaults."""
        try:
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r") as f:
                    saved = json.load(f)
                # merge with defaults if they're missing fields
                merged = {**DEFAULT_SETTINGS, **saved}
                return merged
        except Exception as e:
            print(f"[SETTINGS] Failed to load: {e}")
        return dict(DEFAULT_SETTINGS)

    def _save_settings(self):
        """Persist current settings to JSON file."""
        try:
            with open(SETTINGS_FILE, "w") as f:
                json.dump(self.settings, f, indent=2)
        except Exception as e:
            print(f"[SETTINGS] Failed to save: {e}")

    # steamvr lifecycle madness
    def launch_steamvr(self):
        # Step 1: Check if compositor is ALREADY running
        try:
            if os.name == 'nt':
                result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq vrcompositor.exe"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                is_running = "vrcompositor.exe" in result.stdout
            else:
                result = subprocess.run(["pgrep", "-f", "vrcompositor"], capture_output=True)
                is_running = (result.returncode == 0)

            if is_running:
                self.append_log("> SteamVR Compositor already running! Skipping launch.", "#2ecc71")
                self.steamvr_ready = True
                self.btn_launch.setEnabled(False)
                self.btn_launch.setText("SteamVR Running ✓")
                self.launch_backend()
                return
        except Exception as e:
            self.append_log(f"> WARNING: Could not check compositor: {e}", "#f39c12")

        # Step 2: Check if Steam client is actually alive
        try:
            if os.name == 'nt':
                steam_check = subprocess.run(["tasklist", "/FI", "IMAGENAME eq steam.exe"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                steam_running = "steam.exe" in steam_check.stdout.lower()
            else:
                steam_check = subprocess.run(["pgrep", "-x", "steam"], capture_output=True)
                steam_running = (steam_check.returncode == 0)

            if not steam_running:
                self.append_log("> ERROR: Steam client is not running. Open Steam first!", "#e74c3c")
                return
        except Exception as e:
            self.append_log(f"> ERROR: Failed to check Steam process: {e}", "#e74c3c")
            return

        # Step 3: Try to launch SteamVR via Steam URI and pray it works
        self.append_log("> Attempting to launch SteamVR via Steam Client...", "#3498db")
        self.append_log("> TIP: If SteamVR doesn't open, launch it manually from Steam.", "#f39c12")
        self.btn_launch.setEnabled(False)
        self.btn_launch.setText("Waiting for SteamVR Compositor...")
        
        try:
            if os.name == 'nt':
                os.startfile(STEAMVR_URI)
            else:
                subprocess.Popen(["xdg-open", STEAMVR_URI],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception as e:
            self.append_log(f"> WARNING: URI launch failed: {e}. Launch SteamVR manually.", "#f39c12")

        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._check_compositor)
        self._poll_count = 0
        self._poll_timer.start(2000)

    def _check_compositor(self):
        self._poll_count += 1
        try:
            if os.name == 'nt':
                result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq vrcompositor.exe"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                is_running = "vrcompositor.exe" in result.stdout
            else:
                result = subprocess.run(["pgrep", "-f", "vrcompositor"], capture_output=True)
                is_running = (result.returncode == 0)

            if is_running:
                self._poll_timer.stop()
                self.append_log("> SteamVR Compositor detected! Launching daemon...", "#2ecc71")
                self.steamvr_ready = True
                self.launch_backend()
                return
        except Exception as e:
            self.append_log(f"> ERROR polling compositor: {e}", "#e74c3c")

        if self._poll_count >= 30:
            self._poll_timer.stop()
            self.append_log("> TIMEOUT: SteamVR compositor not detected after 60 seconds.", "#e74c3c")
            self.append_log("> TIP: Launch SteamVR manually, then click the button again.", "#f39c12")
            self.btn_launch.setEnabled(True)
            self.btn_launch.setText("Launch SteamVR + Engine")
        else:
            self.append_log(f"> Polling for compositor... ({self._poll_count}/30)", "#7f8c8d")

    def _check_compositor_timeout(self):
        self._poll_timer.stop()
        if not self.steamvr_ready:
            self.append_log("> VR Compositor not detected after 60 seconds.", "#e74c3c")
            self.btn_launch.setEnabled(True)
            self.btn_launch.setText("Launch SteamVR + Engine")

    # launching the big boy c++ daemon
    def launch_backend(self, desktop_mode=False):
        exe_name = "MuteMotionVR.exe" if os.name == 'nt' else "MuteMotionVR"
        
        if getattr(sys, 'frozen', False):
            # Running as compiled executable
            exe_dir = os.path.dirname(sys.executable)
            if os.path.basename(exe_dir).lower() == "dist":
                base_dir = os.path.dirname(exe_dir)
            else:
                base_dir = exe_dir
        else:
            # Running as python script
            base_dir = os.path.dirname(os.path.abspath(__file__))
            
        backend_path = os.path.join(base_dir, "build", exe_name)
        if not os.path.exists(backend_path):
            self.append_log(f"> FATAL: C++ binary not found at {backend_path}", "#e74c3c")
            return

        mode_label = "DESKTOP PREVIEW" if desktop_mode else "VR"
        self.append_log(f"> Launching C++ Daemon ({mode_label}): {backend_path}", "#3498db")
        self.daemon_running = True
        self.is_active = True
        self.btn_toggle.setText("ACTIVE")
        self.btn_toggle.setStyleSheet("background-color: #27ae60; color: white; border-radius: 6px;")
        self.write_state()
        
        # inject steamvr lib paths so the daemon can hitch a ride on the existing compositor
        # without launching a second one and causing a scene
        steamvr_bin = os.path.expanduser("~/.steam/root/steamapps/common/SteamVR/bin/linux64")
        env = QProcessEnvironment.systemEnvironment()
        current_ldpath = env.value("LD_LIBRARY_PATH", "")
        env.insert("LD_LIBRARY_PATH", f"{steamvr_bin}:{current_ldpath}")
        
        try:
            self.cpp_process = QProcess(self)
            self.cpp_process.setProcessEnvironment(env)
            self.cpp_process.setProcessChannelMode(QProcess.MergedChannels)
            self.cpp_process.readyReadStandardOutput.connect(self.read_engine_log)
            self.cpp_process.finished.connect(self._on_daemon_exit)
            self.cpp_process.errorOccurred.connect(self._on_daemon_error)
            self.cpp_process.setProgram(backend_path)
            if desktop_mode:
                self.cpp_process.setArguments(["-desktop"])
            self.cpp_process.start()
        except Exception as e:
            self.append_log(f"> FATAL: Exception spawning daemon: {e}", "#e74c3c")
            self._on_daemon_exit(-1, 0)

    def _on_daemon_error(self, error):
        self.append_log(f"> QProcess Error: Daemon failed to execute or crashed immediately.", "#e74c3c")
        self._on_daemon_exit(-1, 0)

    # desktop preview for the devs that don't want to put the headset on
    def launch_desktop(self):
        """Launch the daemon in -desktop mode, bypassing SteamVR entirely."""
        if self.daemon_running:
            self.append_log("> Engine already running. Restart first.", "#f39c12")
            return
        self.append_log("> ─ DESKTOP PREVIEW MODE ─", "#9b59b6")
        self.append_log("> No VR headset required. Simulating sensor data.", "#9b59b6")
        self.steamvr_ready = False
        self.btn_launch.setEnabled(False)
        self.btn_launch.setText("Desktop Preview Active")
        self.launch_backend(desktop_mode=True)

    def _on_daemon_exit(self, exit_code, status):
        self.daemon_running = False
        self.is_active = False
        self.btn_toggle.setText("STANDBY")
        self.btn_toggle.setStyleSheet("background-color: #7f8c8d; color: white; border-radius: 6px;")
        self.append_log(f"> Daemon exited (code {exit_code})", "#e74c3c")
        self.btn_launch.setEnabled(True)
        self.btn_launch.setText("⚡ Relaunch SteamVR + Engine")
        self.write_state()

    def read_engine_log(self):
        data = self.cpp_process.readAllStandardOutput()
        stdout = bytes(data).decode("utf-8")
        for line in stdout.split("\n"):
            if line.strip():
                color = "white"
                
                # Check for Telemetry
                if line.startswith("[IMU_DATA]"):
                    try:
                        # Format: [IMU_DATA] pitch,yaw,roll|outX,outY
                        data_str = line.replace("[IMU_DATA]", "").strip()
                        raw_str, out_str = data_str.split("|")
                        pitch, yaw, roll = map(float, raw_str.split(","))
                        out_x, out_y = map(float, out_str.split(","))
                        
                        self.lbl_raw_pitch.setText(f"Raw Pitch (X): {pitch:6.3f}")
                        self.lbl_raw_yaw.setText(f"Raw Yaw (Y): {yaw:6.3f}")
                        self.lbl_raw_roll.setText(f"Raw Roll (Z): {roll:6.3f}")
                        self.lbl_out_roll.setText(f"Out Roll (X): {out_x:6.3f}")
                        self.lbl_out_pitch.setText(f"Out Pitch (Y): {out_y:6.3f}")
                    except Exception:
                        pass
                    continue
                    
                # Check for HMD detection
                if line.startswith("[MuteMotionVR] DETECTED_HMD:"):
                    hmd_model = line.split("DETECTED_HMD:")[1].strip()
                    self.lbl_hmd.setText(f"Detected HMD: {hmd_model}")
                    self.lbl_hmd.setStyleSheet("color: #2ecc71; font-size: 12px; font-weight: bold;")
                    self.append_log(f"> HMD Bound: {hmd_model}", "#2ecc71")
                    continue
                    
                if "FATAL" in line or "Error" in line or "error" in line:
                    color = "#e74c3c"
                elif "successfully" in line.lower() or "running" in line.lower():
                    color = "#2ecc71"
                elif "vrcompositor" in line.lower():
                    color = "#f39c12"
                elif "Calibrated" in line:
                    color = "#3498db"
                self.append_log(line, color)

    # console log appending logic
    def append_log(self, text, color="white"):
        self.console_log.append(f'<span style="color:{color};">{text}</span>')
        cursor = self.console_log.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        self.console_log.setTextCursor(cursor)

    # unga bunga restart the engine
    def restart_engine(self):
        self.append_log("> ─ RESTARTING ENGINE ─", "#f39c12")
        
        # terminate with extreme prejudice if it's already running
        if self.cpp_process and self.cpp_process.state() != QProcess.NotRunning:
            self.append_log("> Terminating existing daemon...", "#f39c12")
            self.cpp_process.terminate()
            self.cpp_process.waitForFinished(3000)
            if self.cpp_process.state() != QProcess.NotRunning:
                self.append_log("> Force killing daemon...", "#e74c3c")
                self.cpp_process.kill()
        
        # Reset state
        self.daemon_running = False
        self.steamvr_ready = False
        self.is_active = False
        self.btn_toggle.setText("STANDBY")
        self.btn_toggle.setStyleSheet("background-color: #7f8c8d; color: white; border-radius: 6px;")
        self.btn_launch.setEnabled(True)
        self.btn_launch.setText("Launch SteamVR + Engine")
        self.append_log("> Ready to relaunch. Click the Launch button.", "#2ecc71")

    # calibration so we don't end up looking at the floor
    def calibrate(self):
        """Send calibration signal via state file."""
        self.append_log("> Sending calibration signal (zero reset)...", "#3498db")
        self.write_state(calibrate=True)
        QTimer.singleShot(500, lambda: self.write_state(calibrate=False))

    def closeEvent(self, event):
        if self.cpp_process and self.cpp_process.state() != QProcess.NotRunning:
            self.cpp_process.terminate()
            self.cpp_process.waitForFinished(2000)
        super().closeEvent(event)

    # - UI Construction ─
    def init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # Title
        title = QLabel("MuteMotionVR Interface")
        title.setFont(QFont("Arial", 16, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)
        
        # HMD Info Label
        self.lbl_hmd = QLabel("Detected HMD: Waiting for SteamVR...")
        self.lbl_hmd.setAlignment(Qt.AlignCenter)
        self.lbl_hmd.setStyleSheet("color: #7f8c8d; font-size: 12px;")
        layout.addWidget(self.lbl_hmd)

        # Launch Button
        self.btn_launch = QPushButton("Launch SteamVR + Engine")
        self.btn_launch.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        self.btn_launch.setMinimumHeight(45)
        self.btn_launch.setStyleSheet(
            "background-color: #2980b9; color: white; border-radius: 6px;"
        )
        self.btn_launch.clicked.connect(self.launch_steamvr)
        layout.addWidget(self.btn_launch)

        # Desktop Preview Button (No VR Required)
        self.btn_desktop = QPushButton("Desktop Preview (No VR)")
        self.btn_desktop.setFont(QFont("Arial", 11, QFont.Weight.Bold))
        self.btn_desktop.setMinimumHeight(40)
        self.btn_desktop.setStyleSheet(
            "background-color: #8e44ad; color: white; border-radius: 6px;"
        )
        self.btn_desktop.clicked.connect(self.launch_desktop)
        layout.addWidget(self.btn_desktop)

        # Action buttons row
        action_row = QHBoxLayout()
        
        # Restart Button
        self.btn_restart = QPushButton("Restart")
        self.btn_restart.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        self.btn_restart.setMinimumHeight(35)
        self.btn_restart.setStyleSheet(
            "background-color: #e67e22; color: white; border-radius: 6px;"
        )
        self.btn_restart.clicked.connect(self.restart_engine)
        action_row.addWidget(self.btn_restart)
        
        # Calibrate Button
        self.btn_calibrate = QPushButton("Calibrate")
        self.btn_calibrate.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        self.btn_calibrate.setMinimumHeight(35)
        self.btn_calibrate.setStyleSheet(
            "background-color: #16a085; color: white; border-radius: 6px;"
        )
        self.btn_calibrate.clicked.connect(self.calibrate)
        action_row.addWidget(self.btn_calibrate)
        
        layout.addLayout(action_row)

        # Master Toggle
        self.btn_toggle = QPushButton("STANDBY")
        self.btn_toggle.setFont(QFont("Arial", 14, QFont.Weight.Bold))
        self.btn_toggle.setMinimumHeight(60)
        self.btn_toggle.setStyleSheet("background-color: #7f8c8d; color: white; border-radius: 6px;")
        self.btn_toggle.clicked.connect(self.toggle_active)
        layout.addWidget(self.btn_toggle)

        # Presets Group
        group_presets = QGroupBox("Animation Engine Cues")
        group_layout = QVBoxLayout(group_presets)
        self.combo_presets = QComboBox()
        self.combo_presets.setFont(QFont("Arial", 12))
        self.combo_presets.addItems([
            "Single Dot [Anchor]",
            "Dot Grid [Peripheral]",
            "Horizon Bar [Attitude]",
            "Liquid Wave [Fluid]",
        ])
        self.combo_presets.setCurrentIndex(self.settings["preset"])
        self.combo_presets.currentIndexChanged.connect(self.change_preset)
        group_layout.addWidget(self.combo_presets)
        layout.addWidget(group_presets)

        # - Parameters Group (Intensity, Opacity, Invert) ─
        group_params = QGroupBox("Motion Parameters")
        params_layout = QVBoxLayout(group_params)

        # Intensity Slider
        intensity_row = QHBoxLayout()
        intensity_row.addWidget(QLabel("Intensity:"))
        self.slider_intensity = QSlider(Qt.Horizontal)
        self.slider_intensity.setRange(0, 200)  # 0.0 to 2.0
        self.slider_intensity.setValue(int(self.settings["intensity"] * 100))
        self.slider_intensity.valueChanged.connect(self._on_intensity_changed)
        intensity_row.addWidget(self.slider_intensity)
        self.lbl_intensity = QLabel(f"{self.settings['intensity']:.1f}")
        self.lbl_intensity.setMinimumWidth(30)
        intensity_row.addWidget(self.lbl_intensity)
        params_layout.addLayout(intensity_row)

        # Opacity Slider
        opacity_row = QHBoxLayout()
        opacity_row.addWidget(QLabel("Opacity:"))
        self.slider_opacity = QSlider(Qt.Horizontal)
        self.slider_opacity.setRange(0, 100)  # 0.0 to 1.0
        self.slider_opacity.setValue(int(self.settings["opacity"] * 100))
        self.slider_opacity.valueChanged.connect(self._on_opacity_changed)
        opacity_row.addWidget(self.slider_opacity)
        self.lbl_opacity = QLabel(f"{self.settings['opacity']:.1f}")
        self.lbl_opacity.setMinimumWidth(30)
        opacity_row.addWidget(self.lbl_opacity)
        params_layout.addLayout(opacity_row)

        # Invert Axis Checkbox
        self.chk_invert = QCheckBox("Invert Axis (Opposite-Inertia Mode)")
        self.chk_invert.setChecked(self.settings.get("invert_axis", True))
        self.chk_invert.stateChanged.connect(self._on_invert_changed)
        params_layout.addWidget(self.chk_invert)

        self.chk_nyan = QCheckBox("Enable Nyan Cat Skin (Single Dot Only)")
        self.chk_nyan.setChecked(self.settings.get("nyan_cat", False))
        self.chk_nyan.stateChanged.connect(self._on_nyan_changed)
        self.chk_nyan.installEventFilter(self)
        params_layout.addWidget(self.chk_nyan)

        # HIDDEN: Latency Tracker is not working reliably right now, leaving it dead.
        # self.chk_latency = QCheckBox("Show Latency Tracker Overlay")
        # self.chk_latency.setChecked(self.settings.get("show_latency", False))
        # self.chk_latency.stateChanged.connect(self._on_latency_changed)
        # params_layout.addWidget(self.chk_latency)

        layout.addWidget(group_params)

        # - Developer Options: Telemetry ─
        group_telemetry = QGroupBox("Live Sensor Telemetry (Developer Options)")
        telemetry_layout = QVBoxLayout(group_telemetry)
        
        self.chk_telemetry = QCheckBox("Enable Real-Time Sensor Telemetry")
        self.chk_telemetry.setChecked(self.settings.get("telemetry", False))
        self.chk_telemetry.stateChanged.connect(self._on_telemetry_changed)
        telemetry_layout.addWidget(self.chk_telemetry)
        
        # Grid for values
        grid_telemetry = QGridLayout()
        self.lbl_raw_pitch = QLabel("Raw Pitch (X): 0.000")
        self.lbl_raw_yaw = QLabel("Raw Yaw (Y): 0.000")
        self.lbl_raw_roll = QLabel("Raw Roll (Z): 0.000")
        self.lbl_out_roll = QLabel("Out Roll (X): 0.000")
        self.lbl_out_pitch = QLabel("Out Pitch (Y): 0.000")
        
        self.lbl_raw_pitch.setStyleSheet("color: #e74c3c; font-family: monospace;")
        self.lbl_raw_yaw.setStyleSheet("color: #2ecc71; font-family: monospace;")
        self.lbl_raw_roll.setStyleSheet("color: #3498db; font-family: monospace;")
        self.lbl_out_roll.setStyleSheet("color: #9b59b6; font-family: monospace; font-weight: bold;")
        self.lbl_out_pitch.setStyleSheet("color: #f1c40f; font-family: monospace; font-weight: bold;")
        
        grid_telemetry.addWidget(self.lbl_raw_pitch, 0, 0)
        grid_telemetry.addWidget(self.lbl_raw_yaw, 1, 0)
        grid_telemetry.addWidget(self.lbl_raw_roll, 2, 0)
        grid_telemetry.addWidget(self.lbl_out_roll, 0, 1)
        grid_telemetry.addWidget(self.lbl_out_pitch, 1, 1)
        
        telemetry_layout.addLayout(grid_telemetry)
        layout.addWidget(group_telemetry)

        # Watchdog Console
        group_console = QGroupBox("Live Engine Tracker (stdout)")
        console_layout = QVBoxLayout(group_console)
        self.console_log = QTextEdit()
        self.console_log.setReadOnly(True)
        self.console_log.setStyleSheet(
            "background-color: #1e1e1e; color: #ccc; font-family: monospace; font-size: 11px;"
        )
        console_layout.addWidget(self.console_log)
        layout.addWidget(group_console)

        # Status bar
        self.lbl_status = QLabel(f"State file: {STATE_FILE} | Settings: {SETTINGS_FILE}")
        self.lbl_status.setStyleSheet("color: gray; font-size: 10px;")
        layout.addWidget(self.lbl_status)

    # slider things
    def _on_intensity_changed(self, value):
        intensity = value / 100.0
        self.settings["intensity"] = intensity
        self.lbl_intensity.setText(f"{intensity:.1f}")
        self._save_settings()
        self.write_state()

    def _on_opacity_changed(self, value):
        opacity = value / 100.0
        self.settings["opacity"] = opacity
        self.lbl_opacity.setText(f"{opacity:.1f}")
        self._save_settings()
        self.write_state()

    def _on_invert_changed(self, state):
        self.settings["invert_axis"] = (state == Qt.Checked.value)
        self._save_settings()
        self.write_state()

    def _on_nyan_changed(self, state):
        is_nyan = (state == Qt.Checked.value)
        if is_nyan:
            # snap opacity to max when they turn on the skin
            self.slider_opacity.setValue(100)
            
        self.settings["nyan_cat"] = is_nyan
        self._save_settings()
        self.write_state()

    # generate a user ID because privacy is goated
    def _init_user_id(self):
        user_id_file = os.path.expanduser("~/.mutemotion/user.id")
        if os.path.exists(user_id_file):
            try:
                with open(user_id_file, "r") as f:
                    self.user_id = f.read().strip()
            except Exception:
                self.user_id = str(uuid.uuid4())
        else:
            self.user_id = str(uuid.uuid4())
            os.makedirs(os.path.dirname(user_id_file), exist_ok=True)
            try:
                with open(user_id_file, "w") as f:
                    f.write(self.user_id)
            except Exception:
                pass

    def _check_unlocks(self):
        try:
            resp = requests.get(f"{SERVER_URL}/unlock/{self.user_id}", timeout=5)
            if resp.status_code == 200:
                self.unlocked_skins = resp.json().get("unlocked", [])
            else:
                self.unlocked_skins = []
        except Exception as e:
            self.unlocked_skins = []
        self._update_nyan_ui()

    def _update_nyan_ui(self):
        if "nyan_cat" in self.unlocked_skins:
            self.chk_nyan.setEnabled(True)
            self.chk_nyan.setText("Enable Nyan Cat Skin (Single Dot Only)")
        else:
            self.chk_nyan.setEnabled(False)
            self.chk_nyan.setText("🔒 Cat Cosmetic Skin (Single Dot Only) Click to purchase (€0.99)")
            if self.chk_nyan.isChecked():
                self.chk_nyan.blockSignals(True)
                self.chk_nyan.setChecked(False)
                self.chk_nyan.blockSignals(False)
                self.settings["nyan_cat"] = False
                self._save_settings()
                self.write_state()

    def eventFilter(self, obj, event):
        if obj == self.chk_nyan and event.type() == QEvent.Type.MouseButtonPress:
            if not self.chk_nyan.isEnabled():
                webbrowser.open(f"{SERVER_URL}/?user_id={self.user_id}")
                self._purchase_poll_elapsed = 0
                self._purchase_poll_timer.start(2000)
                return True
        return super().eventFilter(obj, event)

    def _poll_purchase(self):
        self._purchase_poll_elapsed += 2
        self._check_unlocks()
        if "nyan_cat" in self.unlocked_skins:
            self._purchase_poll_timer.stop()
        if self._purchase_poll_elapsed >= 300: # 5 minutes of polling before giving up
            self._purchase_poll_timer.stop()

    def _on_latency_changed(self, state):
        self.settings["show_latency"] = (state == 2)
        self._save_settings()
        self.write_state()

    def _on_telemetry_changed(self, state):
        self.settings["telemetry"] = (state == 2)
        self._save_settings()
        self.write_state()

    # state file ipc dump
    def toggle_active(self):
        if not self.daemon_running:
            self.append_log("> Cannot toggle: daemon is not running.", "#e74c3c")
            return
        self.is_active = not self.is_active
        if self.is_active:
            self.btn_toggle.setText("ACTIVE")
            self.btn_toggle.setStyleSheet("background-color: #27ae60; color: white; border-radius: 6px;")
            self.append_log("> OVERLAY = ACTIVE", "#f39c12")
        else:
            self.btn_toggle.setText("INACTIVE / BYPASSED")
            self.btn_toggle.setStyleSheet("background-color: #c0392b; color: white; border-radius: 6px;")
            self.append_log("> OVERLAY = INACTIVE (GPU yield)", "#f39c12")
        self.write_state()

    def change_preset(self, index):
        self.current_preset = index
        self.settings["preset"] = index
        self.append_log(f"> CUE = {self.combo_presets.currentText()}", "#3498db")
        self._save_settings()
        self.write_state()

    def write_state(self, calibrate=False):
        try:
            with open(STATE_FILE, "w") as f:
                f.write(f"ACTIVE={1 if self.is_active else 0}\n")
                f.write(f"PRESET={self.current_preset}\n")
                f.write(f"INTENSITY={self.settings['intensity']:.2f}\n")
                f.write(f"OPACITY={self.settings['opacity']:.2f}\n")
                f.write(f"INVERT={1 if self.settings['invert_axis'] else 0}\n")
                f.write(f"LATENCY={1 if self.settings.get('show_latency', False) else 0}\n")
                f.write(f"TELEMETRY={1 if self.settings.get('telemetry', False) else 0}\n")
                f.write(f"SKIN={1 if self.settings.get('nyan_cat', False) else 0}\n")
                f.write(f"CALIBRATE={1 if calibrate else 0}\n")
        except Exception as e:
            self.lbl_status.setText(f"Error: {e}")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = VRControlPanel()
    window.show()
    sys.exit(app.exec())
