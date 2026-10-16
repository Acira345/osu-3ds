import socket
import http.server
import socketserver
import threading
import qrcode

PORT = 8000

# 1. Detectar automáticamente tu IP local (ej. 192.168.1.XX)
def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

local_ip = get_local_ip()
download_url = f"http://{local_ip}:{PORT}/osu-3ds.3dsx"

# 2. Iniciar el servidor HTTP en segundo plano
handler = http.server.SimpleHTTPRequestHandler
httpd = socketserver.TCPServer(("", PORT), handler)
server_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
server_thread.start()

print(f"\nServidor activo en: {download_url}")
print("Escanea este codigo con FBI en tu 3DS:\n")

# 3. Imprimir el codigo QR directamente en la consola
qr = qrcode.QRCode(border=1)
qr.add_data(download_url)
qr.print_ascii(invert=True)

print("\nPresiona Ctrl+C para detener el servidor una vez descargado.")
try:
    server_thread.join()
except KeyboardInterrupt:
    print("\nServidor detenido.")
    httpd.shutdown()