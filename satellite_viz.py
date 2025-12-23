import streamlit as st
import json
import os
import numpy as np
import plotly.graph_objects as go
import pandas as pd

# ==========================================
# 配置与工具函数
# ==========================================

st.set_page_config(layout="wide", page_title="LEO Satellite Network Visualization")

# 默认数据路径 (根据你的描述调整)
DEFAULT_DATA_DIR = "SceneDepiction/simulation_data"
PARAM_FILE = "simulation_params.json"

@st.cache_data
def load_params():
    """加载仿真参数"""
    # 尝试在当前目录或数据目录查找参数文件
    paths = [PARAM_FILE, os.path.join(DEFAULT_DATA_DIR, "simulation_params.json")]
    for p in paths:
        if os.path.exists(p):
            with open(p, 'r') as f:
                return json.load(f)
    return None

@st.cache_data
def load_simulation_step(data_dir, round_num, time_step):
    """加载特定时间步的仿真数据"""
    filename = f"round_{round_num}_timestep_{time_step}.json"
    filepath = os.path.join(data_dir, filename)
    
    if not os.path.exists(filepath):
        return None
        
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    # 转换 positions 的 key 为 int (JSON中key默认为str)
    if 'positions' in data:
        data['positions'] = {int(k): v for k, v in data['positions'].items()}
        
    return data

def create_earth_sphere(radius):
    """生成地球的3D球面数据"""
    phi = np.linspace(0, np.pi, 50)
    theta = np.linspace(0, 2 * np.pi, 50)
    phi, theta = np.meshgrid(phi, theta)
    
    x = radius * np.sin(phi) * np.cos(theta)
    y = radius * np.sin(phi) * np.sin(theta)
    z = radius * np.cos(phi)
    return x, y, z

# ==========================================
# 页面布局与逻辑
# ==========================================

st.title("🛰️ LEO Satellite Network Traffic Visualization")

# 1. 侧边栏：控制面板
st.sidebar.header("Simulation Controls")

# 检查数据目录
data_dir = st.sidebar.text_input("Data Directory", DEFAULT_DATA_DIR)
if not os.path.exists(data_dir):
    st.error(f"Directory not found: {data_dir}")
    st.stop()

# 加载参数
params = load_params()
if params:
    earth_radius = params.get('EARTH_RADIUS', 6371)
    sim_rounds = params.get('SIM_ROUNDS', 5)
    time_steps = params.get('TIME_STEPS', 10)
    total_sats = params.get('TOTAL_SATS', 1000)
else:
    st.warning("Params file not found, using defaults.")
    earth_radius = 6371
    sim_rounds = 5
    time_steps = 10
    total_sats = 1000

# 选择 Round 和 TimeStep
selected_round = st.sidebar.slider("Simulation Round (Load Factor Level)", 1, sim_rounds, 1)
selected_step = st.sidebar.slider("Time Step", 0, time_steps - 1, 0)

# 加载当前帧数据
data = load_simulation_step(data_dir, selected_round, selected_step)

if data is None:
    st.error(f"Data file not found for Round {selected_round}, Step {selected_step}")
    st.stop()

# 2. 主要指标展示
col1, col2, col3, col4 = st.columns(4)
with col1:
    st.metric("Congestion Ratio", f"{data.get('congestion_ratio', 0)*100:.2f}%")
with col2:
    st.metric("Avg Link Load", f"{data.get('avg_load', 0):.2f} Mbps")
with col3:
    st.metric("Max Link Load", f"{data.get('max_load', 0):.2f} Mbps")
with col4:
    active_flows = data.get('active_flows', [])
    st.metric("Active Flows", len(active_flows))

# 3. 构建 3D 可视化
st.subheader("Constellation & Traffic 3D View")

fig = go.Figure()

# A. 绘制地球
ex, ey, ez = create_earth_sphere(earth_radius * 0.98) # 稍微小一点避免遮挡
fig.add_trace(go.Surface(
    x=ex, y=ey, z=ez,
    colorscale='Blues',
    showscale=False,
    opacity=0.8,
    hoverinfo='skip',
    name="Earth"
))

# 获取卫星位置
positions = data['positions']
# 将 dict 转为 list 以便绘图
sat_ids = sorted(positions.keys())
sat_x = [positions[i][0] for i in sat_ids]
sat_y = [positions[i][1] for i in sat_ids]
sat_z = [positions[i][2] for i in sat_ids]

# B. 绘制卫星 (节点)
fig.add_trace(go.Scatter3d(
    x=sat_x, y=sat_y, z=sat_z,
    mode='markers',
    marker=dict(size=2, color='white', opacity=0.8),
    text=[f"Sat {i}" for i in sat_ids],
    name="Satellites"
))

