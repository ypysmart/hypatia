import networkx as nx
import numpy as np
from scipy import stats  # Pareto分布
from collections import defaultdict
import plotly.graph_objects as go  # 用于交互式3D可视化和动画

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
def simulate_round(G, flows, round_num):
    frames_3d = []
    frames_2d = []
    current_time_min = 0.0
    
    for t in range(TIME_STEPS):
        # 更新卫星位置（动态运动）
        positions = update_sat_positions(NUM_PLANES, SAT_PER_PLANE, INCLINATION, current_time_min)
        
        # 清空负载
        for u, v in G.edges():
            G[u][v]['load'] = 0
        
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
        
        congestion_ratio, avg_load, max_load = calculate_congestion(G)
        print(f"轮次 {round_num} 时间步 {t}: 活跃flows={active_count}, 拥塞比例={congestion_ratio*100:.2f}%, 平均负载={avg_load:.2f} Mbps, 最大负载={max_load:.2f} Mbps")
        
        # 创建3D动画帧
        frame_3d = go.Frame(data=create_3d_traces(G, positions), name=str(t))
        frames_3d.append(frame_3d)
        
        # 创建2D动画帧
        frame_2d = go.Frame(data=create_2d_traces(G, positions, NUM_PLANES, SAT_PER_PLANE), name=str(t))
        frames_2d.append(frame_2d)
        
        current_time_min += DELTA_T_MIN  # 更新时间
    
    # 生成3D动画
    fig_3d = go.Figure(data=frames_3d[0].data, frames=frames_3d)
    fig_3d.update_layout(
        title=f'动态3D卫星星座可视化 (轮次 {round_num}) - 红色为拥塞链路',
        scene=dict(xaxis_title='X (km)', yaxis_title='Y (km)', zaxis_title='Z (km)', aspectmode='cube'),
        updatemenus=[dict(type='buttons', buttons=[dict(label='Play', method='animate', args=[None, dict(frame=dict(duration=500, redraw=True), fromcurrent=True)])])],
        sliders=[dict(steps=[dict(method='relayout', label=str(k), args=[{'frame': k}]) for k in range(TIME_STEPS)])]
    )
    html_file_3d = f'3d_dynamic_congestion_round_{round_num}.html'
    fig_3d.write_html(html_file_3d)
    print(f"动态3D可视化动画已保存为 '{html_file_3d}'（浏览器中打开可播放动画）")
    
    # 生成2D动画（类似）
    fig_2d = go.Figure(data=frames_2d[0].data, frames=frames_2d)
    fig_2d.update_layout(
        title=f'动态2D矩形卫星星座可视化 (轮次 {round_num}) - 红色为拥塞链路',
        xaxis_title='卫星在平面内位置',
        yaxis_title='轨道平面ID',
        updatemenus=[dict(type='buttons', buttons=[dict(label='Play', method='animate', args=[None, dict(frame=dict(duration=500, redraw=True), fromcurrent=True)])])],
        sliders=[dict(steps=[dict(method='relayout', label=str(k), args=[{'frame': k}]) for k in range(TIME_STEPS)])]
    )
    html_file_2d = f'2d_dynamic_congestion_round_{round_num}.html'
    fig_2d.write_html(html_file_2d)
    print(f"动态2D可视化动画已保存为 '{html_file_2d}'（浏览器中打开可播放动画）")

