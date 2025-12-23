import networkx as nx
import numpy as np
from scipy import stats  # Pareto分布
from collections import defaultdict
import plotly.graph_objects as go  # 用于交互式3D可视化和动画
import json  # 用于保存数据到JSON文件
import os  # 用于创建目录

# 参数设置（可调整以控制拥塞）
NUM_PLANES = 25
SAT_PER_PLANE = 40
TOTAL_SATS = NUM_PLANES * SAT_PER_PLANE

LINK_CAPACITY = 10000
LINK_DELAY = 5

BASE_NUM_FLOWS = 1000  # 基础flows数，Poisson λ基于此
TIME_STEPS = 10  # 增加时间步以更好地展示动态
SIM_ROUNDS = 5
LOAD_FACTOR_START = 1.0
LOAD_FACTOR_END = 5.0
HOTSPOT_RATIO = 0.7  # 热点比例，提高确保拥塞

# 真实卫星参数
EARTH_RADIUS = 6371  # km
ORBIT_ALTITUDE = 550  # km, 如Starlink
INCLINATION = 53  # 度, 如Starlink典型倾角
ORBIT_RADIUS = EARTH_RADIUS + ORBIT_ALTITUDE

# LEO动态参数
ORBITAL_PERIOD_MIN = 95  # 典型LEO轨道周期（分钟）
DELTA_T_MIN = 0.5  # 每个时间步的模拟时间增量（分钟），控制动画速度

# 流量种类（Pareto参数）
FLOW_TYPES = {
    'video': {'prob': 0.3, 'bw_shape': 1.2, 'bw_scale': 20, 'dur_shape': 1.5, 'dur_scale': 5},  # 重尾大带宽/长时
    'file': {'prob': 0.4, 'bw_shape': 1.5, 'bw_scale': 5, 'dur_shape': 1.8, 'dur_scale': 3},
    'voip': {'prob': 0.3, 'bw_shape': 2.0, 'bw_scale': 1, 'dur_shape': 2.0, 'dur_scale': 1}
}

# 保存目录
SAVE_DIR = "SceneDepiction/simulation_data"
os.makedirs(SAVE_DIR, exist_ok=True)

