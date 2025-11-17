from http.server import BaseHTTPRequestHandler, HTTPServer

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers['Content-Length'])
        data = self.rfile.read(length).decode()

        print(data)

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


def run_action(device_id):
    print(f">>> Taking action for {device_id}!")

server = HTTPServer(("0.0.0.0", 8080), Handler)
print("Server running on port 8080...")
server.serve_forever()
