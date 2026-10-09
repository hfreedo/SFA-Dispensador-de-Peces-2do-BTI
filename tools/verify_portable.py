"""Extrae y prueba el EXE con PATH sin Python y directorio de trabajo ajeno."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path


def request(base, path, body=None):
    payload = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(base + path, data=payload,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=3) as response:
        return response.read()


def main():
    archive = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="SFA portable prueba ") as tmp:
        extracted = Path(tmp) / "Entrega con espacios"
        with zipfile.ZipFile(archive) as bundle:
            bundle.extractall(extracted)
        executable, = extracted.rglob("*.exe")
        env = os.environ.copy()
        system_dir = Path(os.environ["SYSTEMROOT"]) / "System32"
        for key in list(env):
            if key.upper() in ("PATH", "PYTHONHOME", "PYTHONPATH", "VIRTUAL_ENV"):
                env.pop(key)
        env["PATH"] = str(system_dir)
        # Ocupar el puerto inicial para comprobar el cambio automatico de puerto.
        with socket.socket() as occupied:
            occupied.bind(("127.0.0.1", 0))
            occupied.listen()
            port = occupied.getsockname()[1]
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", port + 1))
            base = f"http://127.0.0.1:{port + 1}"
            with open(Path(tmp) / "servidor.log", "wb") as log:
                proc = subprocess.Popen([str(executable), "--no-browser", "--port", str(port)],
                                        cwd=tmp, env=env, stdout=log, stderr=log,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    deadline = time.monotonic() + 45
                    while True:
                        if proc.poll() is not None:
                            raise RuntimeError("El EXE termino al iniciar")
                        try:
                            health = json.loads(request(base, "/api/health"))
                            break
                        except OSError:
                            if time.monotonic() > deadline:
                                raise RuntimeError("El portable no respondio en 45 segundos")
                            time.sleep(0.25)
                    assert health == {"app": "SFA_Dispensador_Peces_2BTI", "version": "1.1.0"}
                    assert b"updateLedPreview" in request(base, "/")
                    assert request(base, "/assets/clownfish.png").startswith(b"\x89PNG")
                    json.loads(request(base, "/assets/Pez1.json"))
                    assert isinstance(json.loads(request(base, "/api/ports")), list)
                    reply = json.loads(request(base, "/api/command", {"command": "ESTADO"}))
                    assert reply == {"ok": False, "msg": "No conectado"}
                    print("OK: EXE extraido, PATH sin Python, ruta con espacios, puerto alternativo,")
                    print("interfaz, recursos, listado COM y rechazo de comandos sin conexion.")
                except Exception:
                    log.flush()
                    print((Path(tmp) / "servidor.log").read_text(errors="replace"))
                    raise
                finally:
                    # taskkill /T cierra tambien el hijo del ejecutable onefile.
                    subprocess.run([str(system_dir / "taskkill.exe"),
                                    "/PID", str(proc.pid), "/T", "/F"], capture_output=True)
                    proc.wait(timeout=15)


if __name__ == "__main__":
    main()
