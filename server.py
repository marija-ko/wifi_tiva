import socket
import signal
import sys 

serv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serv.bind(('', 3336))
serv.listen(5)
while True:
	conn, addr = serv.accept()
	from_client = ''
        while True:
            data = conn.recv(4096)
            if not data: break
            from_client = data
            print from_client
            conn.send("I am SERVER")
        conn.close()
        print 'client disconnected'

def sigint_handler(signal, frame):
    print 'Interrupted'
    conn.close()
    sys.exit(0)
signal.signal(signal.SIGINT, sigint_handler)
