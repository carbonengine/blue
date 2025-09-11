import functools
import socket
import sys
import unittest

import blue
import stackless

LOCALHOST = "127.0.0.1"

def find_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind((LOCALHOST, 0))
        port = s.getsockname()[1]
        return port
    finally:
        s.close()

def run_on_server(channel, function):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.setmaxpacketsize(10 * 1024 * 1024)
        server.bind(("127.0.0.1", 0))
        channel.send(server.getsockname())
        server.listen(0)
        connection, address = server.accept()
        function(connection)
    finally:
        server.close()

def run_on_client(channel, function):
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        host_address = channel.receive()
        client.connect(host_address)
        client.setmaxpacketsize(10 * 1024 * 1024)
        client.setblockingsend(True)
        function(client)
    finally:
        client.close()

def send_packet(packet, oob_data, connection):
    if oob_data is not None:
        connection.sendpacket(packet, oob_data)
    else:
        connection.sendpacket(packet)

def receive_packet(connection):
    receive_packet.received = connection.recvpacketoob()
receive_packet.received = None

def send(data, connection):
    connection.send(data)

def receive(connection):
    receive.received = connection.recv(65535)
receive.received = None

def receive_bytes(byte_count, connection):
    receive_bytes.received = ""
    while len(receive_bytes.received) < byte_count:
        data = connection.recv(byte_count - len(receive_bytes.received))
        receive_bytes.received += data
receive_bytes.received = None

def run_connected(client_func, server_func):
    channel = stackless.channel()
    c = stackless.tasklet(run_on_client)(channel, client_func)
    s = stackless.tasklet(run_on_server)(channel, server_func)
    sleeptime = blue.os.sleeptime
    try:
        blue.os.sleeptime = 100
        while c.alive or s.alive:
            blue.os.Pump()
    finally:
        blue.os.sleeptime = sleeptime

class TestCarbonIO(unittest.TestCase):

    def setUp(self):
        try:
            import carbonio
        except ImportError:
            raise unittest.SkipTest("CarbonIO not supported on this system")
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

class TestStacklessIO(unittest.TestCase):

    def setUp(self):
        import stacklessio
        import _slsocket
        import slselect
        if hasattr(_slsocket, "use_carbonio"):
            _slsocket.use_carbonio(False)
        stacklessio._socket = _slsocket
        sys.modules["_socket"] = _slsocket
        sys.modules["select"] = slselect
        reload(socket) # Make sure the socket module is using _slsocket instead of _socket

    def test_sendpacket(self):
        PACKET_DATA = "data"
        OOB_DATA = None
        run_connected(functools.partial(send_packet, PACKET_DATA, OOB_DATA), receive_packet)
        self.assertEqual(receive_packet.received, (PACKET_DATA, None, 0))

    def test_sent_packet_format(self):
        PACKET_DATA = "data"
        OOB_DATA = None
        run_connected(functools.partial(send_packet, PACKET_DATA, OOB_DATA), functools.partial(receive_bytes, 8))
        self.assertEqual(receive_bytes.received, "\x00\x00\x00\x04data")

    def test_receive_formatted_packet(self):
        PACKET_DATA = "\x00\x00\x00\x04data"
        run_connected(functools.partial(send, PACKET_DATA), receive_packet)
        self.assertEqual(receive_packet.received, ("data", None, 0))
