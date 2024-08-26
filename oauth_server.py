import time
import secrets
import ssl
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse as urlparse

class OAuthHandler(BaseHTTPRequestHandler):
    auth_codes = {}
    state_tokens = {}

    @classmethod
    def add_auth_code(cls, code):
        cls.auth_codes[code] = time.time()

    @classmethod
    def validate_auth_code(cls, code):
        if code in cls.auth_codes:
            if time.time() - cls.auth_codes[code] < 300:  # 5分間有効
                del cls.auth_codes[code]  # 使用後に削除
                return True
        return False

    @classmethod
    def generate_state_token(cls):
        token = secrets.token_urlsafe()
        cls.state_tokens[token] = time.time()
        return token

    @classmethod
    def validate_state_token(cls, token):
        if token in cls.state_tokens:
            if time.time() - cls.state_tokens[token] < 600:  # 10分間有効
                del cls.state_tokens[token]
                return True
        return False

    def do_GET(self):
        parsed_path = urlparse.urlparse(self.path)
        query = urlparse.parse_qs(parsed_path.query)

        if parsed_path.path == '/':
            code = query.get('code')
            state = query.get('state', [None])[0]
            
            if code and state and self.validate_state_token(state):
                self.add_auth_code(code[0])
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                self.wfile.write(b"Authentication successful. You can close this window.")
                print(f"Authorization code received and stored securely.")
            else:
                self.send_response(400)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                self.wfile.write(b"Authentication failed. Invalid request.")
        
        elif parsed_path.path == '/get_code':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            for code in self.auth_codes:
                if self.validate_auth_code(code):
                    self.wfile.write(code.encode())
                    return
            self.wfile.write(b"No valid auth code available")

        elif parsed_path.path == '/get_state':
            state_token = self.generate_state_token()
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(state_token.encode())

    def log_message(self, format, *args):
        # Override to prevent logging sensitive information
        pass

def run_server(server_class=HTTPServer, handler_class=OAuthHandler, port=8080):
    server_address = ('', port)
    
    # SSL Context setup
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain('cert.pem', 'key.pem')  # Replace with actual paths

    httpd = server_class(server_address, handler_class)
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
    
    print(f'Starting secure HTTPS server on port {port}...')
    httpd.serve_forever()

if __name__ == '__main__':
    # サーバーを別スレッドで起動
    server_thread = threading.Thread(target=run_server)
    server_thread.start()

    print("Secure local server is running. Proceed with the OAuth flow in your C program.")
    print("Press Ctrl+C to stop the server when you're done.")

    try:
        # メインスレッドを実行し続ける
        server_thread.join()
    except KeyboardInterrupt:
        print("Stopping the server...")