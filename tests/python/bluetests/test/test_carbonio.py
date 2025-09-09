import functools
import socket
import sys
import unittest

import blue
import stackless

LOCALHOST = "127.0.0.1"

def find_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.bind((LOCALHOST, 0))
        port = s.getsockname()[1]
        return port
    finally:
        s.close()

def run_on_server(host_address, function):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.setmaxpacketsize(10 * 1024 * 1024)
        server.bind(host_address)
        server.listen(0)
        connection, address = server.accept()
        function(connection)
    finally:
        server.close()

def run_on_client(host_address, function):
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        client.connect(host_address)
        client.setmaxpacketsize(10 * 1024 * 1024)
        client.setblockingsend(True)
        function(client)
    finally:
        client.close()

def send_packet(packet, oob_data, connection):
    connection.sendpacket(packet, oob_data)

def receive_packet(connection):
    receive_packet.received = connection.recvpacketoob()
receive_packet.received = None

def send(data, connection):
    connection.send(data)

def receive(connection):
    receive.received = connection.recv(65535)
receive.received = None

def run_connected(client_func, server_func):
    port = find_port()
    address = (LOCALHOST, port)
    c = stackless.tasklet(run_on_client)(address, client_func)
    s = stackless.tasklet(run_on_server)(address, server_func)
    while c.alive or s.alive:
        blue.os.Pump()

class TestCarbonIO(unittest.TestCase):

    def setUp(self):
        import carbonio
        import _slsocket
        _slsocket.use_carbonio(True)
        carbonio._socket = _slsocket
        sys.modules["_socket"] = _slsocket
        sys.modules["select"] = None
        reload(socket) # Make sure the socket module is using _slsocket instead of _socket

    def test_sendpacket(self):
        PACKET_DATA = "data"
        OOB_DATA = None
        run_connected(functools.partial(send_packet, PACKET_DATA, OOB_DATA), receive_packet)
        self.assertEqual(receive_packet.received, (PACKET_DATA, "", 1))

    def test_sent_packet_format(self):
        PACKET_DATA = "data"
        OOB_DATA = None
        run_connected(functools.partial(send_packet, PACKET_DATA, OOB_DATA), receive)
        self.assertEqual(receive.received, "\x00\x00\x00\x04data")


    def test_receive_formatted_packet(self):
        PACKET_DATA = "\x00\x00\x00\x04data"
        run_connected(functools.partial(send, PACKET_DATA), receive_packet)
        self.assertEqual(receive_packet.received, ("data", "", 1))

    def test_sent_packet_format_with_oob_data(self):
        PACKET_DATA = "Hello"
        OOB_DATA = "World"
        run_connected(functools.partial(send_packet, PACKET_DATA, OOB_DATA), receive)
        self.assertEqual(receive.received, "\x10\x00\x00\x0E\x00\x00\x00\x05WorldHello")

    def test_receive_formatted_packet_with_oob_data(self):
        PACKET_DATA = "\x10\x00\x00\x0E\x00\x00\x00\x05WorldHello"
        run_connected(functools.partial(send, PACKET_DATA), receive_packet)
        self.assertEqual(receive_packet.received, ("Hello", "World", 1))
