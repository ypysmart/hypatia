import logging
import subprocess
import time
from subnet_allocator import SubnetAllocator, ip2str
from config_object import Config

class NamespaceClient:
    def __init__(self, host_ip, config: Config):
        self.allocator = SubnetAllocator(29)
        self.host_ip = host_ip
        self.config = config
        self.quagga_env = {  # Quagga环境变量
            "THRESHOLD": str(5.0)
        }

    def _run_cmd(self, cmd, check=True):
        """运行shell命令"""
        logging.info(f"Executing: {cmd}")
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        if check and result.returncode != 0:
            logging.error(f"Command failed: {result.stderr}")
            raise RuntimeError(f"Command '{cmd}' failed with code {result.returncode}")
        return result.stdout.strip()

    def create_satellite(self, node_id: str, port: str) -> str:
        ns_name = node_id  # netns名称即node_id
        # 创建netns
        self._run_cmd(f"ip netns add {ns_name}")
        # 在netns中设置loopback
        self._run_cmd(f"ip netns exec {ns_name} ip link set lo up")
        # 启动Quagga (zebra + ospfd)
        # 先创建Quagga配置文件（简单配置，vtysh端口2604 for ospfd）
        conf_dir = self.config.QuaggaConfDir
        zebra_conf = f"{conf_dir}/zebra-{node_id}.conf"
        ospfd_conf = f"{conf_dir}/ospfd-{node_id}.conf"
        with open(zebra_conf, 'w') as f:
            f.write("hostname zebra\npassword zebra\nenable password zebra\n")  # 基本配置
        with open(ospfd_conf, 'w') as f:
            f.write("hostname ospfd\npassword zebra\nenable password zebra\nrouter ospf\n")  # 基本OSPF
        # 启动zebra和ospfd在netns中
        self._run_cmd(f"ip netns exec {ns_name} {self.config.QuaggaZebraPath} -d -f {zebra_conf} -i /var/run/quagga/zebra-{node_id}.pid")
        self._run_cmd(f"ip netns exec {ns_name} {self.config.QuaggaOspfdPath} -d -f {ospfd_conf} -i /var/run/quagga/ospfd-{node_id}.pid")
        time.sleep(1)  # 等待启动
        return ns_name  # 返回netns名称作为"container_id"

    def stop_satellite(self, ns_name: str):
        # 停止Quagga
        self._run_cmd(f"ip netns exec {ns_name} killall zebra ospfd", check=False)

    def rm_satellite(self, ns_name: str):
        self.stop_satellite(ns_name)
        self._run_cmd(f"ip netns del {ns_name}")

    def create_network(self, network_index: int):
        subnet_ip = self.allocator.alloc_local_subnet()
        subnet_ip = (subnet_ip // 8) * 8
        subnet_ip_str = ip2str(subnet_ip)
        network_name = f"Network_{network_index}"
        return network_name, subnet_ip_str  # 返回网络名和子网

    def connect_node(self, ns_name1: str, ns_name2: str, network_name: str, subnet_ip_str: str):
        # 创建veth pair
        veth1 = f"veth-{ns_name1}-{network_name}"
        veth2 = f"veth-{ns_name2}-{network_name}"
        self._run_cmd(f"ip link add {veth1} type veth peer name {veth2}")
        # 移动到netns
        self._run_cmd(f"ip link set {veth1} netns {ns_name1}")
        self._run_cmd(f"ip link set {veth2} netns {ns_name2}")
        # 设置IP (简单分配: .2 和 .3)
        self._run_cmd(f"ip netns exec {ns_name1} ip addr add {subnet_ip_str}.2/29 dev {veth1}")
        self._run_cmd(f"ip netns exec {ns_name2} ip addr add {subnet_ip_str}.3/29 dev {veth2}")
        # 启用接口
        self._run_cmd(f"ip netns exec {ns_name1} ip link set {veth1} up")
        self._run_cmd(f"ip netns exec {ns_name2} ip link set {veth2} up")
        # 应用Traffic Control (延迟等)
        self._apply_tc(veth1, ns_name1)
        self._apply_tc(veth2, ns_name2)

    def _apply_tc(self, interface: str, ns_name: str):
        delay = self.config.Delay
        loss = self.config.Loss
        limit = self.config.Limit
        corrupt = self.config.Corrupt
        cmd = f"ip netns exec {ns_name} tc qdisc add dev {interface} root netem delay {delay} loss {loss} rate {limit} corrupt {corrupt}"
        self._run_cmd(cmd)

    def get_container_interfaces(self, ns_name: str):
        cmd = f"ip netns exec {ns_name} ip addr show | grep 'inet ' | awk '{{print $2}}'"
        output = self._run_cmd(cmd)
        interfaces = []
        prefix_len = []
        for line in output.split('\n'):
            if line:
                ip, plen = line.split('/')
                interfaces.append(ip)
                prefix_len.append(int(plen))
        return interfaces, prefix_len