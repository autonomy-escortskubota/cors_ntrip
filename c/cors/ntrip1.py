import threading
import serial
import time
import socket
import base64
import argparse
from pyrtcm import RTCMReader

class GNSSHandler:
    def __init__(self, serial_port, baudrate, gga_file, serial_lock):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.gga_file = gga_file
        self.serial_lock = serial_lock
        self.ser = None
        self.latest_gga = None

    def start(self):
        try:
            self.ser = serial.Serial(self.serial_port, self.baudrate, timeout=1)
            print(f"[GNSS] Listening on {self.serial_port}")
            threading.Thread(target=self.read_gga_loop, daemon=True).start()
        except Exception as e:
            print(f"[GNSS ERROR] Failed to open port {self.serial_port}: {e}")

    @staticmethod
    def nmea_to_decimal(degrees_minutes, direction):
        if not degrees_minutes or degrees_minutes == "0":
            return 0.0
        d = int(float(degrees_minutes) // 100)
        m = float(degrees_minutes) - d * 100
        decimal = d + m / 60
        if direction in ['S', 'W']:
            decimal = -decimal
        return decimal

    def read_gga_loop(self):
        """Continuously read GGA sentences from GNSS receiver"""
        while True:
            try:
                with self.serial_lock:
                    line = self.ser.readline().decode(errors='ignore').strip()

                if not line:
                    continue

                if line.startswith('$GNGGA') or line.startswith('$GPGGA'):
                    self.latest_gga = line
                    print(self.latest_gga)
                    # Save GGA to file for NTRIP use
                    with open(self.gga_file, 'w') as f:
                        f.write(self.latest_gga)

                    ## Log all raw data
                    with open("rawdata_QUEC_1702.txt", 'a') as f:
                        f.write(self.latest_gga + "\n")

                    # Extract lat/lon for visualization
                    # parts = line.split(',')
                    # if len(parts) < 6:
                    #     continue
                    # lat_raw, lat_dir, lon_raw, lon_dir = parts[2], parts[3], parts[4], parts[5]
                    # lat = GNSSHandler.nmea_to_decimal(lat_raw, lat_dir)
                    # lon = GNSSHandler.nmea_to_decimal(lon_raw, lon_dir)

                    # with open("data.buf", 'a') as f:
                    #     f.write(f"s:1 Lat:{lat} Lng:{lon}\n")

            except Exception as e:
                print(f"[GNSS ERROR] {e}")
                time.sleep(1)

    def write_rtcm(self, data):
        """Send RTCM correction data to GNSS receiver"""
        if self.ser is None:
            return
        try:
            with self.serial_lock:
                self.ser.write(data)
            print(f"[GNSS] Wrote {len(data)} bytes of RTCM correction")
        except Exception as e:
            print(f"[GNSS ERROR] Failed to write RTCM: {e}")


class NTRIPClient:
    def __init__(self, caster_host, caster_port, mountpoint, username, password, gga_file, gnss_handler, send_gga):
        self.caster_host = caster_host
        self.caster_port = caster_port
        self.mountpoint = mountpoint
        self.username = username
        self.password = password
        self.gga_file = gga_file
        self.gnss_handler = gnss_handler
        self.send_gga = send_gga  # only True for VRS

    def get_latest_gga(self):
        """Read latest GGA from file"""
        try:
            with open(self.gga_file, "r") as f:
                return f.read().strip()
        except Exception:
            return None

    def connect_and_stream(self):
        """Connect to CORS/VRS or Base and stream RTCM corrections"""
        while True:
            try:
                print(f"[NTRIP] Connecting to {self.caster_host}:{self.caster_port} ...")
                sock = socket.create_connection((self.caster_host, self.caster_port), timeout=10)

                credentials = base64.b64encode(f"{self.username}:{self.password}".encode()).decode()
                request = (
                    f"GET /{self.mountpoint} HTTP/1.1\r\n"
                    f"Host: {self.caster_host}\r\n"
                    f"Ntrip-Version: Ntrip/2.0\r\n"
                    f"User-Agent: NTRIP PythonClient/1.0\r\n"
                    f"Authorization: Basic {credentials}\r\n"
                    f"Connection: keep-alive\r\n"
                    f"\r\n"
                )
                sock.sendall(request.encode())

                # Wait for HTTP 200 OK
                response = b""
                while b"\r\n\r\n" not in response:
                    response += sock.recv(1024)
                if b"200 OK" not in response:
                    print("[NTRIP] Failed response from server:")
                    print(response.decode(errors='ignore'))
                    sock.close()
                    time.sleep(5)
                    continue

                print("[NTRIP] Connected to caster (200 OK).")

                # Send initial GGA immediately (only if VRS mode)
                if self.send_gga:
                    gga = self.get_latest_gga()
                    if gga:
                        sock.sendall((gga + "\r\n").encode())
                        print(f"[→] Sent initial GGA: {gga}")

                last_gga_time = time.time()

                # --- Main RTCM reading loop ---
                while True:
                    if self.send_gga and (time.time() - last_gga_time >= 10):
                        gga = self.get_latest_gga()
                        if gga:
                            sock.sendall((gga + "\r\n").encode())
                            print(f"[→] Sent GGA: {gga}")
                            last_gga_time = time.time()
                    
                    data = sock.recv(4096)
                    
                    if not data:
                        print("[NTRIP] No data, reconnecting...")
                        break
                    print(len(data))
                    # msg=RTCMReader.parse(data)
                    # print(msg)
                    
                    # Pass RTCM data to GNSS receiver
                    self.gnss_handler.write_rtcm(data)

            except Exception as e:
                print(f"[NTRIP ERROR] {e}")

            finally:
                try:
                    sock.close()
                except:
                    pass
                print("[NTRIP] Reconnecting in 5 seconds...")
                time.sleep(5)


def main():
    parser = argparse.ArgumentParser(description="NTRIP client for CORS/VRS or EKL base")
    parser.add_argument("mode", nargs="?", default="default", help="Use 'cors' for VRS; leave empty for EKL base")
    args = parser.parse_args()

    SERIAL_PORT = "/dev/ttyUSB0"
    BAUDRATE = 115200
    GGA_FILE = "latest_gga.txt"
    SERIAL_LOCK = threading.Lock()

    gnss_handler = GNSSHandler(SERIAL_PORT, BAUDRATE, GGA_FILE, SERIAL_LOCK)
    gnss_handler.start()

    # === Choose connection ===
    if args.mode.lower() == "cors":
        caster = "103.205.244.106"
        port = 2101
        mountpoint = "RTCM_VRS"
        username = "sumit.chatterjee"
        password = "cors@2025"
        send_gga = True
        print("[MODE] Connecting to CORS VRS server (GGA enabled)...")
    elif args.mode.lower()=="quec":
        print("here")
        caster = "qrtksa1.quectel.com"
        port = 2101
        mountpoint = "AUTO_ITRF2020"
        username = "escortskubota_0000001"
        password = "m9jdeb6z"
        send_gga = True        
    else:
        caster = "eklntrip.escortskubota.com"
        port = 2101
        mountpoint = "test2"
        username = "admin"
        password = "PAN@bx992"
        send_gga = False
        print("[MODE] Connecting to EKL base server (no GGA sent)...")

    ntrip_client = NTRIPClient(caster, port, mountpoint, username, password, GGA_FILE, gnss_handler, send_gga)
    ntrip_client.connect_and_stream()


if __name__ == "__main__":
    main()

