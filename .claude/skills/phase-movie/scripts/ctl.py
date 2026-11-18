"""Send JSON requests to the dm2 control socket, one per argument; print each reply."""
import socket
import sys
import time

sock_path = sys.argv[1]
for _ in range(100):
    try:
        s = socket.socket(socket.AF_UNIX)
        s.connect(sock_path)
        break
    except OSError:
        time.sleep(0.1)
f = s.makefile('rwb')
for arg in sys.argv[2:]:
    f.write(arg.encode() + b'\n')
    f.flush()
    print(f.readline().decode().strip(), flush=True)
