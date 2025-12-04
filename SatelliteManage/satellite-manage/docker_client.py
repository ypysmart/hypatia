import logging
from config_object import Config
import docker
from subnet_allocator import SubnetAllocator, ip2str
import time

class DockerClient:

    def __init__(self, image_name, host_ip, config: Config):
        self.client = docker.from_env()
        self.image_name = image_name
        self.allocator = SubnetAllocator(29)
        self.host_ip = host_ip
        self.config = config

    def create_satellite(self, node_id: str, port: str) -> str:
        container_info = self.client.containers.run(self.image_name, detach=True, environment=[
            'NODE_ID=' + node_id,
            'HOST_IP=' + self.host_ip,
            'BROAD_PORT=' + port,
            'THRESHOLD=' + str(5.0)
        ], labels={
            "com.docker-tc.enabled": "1",
            "com.docker-tc.limit": self.config.Limit,
            "com.docker-tc.delay": self.config.Delay,
            "com.docker-tc.loss": self.config.Loss,
            "com.docker-tc.corrupt": self.config.Corrupt
        }, cap_add=['NET_ADMIN'], name=node_id)
        return container_info.id

    def stop_satellite(self, container_id: str):
        self.client.containers.get(container_id).stop()

    def rm_satellite(self, container_id: str):
        self.client.containers.get(container_id).remove()

    def connect_node(self, container_id: str, network_id: str, alias_name: str):
        try:
            container_object = self.client.containers.get(container_id)
            network = self.client.networks.get(network_id)
        
            # 打印更多信息
            logging.info(f"连接前的网络状态：{network.attrs['Containers']}")
        
            # 检查容器是否已经连接到网络
            if container_id not in network.attrs['Containers']:
                logging.info(f"将容器 {container_id} 连接到网络 {network_id} 中")
                self.client.networks.get(network_id).connect(container_object, aliases=[alias_name])
            
                # 等待网络配置生效
                time.sleep(1)
            
                # 重新获取网络信息并验证
                network = self.client.networks.get(network_id)
                if container_id in network.attrs['Containers']:
                    logging.info(f"容器 {container_id} 成功连接到网络 {network_id}")
                else:
                    logging.error(f"容器 {container_id} 未能成功连接到网络 {network_id}")
            else:
                logging.info(f"容器 {container_id} 已经连接到网络 {network_id}")
            
            # 检查接口
            interfaces, prefix_len = self.get_container_interfaces(container_id)
            logging.info(f"容器 {container_id} 的接口: {interfaces}, 前缀长度: {prefix_len}")
        
        except Exception as e:
            logging.error(f"连接容器到网络时出错: {e}")
            raise

    def disconnect_node(self, container_id: str, network_id: str):
        container_object = self.client.containers.get(container_id)
        self.client.networks.get(network_id).disconnect(container_object)

    def pull_image(self):
        image = self.client.pull(self.image_name)
        return image.id

    def create_network(self, network_index: int):
        net = None
        for i in range(100):
            try:
                subnet_ip = self.allocator.alloc_local_subnet()
                subnet_ip = (subnet_ip // 8) * 8  # 确保子网地址是合法的
                subnet_ip_str = ip2str(subnet_ip)
                gateway_str = ip2str(subnet_ip + 1)
                ipam_pool = docker.types.IPAMPool(subnet='%s/29' % subnet_ip_str, gateway=gateway_str)
                ipam_config = docker.types.IPAMConfig(pool_configs=[ipam_pool])
                network_name = f"Network_{network_index}"
            
                # 如果网络已经存在，跳过创建
                existing_networks = self.client.networks.list(names=[network_name])
                if existing_networks:
                    logging.info(f"Network {network_name} already exists, skipping creation.")
                    return existing_networks[0].id
            
                net = self.client.networks.create(network_name, driver='bridge', ipam=ipam_config)
                break
            except Exception as e:
                logging.error(f"Error during network creation attempt {i+1}: {e}")
                logging.info(f"Retrying network creation...")
    
        if net is None:
            raise RuntimeError("Failed to create network after 100 retries.")
    
        logging.info(f"Network created with ID: {net.id}")
        return net.id


    def get_container_interfaces(self, container_id: str):
        try:
            ans = []
            free_bit = []
            container = self.client.containers.get(container_id)
        
            # 方法1：使用Docker API
            nets = container.attrs["NetworkSettings"]["Networks"]
            logging.info(f"容器 {container_id} 的网络信息: {nets}")
        
            for net_name in nets.keys():
                if "IPAddress" in nets[net_name] and nets[net_name]["IPAddress"]:
                    ans.append(nets[net_name]["IPAddress"])
                    free_bit.append(int(nets[net_name]["IPPrefixLen"]))
        
            # 方法2：在容器内执行命令获取接口信息
            if not ans:
                logging.warning(f"通过Docker API无法获取容器 {container_id} 的接口，尝试在容器内执行命令")
                try:
                    cmd_result = container.exec_run("ip addr | grep 'inet ' | awk '{print $2}'")
                    if cmd_result.exit_code == 0:
                        interfaces = cmd_result.output.decode().strip().split('\n')
                        for intf in interfaces:
                            if '/' in intf:  # 格式如 "192.168.1.1/24"
                                ip, prefix = intf.split('/')
                                if ip and ip != "127.0.0.1":
                                    ans.append(ip)
                                    free_bit.append(int(prefix))
                                    logging.info(f"从容器内获取到接口: {ip}/{prefix}")
                except Exception as e:
                    logging.error(f"在容器内执行命令时出错: {e}")
                
            if not ans:
                logging.warning(f"容器 {container_id} 没有检测到任何网络接口")
            
            return ans, free_bit
        except Exception as e:
            logging.error(f"获取容器接口时出错: {e}")
            return [], []


if __name__ == '__main__':
    cli = DockerClient('aaa', 'bbb')
    resp = cli.get_container_interfaces("6ad80cb5be8ba205027c157814b0f47eda11c2a635862fba367316df8a2720d0")
    print(resp)
