# 🌐 Webserv

> "This is when you finally understand why a URL starts with HTTP."

## Overview

`Webserv` is a lightweight HTTP server written in **C++17**, built from scratch as part of the 42 curriculum. It supports essential HTTP features such as GET, POST, and DELETE methods, file uploads, CGI execution, and custom configuration via a config file.

This project was an in-depth journey into the internals of HTTP, non-blocking I/O, and socket programming, with a focus on resilience and protocol compliance.

---

## ✨ Features

- ✅ **Supports HTTP 1.1**
- ✅ Handles **GET**, **POST**, and **DELETE** requests
- ✅ **Non-blocking** server with `poll()` (or equivalent)
- ✅ Configurable via NGINX-style configuration files
- ✅ Multiple **virtual servers and ports**
- ✅ Static file hosting
- ✅ Directory listing (optional)
- ✅ **File upload** support via POST
- ✅ **CGI execution** (e.g., Python or PHP scripts)
- ✅ Custom and default error pages

---

## 🖼️ Interface Preview

Here’s how the interactive web interface looks in the browser:

<img width="857" alt="interface" src="https://github.com/user-attachments/assets/f29cc616-4fce-46da-ace5-aa6de60588d2" />

---

## ⚙️ Usage

### 1. Compile the Server

```bash
make

2. Run the Server
bash

./webserv [path_to_config_file]
If no config path is provided, it will use a default configuration.

Here's a minimal example of a config file:

```conf
server {
    listen 127.0.0.1:8080;
    server_name localhost;

    root ./www;
    index index.html;

    error_page 404 ./errors/404.html;

    location /upload {
        methods POST;
        upload_path ./uploads;
    }

    location /cgi-bin {
        cgi_extension .py;
        cgi_path /usr/bin/python3;
    }
}
```

---

## ✅ Requirements Covered

- ✅ No external libraries  
- ✅ Single `poll()` or equivalent for all I/O  
- ✅ No `fork()` outside of CGI  
- ✅ Proper HTTP response codes  
- ✅ Configurable server behavior  
- ✅ Works with modern browsers  
- ✅ Handles stress testing gracefully
```
