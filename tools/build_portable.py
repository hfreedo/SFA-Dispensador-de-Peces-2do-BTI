"""Construye el ZIP para usuarios sin Python; ejecutar en Windows x64."""
import hashlib
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = "1.1.0"
NAME = "SFA_Dispensador_Peces_2BTI"
PACKAGE = f"{NAME}_v{VERSION}_Windows_x64"


def main():
    if sys.platform != "win32" or platform.architecture()[0] != "64bit":
        raise SystemExit("Construir con Python de 64 bits en Windows.")
    subprocess.run([
        sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
        "--onefile", "--console", "--noupx", "--name", NAME,
        "--distpath", str(ROOT / "dist"),
        "--workpath", str(ROOT / "build" / "pyinstaller"),
        "--specpath", str(ROOT / "build"),
        "--add-data", f"{ROOT / 'server' / 'static' / 'index.html'}:static",
        "--add-data", f"{ROOT / 'server' / 'static' / 'assets'}:static/assets",
        "--hidden-import", "serial.tools.list_ports_windows",
        str(ROOT / "server" / "server.py"),
    ], check=True, cwd=ROOT)
    delivery = ROOT / "dist" / PACKAGE
    delivery.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "dist" / f"{NAME}.exe", delivery)
    shutil.copy2(ROOT / "docs" / "USO_PORTABLE.txt", delivery)
    archive = Path(shutil.make_archive(str(ROOT / "dist" / PACKAGE), "zip",
                                       root_dir=ROOT / "dist", base_dir=PACKAGE))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(".sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    print(f"Entrega: {archive}\nSHA-256: {digest}")


if __name__ == "__main__":
    main()
