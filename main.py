from http.server import BaseHTTPRequestHandler, HTTPServer
import requests

BOT_TOKEN = "8391197716:AAEceZ47cupC7odBuGzAYms4mRdMtStEnMA"
CHAT_ID = "-1003413811334"

TELE_URL = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

def send_telegram_message(text):
    requests.post(TELE_URL, json={
        "chat_id": CHAT_ID,
        "text": text
    })

def leaving_alert(device_id, distance):
    print(f">>> Taking action for {device_id}!")
    text = f"ALERT: {device_id} is leaving the facility! Last known distance: {distance} meters."
    send_telegram_message(text)
    
def fall_alert(device_id, distance):
    print(f">>> Taking action for {device_id}!")
    text = f"ALERT: {device_id} has fallen! Last known distance: {distance} meters."
    send_telegram_message(text)

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers['Content-Length'])
        data = self.rfile.read(length).decode()

        # -------------------------
        # Parse device ID
        # -------------------------
        device_id = "Unknown"
        distance = 0
        alert_type = "None"

        # Split by semicolon
        parts = data.split(";")
        # Extract device
        device_id = parts[0].split("=")[1]
        # Extract distance
        distance = int(parts[1].split("=")[1])
        # Extract alert type
        alert_type = parts[2].split("=")[1]

        print(device_id, distance)

        # -------------------------
        # React to alert
        # -------------------------
        if "LEAVING" in data:
            print(f"ALERT RECEIVED from {device_id}! Running action...")
            #leaving_alert(device_id, distance)

        if "FALL" in data:
            print(f"FALL ALERT RECEIVED from {device_id}! Running action...")
            #fall_alert(device_id, distance)

        self.send_response(200)
        self.end_headers()


server = HTTPServer(("0.0.0.0", 8080), Handler)
print("Server running on port 8080...")
server.serve_forever()
