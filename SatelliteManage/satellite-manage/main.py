import logging
import time
import json
import os.path
from collections import OrderedDict
import multiprocessing as mp
from docker_client import DockerClient
from data_updater import DataUpdater
from config_object import Config
from satellite_node import SatelliteNode, worker,satellites
from satellite_config_parser import SatelliteConfigParser
from quagga_operator import configure_quagga
from tools import ip_to_subnet
from tle_generator import generate_tle
from configure_services import set_monitor, init_monitor
from network_controller import Network,networks,update_network_delay
from macro_defines import LATITUDE_KEY, LONGITUDE_KEY, HEIGHT_KEY, host_prefix_len
from network_controller import node_to_container_map

if __name__ == "__main__":
    
    # 获取当前的项目路径
    file_dir = os.path.abspath('.')
    # 获取当前的配置文件路径
    file_path = file_dir + '/config.ini'
    config = Config(file_path)
    # 读取当前项目的部分配置参数
    satellite_config_path = config.SatelliteConfigPath
    host_ip = config.DockerHostIP
    image_name = config.DockerImageName
    udp_port = config.UDPPort

    # 读取卫星节点与拓扑配置文集
    satellite_config = SatelliteConfigParser(satellite_config_path)
    # 初始化位置信息更新模块
    updater = DataUpdater("<broadcast>", host_ip, int(udp_port))
    # 初始化Docker客户端
    docker_client = DockerClient(image_name, host_ip, config)
    # 生成卫星节点信息与拓扑结构
    satellite_infos, connections = generate_tle(3, 3, 0, 0, 5, 0.06)
    satellite_num = len(satellite_infos)
    # 获取主机子网段
    host_subnet = ip_to_subnet(host_ip, host_prefix_len)
    # 初始化网络编号、连接顺序、卫星数组、位置信息数组、卫星容器映射表、子网映射表
    network_index = 0 # 网络编号，用于创建网络时计数
    connect_order_map = {} # 连接顺序表，记录拓扑连接的顺序
    position_datas = {} # 位置信息，记录卫星的经纬度、高度等位置信息列表
    satellite_map = {} # 卫星表,做node_id到Satellite实例的映射
    subnet_map = OrderedDict() # 子网，有序列表，顺序为节点加入子网的顺序
    node_to_container_map = {}  # 节点ID到容器ID的映射
    # 初始化Monitor
    try:
        print("正在启动Monitor服务...")
        init_monitor(config.MonitorImageName, docker_client, int(udp_port))
        print("Monitor服务启动成功")
    except Exception as e:
        logging.error(f"启动Monitor服务失败: {e}")
        #init_monitor(config.MonitorImageName, docker_client, int(udp_port))

    # 创建容器
    for i in range(satellite_num):
        node_id = "node_" + str(i)
        container_id = docker_client.create_satellite(
            node_id,
            udp_port
        )
        satellites.append(
            SatelliteNode((
                satellite_infos[i][0],
                satellite_infos[i][1],
                satellite_infos[i][2],
            ), node_id, container_id)
        )
        satellite_map[node_id] = satellites[-1]
        #node_to_container_map[node_id] = container_id  # 添加映射
        position_datas[node_id] = {
            LATITUDE_KEY: 0.0,
            LONGITUDE_KEY: 0.0,
            HEIGHT_KEY: 0.0
        }
        node_to_container_map[node_id] = container_id
    # 链接容器
    for conn_index in connections.keys():
        for network_node_id in connections[conn_index]:
            # 跳过自连接
            if int(conn_index) == network_node_id:
                print(f"跳过自连接: {conn_index} -> {network_node_id}")
                continue
            
            # 创建网络
            net_id = str(docker_client.create_network(network_index))
            network_index += 1
        
            # 获取容器ID和节点ID
            container_id1 = satellite_map[satellites[int(conn_index)].node_id].container_id
            node_id1 = satellites[int(conn_index)].node_id
            container_id2 = satellite_map[satellites[network_node_id].node_id].container_id
            node_id2 = satellites[network_node_id].node_id
            # 再次检查容器ID
            if container_id1 == container_id2:
                print(f"跳过相同容器ID: {container_id1}")
                continue
            
            # 再次检查节点ID
            if node_id1 == node_id2:
                print(f"跳过相同节点ID: {node_id1}")
                continue

            # 连接容器到网络
            print(f"连接 {node_id1} ({container_id1}) 和 {node_id2} ({container_id2})")
            docker_client.connect_node(container_id1, net_id, node_id1)
        
            # 等待第一个连接生效
            time.sleep(1)
        
            docker_client.connect_node(container_id2, net_id, node_id2)
        
            # 等待第二个连接生效
            time.sleep(1)
        
            # 验证连接是否成功
            interfaces1, _ = docker_client.get_container_interfaces(container_id1)
            interfaces2, _ = docker_client.get_container_interfaces(container_id2)
            print(f"{node_id1} 接口: {interfaces1}")
            print(f"{node_id2} 接口: {interfaces2}")
        
            # 只有在检测到有效接口后才继续
            if len(interfaces1) > 1 and len(interfaces2) > 1:
                # 记录连接顺序
                if satellites[int(conn_index)].node_id in connect_order_map.keys():
                    connect_order_map[satellites[int(conn_index)].node_id].append(satellites[network_node_id].node_id)
                else:
                    connect_order_map[satellites[int(conn_index)].node_id] = [satellites[network_node_id].node_id]
                
                print(f"成功连接 {node_id1} 和 {node_id2}")
                networks[net_id] = Network(net_id, container_id1, container_id2)
            else:
                print(f"警告: {node_id1} 和 {node_id2} 的连接可能不成功，接口数量不足")
            
    # 配置路由
    for container_id_key in satellite_map.keys():
        container_id = satellite_map[container_id_key].container_id
        # 获取容器接口
        interfaces, prefix_len = docker_client.get_container_interfaces(container_id)
        toDelete = -1.
        host_interface = ""

        for i in range(len(interfaces)):
            if ip_to_subnet(interfaces[i], host_prefix_len) == host_subnet:
                toDelete = i
                break

        # 排除主机接口
        if toDelete >= 0:
            satellite_map[container_id_key].host_ip = interfaces[int(toDelete)]
            host_interface = interfaces[int(toDelete)]
            print("配置%s,排除主机接口加入路由:%s, 对应的接口为:" % (container_id_key, host_interface))
            del interfaces[int(toDelete)]
            del prefix_len[int(toDelete)]
            print(interfaces)
        sub_nets = [ip_to_subnet(interfaces[i], prefix_len[i]) for i in range(len(prefix_len))]

        # 调用方法配置容器Quagga
        while True:
            try:
                configure_quagga(host_interface, 2604, sub_nets, prefix_len)
                break
            except Exception as e:
                time.sleep(1)
                logging.warning("Configure Quagga Fail, Retry...")

        # 获取子网信息
        # 根据连接创建的顺序,获取到的前半部分网络有该卫星控制
        sub_start_index = 0
        for sub_index in range(len(sub_nets)):
            sub = sub_nets[sub_index]
            satellite_map[container_id_key].subnet_ip[sub] = interfaces[sub_index]

            if sub in subnet_map.keys():
                subnet_map[sub].append(satellite_map[container_id_key])
            else:
                subnet_map[sub] = [satellite_map[container_id_key]]

        # 由于在节点中同样需要以下标判断卫星控制关系
        # 需要在初始化数据中，维持这一顺序
        # 在创建和连接容器之后，应该有代码来填充每个节点的connections数组
    print("subnet_map keys:", subnet_map.keys())
    print("connect_order_map:", connect_order_map)
    for k in subnet_map.keys():
        if subnet_map[k][1].node_id in connect_order_map.keys() \
            and subnet_map[k][0].node_id in connect_order_map[subnet_map[k][1].node_id]:
            temp = subnet_map[k][0]
            subnet_map[k][0] = subnet_map[k][1]
            subnet_map[k][1] = temp

        subnet_map[k][0].topo.append({
            'source_ip': subnet_map[k][0].subnet_ip[k],
            'target_ip': subnet_map[k][1].subnet_ip[k],
            'target_node_id': subnet_map[k][1].node_id
        })

    monitor_payloads = []

    for k in satellite_map.keys():
        monitor_payloads.append({
            "node_id": satellite_map[k].node_id,
            "host_ip": satellite_map[k].host_ip,
            "connections": satellite_map[k].topo
        })
    print(monitor_payloads)
    set_monitor(monitor_payloads)
    cpu_count = min(mp.cpu_count(), satellite_num)
    print("CPU Count = %d" % cpu_count)
    res = mp.Array('f', range(3 * satellite_num), lock=False)
    index = 0
    for i in range(cpu_count):
        p = mp.Process(target=worker, args=(i, satellite_num, cpu_count, res))
        p.start()
    while True:
        for i in range(satellite_num):
            node_id = 'node_' + str(i)
            index_base = 3 * i
            position_datas[node_id][LATITUDE_KEY] = res[index_base]
            position_datas[node_id][LONGITUDE_KEY] = res[index_base + 1]
            position_datas[node_id][HEIGHT_KEY] = res[index_base + 2]
        data_str = json.dumps(position_datas)
        for node_id, satellite in satellite_map.items():
            node_to_container_map[node_id] = satellite.container_id
        update_network_delay(position_datas,connect_order_map)
        try:
            updater.broadcast_position(data_str)
            index += 1
            if index > 120:
                set_monitor(monitor_payloads)
                index = 0
        except OSError as e:
            logging.error('OSError: ' + str(e))
        time.sleep(1)
