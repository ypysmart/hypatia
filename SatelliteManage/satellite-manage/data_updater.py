from socket import socket,SOL_SOCKET,SO_BROADCAST,AF_INET,SOCK_DGRAM


class DataUpdater:

    def __init__(self, host: str, eth_ip: str, port: int):
        self.addr = (host, port)
        self.udp_cli_sock = socket(AF_INET, SOCK_DGRAM)
        self.udp_cli_sock.bind((eth_ip, port))
        self.udp_cli_sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

    def broadcast_position(self, json_data: str):
        self.udp_cli_sock.sendto(json_data.encode(), self.addr)
