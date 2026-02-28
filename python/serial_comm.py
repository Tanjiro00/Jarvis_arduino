"""Модуль связи с Arduino по Serial."""

import threading
import time
import serial
import serial.tools.list_ports


class ArduinoSerial:
    """Управление связью с Arduino: отправка команд матрице, приём WAKE/SLEEP."""

    def __init__(self, baud_rate=9600, port=None):
        self.baud_rate = baud_rate
        self.ser = None
        self._running = False
        self._reader_thread = None
        self._on_wake = None
        self._on_sleep = None

        if port:
            self._connect(port)
        else:
            self._auto_connect()

    def _auto_connect(self):
        """Автопоиск порта Arduino."""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            # Arduino Uno обычно имеет VID:PID 2341:0043 или CH340: 1A86:7523
            if p.vid in (0x2341, 0x1A86, 0x0403):
                self._connect(p.device)
                return
        # Если не нашли по VID — берём первый доступный
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
            time.sleep(2)  # ждём перезагрузку Arduino
            print(f"[Serial] Подключено к {port}")
        except serial.SerialException as e:
            print(f"[Serial] Ошибка подключения к {port}: {e}")
            self.ser = None

    @property
    def connected(self):
        return self.ser is not None and self.ser.is_open

    def on_wake(self, callback):
        """Регистрация callback на WAKE."""
        self._on_wake = callback

    def on_sleep(self, callback):
        """Регистрация callback на SLEEP."""
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
        """Остановка фонового чтения."""
        self._running = False
        if self._reader_thread:
            self._reader_thread.join(timeout=2)

    def _read_loop(self):
        """Фоновый цикл чтения сообщений от Arduino."""
        while self._running and self.connected:
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    if line == "WAKE" and self._on_wake:
                        self._on_wake()
                    elif line == "SLEEP" and self._on_sleep:
                        self._on_sleep()
                    elif line.startswith("DIST:"):
                        pass  # отладка, можно логировать
                else:
                    time.sleep(0.05)
            except (serial.SerialException, OSError):
                print("[Serial] Соединение потеряно")
                self._running = False

    def send(self, command):
        """Отправка команды на Arduino."""
        if not self.connected:
            return
        try:
            self.ser.write(f"{command}\n".encode("utf-8"))
        except serial.SerialException:
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

    def close(self):
        """Закрытие соединения."""
        self.stop_reading()
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[Serial] Соединение закрыто")