# NumPy 类型编码器
def np_encoder(o):
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.floating):
        return float(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    raise TypeError(f'Object of type {o.__class__.__name__} is not JSON serializable')

# 生成星座（初始拓扑）
def create_walker_constellation():
    G = nx.Graph()
    for sat_id in range(TOTAL_SATS):
        G.add_node(sat_id)
    for plane in range(NUM_PLANES):
        for sat in range(SAT_PER_PLANE):
            sat_id = plane * SAT_PER_PLANE + sat
            front = (sat + 1) % SAT_PER_PLANE
            front_id = plane * SAT_PER_PLANE + front
            G.add_edge(sat_id, front_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            back = (sat - 1) % SAT_PER_PLANE
            back_id = plane * SAT_PER_PLANE + back
            G.add_edge(sat_id, back_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            left_plane = (plane - 1) % NUM_PLANES
            left_id = left_plane * SAT_PER_PLANE + (sat + 1) % SAT_PER_PLANE
            G.add_edge(sat_id, left_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            right_plane = (plane + 1) % NUM_PLANES
            right_id = right_plane * SAT_PER_PLANE + (sat - 1) % SAT_PER_PLANE
            G.add_edge(sat_id, right_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
    return G

# 更新卫星3D位置（动态，基于时间）
def update_sat_positions(num_planes, sats_per_plane, inclination_deg, current_time_min):
    inclination = np.deg2rad(inclination_deg)
    positions = {}
    for plane in range(num_planes):
        raan = 2 * np.pi * plane / num_planes
        for sat in range(sats_per_plane):
            # 真近点角随时间动态变化（简化匀速圆轨道）
            initial_anomaly = 2 * np.pi * sat / sats_per_plane
            angular_speed = 2 * np.pi / ORBITAL_PERIOD_MIN  # 弧度/分钟
            true_anomaly = initial_anomaly + angular_speed * current_time_min
            true_anomaly %= 2 * np.pi
            
            x_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.cos(raan) - np.sin(true_anomaly) * np.sin(raan) * np.cos(inclination))
            y_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.sin(raan) + np.sin(true_anomaly) * np.cos(raan) * np.cos(inclination))
            z_orbit = ORBIT_RADIUS * (np.sin(true_anomaly) * np.sin(inclination))
            sat_id = plane * sats_per_plane + sat
            positions[sat_id] = (x_orbit, y_orbit, z_orbit)
    return positions

# 生成flows
def generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests):
    flows = []
    type_probs = [FLOW_TYPES[t]['prob'] for t in FLOW_TYPES]
    types = list(FLOW_TYPES.keys())
    
    num_new_flows = np.random.poisson(poisson_lambda)
    for _ in range(num_new_flows):
        flow_type = np.random.choice(types, p=type_probs)
        bw_shape, bw_scale = FLOW_TYPES[flow_type]['bw_shape'], FLOW_TYPES[flow_type]['bw_scale']
        dur_shape, dur_scale = FLOW_TYPES[flow_type]['dur_shape'], FLOW_TYPES[flow_type]['dur_scale']
        
        bandwidth = stats.pareto.rvs(bw_shape, scale=bw_scale) * load_factor
        duration = int(stats.pareto.rvs(dur_shape, scale=dur_scale))
        
        if np.random.rand() < HOTSPOT_RATIO:
            src = np.random.choice(hotspot_sources)
            dst = np.random.choice(hotspot_dests)
        else:
            src = np.random.randint(0, TOTAL_SATS)
            dst = np.random.randint(0, TOTAL_SATS)
            while dst == src:
                dst = np.random.randint(0, TOTAL_SATS)
        
        flows.append({'type': flow_type, 'bw': bandwidth, 'dur': duration, 'start': 0, 'src': src, 'dst': dst, 'path': None})
    return flows

# 模拟轮次（动态计算负载）
def simulate_round(G, flows, round_num, params, hotspot_sources, hotspot_dests):
    frames_3d = []
    frames_2d = []
    current_time_min = 0.0
    
    for t in range(TIME_STEPS):
        # 更新卫星位置（动态运动）
        positions = update_sat_positions(NUM_PLANES, SAT_PER_PLANE, INCLINATION, current_time_min)
        
        # 清空负载
        for u, v in G.edges():
            G[u][v]['load'] = 0
        
        active_flows = []
        active_count = 0
        for flow in flows:
            if flow['start'] <= t < flow['start'] + flow['dur']:
                if flow['path'] is None or t % 5 == 0:  # 每5步重新计算路径（模拟动态路由）
                    try:
                        flow['path'] = nx.shortest_path(G, flow['src'], flow['dst'], weight='delay')
                    except nx.NetworkXNoPath:
                        flow['path'] = []
                path = flow['path']
                for i in range(len(path) - 1):
                    u, v = path[i], path[i+1]
                    if G.has_edge(u, v):
                        G[u][v]['load'] += flow['bw']
                    elif G.has_edge(v, u):
                        G[v][u]['load'] += flow['bw']
                active_count += 1
                active_flows.append(flow.copy())  # 保存活跃flow的副本
        
        congestion_ratio, avg_load, max_load = calculate_congestion(G)
        print(f"轮次 {round_num} 时间步 {t}: 活跃flows={active_count}, 拥塞比例={congestion_ratio*100:.2f}%, 平均负载={avg_load:.2f} Mbps, 最大负载={max_load:.2f} Mbps")
        
        # 收集所有数据
        data = {
            'params': params,
            'round_num': round_num,
            'time_step': t,
            'current_time_min': current_time_min,
            'positions': {k: list(v) for k, v in positions.items()},  # 转换为列表以JSON序列化
            'graph_nodes': list(G.nodes()),
            'graph_edges': [
                {
                    'u': u,
                    'v': v,
                    'capacity': data['capacity'],
                    'delay': data['delay'],
                    'load': data['load']
                } for u, v, data in G.edges(data=True)
            ],
            'flows': [f.copy() for f in flows],  # 所有flows
            'active_flows': active_flows,  # 活跃flows
            'congestion_ratio': congestion_ratio,
            'avg_load': avg_load,
            'max_load': max_load,
            'hotspot_sources': hotspot_sources.tolist(),
            'hotspot_dests': hotspot_dests.tolist()
        }
        
        # 保存到文件
        file_path = os.path.join(SAVE_DIR, f"round_{round_num}_timestep_{t}.json")
        with open(file_path, 'w') as f:
            json.dump(data, f, indent=4, default=np_encoder)
        #print(f"数据保存到: {file_path}")
        
        # 更新当前时间
        current_time_min += DELTA_T_MIN

# 计算拥塞（同原）
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

# 主函数
def run_simulation():
    # 收集所有参数
    params = {
        'NUM_PLANES': NUM_PLANES,
        'SAT_PER_PLANE': SAT_PER_PLANE,
        'TOTAL_SATS': TOTAL_SATS,
        'LINK_CAPACITY': LINK_CAPACITY,
        'LINK_DELAY': LINK_DELAY,
        'BASE_NUM_FLOWS': BASE_NUM_FLOWS,
        'TIME_STEPS': TIME_STEPS,
        'SIM_ROUNDS': SIM_ROUNDS,
        'LOAD_FACTOR_START': LOAD_FACTOR_START,
        'LOAD_FACTOR_END': LOAD_FACTOR_END,
        'HOTSPOT_RATIO': HOTSPOT_RATIO,
        'EARTH_RADIUS': EARTH_RADIUS,
        'ORBIT_ALTITUDE': ORBIT_ALTITUDE,
        'INCLINATION': INCLINATION,
        'ORBIT_RADIUS': ORBIT_RADIUS,
        'ORBITAL_PERIOD_MIN': ORBITAL_PERIOD_MIN,
        'DELTA_T_MIN': DELTA_T_MIN,
        'FLOW_TYPES': FLOW_TYPES,
        'SAVE_DIR': SAVE_DIR
    }
    
    # 保存参数到文件
    params_file = os.path.join(SAVE_DIR, "simulation_params.json")
    with open(params_file, 'w') as f:
        json.dump(params, f, indent=4)
    print(f"参数保存到: {params_file}")
    
    G = create_walker_constellation()
    print(f"星座生成完成: {TOTAL_SATS} 颗卫星, {G.number_of_edges()} 条链路")
    
    hotspot_sources = np.random.choice(TOTAL_SATS, size=10, replace=False)
    hotspot_dests = np.random.choice(TOTAL_SATS, size=10, replace=False)
    
    load_factors = np.linspace(LOAD_FACTOR_START, LOAD_FACTOR_END, SIM_ROUNDS)
    
    for round_num, load_factor in enumerate(load_factors, 1):
        print(f"=================第{round_num}轮次=================")
        poisson_lambda = BASE_NUM_FLOWS * load_factor
        flows = generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests)
        simulate_round(G, flows, round_num, params, hotspot_sources, hotspot_dests)
        print(f"=================第{round_num}轮次=================\n\n")
if __name__ == "__main__":
    run_simulation()