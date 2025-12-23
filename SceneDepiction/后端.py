import streamlit as st
import networkx as nx
import numpy as np
from scipy import stats
import plotly.graph_objects as go

# ==========================================
# 1. 核心仿真参数与逻辑 (复用你的模型)
# ==========================================

# 参数设置
NUM_PLANES = 25
SAT_PER_PLANE = 40
TOTAL_SATS = NUM_PLANES * SAT_PER_PLANE

LINK_CAPACITY = 10000
LINK_DELAY = 5

BASE_NUM_FLOWS = 500  # 稍微调低以便可视化不至于太乱
TIME_STEPS_PER_ROUND = 10
SIM_ROUNDS = 3  # 演示用，设为3轮
HOTSPOT_RATIO = 0.7

EARTH_RADIUS = 6371
ORBIT_ALTITUDE = 550
INCLINATION = 53
ORBIT_RADIUS = EARTH_RADIUS + ORBIT_ALTITUDE
ORBITAL_PERIOD_MIN = 95

FLOW_TYPES = {
    'video': {'prob': 0.3, 'bw_shape': 1.2, 'bw_scale': 20, 'dur_shape': 1.5, 'dur_scale': 5},
    'file': {'prob': 0.4, 'bw_shape': 1.5, 'bw_scale': 5, 'dur_shape': 1.8, 'dur_scale': 3},
    'voip': {'prob': 0.3, 'bw_shape': 2.0, 'bw_scale': 1, 'dur_shape': 2.0, 'dur_scale': 1}
}

# --- 辅助函数 ---

