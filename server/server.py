"""
Dispensador de Alimento para Peces - Servidor Web
Proyecto: SFA - Dispensador de Peces 2do. BTI
Ejecutar: python server.py
"""
import sys, json, time, threading, socket
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
import os

INSTALL_MSG = """
   No se encontro pyserial. Instalando...
"""
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print(INSTALL_MSG)
    import subprocess
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'pyserial', '-q'])
    import serial
    import serial.tools.list_ports

HOST = "127.0.0.1"
PORT = 8765
BAUD = 9600
STATIC = os.path.join(os.path.dirname(__file__), "static")

# ─── Estado global ───────────────────────────────────────────────────────────
ser = None
ser_lock = threading.Lock()
shutdown_event = threading.Event()
lines_buf = []
lines_lock = threading.Lock()

def find_free_port(preferred):
    for port in range(preferred, preferred + 20):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            try:
                probe.bind((HOST, port))
                return port
            except OSError:
                continue
    raise OSError("No hay puertos libres disponibles para el servidor local")

def push_line(text):
    # Filtrar caracteres basura (binario de bootloader, ruido)
    clean = ''.join(c for c in text if 32 <= ord(c) <= 126 or ord(c) in (0xA1,0xBF,0xC1,0xC9,0xCD,0xD1,0xD3,0xDA,0xDC,0xE1,0xE9,0xED,0xF1,0xF3,0xFA,0xFC))
    clean = clean.strip()
    if not clean:
        return
    with lines_lock:
        lines_buf.append(clean)

def drain_lines():
    with lines_lock:
        out = '\n'.join(lines_buf)
        lines_buf.clear()
    return out

MIME = {
    ".html": "text/html; charset=utf-8",
    ".css":  "text/css; charset=utf-8",
    ".js":   "application/javascript; charset=utf-8",
    ".png":  "image/png",
    ".ico":  "image/x-icon",
    ".svg":  "image/svg+xml",
}

# ─── Servidor HTTP multihilo ─────────────────────────────────────────────────
class Pool(ThreadingMixIn, HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path == "/stream":
            return self._sse()
        if self.path == "/api/ports":
            return self._ports()
        if self.path in ("/", "/index.html"):
            return self._file("index.html", "text/html; charset=utf-8")
        self._file(self.path.lstrip("/"))

    def do_POST(self):
        if self.path == "/api/serial":
            return self._serial_ctrl()
        if self.path == "/api/command":
            return self._command()

    # ── SSE (Server-Sent Events) ───────────────────────────────────────
    def _sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        last_heartbeat = time.time()
        try:
            while not shutdown_event.is_set():
                cur = drain_lines()
                now = time.time()
                if cur and cur.strip():
                    self.wfile.write(f"data: {cur}\n\n".encode())
                    self.wfile.flush()
                    last_heartbeat = now
                elif now - last_heartbeat > 10:
                    self.wfile.write(": hb\n\n".encode())
                    self.wfile.flush()
                    last_heartbeat = now
                time.sleep(0.2)
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    # ── Listar puertos ─────────────────────────────────────────────────
    def _ports(self):
        pp = []
        for p in serial.tools.list_ports.comports():
            desc = f"{p.description or ''} {p.manufacturer or ''}"
            hint = any(k in desc.upper() for k in
                       ['ARDUINO','CH340','USB SERIAL','FT232','CP210','USB2.0-SERIAL'])
            pp.append({
                "port": p.device,
                "desc": p.description or p.device,
                "hint": hint
            })
        self._json(pp)

    # ── Abrir / Cerrar puerto serial ────────────────────────────────────
    def _serial_ctrl(self):
        body = self._read_body()
        action = body.get("action")
        ok = False
        if action == "open":
            name = body.get("port", "")
            ok = self._ser_open(name)
        elif action == "close":
            ok = self._ser_close()
        self._json({"ok": ok})

    def _ser_open(self, name):
        global ser
        with ser_lock:
            try:
                if ser and ser.is_open:
                    ser.close()
                ser = serial.Serial(name, BAUD, timeout=0.2)
                time.sleep(2)  # Esperar reset del Arduino
                ser.reset_input_buffer()
                push_line(f"[SISTEMA] Conectado a {name}")
                return True
            except Exception as e:
                push_line(f"[ERROR] {e}")
                return False

    def _ser_close(self):
        global ser
        with ser_lock:
            try:
                if ser and ser.is_open:
                    ser.close()
                ser = None
                push_line("[SISTEMA] Desconectado")
                return True
            except Exception:
                return False

    # ── Enviar comando al Arduino ───────────────────────────────────────
    def _command(self):
        body = self._read_body()
        cmd = body.get("command", "").strip()
        if not cmd:
            self._json({"ok": False, "msg": "Comando vacio"})
            return
        with ser_lock:
            if not ser or not ser.is_open:
                self._json({"ok": False, "msg": "No conectado"})
                return
            try:
                drain_lines()
                ser.reset_input_buffer()
                ser.write((cmd + "\n").encode("utf-8"))
            except Exception as e:
                self._json({"ok": False, "msg": str(e)})
                return
        # Esperar respuesta fuera del lock para que serial_loop pueda leer
        time.sleep(0.6)
        resp = drain_lines()
        self._json({"ok": True, "response": resp})

    # ── Utilidades ──────────────────────────────────────────────────────
    def _read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return json.loads(self.rfile.read(length)) if length else {}

    def _json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _file(self, name, ctype=None):
        fp = os.path.join(STATIC, name)
        if not os.path.isfile(fp):
            self.send_response(404)
            self.end_headers()
            return
        if not ctype:
            ctype = MIME.get(os.path.splitext(name)[1], "application/octet-stream")
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        with open(fp, "rb") as f:
            self.wfile.write(f.read())

# ─── Hilo lector del puerto serial ────────────────────────────────────────────
def serial_loop():
    line_buf = ""
    while not shutdown_event.is_set():
        with ser_lock:
            s = ser
        if s and s.is_open:
            try:
                if s.in_waiting:
                    chunk = s.read(s.in_waiting).decode("utf-8", errors="replace")
                    line_buf += chunk
                    while "\n" in line_buf:
                        idx = line_buf.index("\n")
                        line = line_buf[:idx].strip()
                        line_buf = line_buf[idx+1:]
                        if line:
                            push_line(line)
            except Exception:
                pass
        time.sleep(0.05)

# ─── Punto de entrada ────────────────────────────────────────────────────────
if __name__ == "__main__":
    active_port = find_free_port(PORT)
    server = Pool((HOST, active_port), Handler)
    t = threading.Thread(target=serial_loop, daemon=True)
    t.start()
    print(r"""
   ____  _                                          _
  |  _ \(_)___ _ __  _ __   ___  __ _  __ _ _ __ __| | ___  _ __
  | | | | / __| '_ \\| '_ \\ / _ \\/ _` |/ _` | '__/ _` |/ _ \\| '__|
  | |_| | \\__ \\ |_) | | | |  __/ (_| | (_| | | | (_| | (_) | |
  |____/|_|___/ .__/|_| |_|\\___|\\__,_|\\__,_|_|  \\__,_|\\___/|_|
              |_|          de ALIMENTO para PECES - 2do BTI
""")
    print(f"   http://{HOST}:{active_port}")
    print("   Presiona Ctrl+C para detener\n")
    import webbrowser
    webbrowser.open(f"http://{HOST}:{active_port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        shutdown_event.set()
        with ser_lock:
            if ser and ser.is_open:
                ser.close()
        server.shutdown()
