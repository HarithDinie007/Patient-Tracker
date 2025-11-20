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

def run_action(device_id):
    print(f">>> Taking action for {device_id}!")
    text = f"ALERT: {device_id} is leaving the facility!"
    send_telegram_message(text)
    

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers['Content-Length'])
        data = self.rfile.read(length).decode()

        # -------------------------
        # Parse device ID
        # -------------------------
        device_id = "Unknown"
        if "device=" in data:
            try:
                device_id = data.split("device=")[1].split(";")[0]
            except:
                pass

        print("From device:", device_id)

        # -------------------------
        # React to alert
        # -------------------------
        if "ALERT" in data:
            print(f"ALERT RECEIVED from {device_id}! Running action...")
            run_action(device_id)

        self.send_response(200)
        self.end_headers()


server = HTTPServer(("0.0.0.0", 8080), Handler)
print("Server running on port 8080...")
server.serve_forever()
