import telnetlib
import time
from docker_client import DockerClient
from tools import ip_to_subnet


def telnet_write_delay(client, cmd):
    client.write(cmd)
    time.sleep(0.01)


def configure_quagga(host: str, port: int, sub_nets: list, free_bits: list, timeout=100):
    telnet_client = telnetlib.Telnet(host, port, timeout)
    telnet_client.set_debuglevel(1)
    telnet_write_delay(telnet_client, b'enable\n')
    telnet_write_delay(telnet_client, b'configure terminal\n')
    telnet_write_delay(telnet_client, b'router ospf\n')
    for nets_index in range(len(sub_nets)):
        cmd = "network %s/%s area 0\n" % (sub_nets[nets_index], free_bits[nets_index])
        telnet_write_delay(telnet_client, bytes(cmd, encoding='utf8'))
    telnet_write_delay(telnet_client, b'end\n')
    telnet_write_delay(telnet_client, b'write\n')
    telnet_client.close()


if __name__ == "__main__":
    host_ip = "172.17.0.1"
    host_prefix_len = 16
    host_subnet = ip_to_subnet(host_ip, host_prefix_len)
    cli = DockerClient('realssd/quagga', host_ip)
    container_ids = []
    connection_info = [
        [0, 1], [0, 2], [0, 3], [0, 4],
        [1, 5], [2, 6], [3, 7], [4, 8],
        [5, 9], [5, 10], [6, 11], [6, 12], [7, 13], [7, 14], [8, 15], [8, 16],
        [7, 17], [17, 18], [17, 19],
    ]
    for i in range(20):
        container_id = cli.create_satellite(str(i), "3000")
        container_ids.append(container_id)
    print("容器ID为:")
    print(container_ids)
    index = 0
    for c in connection_info:
        net_id = cli.create_network(index)
        index += 1
        cli.connect_node(container_ids[c[0]], net_id)
        cli.connect_node(container_ids[c[1]], net_id)

    for container_id in container_ids:
        interfaces, free_bits = cli.get_container_interfaces(container_id)
        toDelete = -1.
        host_interface = ""
        for i in range(len(interfaces)):
            if ip_to_subnet(interfaces[i], host_prefix_len) == host_subnet:
                toDelete = i
                break
        if toDelete >= 0:
            host_interface = interfaces[toDelete]
            print("排除主机接口加入路由:%s, 对应的接口为:" % host_interface)
            del interfaces[toDelete]
            del free_bits[toDelete]
            print(interfaces)
        sub_nets = [ip_to_subnet(interfaces[i], free_bits[i]) for i in range(len(free_bits))]
        configure_quagga(host_interface, 2604, sub_nets, free_bits)
