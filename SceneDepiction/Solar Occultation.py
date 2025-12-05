import numpy as np
from datetime import datetime, timedelta
from astropy.time import Time
from astropy.coordinates import get_sun, EarthLocation
from astropy import units as u
from astropy.coordinates import CartesianDifferential, CartesianRepresentation, ITRS, GCRS

# --- 1. 常量和参数设置 ---
# 地球参数 (使用 WGS-84 常量)
R_E = 6378.137e3  # 地球赤道半径 (m)
MU_E = 3.986004418e14  # 地球标准引力参数 (m^3/s^2)

# 星座参数 (Walker 1200/40/1)
N_SAT = 1200
P_PLANES = 40
S_PER_PLANE = 30
F_PHASE = 1

H_ALTITUDE = 1000e3  # 轨道高度 (m)
A_SEMI_MAJOR = R_E + H_ALTITUDE  # 半长轴 (m)
INCLINATION_DEG = 55.0  # 倾角 (度)
INCLINATION_RAD = np.deg2rad(INCLINATION_DEG)

# 仿真参数
T0_START = datetime(2025, 3, 20, 0, 0, 0)
DT_STEP = 150.0  # 时间步长 (秒)
T_TOTAL = 86400.0  # 仿真总时长 (秒, 1天)
NUM_STEPS = int(T_TOTAL / DT_STEP)

# 日凌参数
THETA_CRIT_DEG = 1.0  # 太阳排除角 (度)
THETA_CRIT_RAD = np.deg2rad(THETA_CRIT_DEG)

# --- 2. 核心函数 ---

def generate_walker_positions(a, inc_rad, P, S, F, t_seconds):
    """
    生成 Walker Delta 星座在给定时间的 ECI (Earth-Centered Inertial) 位置。
    由于没有考虑 J2 摄动，该函数提供了基础的轨道动力学近似。
    """
    
    # 平均角速度 (Mean Motion)
    n = np.sqrt(MU_E / a**3)
    
    positions_eci = np.zeros((N_SAT, 3))
    sat_index = 0
    
    for p in range(P):  # 轨道平面
        # 升交点赤经 (RAAN)
        RAAN = p * 2 * np.pi / P
        
        for s in range(S):  # 每平面卫星
            # 初始真近点角 (Initial True Anomaly, 假设圆形轨道, 即平均近点角)
            M0 = s * 2 * np.pi / S + (p * F * 2 * np.pi) / (N_SAT)
            
            # 当前平均近点角 (Mean Anomaly)
            M_current = (M0 + n * t_seconds) % (2 * np.pi)
            
            # --- 轨道坐标系 (Perifocal Frame) ---
            r_perifocal = np.array([
                a * np.cos(M_current),
                a * np.sin(M_current),
                0.0
            ])
            
            # --- 转换到 ECI 坐标系 ---
            # 旋转矩阵 R3(-RAAN) * R1(-inc) * R3(-w) => w=0 (圆形)
            
            cos_RAAN = np.cos(RAAN)
            sin_RAAN = np.sin(RAAN)
            cos_inc = np.cos(inc_rad)
            sin_inc = np.sin(inc_rad)
            
            # 轨道到 ECI 转换矩阵
            R_ECI_to_Perifocal = np.array([
                [cos_RAAN, -sin_RAAN, 0],
                [sin_RAAN, cos_RAAN, 0],
                [0, 0, 1]
            ]) @ np.array([
                [1, 0, 0],
                [0, cos_inc, -sin_inc],
                [0, sin_inc, cos_inc]
            ])
            
            # 最终的 ECI 转换矩阵 (简化后的 Rz(RAAN) * Rx(inc) * Rz(M_current))
            # 注意: 这是直接从轨道元素到 ECI 的转换，由于是圆形轨道，偏心率 e=0, 近点角 w=0
            
            r_x = (cos_RAAN * np.cos(M_current) - sin_RAAN * np.sin(M_current) * cos_inc) * a
            r_y = (sin_RAAN * np.cos(M_current) + cos_RAAN * np.sin(M_current) * cos_inc) * a
            r_z = (np.sin(M_current) * sin_inc) * a
            
            positions_eci[sat_index] = [r_x, r_y, r_z]
            sat_index += 1
            
    return positions_eci

def get_sun_position_eci(current_time):
    """
    获取给定时间点的太阳在 ECI 坐标系下的位置向量 (m)。
    """
    # 使用 Astropy 的高精度内置函数
    t_astropy = Time(current_time, format='datetime', scale='utc')
    # GCRS 是一个近似 ECI 的天体坐标系
    sun_gcrs = get_sun(t_astropy).cartesian.xyz.to(u.m).value
    return sun_gcrs

