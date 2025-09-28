#!/usr/bin/python3
"""
TCP Server for the Pronto Script test cases.

Note that this server uses the insecure Python 'http' module.

This file is part of the ProntoScript replication distribution
(https://github.com/stefan-sinnige/prontoscript)

Copyright (C) 2025, Stefan Sinnige <stefan@kalion.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
"""

import http.server
import socketserver
import json

PORT = 52001

# The fixed mapping of request to return message
request_map = [
    {
      "path": "/json-1",
      "value": json.dumps(
        [
          {"artist": "Fleetwood Mac", "title": "Dreams"}, 
          {"artist": "Led Zepelin", "title": "Communication Breakdown"}
        ]),
      "description": "Simple JSON request"
    }
]

class Handler(http.server.SimpleHTTPRequestHandler):
    """
    The hander for HTTP requests. This is a fixed request - response server.
    """
    protocol_version = "HTTP/1.0"
    def do_GET(self):
        mapping = next(m for m in request_map if m["path"] == self.path)
        if mapping:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(mapping["value"].encode("utf-8"))
        else:
            print(f"No mapping for {self.path}")
            self.send_response(500)

# Start the server
print(f"Starting HTTP server on http://localhost:{PORT}")
print(f"Supported request queries:")
for mapping in request_map:
    print(f"    {mapping['path']:10}  {mapping['description']:50}")
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("", PORT), Handler) as httpd:
    httpd.serve_forever()

# vi: ai ts=4 expandtab:
