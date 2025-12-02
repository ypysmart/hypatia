import networkx as nx
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

# 参数设置（专业配置，可调整）
NUM_PLANES = 32  # 轨道平面数 (P)
SAT_PER_PLANE = 32  # 每平面卫星数 (T/P)
TOTAL_SATS = NUM_PLANES * SAT_PER_PLANE  # 总卫星数 = 1024 (>1000)

LINK_CAPACITY = 10000  # 链路容量 (Mbps, 10 Gbps)
LINK_DELAY = 5  # 链路延迟 (ms, 典型ISL)

NUM_FLOWS = 1000  # 模拟流量数
FLOW_BANDWIDTH_MIN = 1  # 每个flow最小带宽需求 (Mbps)
FLOW_BANDWIDTH_MAX = 5  # 每个flow最大带宽需求 (Mbps)

SIM_ROUNDS = 10  # 模拟轮次，逐步增加负载
LOAD_FACTOR_START = 0.5  # 初始负载因子
LOAD_FACTOR_END = 1.5  # 结束负载因子

# 步骤1: 生成Walker星座近似网格拓扑
def create_walker_constellation():
    G = nx.Graph()
    
    # 添加卫星节点 (编号0到1023)
    for sat_id in range(TOTAL_SATS):
        G.add_node(sat_id)
    
    # 添加ISL链路: 环面网格 (前后 + 左右)
    for plane in range(NUM_PLANES):
        for sat in range(SAT_PER_PLANE):
            sat_id = plane * SAT_PER_PLANE + sat
            
            # 前后链路 (同一平面，环形)
            front = (sat + 1) % SAT_PER_PLANE
            front_id = plane * SAT_PER_PLANE + front
            G.add_edge(sat_id, front_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            back = (sat - 1) % SAT_PER_PLANE
            back_id = plane * SAT_PER_PLANE + back
            G.add_edge(sat_id, back_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            # 左右链路 (相邻平面，环形，模拟Walker相位偏移)
            left_plane = (plane - 1) % NUM_PLANES
            left_id = left_plane * SAT_PER_PLANE + (sat + 1) % SAT_PER_PLANE  # 简单相位偏移F=1
            G.add_edge(sat_id, left_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            right_plane = (plane + 1) % NUM_PLANES
            right_id = right_plane * SAT_PER_PLANE + (sat - 1) % SAT_PER_PLANE  # 偏移
            G.add_edge(sat_id, right_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
    
    return G

# 步骤2: 模拟流量注入和路由
def simulate_traffic(G, num_flows, load_factor):
    # 重置链路负载
    for u, v in G.edges():
        G[u][v]['load'] = 0
    
    total_injected = 0
    for _ in range(num_flows):
        # 随机源-目的 (避免自环)
        src = np.random.randint(0, TOTAL_SATS)
        dst = np.random.randint(0, TOTAL_SATS)
        while dst == src:
            dst = np.random.randint(0, TOTAL_SATS)
        
        # 随机带宽需求，乘负载因子
        bandwidth = np.random.uniform(FLOW_BANDWIDTH_MIN, FLOW_BANDWIDTH_MAX) * load_factor
        
        # 计算最短路径 (基于延迟，模拟Dijkstra路由)
        try:
            path = nx.shortest_path(G, src, dst, weight='delay')
            # 沿路径累积负载
            for i in range(len(path) - 1):
                u, v = path[i], path[i+1]
                # 无向图，确保边存在
                if G.has_edge(u, v):
                    G[u][v]['load'] += bandwidth
                elif G.has_edge(v, u):
                    G[v][u]['load'] += bandwidth
            total_injected += bandwidth
        except nx.NetworkXNoPath:
            pass  # 忽略无路径情况（虽在环面中罕见）
    
    return total_injected

# 步骤3: 计算拥塞指标
def calculate_congestion(G):
    congested_edges = 0
    total_edges = G.number_of_edges()
    total_load = 0
    max_load = 0
    
    for u, v, data in G.edges(data=True):
        load = data['load']
        capacity = data['capacity']
        total_load += load
        max_load = max(max_load, load)
        if load > capacity:
            congested_edges += 1
    
    congestion_ratio = congested_edges / total_edges if total_edges > 0 else 0
    avg_load = total_load / total_edges if total_edges > 0 else 0
    
    return congestion_ratio, avg_load, max_load

# 步骤4: 可视化拥塞热图 (可选)
def visualize_congestion(G):
    loads = [data['load'] for u, v, data in G.edges(data=True)]
    plt.hist(loads, bins=50)
    plt.title('链路负载分布')
    plt.xlabel('负载 (Mbps)')
    plt.ylabel('链路数')
    plt.axvline(LINK_CAPACITY, color='r', linestyle='--', label='容量阈值')
    plt.legend()
    plt.show()

# 主模拟函数
def run_simulation():
    G = create_walker_constellation()
    print(f"星座生成完成: {TOTAL_SATS} 颗卫星, {G.number_of_edges()} 条链路")
    
    results = []
    load_factors = np.linspace(LOAD_FACTOR_START, LOAD_FACTOR_END, SIM_ROUNDS)
    
    for round_num, load_factor in enumerate(load_factors, 1):
        total_injected = simulate_traffic(G, NUM_FLOWS, load_factor)
        congestion_ratio, avg_load, max_load = calculate_congestion(G)
        
        print(f"\n轮次 {round_num} (负载因子: {load_factor:.2f}):")
        print(f"  注入总流量: {total_injected:.2f} Mbps")
        print(f"  拥塞链路比例: {congestion_ratio * 100:.2f}%")
        print(f"  平均链路负载: {avg_load:.2f} Mbps")
        print(f"  最大链路负载: {max_load:.2f} Mbps")
        
        results.append((load_factor, congestion_ratio, avg_load))
    
    # 可视化（注释掉若无需）
    visualize_congestion(G)
    
    return results

# 运行模拟
if __name__ == "__main__":
    run_simulation()