def check_sun_interference(pos_satellites_eci, pos_sun_eci, crit_angle_rad):
    """
    检查所有可能的星间链路是否受到日凌影响。
    """
    N = pos_satellites_eci.shape[0]
    sun_interference_links = []
    
    # 太阳位置向量 (Rx -> Sun)
    # 在 ECI 坐标系中，Rx 卫星的位置 r_Rx 也是从 Rx 指向 Sun 的向量
    # 太阳位置: pos_sun_eci
    # 卫星位置: pos_satellites_eci[rx_idx]
    
    for tx_idx in range(N):
        for rx_idx in range(tx_idx + 1, N): # 仅检查 tx -> rx (无向链路)
            
            # 1. 链路向量 (Tx -> Rx)
            r_link = pos_satellites_eci[rx_idx] - pos_satellites_eci[tx_idx]
            r_link_norm = np.linalg.norm(r_link)
            
            # --- 2. 检查 Rx 处的日凌影响 ---
            # 接收卫星 (Rx) 处的太阳向量 (Rx -> Sun)
            r_sun_rx = pos_sun_eci - pos_satellites_eci[rx_idx]
            
            # 计算分离角 (Separation Angle)
            # 夹角 arccos((v_link . v_sun) / (|v_link|*|v_sun|))
            
            # 归一化向量
            u_link = r_link / r_link_norm
            u_sun_rx = r_sun_rx / np.linalg.norm(r_sun_rx)
            
            # 点积
            dot_product_rx = np.dot(u_link, u_sun_rx)
            # 确保点积在 [-1, 1] 范围内，避免浮点误差
            dot_product_rx = np.clip(dot_product_rx, -1.0, 1.0)
            
            # 分离角
            angle_rad_rx = np.arccos(dot_product_rx)
            
            # 3. 日凌判断
            if angle_rad_rx < crit_angle_rad:
                # 链路中断 (Tx -> Rx)
                interference_info = {
                    'Tx_ID': tx_idx,
                    'Rx_ID': rx_idx,
                    'Angle_Deg': np.rad2deg(angle_rad_rx),
                    'Direction': 'Tx->Rx (Rx 受影响)',
                    'Distance_km': r_link_norm / 1e3
                }
                sun_interference_links.append(interference_info)
            
            # --- 4. 检查 Tx 处的日凌影响 (Rx -> Tx 链路方向) ---
            # 太阳向量 (Tx -> Sun)
            r_sun_tx = pos_sun_eci - pos_satellites_eci[tx_idx]
            
            # 链路向量 (Rx -> Tx) 是 -r_link
            u_link_rev = -u_link
            u_sun_tx = r_sun_tx / np.linalg.norm(r_sun_tx)
            
            dot_product_tx = np.dot(u_link_rev, u_sun_tx)
            dot_product_tx = np.clip(dot_product_tx, -1.0, 1.0)
            
            angle_rad_tx = np.arccos(dot_product_tx)
            
            if angle_rad_tx < crit_angle_rad:
                # 链路中断 (Rx -> Tx)
                interference_info = {
                    'Tx_ID': rx_idx, # 我们总是将发送方标记为受影响方
                    'Rx_ID': tx_idx,
                    'Angle_Deg': np.rad2deg(angle_rad_tx),
                    'Direction': 'Rx->Tx (Tx 受影响)',
                    'Distance_km': r_link_norm / 1e3
                }
                sun_interference_links.append(interference_info)
                
    return sun_interference_links

# --- 3. 仿真主循环 ---

print(f"## 启动 Walker {N_SAT}/{P_PLANES}/{S_PER_PLANE} 星座日凌模拟")
print(f"   - 卫星总数: {N_SAT} 颗")
print(f"   - 轨道高度: {H_ALTITUDE/1e3:.0f} km")
print(f"   - 太阳排除角 (Theta_crit): {THETA_CRIT_DEG} 度")
print(f"   - 仿真开始时间: {T0_START.isoformat()}")
print("-" * 50)

# 存储结果
simulation_results = []
current_time = T0_START

for step in range(NUM_STEPS + 1):
    t_elapsed = step * DT_STEP
    current_time = T0_START + timedelta(seconds=t_elapsed)
    
    # 1. 计算所有卫星的位置 (ECI)
    sat_positions = generate_walker_positions(A_SEMI_MAJOR, INCLINATION_RAD, P_PLANES, S_PER_PLANE, F_PHASE, t_elapsed)
    
    # 2. 计算太阳的位置 (ECI)
    sun_position = get_sun_position_eci(current_time)
    
    # 3. 检查日凌影响
    # 注意: 这个检查过程涉及 N_SAT * (N_SAT - 1) / 2 约为 72 万次链路几何计算，计算量巨大。
    interferences = check_sun_interference(sat_positions, sun_position, THETA_CRIT_RAD)
    
    # 4. 记录和输出结果
    
    # 统计链路总数 (假设所有卫星都与所有其他卫星形成 ISL)
    # N_links = N_SAT * (N_SAT - 1) / 2 = 719,400 (无向)
    # N_links_directional = N_SAT * (N_SAT - 1) = 1,438,800 (有向)
    
    if len(interferences) > 0:
        total_unique_links = N_SAT * (N_SAT - 1)
        
        step_result = {
            'Time_UTC': current_time.isoformat(),
            'Step': step,
            'Total_Interferences': len(interferences),
            'Interference_Details': interferences
        }
        simulation_results.append(step_result)
        
        # 打印详细结果
        print(f"## 🕒 时间步 {step} (T={t_elapsed/3600:.2f}h) - {current_time.strftime('%H:%M:%S')} UTC")
        print(f"   **总受影响链路数**: {len(interferences)}")
        
        # 仅打印前 3 条受影响的链路作为示例
        for i, link in enumerate(interferences[:3]):
            print(f"   - 链路 {link['Tx_ID']} <-> {link['Rx_ID']}:")
            print(f"     -> 方向: {link['Direction']}")
            print(f"     -> 分离角: {link['Angle_Deg']:.2f}° (临界角: {THETA_CRIT_DEG}°) **中断/影响**")
        if len(interferences) > 3:
            print("   - ... 更多链路受影响 ...")
    
    # 打印一个状态点，即使没有中断
    if step % (NUM_STEPS // 10 + 1) == 0 and len(interferences) == 0:
        print(f"🕒 时间步 {step} (T={t_elapsed/3600:.2f}h) - {current_time.strftime('%H:%M:%S')} UTC: 无日凌影响")

print("-" * 50)
print("## ✅ 仿真结束。")
print(f"   - 总共发生日凌影响的时间步数: {len(simulation_results)} / {NUM_STEPS + 1}")