#!/usr/bin/env python3

import os
import sys

# Read POST data from stdin
post_data = ""
if os.environ.get("REQUEST_METHOD") == "POST":
    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", 0))
        if content_length > 0:
            post_data = sys.stdin.read(content_length)
    except (ValueError, KeyError):
        post_data = "Error reading POST data"

# Print HTTP headers
print("Content-Type: text/html")
print()

# Print HTML content
print("<html>")
print("<head><title>CGI Script Works!</title></head>")
print("<body>")
print("<h1>CGI Script Works!</h1>")
print("<p>REQUEST_METHOD: {}</p>".format(os.environ.get("REQUEST_METHOD", "")))
print("<p>QUERY_STRING: {}</p>".format(os.environ.get("QUERY_STRING", "")))
print("<p>CONTENT_LENGTH: {}</p>".format(os.environ.get("CONTENT_LENGTH", "")))
print("<p>SCRIPT_NAME: {}</p>".format(os.environ.get("SCRIPT_NAME", "")))
print("<p>POST_DATA: {}</p>".format(post_data))
print("</body>")
print("</html>")