from http import server

class ReqHandler(server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")

        server.SimpleHTTPRequestHandler.end_headers(self)

server.test(HandlerClass=ReqHandler)