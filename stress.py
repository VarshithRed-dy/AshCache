import socket
import threading

def worker(worker_id):
    s = socket.create_connection(("localhost", 6380))

    for i in range(1000):
        s.sendall(f"SET key {worker_id}-{i}\n".encode())
        s.recv(1024)

        s.sendall(b"GET key\n")
        s.recv(1024)

        s.sendall(b"DEL key\n")
        s.recv(1024)

    s.close()

threads = []

for i in range(50):
    t = threading.Thread(target=worker, args=(i,))
    t.start()
    threads.append(t)

for t in threads:
    t.join()

print("done")
