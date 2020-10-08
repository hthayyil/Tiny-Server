# TinyWebServer

* C Web server that supports HTML, plain text, jpeg, gif, png, and PDF formats.
* 404 error code is returned when a requested file is not found.
* index.html pages are returned if exists in a directory, otherwise a list of files is returned.
* Handles concurrent client connections by the use of multithreading.
* Supports Common Gateway Interface (CGI) for serving dynamic content
