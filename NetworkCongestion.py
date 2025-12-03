import networkx as nx
import numpy as np
from scipy import stats  # Pareto分布
from collections import defaultdict
import plotly.graph_objects as go  # 新增：用于交互式3D可视化

# 参数设置（可调整以控制拥塞）
NUM_PLANES = 25
SAT_PER_PLANE = 40
TOTAL_SATS = NUM_PLANES * SAT_PER_PLANE

LINK_CAPACITY = 10000
LINK_DELAY = 5

BASE_NUM_FLOWS = 1000  # 基础flows数，Poisson λ基于此
TIME_STEPS = 10
SIM_ROUNDS = 5
LOAD_FACTOR_START = 1.0
LOAD_FACTOR_END = 5.0
HOTSPOT_RATIO = 0.7  # 热点比例，提高确保拥塞

# 真实卫星参数
EARTH_RADIUS = 6371  # km
ORBIT_ALTITUDE = 550  # km, 如Starlink
INCLINATION = 53  # 度, 如Starlink典型倾角
ORBIT_RADIUS = EARTH_RADIUS + ORBIT_ALTITUDE

# 流量种类（Pareto参数）
FLOW_TYPES = {
    'video': {'prob': 0.3, 'bw_shape': 1.2, 'bw_scale': 20, 'dur_shape': 1.5, 'dur_scale': 5},  # 重尾大带宽/长时
    'file': {'prob': 0.4, 'bw_shape': 1.5, 'bw_scale': 5, 'dur_shape': 1.8, 'dur_scale': 3},
    'voip': {'prob': 0.3, 'bw_shape': 2.0, 'bw_scale': 1, 'dur_shape': 2.0, 'dur_scale': 1}
}

# 生成星座（同前）
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

# 计算卫星3D位置
def get_sat_positions(num_planes, sats_per_plane, inclination_deg):
    inclination = np.deg2rad(inclination_deg)
    positions = {}
    for plane in range(num_planes):
        raan = 2 * np.pi * plane / num_planes  # Right Ascension of Ascending Node
        for sat in range(sats_per_plane):
            true_anomaly = 2 * np.pi * sat / sats_per_plane
            # 简化：假设所有卫星在轨道上均匀分布，无时间动态
            # 轨道坐标
            x_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.cos(raan) - np.sin(true_anomaly) * np.sin(raan) * np.cos(inclination))
            y_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.sin(raan) + np.sin(true_anomaly) * np.cos(raan) * np.cos(inclination))
            z_orbit = ORBIT_RADIUS * (np.sin(true_anomaly) * np.sin(inclination))
            sat_id = plane * sats_per_plane + sat
            positions[sat_id] = (x_orbit, y_orbit, z_orbit)
    return positions

# 生成flows：使用Poisson到达和Pareto分布
def generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests):
    flows = []
    type_probs = [FLOW_TYPES[t]['prob'] for t in FLOW_TYPES]
    types = list(FLOW_TYPES.keys())
    
    num_new_flows = np.random.poisson(poisson_lambda)  # Poisson生成数量
    for _ in range(num_new_flows):
        flow_type = np.random.choice(types, p=type_probs)
        bw_shape, bw_scale = FLOW_TYPES[flow_type]['bw_shape'], FLOW_TYPES[flow_type]['bw_scale']
        dur_shape, dur_scale = FLOW_TYPES[flow_type]['dur_shape'], FLOW_TYPES[flow_type]['dur_scale']
        
        bandwidth = stats.pareto.rvs(bw_shape, scale=bw_scale) * load_factor  # Pareto带宽
        duration = int(stats.pareto.rvs(dur_shape, scale=dur_scale))  # Pareto时长
        
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

# 模拟轮次（动态时间步，同前）
def simulate_round(G, flows):
    results = []
    for t in range(TIME_STEPS):
        for u, v in G.edges():
            G[u][v]['load'] = 0
        
        active_count = 0
        for flow in flows:
            if flow['start'] <= t < flow['start'] + flow['dur']:
                if flow['path'] is None:
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
        
        congestion_ratio, avg_load, max_load = calculate_congestion(G)
        results.append((t, congestion_ratio, avg_load, max_load, active_count))
    return results

# 计算拥塞（同前）
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