# C. 绘制链路 (根据负载着色)
# 为了性能，我们将链路分为几类批量绘制，而不是每条线一个trace
edges_low = {'x': [], 'y': [], 'z': []}   # Load < 50%
edges_med = {'x': [], 'y': [], 'z': []}   # 50% <= Load < 80%
edges_high = {'x': [], 'y': [], 'z': []}  # Load >= 80%
edges_congested = {'x': [], 'y': [], 'z': []} # Load >= 100% (Congested)

link_capacity = data['params']['LINK_CAPACITY'] if 'params' in data else 10000

for edge in data['graph_edges']:
    u, v = edge['u'], edge['v']
    load = edge['load']
    
    # 忽略没有负载的链路以减少视觉混乱 (或者设为极细的灰色)
    if load <= 0:
        continue
        
    p1 = positions[u]
    p2 = positions[v]
    
    ratio = load / link_capacity
    
    target = edges_low
    if ratio >= 1.0:
        target = edges_congested
    elif ratio >= 0.8:
        target = edges_high
    elif ratio >= 0.5:
        target = edges_med
        
    target['x'].extend([p1[0], p2[0], None])
    target['y'].extend([p1[1], p2[1], None])
    target['z'].extend([p1[2], p2[2], None])

# 添加不同负载等级的链路层
link_layers = [
    (edges_low, 'Low Load (<50%)', 'green', 1),
    (edges_med, 'Medium Load (50-80%)', 'yellow', 2),
    (edges_high, 'High Load (80-100%)', 'orange', 3),
    (edges_congested, 'Congested (>100%)', 'red', 5)
]

for edges_data, name, color, width in link_layers:
    if edges_data['x']:
        fig.add_trace(go.Scatter3d(
            x=edges_data['x'], y=edges_data['y'], z=edges_data['z'],
            mode='lines',
            line=dict(color=color, width=width),
            hoverinfo='skip',
            name=name
        ))

# D. 绘制特定的路由路径 (Traffic Routing Paths)
# 在侧边栏增加选择器
if active_flows:
    # 提取有路径的 flow ID
    flow_options = {f"{i}: {f['type']} (src:{f['src']}->dst:{f['dst']})": i for i, f in enumerate(active_flows) if f['path']}
    selected_flow_labels = st.sidebar.multiselect(
        "Highlight Routing Paths", 
        options=list(flow_options.keys()),
        default=[] # 默认不选，避免太乱
    )
    
    for label in selected_flow_labels:
        idx = flow_options[label]
        flow = active_flows[idx]
        path = flow['path']
        
        path_x, path_y, path_z = [], [], []
        for node_id in path:
            pos = positions[node_id]
            path_x.append(pos[0])
            path_y.append(pos[1])
            path_z.append(pos[2])
            
        fig.add_trace(go.Scatter3d(
            x=path_x, y=path_y, z=path_z,
            mode='lines+markers',
            line=dict(color='cyan', width=6),
            marker=dict(size=4, color='cyan'),
            name=f"Flow {idx} Path"
        ))
        
        # 标记起点和终点
        fig.add_trace(go.Scatter3d(
            x=[path_x[0], path_x[-1]],
            y=[path_y[0], path_y[-1]],
            z=[path_z[0], path_z[-1]],
            mode='text',
            text=["SRC", "DST"],
            textposition="top center",
            textfont=dict(color="cyan", size=15, weight="bold"),
            showlegend=False
        ))

# 更新布局样式
fig.update_layout(
    scene=dict(
        xaxis=dict(showbackground=False, visible=False),
        yaxis=dict(showbackground=False, visible=False),
        zaxis=dict(showbackground=False, visible=False),
        bgcolor='black',
        aspectmode='data' # 保持地球比例
    ),
    paper_bgcolor='black',
    plot_bgcolor='black',
    margin=dict(l=0, r=0, t=0, b=0),
    legend=dict(x=0, y=1, font=dict(color='white')),
    height=800
)

st.plotly_chart(fig, use_container_width=True)

# 4. 数据表格展示
st.subheader("Detailed Traffic Data")
tab1, tab2 = st.tabs(["Active Flows", "Hotspots"])

with tab1:
    if active_flows:
        # 将 flows 数据转换为 DataFrame 以便展示
        flows_df = pd.DataFrame(active_flows)
        # 简化 path 显示
        flows_df['path_length'] = flows_df['path'].apply(lambda x: len(x) if x else 0)
        flows_df['path_str'] = flows_df['path'].apply(lambda x: str(x)[:50] + "..." if x and len(str(x)) > 50 else str(x))
        st.dataframe(flows_df[['type', 'src', 'dst', 'bw', 'dur', 'path_length', 'path_str']])
    else:
        st.info("No active flows in this time step.")

with tab2:
    if 'hotspot_sources' in data:
        st.write("Hotspot Sources:", data['hotspot_sources'])
        st.write("Hotspot Destinations:", data['hotspot_dests'])
    else:
        st.info("No hotspot data available.")

