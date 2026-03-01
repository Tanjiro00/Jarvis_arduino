"""Модуль связи с Arduino по Serial с автопереподключением."""

import threading
import time

import serial
import serial.tools.list_ports

import config


class ArduinoSerial:
    """Управление связью с Arduino: отправка команд, приём WAKE/SLEEP."""

    def __init__(self, port=None):
        self.baud_rate = config.BAUD_RATE
        self.ser = None
        self._running = False
        self._reader_thread = None
        self._on_wake = None
        self._on_sleep = None
        self._port = port
        self._reconnect_lock = threading.Lock()

        if port:
            self._connect(port)
        else:
            self._auto_connect()

    def _auto_connect(self):
        """Автопоиск порта Arduino."""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if p.vid in (0x2341, 0x1A86, 0x0403):
                self._connect(p.device)
                return
        for p in ports:
            if "ttyUSB" in p.device or "ttyACM" in p.device or "COM" in p.device:
                self._connect(p.device)
                return
        print("[Serial] Arduino не найдена. Доступные порты:")
        for p in ports:
            print(f"  {p.device} — {p.description}")

    def _connect(self, port):
        """Подключение к порту."""
        try:
            self.ser = serial.Serial(port, self.baud_rate, timeout=1)
            self._port = port
            time.sleep(2)  # ждём перезагрузку Arduino
            print(f"[Serial] Подключено к {port}")
        except serial.SerialException as e:
            print(f"[Serial] Ошибка подключения к {port}: {e}")
            self.ser = None

    def _reconnect(self):
        """Попытка переподключения."""
        with self._reconnect_lock:
            if self.connected:
                return True
            port = self._port
            if not port:
                return False
            try:
                if self.ser:
                    self.ser.close()
            except Exception:
                pass
            try:
                self.ser = serial.Serial(port, self.baud_rate, timeout=1)
                time.sleep(2)
                print(f"[Serial] Переподключено к {port}")
                return True
            except serial.SerialException:
                self.ser = None
                return False

    @property
    def connected(self):
        try:
            return self.ser is not None and self.ser.is_open
        except Exception:
            return False

    def on_wake(self, callback):
        self._on_wake = callback

    def on_sleep(self, callback):
        self._on_sleep = callback

    def start_reading(self):
        """Запуск фонового потока чтения Serial."""
        if not self.connected:
            print("[Serial] Нет подключения, чтение не запущено")
            return
        self._running = True
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

    def stop_reading(self):
        self._running = False
        if self._reader_thread:
            self._reader_thread.join(timeout=2)

    def _read_loop(self):
        """Фоновый цикл чтения с автопереподключением."""
        while self._running:
            try:
                if not self.connected:
                    print("[Serial] Соединение потеряно, попытка переподключения...")
                    if not self._reconnect():
                        time.sleep(3)
                        continue

                if self.ser.in_waiting:
                    line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    if line == "WAKE" and self._on_wake:
                        self._on_wake()
                    elif line == "SLEEP" and self._on_sleep:
                        self._on_sleep()
                else:
                    time.sleep(0.05)

            except (serial.SerialException, OSError):
                print("[Serial] Ошибка чтения, попытка переподключения через 3 сек...")
                try:
                    if self.ser:
                        self.ser.close()
                except Exception:
                    pass
                self.ser = None
                time.sleep(3)

    def send(self, command):
        """Отправка команды на Arduino (graceful — не падает при потере)."""
        if not self.connected:
            return
        try:
            self.ser.write(f"{command}\n".encode("utf-8"))
        except (serial.SerialException, OSError):
            print(f"[Serial] Ошибка отправки: {command}")

    # --- Удобные методы ---

    def mouth_closed(self):
        self.send("M0")

    def mouth_small(self):
        self.send("M1")

    def mouth_medium(self):
        self.send("M2")

    def mouth_wide(self):
        self.send("M3")

    def mouth_full(self):
        self.send("M4")

    def start_listening_animation(self):
        self.send("L1")

    def start_talking_animation(self):
        self.send("A1")

    def stop_animation(self):
        self.send("A0")

    def sleep_mode(self):
        self.send("S1")

    def wake_mode(self):
        self.send("S0")

    def set_emotion(self, emotion_cmd):
        """Отправить команду эмоции (E1-E4)."""
        if emotion_cmd:
            self.send(emotion_cmd)

    def blink_confirm(self):
        """Быстрое моргание — подтверждение wake-word."""
        self.send("BL")

    def close(self):
        self.stop_reading()
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[Serial] Соединение закрыто")
