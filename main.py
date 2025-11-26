from http.server import BaseHTTPRequestHandler, HTTPServer
import requests, time

BOT_TOKEN = "8391197716:AAEceZ47cupC7odBuGzAYms4mRdMtStEnMA"
CHAT_ID = "-1003413811334"

TELE_URL = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

device_state = {}

GAP_SECONDS = 5
BURST_LIMIT = 5

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
        global device_state
        
        length = int(self.headers['Content-Length'])
        data = self.rfile.read(length).decode()

        # -------------------------
        # Parse device ID
        # -------------------------
        device_id = "Unknown"
        distance = 0

        # Split by semicolon
        parts = data.split(";")
        # Extract device
        device_id = parts[0].split("=")[1]
        # Extract distance
        distance = int(parts[1].split("=")[1])


        print(data)
        
        now = time.time()
        
        if device_id not in device_state:
            device_state[device_id] = {"count": 0, "last_time": now}

        time_gap = now - device_state[device_id]["last_time"]

        if time_gap > GAP_SECONDS:
            print(f"[RESET] Gap detected for {device_id}, resetting counter")
            device_state[device_id]["count"] = 0

        device_state[device_id]["last_time"] = now

        # -------------------------
        # React to alert
        # -------------------------
        
        count = device_state[device_id]["count"]
        
        if "FALL" in data:
                print(f"FALL ALERT RECEIVED from {device_id}! Running action...")
                fall_alert(device_id, distance)
        
        if count == 0:
            if "LEAVING" in data:
                print(f"ALERT RECEIVED from {device_id}! Running action...")
                leaving_alert(device_id, distance)
                
        else:
            print(f"Skipping alert for {device_id} (count = {count})")
            
        device_state[device_id]["count"] = (count + 1) % BURST_LIMIT

        self.send_response(200)
        self.end_headers()

server = HTTPServer(("0.0.0.0", 8080), Handler)
print("Server running on port 8080...")
server.serve_forever()