# 修改可视化函数为交互式3D HTML（使用Plotly）
def visualize_constellation(G, positions, round_num):
    fig = go.Figure()

    # 绘制地球（简化球体，使用Surface）
    phi = np.linspace(0, 2 * np.pi, 100)
    theta = np.linspace(0, np.pi, 100)
    phi, theta = np.meshgrid(phi, theta)
    x_earth = EARTH_RADIUS * np.sin(theta) * np.cos(phi)
    y_earth = EARTH_RADIUS * np.sin(theta) * np.sin(phi)
    z_earth = EARTH_RADIUS * np.cos(theta)
    fig.add_trace(go.Surface(x=x_earth, y=y_earth, z=z_earth, colorscale='Blues', showscale=False, opacity=1.0, hoverinfo='none'))

    # 绘制卫星节点（散点）
    xs, ys, zs, hover_texts = [], [], [], []
    for sat_id, (x, y, z) in positions.items():
        xs.append(x)
        ys.append(y)
        zs.append(z)
        # 计算卫星的总负载（相连链路的负载和）
        total_load = sum(G[sat_id][nbr]['load'] for nbr in G.neighbors(sat_id))
        hover_texts.append(f"Satellite ID: {sat_id}<br>Total Connected Load: {total_load:.2f} Mbps<br>Delay: {LINK_DELAY} ms")
    fig.add_trace(go.Scatter3d(
        x=xs, y=ys, z=zs, 
        mode='markers', 
        marker=dict(size=4, color='lightblue'), 
        name='Satellites',
        text=hover_texts,
        hovertemplate="%{text}<extra></extra>"
    ))

    # 绘制链路（线段）
    congested_edges = []
    for u, v, data in G.edges(data=True):
        pos_u = positions[u]
        pos_v = positions[v]
        color = 'red' if data['load'] > data['capacity'] else 'gray'
        load = data['load']
        delay = data['delay']
        hover_text = f"Link {u}-{v}<br>Load: {load:.2f} Mbps<br>Capacity: {data['capacity']} Mbps<br>Delay: {delay} ms"
        if color == 'red':
            congested_edges.append((u, v))
        fig.add_trace(go.Scatter3d(
            x=[pos_u[0], pos_v[0]], y=[pos_u[1], pos_v[1]], z=[pos_u[2], pos_v[2]],
            mode='lines', 
            line=dict(color=color, width=2), 
            opacity=0.5, 
            showlegend=False,
            text=[hover_text, hover_text],  # 重复以覆盖两个点
            hovertemplate="%{text}<extra></extra>"
        ))

    # 设置布局
    fig.update_layout(
        title=f'3D 卫星星座可视化 (轮次 {round_num}) - 红色为拥塞链路<br>轨道高度: {ORBIT_ALTITUDE} km, 倾角: {INCLINATION}°',
        scene=dict(
            xaxis_title='X (km)', yaxis_title='Y (km)', zaxis_title='Z (km)',
            aspectmode='cube',
            camera=dict(eye=dict(x=1.5, y=1.5, z=1.5))  # 初始视角
        ),
        width=800, height=800
    )

    # 保存为交互HTML
    html_file = f'3d_congestion_round_{round_num}.html'
    fig.write_html(html_file)
    print(f"交互式3D可视化已保存为 '{html_file}'（在浏览器中打开可动态查看）")
    
    # 打印拥塞链路列表
    if congested_edges:
        print("拥塞链路列表:")
        for edge in congested_edges[:10]:  # 打印前10个，避免太多
            print(f"  ({edge[0]}, {edge[1]}) - 负载: {G[edge[0]][edge[1]]['load']:.2f} Mbps")
        if len(congested_edges) > 10:
            print(f"  ... (总 {len(congested_edges)} 条拥塞链路)")
    else:
        print("无拥塞链路")

# 主函数
def run_simulation():
    G = create_walker_constellation()
    print(f"星座生成完成: {TOTAL_SATS} 颗卫星, {G.number_of_edges()} 条链路")
    
    positions = get_sat_positions(NUM_PLANES, SAT_PER_PLANE, INCLINATION)
    
    hotspot_sources = np.random.choice(TOTAL_SATS, size=10, replace=False)
    hotspot_dests = np.random.choice(TOTAL_SATS, size=10, replace=False)
    
    load_factors = np.linspace(LOAD_FACTOR_START, LOAD_FACTOR_END, SIM_ROUNDS)
    
    for round_num, load_factor in enumerate(load_factors, 1):
        poisson_lambda = BASE_NUM_FLOWS * load_factor  # λ随因子递增，确保拥塞
        flows = generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests)
        round_results = simulate_round(G, flows)
        
        print(f"\n轮次 {round_num} (负载因子: {load_factor:.2f}, Poisson λ: {poisson_lambda:.2f}):")
        for t, cong_ratio, avg_load, max_load, active in round_results:
            print(f"  时间步 {t}: 活跃flows={active}, 拥塞比例={cong_ratio*100:.2f}%, 平均负载={avg_load:.2f} Mbps, 最大负载={max_load:.2f} Mbps")
        
        # 可视化最后一个时间步的负载，使用交互3D
        visualize_constellation(G, positions, round_num)

if __name__ == "__main__":
    run_simulation()