def create_walker_constellation():
    G = nx.Graph()
    for sat_id in range(TOTAL_SATS):
        G.add_node(sat_id)
    for plane in range(NUM_PLANES):
        for sat in range(SAT_PER_PLANE):
            sat_id = plane * SAT_PER_PLANE + sat
            # 同轨道链路
            front = (sat + 1) % SAT_PER_PLANE
            front_id = plane * SAT_PER_PLANE + front
            G.add_edge(sat_id, front_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            back = (sat - 1) % SAT_PER_PLANE
            back_id = plane * SAT_PER_PLANE + back
            G.add_edge(sat_id, back_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            # 异轨道链路 (简化Grid连接)
            left_plane = (plane - 1) % NUM_PLANES
            left_id = left_plane * SAT_PER_PLANE + (sat + 1) % SAT_PER_PLANE
            G.add_edge(sat_id, left_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
            
            right_plane = (plane + 1) % NUM_PLANES
            right_id = right_plane * SAT_PER_PLANE + (sat - 1) % SAT_PER_PLANE
            G.add_edge(sat_id, right_id, capacity=LINK_CAPACITY, delay=LINK_DELAY, load=0)
    return G

def update_sat_positions(current_time_min):
    inclination = np.deg2rad(INCLINATION)
    positions = {}
    for plane in range(NUM_PLANES):
        raan = 2 * np.pi * plane / NUM_PLANES
        for sat in range(SAT_PER_PLANE):
            initial_anomaly = 2 * np.pi * sat / SAT_PER_PLANE
            angular_speed = 2 * np.pi / ORBITAL_PERIOD_MIN
            true_anomaly = initial_anomaly + angular_speed * current_time_min
            true_anomaly %= 2 * np.pi
            
            x_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.cos(raan) - np.sin(true_anomaly) * np.sin(raan) * np.cos(inclination))
            y_orbit = ORBIT_RADIUS * (np.cos(true_anomaly) * np.sin(raan) + np.sin(true_anomaly) * np.cos(raan) * np.cos(inclination))
            z_orbit = ORBIT_RADIUS * (np.sin(true_anomaly) * np.sin(inclination))
            sat_id = plane * SAT_PER_PLANE + sat
            positions[sat_id] = (x_orbit, y_orbit, z_orbit)
    return positions

def generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests):
    flows = []
    type_probs = [FLOW_TYPES[t]['prob'] for t in FLOW_TYPES]
    types = list(FLOW_TYPES.keys())
    
    num_new_flows = np.random.poisson(poisson_lambda)
    for _ in range(num_new_flows):
        flow_type = np.random.choice(types, p=type_probs)
        bw_scale = FLOW_TYPES[flow_type]['bw_scale']
        bw_shape = FLOW_TYPES[flow_type]['bw_shape']
        dur_scale = FLOW_TYPES[flow_type]['dur_scale']
        dur_shape = FLOW_TYPES[flow_type]['dur_shape']
        
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

def calculate_congestion(G):
    congested_edges = 0
    total_edges = G.number_of_edges()
    total_load = 0
    max_load = 0
    link_status = [] # Store status for visualization
    
    for u, v, data in G.edges(data=True):
        load = data['load']
        capacity = data['capacity']
        total_load += load
        max_load = max(max_load, load)
        if load > capacity:
            congested_edges += 1
        
        ratio = load / capacity if capacity > 0 else 0
        link_status.append({'u': u, 'v': v, 'load': load, 'ratio': ratio})
        
    congestion_ratio = congested_edges / total_edges if total_edges > 0 else 0
    avg_load = total_load / total_edges if total_edges > 0 else 0
    return congestion_ratio, avg_load, max_load, link_status

# ==========================================
# 2. 仿真执行器 (生成数据帧)
# ==========================================

@st.cache_data(show_spinner=True)
def run_full_simulation():
    """运行完整仿真并返回每一帧的数据供可视化"""
    frames = []
    G = create_walker_constellation()
    
    hotspot_sources = np.random.choice(TOTAL_SATS, size=10, replace=False)
    hotspot_dests = np.random.choice(TOTAL_SATS, size=10, replace=False)
    
    # 模拟多轮负载变化
    load_factors = np.linspace(1.0, 5.0, SIM_ROUNDS)
    current_time_min = 0.0
    
    total_steps = 0
    
    for round_num, load_factor in enumerate(load_factors, 1):
        poisson_lambda = BASE_NUM_FLOWS * load_factor
        flows = generate_flows(poisson_lambda, load_factor, hotspot_sources, hotspot_dests)
        
        for t in range(TIME_STEPS_PER_ROUND):
            total_steps += 1
            # 1. 更新位置
            positions = update_sat_positions(current_time_min)
            current_time_min += 0.5 # 0.5 min step
            
            # 2. 清空负载
            for u, v in G.edges():
                G[u][v]['load'] = 0
            
            # 3. 路由与流量分配
            active_paths = []
            active_count = 0
            
            for flow in flows:
                if flow['start'] <= t < flow['start'] + flow['dur']:
                    # 简化：只在第一步或每5步计算路径
                    if flow['path'] is None or t % 5 == 0:
                        try:
                            # 简单的最短路，实际可以用带权重的
                            flow['path'] = nx.shortest_path(G, flow['src'], flow['dst'], weight='delay')
                        except nx.NetworkXNoPath:
                            flow['path'] = []
                    
                    path = flow['path']
                    if path:
                        active_paths.append(path)
                        for i in range(len(path) - 1):
                            u, v = path[i], path[i+1]
                            if G.has_edge(u, v):
                                G[u][v]['load'] += flow['bw']
                            elif G.has_edge(v, u):
                                G[v][u]['load'] += flow['bw']
                        active_count += 1
            
            # 4. 计算统计数据
            cong_ratio, avg_load, max_load, link_status = calculate_congestion(G)
            
            # 5. 保存帧数据 (只保存必要的可视化数据以节省内存)
            frame_data = {
                'round': round_num,
                'step': t,
                'total_step': total_steps,
                'time': current_time_min,
                'positions': positions,
                'link_status': link_status, # List of {u, v, ratio}
                'active_paths': active_paths, # List of lists
                'metrics': {
                    'active_flows': active_count,
                    'congestion_ratio': cong_ratio,
                    'avg_load': avg_load,
                    'max_load': max_load
                }
            }
            frames.append(frame_data)
            
    return frames

# ==========================================
# 3. 前端可视化 (Plotly + Streamlit)
# ==========================================

def draw_network_3d(frame, show_routes=False, max_routes=50):
    pos = frame['positions']
    links = frame['link_status']
    
    # 1. 绘制地球
    # 创建球体网格
    phi = np.linspace(0, 2*np.pi, 50)
    theta = np.linspace(0, np.pi, 50)
    phi, theta = np.meshgrid(phi, theta)
    x_earth = EARTH_RADIUS * 0.99 * np.cos(phi) * np.sin(theta)
    y_earth = EARTH_RADIUS * 0.99 * np.sin(phi) * np.sin(theta)
    z_earth = EARTH_RADIUS * 0.99 * np.cos(theta)
    
    earth = go.Surface(
        x=x_earth, y=y_earth, z=z_earth,
        colorscale=[[0, 'rgb(10,10,40)'], [1, 'rgb(30,30,80)']],
        showscale=False, opacity=0.8, name='Earth'
    )
    
    data = [earth]
    
    # 2. 绘制卫星节点
    node_x, node_y, node_z = [], [], []
    for pid, coord in pos.items():
        node_x.append(coord[0])
        node_y.append(coord[1])
        node_z.append(coord[2])
        
    nodes = go.Scatter3d(
        x=node_x, y=node_y, z=node_z,
        mode='markers',
        marker=dict(size=2, color='white'),
        name='Satellites'
    )
    data.append(nodes)
    
    # 3. 绘制链路 (按负载分级着色)
    # 为了性能，将相同颜色的线段合并为一个 trace
    lines_safe_x, lines_safe_y, lines_safe_z = [], [], []
    lines_warn_x, lines_warn_y, lines_warn_z = [], [], []
    lines_crit_x, lines_crit_y, lines_crit_z = [], [], []
    
    for link in links:
        u, v = link['u'], link['v']
        ratio = link['ratio']
        p1, p2 = pos[u], pos[v]
        
        # 插入 None 断开线段
        seg_x = [p1[0], p2[0], None]
        seg_y = [p1[1], p2[1], None]
        seg_z = [p1[2], p2[2], None]
        
        if ratio > 1.0: # 拥塞 (红)
            lines_crit_x.extend(seg_x)
            lines_crit_y.extend(seg_y)
            lines_crit_z.extend(seg_z)
        elif ratio > 0.5: # 警告 (黄)
            lines_warn_x.extend(seg_x)
            lines_warn_y.extend(seg_y)
            lines_warn_z.extend(seg_z)
        else: # 正常 (绿/灰)
            # 为了减少视觉杂乱，负载极低的链路可以设为很淡
            if ratio > 0.01:
                lines_safe_x.extend(seg_x)
                lines_safe_y.extend(seg_y)
                lines_safe_z.extend(seg_z)

    # 添加链路 Trace
    if lines_safe_x:
        data.append(go.Scatter3d(
            x=lines_safe_x, y=lines_safe_y, z=lines_safe_z,
            mode='lines', line=dict(color='rgba(0,255,0,0.3)', width=1),
            name='Healthy Link'
        ))
    if lines_warn_x:
        data.append(go.Scatter3d(
            x=lines_warn_x, y=lines_warn_y, z=lines_warn_z,
            mode='lines', line=dict(color='yellow', width=2),
            name='Busy Link'
        ))
    if lines_crit_x:
        data.append(go.Scatter3d(
            x=lines_crit_x, y=lines_crit_y, z=lines_crit_z,
            mode='lines', line=dict(color='red', width=4),
            name='Congested Link'
        ))
        
    # 4. 绘制路由路径 (高亮显示)
    if show_routes and frame['active_paths']:
        route_x, route_y, route_z = [], [], []
        # 只显示前 N 条以避免卡顿
        display_paths = frame['active_paths'][:max_routes]
        
        for path in display_paths:
            for i in range(len(path)-1):
                u, v = path[i], path[i+1]
                p1, p2 = pos[u], pos[v]
                route_x.extend([p1[0], p2[0], None])
                route_y.extend([p1[1], p2[1], None])
                route_z.extend([p1[2], p2[2], None])
                
        data.append(go.Scatter3d(
            x=route_x, y=route_y, z=route_z,
            mode='lines',
            line=dict(color='cyan', width=3),
            name='Active Flow Paths'
        ))

    layout = go.Layout(
        title=f"Time: {frame['time']:.1f} min | Congestion: {frame['metrics']['congestion_ratio']*100:.2f}%",
        scene=dict(
            xaxis=dict(showbackground=False, showgrid=False, zeroline=False, visible=False),
            yaxis=dict(showbackground=False, showgrid=False, zeroline=False, visible=False),
            zaxis=dict(showbackground=False, showgrid=False, zeroline=False, visible=False),
            bgcolor='black'
        ),
        paper_bgcolor='black',
        font=dict(color='white'),
        margin=dict(l=0, r=0, b=0, t=30),
        height=700
    )
    
    return go.Figure(data=data, layout=layout)

# ==========================================
# 4. Streamlit 主程序
# ==========================================

def main():
    st.set_page_config(layout="wide", page_title="Satellite Net Sim")
    
    st.title("🛰️ LEO Satellite Network Congestion Visualizer")
    st.markdown("User Model: Walker Constellation | Dynamic Traffic & Routing Analysis")
    
    # Sidebar
    st.sidebar.header("Simulation Controls")
    
    if st.sidebar.button("🚀 Run New Simulation"):
        st.session_state['data'] = run_full_simulation()
        st.sidebar.success("Simulation Complete!")
        
    # Check if data exists
    if 'data' in st.session_state:
        frames = st.session_state['data']
        total_frames = len(frames)
        
        # Slider for Time Step
        selected_idx = st.slider("Timeline (Round/Step)", 0, total_frames-1, 0, 
                                 format=f"Frame %d of {total_frames}")
        
        current_frame = frames[selected_idx]
        
        # Display Metrics
        m = current_frame['metrics']
        c1, c2, c3, c4 = st.columns(4)
        c1.metric("Active Flows", m['active_flows'])
        c2.metric("Congestion Ratio", f"{m['congestion_ratio']*100:.2f}%", 
                  delta_color="inverse" if m['congestion_ratio'] > 0 else "normal")
        c3.metric("Avg Load (Mbps)", f"{m['avg_load']:.0f}")
        c4.metric("Max Load (Mbps)", f"{m['max_load']:.0f}")
        
        # View Options
        st.sidebar.subheader("View Options")
        show_routes = st.sidebar.checkbox("Show Traffic Paths", value=True)
        num_routes = st.sidebar.slider("Max Paths to Show", 1, 100, 20)
        
        # Plot
        st.subheader(f"Constellation View - Round {current_frame['round']}")
        fig = draw_network_3d(current_frame, show_routes, num_routes)
        st.plotly_chart(fig, use_container_width=True)
        
    else:
        st.info("Please click 'Run New Simulation' in the sidebar to start.")

if __name__ == "__main__":
    main()
