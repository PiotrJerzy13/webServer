#!/usr/bin/env python3
import cgi
import os

form = cgi.FieldStorage()

print("Content-Type: text/html")
print()

print("<html>")
print("<head><title>CGI POST Data</title></head>")
print("<body>")
print("<h1>CGI Script Works!</h1>")

print(f"<p>Request Method: {os.environ.get('REQUEST_METHOD', 'Unknown')}</p>")

print("<h2>POST Data Received:</h2>")
if len(form) > 0:
    print("<ul>")
    for field in form:
        print(f"<li><strong>{field}</strong>: {form[field].value}</li>")
    print("</ul>")
else:
    print("<p>No POST data received</p>")

print("</body>")
print("</html>")