# 创建3D traces
def create_3d_traces(G, positions):
    traces = []
    
    # 地球
    phi = np.linspace(0, 2 * np.pi, 100)
    theta = np.linspace(0, np.pi, 100)
    phi, theta = np.meshgrid(phi, theta)
    x_earth = EARTH_RADIUS * np.sin(theta) * np.cos(phi)
    y_earth = EARTH_RADIUS * np.sin(theta) * np.sin(phi)
    z_earth = EARTH_RADIUS * np.cos(theta)
    traces.append(go.Surface(x=x_earth, y=y_earth, z=z_earth, colorscale='Blues', showscale=False, opacity=1.0, hoverinfo='none'))
    
    # 卫星
    xs, ys, zs, hover_texts = [], [], [], []
    for sat_id, (x, y, z) in positions.items():
        xs.append(x)
        ys.append(y)
        zs.append(z)
        total_load = sum(G[sat_id][nbr]['load'] for nbr in G.neighbors(sat_id))
        hover_texts.append(f"Satellite ID: {sat_id}<br>Total Connected Load: {total_load:.2f} Mbps")
    traces.append(go.Scatter3d(x=xs, y=ys, z=zs, mode='markers', marker=dict(size=4, color='lightblue'), text=hover_texts, hovertemplate="%{text}<extra></extra>"))
    
    # 链路
    for u, v, data in G.edges(data=True):
        pos_u = positions[u]
        pos_v = positions[v]
        color = 'red' if data['load'] > data['capacity'] else 'gray'
        load = data['load']
        hover_text = f"Link {u}-{v}<br>Load: {load:.2f} Mbps<br>Capacity: {data['capacity']} Mbps"
        traces.append(go.Scatter3d(x=[pos_u[0], pos_v[0]], y=[pos_u[1], pos_v[1]], z=[pos_u[2], pos_v[2]], mode='lines', line=dict(color=color, width=2), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
    
    return traces

# 创建2D traces（矩形展开，类似原代码，但动态）
def create_2d_traces(G, positions, num_planes, sats_per_plane):
    traces = []
    
    # 可调整参数
    node_size = 5
    line_width = 1
    x_spacing = 8.0
    y_spacing = 8.0
    ray_length = 0.5
    
    # 2D位置（固定矩形布局，位置不随时间变，但负载动态）
    positions_2d = {}
    for plane in range(num_planes):
        for sat in range(sats_per_plane):
            sat_id = plane * sats_per_plane + sat
            positions_2d[sat_id] = (sat * x_spacing, -plane * y_spacing)
    
    # 卫星
    xs, ys, hover_texts = [], [], []
    for sat_id, (x, y) in positions_2d.items():
        xs.append(x)
        ys.append(y)
        total_load = sum(G[sat_id][nbr]['load'] for nbr in G.neighbors(sat_id))
        hover_texts.append(f"Satellite ID: {sat_id}<br>Total Connected Load: {total_load:.2f} Mbps")
    traces.append(go.Scatter(x=xs, y=ys, mode='markers', marker=dict(size=node_size, color='lightblue'), text=hover_texts, hovertemplate="%{text}<extra></extra>"))
    
    # 链路（类似原2D逻辑，动态颜色基于负载）
    for u, v, data in G.edges(data=True):
        pos_u = positions_2d[u]
        pos_v = positions_2d[v]
        x_u, y_u = pos_u
        x_v, y_v = pos_v
        delta_x = abs(x_u - x_v)
        delta_y = abs(y_u - y_v)
        color = 'red' if data['load'] > data['capacity'] else 'gray'
        load = data['load']
        hover_text = f"Link {u}-{v}<br>Load: {load:.2f} Mbps<br>Capacity: {data['capacity']} Mbps"
        
        is_wrap = False
        wrap_type = None
        if delta_y == 0 and delta_x > (sats_per_plane * x_spacing) / 2:
            is_wrap = True
            wrap_type = 'horizontal'
        elif delta_y > (num_planes - 1) * y_spacing / 2:  # 垂直环绕
            is_wrap = True
            wrap_type = 'vertical'
        
        if not is_wrap:
            traces.append(go.Scatter(x=[x_u, x_v], y=[y_u, y_v], mode='lines', line=dict(color=color, width=line_width), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
        else:
            # 射线逻辑（同原）
            if wrap_type == 'horizontal':
                min_x = min(x_u, x_v)
                max_x = max(x_u, x_v)
                y = y_u
                traces.append(go.Scatter(x=[min_x, min_x - ray_length], y=[y, y], mode='lines', line=dict(color=color, width=line_width), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
                traces.append(go.Scatter(x=[max_x, max_x + ray_length], y=[y, y], mode='lines', line=dict(color=color, width=line_width), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
            elif wrap_type == 'vertical':
                min_y = min(y_u, y_v)
                max_y = max(y_u, y_v)
                bottom_x = x_u if y_u == min_y else x_v
                top_x = x_v if y_v == max_y else x_u
                traces.append(go.Scatter(x=[bottom_x, bottom_x], y=[min_y, min_y - ray_length], mode='lines', line=dict(color=color, width=line_width), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
                traces.append(go.Scatter(x=[top_x, top_x], y=[max_y, max_y + ray_length], mode='lines', line=dict(color=color, width=line_width), text=[hover_text, hover_text], hovertemplate="%{text}<extra></extra>"))
    
    return traces

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
    G = create_walker_constellation()
    print(f"星座生成完成: {TOTAL_SATS} 颗卫星, {G.number_of_edges()} 条链路")
    
    hotspot_sources = np.random.choice(TOTAL_SATS, size=10, replace=False)
    hotspot_dests = np.random.choice(TOTAL_SATS, size=10, replace=False)
    
    load_factors = np.linspace(LOAD_FACTOR_START, LOAD_FACTOR_END, SIM_ROUNDS)
    
    for round_num, load_factor in enumerate(load_factors, 1):
        poisson_lambda = BASE_NUM_FLOWS * load_factor
        flows = generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests)
        simulate_round(G, flows, round_num)

if __name__ == "__main__":
    run_simulation()