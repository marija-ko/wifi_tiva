import socket
import signal
import sys 

serv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
port = input('Choose a port you would like to use. ')
serv.bind(('', int(port)))
serv.listen(5)
while True:
	conn, addr = serv.accept()
	from_client = ''
        while True:
            data = conn.recv(4096)
            if not data: break
            from_client = data
            print from_client
            from_server = raw_input('Type your message\n')
            conn.send(from_server)
        conn.close()
        print 'client disconnected'

def sigint_handler(signal, frame):
    print 'Interrupted'
    conn.close()
    sys.exit(0)
signal.signal(signal.SIGINT, sigint_handler)
