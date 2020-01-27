import socket
import os
import time

################################################
## change firmware and port to yours
firmware_name = 'hello-world.bin'
server_ip = 8070
################################################


ip_port = ('0.0.0.0', server_ip)

file_size = os.path.getsize(firmware_name)

# open binary file
reopen = True;
while reopen:
	try:
		fd = open(firmware_name, 'rb')
		reopen = False
	except:
		print 'open file failed'
		time.sleep(1)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind(ip_port)
sock.listen(5)

last_package = None
send_count = 0

while True:	
	print 'wait for client'
	conn, addr = sock.accept()
	print addr
	while True:
		client_data = conn.recv(1024)
		if client_data == "req0\r\n\r\n":
			send_fail_retry = True
			is_over = False
			while send_fail_retry:
				try:
					dat = fd.read(512)
					send_data = 'total: ' + str(file_size) + '\r\nlength: ' + str(len(dat)) + '\r\n\r\n' + dat
					last_package = send_data
					conn.send(send_data)
					send_fail_retry = False
					send_count += len(dat)
					print ('send progress: %%%d' %(send_count*100/file_size))
					if send_count >= file_size:
						print 'file send end'
						last_package = None
						send_count = 0
						is_over = True
				except:
					print 'read file error, try again later'
					time.sleep(1)
			if is_over:
				fd.seek(0)
				break
		elif client_data == "req1\r\n\r\n":
			retry = True
			while retry:
				try:
					conn.send(last_package)
					retry = False
					print 'resend OK'
				except:
					print 'resend data failed'
					time.sleep(1)

print 'something went wrong'