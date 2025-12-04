import numpy as np
from astropy.coordinates import SkyCoord, EarthLocation, AltAz, get_sun
from astropy.time import Time
import astropy.units as u
from scipy.spatial.distance import cdist
import networkx as nx
import matplotlib.pyplot as plt
from datetime import datetime, timedelta

# 参数设置：Walker星座配置
NUM_SATELLITES = 1584  # 总卫星数 >1000
NUM_PLANES = 72  # 轨道平面数
SAT_PER_PLANE = NUM_SATELLITES // NUM_PLANES  # 每个平面卫星数 ≈22
INCLINATION = 53 * u.deg  # 倾角
ORBITAL_ALTITUDE = 800  # km
EARTH_RADIUS = 6371  # km
GM = 3.986004418e14  # m^3/s^2，地球引力常数
ORBITAL_PERIOD = 2 * np.pi * np.sqrt((EARTH_RADIUS + ORBITAL_ALTITUDE)**3 * 1e9 / GM)  # 轨道周期（秒）

PHASE_FACTOR = 1  # Walker相位因子
LINK_RANGE_MAX = 5000  # km，最大链路距离
SUN_OUTAGE_ANGLE = 0.5 * u.deg  # 日凌角度阈值
SIM_DURATION = 3600  # 模拟时长（秒，1小时）
TIME_STEP = 1  # 时间步长（秒）

BASE_TRAFFIC = 1e9  # 基础流量 1 Gbps per link
PACKET_SIZE = 1500  # 字节 per packet
SNR_NORMAL = 20  # dB，正常SNR
SNR_OUTAGE_DROP = 10  # dB，日凌时SNR下降

# 计算Walker星座卫星初始位置（简化：使用均布假设）
def generate_walker_positions(t=0):
    positions = []  # ECEF坐标 (x,y,z) in km
    for p in range(NUM_PLANES):
        raan = (360 / NUM_PLANES * p) * u.deg  # 升交点赤经
        for s in range(SAT_PER_PLANE):
            anomaly = (360 / SAT_PER_PLANE * s + PHASE_FACTOR * 360 / NUM_SATELLITES * p) * u.deg
            # 简化轨道计算：假设圆轨道
            theta = anomaly + (360 * u.deg / ORBITAL_PERIOD) * t  # 时间演化
            x = (EARTH_RADIUS + ORBITAL_ALTITUDE) * (np.cos(theta) * np.cos(raan) - np.sin(theta) * np.sin(raan) * np.cos(INCLINATION))
            y = (EARTH_RADIUS + ORBITAL_ALTITUDE) * (np.cos(theta) * np.sin(raan) + np.sin(theta) * np.cos(raan) * np.cos(INCLINATION))
            z = (EARTH_RADIUS + ORBITAL_ALTITUDE) * (np.sin(theta) * np.sin(INCLINATION))
            positions.append([x, y, z])
    return np.array(positions)

# 构建ISL网络拓扑
def build_isl_graph(positions):
    G = nx.Graph()
    dist_matrix = cdist(positions, positions)  # 计算所有卫星间距离
    for i in range(NUM_SATELLITES):
        G.add_node(i, pos=positions[i])
        for j in range(i+1, NUM_SATELLITES):
            if dist_matrix[i,j] <= LINK_RANGE_MAX:
                G.add_edge(i, j, weight=dist_matrix[i,j], traffic=BASE_TRAFFIC)
    return G

# 计算太阳位置（使用astropy）
def get_sun_position(time):
    t = Time(time)
    sun = get_sun(t)
    # 转换为ECEF（简化：假设太阳远距离，方向矢量）
    sun_cart = sun.cartesian.xyz.to(u.km)
    return np.array(sun_cart.value) / np.linalg.norm(sun_cart.value)  # 单位矢量

# 检查日凌：太阳是否在链路视线内
def is_sun_outage(pos1, pos2, sun_dir, angle_thresh=SUN_OUTAGE_ANGLE):
    link_vec = pos2 - pos1
    link_unit = link_vec / np.linalg.norm(link_vec)
    angle = (np.arccos(np.dot(link_unit, sun_dir)) * u.radian).to(u.deg)    
    return angle < angle_thresh

# 模拟流量和影响
def simulate_traffic(G, outages):
    total_traffic = 0
    lost_traffic = 0
    for u, v in G.edges():
        traffic = G[u][v]['traffic']
        total_traffic += traffic
        if (u,v) in outages or (v,u) in outages:
            # 日凌时，假设50%流量丢失（或重路由失败）
            lost_traffic += traffic * 0.5
            # 模拟BER增加：基于SNR计算
            snr = SNR_NORMAL - SNR_OUTAGE_DROP
            ber = 0.5 * np.exp(-snr / 2)  # 简化QPSK BER模型
            print(f"Link {u}-{v}: SNR={snr}dB, BER={ber:.2e}")
    return total_traffic, lost_traffic

# 主模拟函数
def run_simulation():
    start_time = datetime(2025, 12, 4, 0, 0, 0)  # 模拟起始时间
    outage_count = 0
    outages = set()
    positions_over_time = []
    sun_dirs = []

    for step in range(0, SIM_DURATION, TIME_STEP):
        current_time = start_time + timedelta(seconds=step)
        positions = generate_walker_positions(t=step)
        positions_over_time.append(positions)
        sun_dir = get_sun_position(current_time)
        sun_dirs.append(sun_dir)

        G = build_isl_graph(positions)

        # 检查所有链路日凌
        for u, v in G.edges():
            if is_sun_outage(positions[u], positions[v], sun_dir):
                outages.add((u,v))
                outage_count += 1

    # 流量模拟
    total_traffic, lost_traffic = simulate_traffic(G, outages)
    availability = 1 - (len(outages) / G.number_of_edges()) if G.number_of_edges() > 0 else 1
    print(f"模拟结果：")
    print(f"日凌发生次数: {outage_count}")
    print(f"受影响链路数: {len(outages)}")
    print(f"链路可用率: {availability:.2%}")
    print(f"总流量: {total_traffic / 1e9:.2f} Gbps")
    print(f"丢失流量: {lost_traffic / 1e9:.2f} Gbps")

    # 可视化（简化：绘制最后时刻卫星位置）
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    ax.scatter(positions[:,0], positions[:,1], positions[:,2], s=5)
    ax.set_title('Walker Starlink-like Constellation (1584 sats)')
    plt.show()

# 运行模拟
if __name__ == "__main__":
    run_simulation()