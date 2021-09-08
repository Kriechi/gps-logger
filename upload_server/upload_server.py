#!/usr/bin/env python3

import os
import sys
import http.server
import ssl
import datetime
import uuid
import csv
import configparser

import gpxpy
import gpxpy.gpx


LISTEN_HOST = None
LISTEN_PORT = None
CERT_FILE = None
KEY_FILE = None
HTTP_PATH = None
MAGIC_HEADER = None
MAGIC_HEADER_VALUE = None
OUTPUT_PATH = None

class UploadServerRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.response(404)

    def do_PUT(self):
        self.response(404)

    def do_POST(self):
        print(f"Receiving POST from {self.client_address} ...")
        if MAGIC_HEADER and MAGIC_HEADER_VALUE:
            if self.headers.get(MAGIC_HEADER, None) != MAGIC_HEADER_VALUE:
                print("Failed magic header check.")
                self.response(403)
                return
        if self.path != HTTP_PATH:
            print(f"Wrong path: {self.path}")
            self.response(403)
            return

        expected = int(self.headers["Content-Length"])
        if expected > 4 * 1024 * 1024:
            print(f"Unexpectedly large file size - rejecting to prevent disk fill up.")
            self.response(403)
            return
        print(f"Expecting {expected} bytes of GPS data...")

        os.makedirs(OUTPUT_PATH, exist_ok=True)

        copied_bytes = 0
        now = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H%M%SZ%z")
        u = str(uuid.uuid4())
        filename = os.path.join(OUTPUT_PATH, f"gps-logger-{now}-{u}.csv")
        with open(filename, "wb") as f:
            while copied_bytes < expected:
                d = self.rfile.read(min(4096, expected - copied_bytes))
                if not d:
                    break
                written = f.write(d)
                if written != len(d):
                    print("Error writing to file!")
                    self.response(403)
                    return
                copied_bytes += len(d)

        received = os.path.getsize(filename)
        if received != expected:
            print(f"Size mismatch: expected {expected}, received {received} bytes.")
            return

        self.send_response_only(200)
        self.end_headers()

        try:
            convert_to_gpx(filename)
        except Exception as e:
            print("ERROR:", e)

        print(
            f"Received {filename} from {self.address_string()} with {received} bytes."
        )


def convert_to_gpx(filename):
    # GPX file format specification: https://www.topografix.com/gpx_manual.asp
    gpx = gpxpy.gpx.GPX()
    track = gpxpy.gpx.GPXTrack()
    gpx.tracks.append(track)
    segment = None
    last_timestamp = None
    count = 0

    with open(filename) as f:
        reader = csv.reader(f, delimiter=";")
        next(reader)  # skip column headers
        for row in reader:
            try:
                lat, lon, elevation, speed, num_satellites = row[1:]
            except:
                # line might be incomplete when writing happened during power-down (can occur multiple times per file)
                print(f"Skipping broken data row: {row}")
                continue

            timestamp = datetime.datetime.strptime(row[0], "%Y-%m-%dT%H:%M:%SZ")
            if not last_timestamp:
                last_timestamp = timestamp
            if not segment or (
                timestamp - last_timestamp > datetime.timedelta(minutes=5)
            ):
                segment = gpxpy.gpx.GPXTrackSegment()
                track.segments.append(segment)
            last_timestamp = timestamp
            point = gpxpy.gpx.GPXTrackPoint(
                lat, lon, elevation=elevation, speed=speed, time=timestamp
            )
            point.satellites = num_satellites
            segment.points.append(point)
            count += 1

    output_filename = os.path.join(
        OUTPUT_PATH,
        os.path.splitext(os.path.basename(filename))[0] + ".gpx",
    )
    with open(output_filename, "w") as f:
        f.write(gpx.to_xml(version="1.0"))

    print(f"Converted file {filename} to {output_filename} with {count} points.")


def run():
    httpd = http.server.HTTPServer(
        (LISTEN_HOST, LISTEN_PORT), UploadServerRequestHandler
    )
    use_tls = CERT_FILE and KEY_FILE
    if use_tls:
        if not os.path.exists(CERT_FILE):
            print(f"cert_file not found: {CERT_FILE}", file=sys.stderr)
        if not os.path.exists(KEY_FILE):
            print(f"key_file not found: {KEY_FILE}", file=sys.stderr)
        httpd.socket = ssl.wrap_socket(
            httpd.socket,
            certfile=CERT_FILE,
            keyfile=KEY_FILE,
            server_side=True,
        )
    print(
        f"Listening on {'https' if use_tls else 'http'}://{httpd.server_name}:{httpd.server_port} ..."
    )
    httpd.serve_forever()


if __name__ == "__main__":
    SECTION_NAME = "gps-logger-upload-server"

    config = configparser.ConfigParser()
    config.read(["config/config.ini", "config.ini"])
    if SECTION_NAME not in config.sections():
        print("config not found", file=sys.stderr)
        sys.exit(1)

    LISTEN_HOST = config[SECTION_NAME].get("listen_host", "")
    LISTEN_PORT = config[SECTION_NAME].getint("listen_port") or 8472
    CERT_FILE = config[SECTION_NAME].get("cert_file")
    KEY_FILE = config[SECTION_NAME].get("key_file")
    HTTP_PATH = config[SECTION_NAME].get("http_path", "/upload")
    MAGIC_HEADER = config[SECTION_NAME].get("magic_header", "")
    MAGIC_HEADER_VALUE = config[SECTION_NAME].get("magic_header_value", "")
    OUTPUT_PATH = config[SECTION_NAME].get("output_path", "output")

    config = {
        "LISTEN_HOST": LISTEN_HOST,
        "LISTEN_PORT": LISTEN_PORT,
        "CERT_FILE": CERT_FILE,
        "KEY_FILE": KEY_FILE,
        "HTTP_PATH": HTTP_PATH,
        "MAGIC_HEADER": MAGIC_HEADER,
        "MAGIC_HEADER_VALUE": MAGIC_HEADER_VALUE,
        "OUTPUT_PATH": OUTPUT_PATH,
    }
    for key, value in config.items():
        print(f"Configuration {key}: {value}")

    run()
