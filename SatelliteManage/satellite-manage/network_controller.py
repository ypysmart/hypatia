import logging
import os
import docker
from typing import List, Dict
from math import cos,sin,sqrt
from macro_defines import LATITUDE_KEY, LONGITUDE_KEY, HEIGHT_KEY, LIGHT_SPEED, host_prefix_len


def get_laser_delay_ms(position1:dict, position2:dict) -> int:
    lat1,lon1,hei1 = position1[LATITUDE_KEY],position1[LONGITUDE_KEY],position1[HEIGHT_KEY]
    lat2,lon2,hei2 = position2[LATITUDE_KEY],position2[LONGITUDE_KEY],position2[HEIGHT_KEY]
    x1,y1,z1 = hei1*cos(lat1)*cos(lon1),hei1*cos(lat1)*sin(lon1),hei1*sin(lat1)
    x2,y2,z2 = hei2*cos(lat2)*cos(lon2),hei2*cos(lat2)*sin(lon2),hei2*sin(lat2)
    dist_square = (x2-x1)**2 + (y2-y1)**2 + (z2-z1)**2
    return int(sqrt(dist_square) / LIGHT_SPEED)

def get_network_key(container_id1:str, container_id2:str) -> str:
    if container_id1 > container_id2:
        container_id1,container_id2 = container_id2,container_id1
    return container_id1+container_id2

class ContainerEntrypoint:
    def __init__(self, veth_name: str, container_id: str):
        self.veth_name = veth_name
        self.container_id = container_id

def get_bridge_interface_name(bridge_id: str) -> str:
    full_name = "br-" + bridge_id
    br_interfaces_str = os.popen('''ip l | grep -e "br-" | awk 'BEGIN{FS=": "}{print $2}' ''').read()  # popen与system可以执行指令,popen可以接受返回对象
    interface_list = br_interfaces_str.split('\n')[:-1]
    for interface_name in interface_list:
        if full_name.startswith(interface_name,0):
            return interface_name
    raise SystemError("Interface Not Found")

def get_vethes_of_bridge(interface_name:str) -> list:
    command = "ip l | " \
              "grep -e \"veth\" | " \
              "grep \"%s\" | " \
              "awk \'BEGIN{FS=\": \"}{print $2}\' | " \
              "awk \'BEGIN{FS=\"@\"}{print $1}\'" % interface_name
    veth_list_str = os.popen(command).read()
    veth_list = veth_list_str.split("\n")[:-1]
    return veth_list

class Network:

    def __init__(self, bridge_id: str, container_id1: str, container_id2: str):
        # 如果两个容器是相同的，跳过创建网络
        if container_id1 == container_id2:
            raise ValueError("Cannot connect the same container to itself")

        networks[get_network_key(container_id1, container_id2)] = self

        self.br_id = bridge_id
        self.br_interface_name = get_bridge_interface_name(bridge_id)
        self.veth_interface_list = get_vethes_of_bridge(self.br_interface_name)

        if len(self.veth_interface_list) != 2:
            logging.error(f"Invalid veth number: {len(self.veth_interface_list)}. Expected 2 veth interfaces.")
            raise ValueError("wrong veth number of bridge")

        self.veth_map = {
            container_id1: self.veth_interface_list[0],
            container_id2: self.veth_interface_list[1]
        }


    def set_delay(self,time: int):
        command = "tc qdisc add dev %s %s netem delay %dms"%(self.veth_interface_list[0],"root",time)
        exec_res = os.popen(command).read()
        logging.info(exec_res)
        command = "tc qdisc add dev %s %s netem delay %dms" % (self.veth_interface_list[1], "root", time)
        exec_res = os.popen(command).read()
        logging.info(exec_res)

    def unset_delay(self):
        command = "tc qdisc del dev %s %s " % (self.veth_interface_list[0], "root")
        exec_res = os.popen(command).read()
        logging.info(exec_res)
        command = "tc qdisc del dev %s %s " % (self.veth_interface_list[1], "root")
        exec_res = os.popen(command).read()
        logging.info(exec_res)

    def update_delay(self,time: int):
        command = "tc qdisc replace dev %s %s netem delay %dms" % (self.veth_interface_list[0], "root", time)
        exec_res = os.popen(command).read()
        logging.info(exec_res)
        command = "tc qdisc replace dev %s %s netem delay %dms" % (self.veth_interface_list[1], "root", time)
        exec_res = os.popen(command).read()
        logging.info(exec_res)


networks: Dict[str, Network] = {}
#节点到容器的映射
node_to_container_map = {}
def update_network_delay(position_data: dict, topo: dict):
    for start_node_id in topo.keys():
        conn_array = topo[start_node_id]
        for target_node_id in conn_array:
            try:
                delay = get_laser_delay_ms(position_data[start_node_id], position_data[target_node_id])
                
                # 检查映射中是否有这两个节点
                if start_node_id in node_to_container_map and target_node_id in node_to_container_map:
                    container_id1 = node_to_container_map[start_node_id]
                    container_id2 = node_to_container_map[target_node_id]
                    
                    # 使用容器ID查找网络
                    key = get_network_key(container_id1, container_id2)
                    if key in networks:
                        networks[key].update_delay(delay)
                        logging.info(f"更新网络 {key} 的延迟为 {delay}ms")
                    else:
                        logging.warning(f"找不到连接容器 {container_id1} 和 {container_id2} 的网络")
                else:
                    logging.warning(f"找不到节点 {start_node_id} 或 {target_node_id} 的容器ID映射")
            except Exception as e:
                logging.error(f"更新网络延迟出错: {e}")

if __name__ == '__main__':
    client = docker.from_env()
    container1 = client.containers.run("ubuntu:14.04", detach=True, command="tail -f /dev/null")
    container2 = client.containers.run("ubuntu:14.04", detach=True, command="tail -f /dev/null")
    network_info = client.networks.create("test_tc", driver='bridge')
    network_info.connect(container1)
    network_info.connect(container2)
    control = Network(network_info.id,container1,container2)
    control.set_delay(1000)
