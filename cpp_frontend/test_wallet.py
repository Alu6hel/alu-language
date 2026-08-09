import urllib.request
import socket

def test_wallet():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8080))
    # We send nothing, to avoid RST when the server closes without reading!
    # Wait, the server expects us to connect. As soon as we connect, it sends data and closes.
    
    data = b""
    while True:
        try:
            chunk = s.recv(1024)
            if not chunk:
                break
            data += chunk
        except Exception as e:
            print("Exception during recv:", e)
            break
            
    print("Received:")
    print(data.decode('utf-8'))

if __name__ == '__main__':
    test_wallet()
