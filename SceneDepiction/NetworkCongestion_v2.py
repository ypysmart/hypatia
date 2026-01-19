import numpy as np
import networkx as nx
import matplotlib.pyplot as plt
import heapq
import random
import math
import pickle
import os
from collections import deque
from dataclasses import dataclass, field
from typing import List, Dict, Optional
import sys
from datetime import datetime

# ==========================================
# 1. 全局参数配置 (Constants)
# ==========================================

class Config:
    # --- 仿真时间参数 ---
    TOTAL_TIME = 2.0        # 模拟总时长 (分钟)
    DT = 0.1                 # 时间步长 (分钟)
    
    # --- 星座参数 (Walker简化) ---
    NUM_PLANES = 6           # P: 轨道平面数
    SATS_PER_PLANE = 11      # S: 每个平面卫星数
    TOTAL_SATS = 66          # N = P * S
    
    # --- 轨道动力学参数 ---
    R_EARTH = 6371.0         # 地球半径 (km)
    H_ORBIT = 1000.0         # 轨道高度 (km)
    INCLINATION = np.radians(53.0) # 倾角 (弧度)
    ORBIT_PERIOD = 100.0     # 轨道周期 (分钟)
    OMEGA_VEL = 2 * np.pi / ORBIT_PERIOD # 角速度 (rad/min)
    
    # --- 链路参数 ---
    LINK_CAPACITY_MBPS = 5.0 # C: 链路容量 (Mbps)
    LINK_DELAY_MS = 20.0        # D: 传播延迟 (ms)
    BUFFER_SIZE_BYTES = 50 * 1024 * 1024 # K: 缓冲区上限 50MB
    
    # --- 流量切片参数 ---
    SLICE_PARAMS = {
        'mMTC':  {'alpha': 0.005, 'beta': 0.5, 'type': 'fixed', 'size_base': 100},
        'eMBB':  {'alpha': 0.8,   'beta': 1.5, 'type': 'pareto', 'shape': 1.5, 'mode': 1500}, 
        'URLLC': {'alpha': 0.0,   'beta': 0.0, 'type': 'fixed', 'size_base': 64}, 
        'Bulk':  {'alpha': 0.2,   'beta': 1.0, 'type': 'lognormal', 'mean': 1000, 'sigma': 1.0}
    }
    
    # Hawkes过程参数
    MU_BACKGROUND = 10
    MEMORY_KERNEL_C = 0.5
    MEMORY_KERNEL_THETA = 0.2
    
    # 热点参数
    HOTSPOT_RATIO = 1
    HOTSPOT_SET_SIZE = 3

# ==========================================
# 2. 基础数据结构
# ==========================================

@dataclass
class Packet:
    packet_id: int
    flow_id: int
    src: int
    dst: int
    size_bytes: int
    slice_type: str
    gen_time: float
    
    path: List[int] = field(default_factory=list)
    current_hop_idx: int = 0
    
    def __lt__(self, other):
        return self.gen_time < other.gen_time

# ==========================================
# 3. 卫星节点与轨道动力学
# ==========================================

class Satellite:
    def __init__(self, sat_id, plane_idx, sat_idx_in_plane):
        self.id = sat_id
        self.p = plane_idx
        self.s = sat_idx_in_plane
        self.raan = (2 * np.pi / Config.NUM_PLANES) * self.p
        self.M0 = (2 * np.pi / Config.SATS_PER_PLANE) * self.s 
        self.pos_x = 0.0
        self.pos_y = 0.0
        self.pos_z = 0.0
        
    def update_position(self, t):
        M_t = self.M0 + Config.OMEGA_VEL * t
        r = Config.R_EARTH + Config.H_ORBIT
        cos_O = np.cos(self.raan)
        sin_O = np.sin(self.raan)
        cos_M = np.cos(M_t)
        sin_M = np.sin(M_t)
        cos_i = np.cos(Config.INCLINATION)
        sin_i = np.sin(Config.INCLINATION)
        
        self.pos_x = r * (cos_O * cos_M - sin_O * sin_M * cos_i)
        self.pos_y = r * (sin_O * cos_M + cos_O * sin_M * cos_i)
        self.pos_z = r * (sin_M * sin_i)

# ==========================================
# 4. 链路与排队模型
# ==========================================

class Link:
    def __init__(self, u, v):
        self.u = u
        self.v = v
        self.capacity = Config.LINK_CAPACITY_MBPS
        self.delay = Config.LINK_DELAY_MS
        self.limit = Config.BUFFER_SIZE_BYTES
        self.queue = deque()
        self.current_buffer_load = 0
        self.dropped_bytes = 0
        self.total_arrived_bytes = 0
        
    def enqueue(self, packet):
        self.total_arrived_bytes += packet.size_bytes
        if self.current_buffer_load + packet.size_bytes > self.limit:
            self.dropped_bytes += packet.size_bytes
            return False
        else:
            self.queue.append(packet)
            self.current_buffer_load += packet.size_bytes
            return True
            
    def process_transmission(self, dt_minutes):
        dt_seconds = dt_minutes * 60.0
        bytes_can_send = (self.capacity * 1024 * 1024 / 8.0) * dt_seconds
        transferred_packets = []
        
        while self.queue and bytes_can_send > 0:
            pkt = self.queue[0]
            if bytes_can_send >= pkt.size_bytes:
                p = self.queue.popleft()
                self.current_buffer_load -= p.size_bytes
                bytes_can_send -= p.size_bytes
                transferred_packets.append(p)
            else:
                break
        return transferred_packets

# ==========================================
# 5. 流量生成模型 (核心修改)
# ==========================================

class TrafficSource:
    # [核心修改 1/2] 添加一个类级别的计数器，用于生成自增ID
    _packet_id_counter = 0

    def __init__(self, flow_id, src_id, dst_id, slice_type):
        self.flow_id = flow_id
        self.src_id = src_id
        self.dst_id = dst_id
        self.slice_type = slice_type
        self.params = Config.SLICE_PARAMS[slice_type]
        self.history = [] 
        
    def _get_packet_size(self):
        p = self.params
        if p['type'] == 'fixed': return p['size_base']
        elif p['type'] == 'pareto': return int((np.random.pareto(p['shape']) + 1) * p['mode'])
        elif p['type'] == 'lognormal': return int(np.random.lognormal(np.log(p['mean']), p['sigma']))
        return 1000

    def _impact_function(self, size_bytes):
        m_kb = size_bytes / 1024.0
        return self.params['alpha'] * (m_kb ** self.params['beta'])

    def _memory_kernel(self, delta_t):
        if delta_t <= 0: return 0
        return (delta_t + Config.MEMORY_KERNEL_C) ** (-(1 + Config.MEMORY_KERNEL_THETA))

    def generate_traffic(self, current_time, dt):
        intensity = Config.MU_BACKGROUND
        cleanup_idx = 0
        current_sum = 0.0
        
        for i, (t_i, m_i) in enumerate(self.history):
            if current_time - t_i > 5.0:
                cleanup_idx = i
                continue
            phi = self._impact_function(m_i)
            nu = self._memory_kernel(current_time - t_i)
            current_sum += phi * nu
            
        if cleanup_idx > 0:
            self.history = self.history[cleanup_idx:]
            
        intensity += current_sum
        expected_events = intensity * dt
        num_events = np.random.poisson(expected_events)
        
        new_packets = []

        if num_events > 0:
            print(f"--- [t={current_time:.1f} min] Flow {self.flow_id} ({self.src_id}->{self.dst_id}) generating {num_events} packets ---")

        for i in range(num_events):
            size = self._get_packet_size()
            self.history.append((current_time, size))
            
            # [核心修改 2/2] 使用自增ID，并递增计数器
            pkt = Packet(
                packet_id=TrafficSource._packet_id_counter,
                flow_id=self.flow_id,
                src=self.src_id,
                dst=self.dst_id,
                size_bytes=size,
                slice_type=self.slice_type,
                gen_time=current_time
            )
            TrafficSource._packet_id_counter += 1
            
            print(
                f"    [Packet Generated] Flow: {pkt.flow_id}, "
                f"Pkt_ID: {pkt.packet_id}, "
                f"Path: {pkt.src}->{pkt.dst}, "
                f"Size: {pkt.size_bytes} bytes, "
                f"Slice: {pkt.slice_type}, "
                f"Time: {pkt.gen_time:.2f}"
            )
            new_packets.append(pkt)
            
        return new_packets

# ==========================================
# 6. 网络与仿真引擎
# ==========================================

class NetworkSimulator:
    def __init__(self):
        self.satellites = []
        self.links = {} 
        self.graph = nx.Graph() 
        self.traffic_sources = []
        
        all_ids = list(range(Config.TOTAL_SATS))
        self.hotspots = random.sample(all_ids, Config.HOTSPOT_SET_SIZE)
        
        self.stats_time = []
        self.stats_congestion_ratio = []
        self.stats_loss_rate = []

        self.routing_table = None

    def build_topology(self):
        P = Config.NUM_PLANES
        S = Config.SATS_PER_PLANE
        
        for p in range(P):
            for s in range(S):
                sat_id = p * S + s
                sat = Satellite(sat_id, p, s)
                self.satellites.append(sat)
                self.graph.add_node(sat_id)
        
        for sat in self.satellites:
            p, s = sat.p, sat.s
            u = sat.id
            neighbors = [
                p * S + (s + 1) % S,
                p * S + (s - 1 + S) % S,
                ((p + 1) % P) * S + s,
                ((p - 1 + P) % P) * S + s
            ]
            for v in neighbors:
                if not self.graph.has_edge(u, v):
                    self.graph.add_edge(u, v, weight=Config.LINK_DELAY_MS)
                    self.links[(u, v)] = Link(u, v)
                    self.links[(v, u)] = Link(v, u)

    def load_routing_table(self, filename="routing_table.pkl"):
        if not os.path.exists(filename):
            print(f"[Error] Routing table file '{filename}' not found.")
            print("Please run the routing generator script first to create this file.")
            exit(1)
            
        print(f"[-] Loading routing table from '{filename}'...")
        try:
            with open(filename, 'rb') as f:
                self.routing_table = pickle.load(f)
            print("[+] Routing table loaded successfully.")
        except Exception as e:
            print(f"[Error] Failed to load routing table: {e}")
            exit(1)

    def init_traffic_flows(self, num_flows=10):
        print("\n" + "="*20 + " Initializing Traffic Flows " + "="*20)
        slice_types = list(Config.SLICE_PARAMS.keys())
        
        created_flows = 0
        flow_id_counter = 1
        while created_flows < num_flows:
            is_src_hot = False
            is_dst_hot = False
            
            if random.random() < Config.HOTSPOT_RATIO:
                src = random.choice(self.hotspots)
                is_src_hot = True
            else:
                src = random.randint(0, Config.TOTAL_SATS - 1)
                
            if random.random() < Config.HOTSPOT_RATIO:
                dst = random.choice(self.hotspots)
                is_dst_hot = True
            else:
                dst = random.randint(0, Config.TOTAL_SATS - 1)
            
            if src == dst: 
                continue

            stype = random.choice(slice_types)
            is_hotspot_flow = is_src_hot or is_dst_hot

            print(
                f"[Flow Created] ID: {flow_id_counter}, "
                f"Path: {src}->{dst}, "
                f"Slice: {stype}, "
                f"Hotspot-Related: {'Yes' if is_hotspot_flow else 'No'}"
            )
            
            self.traffic_sources.append(TrafficSource(flow_id_counter, src, dst, stype))

            created_flows += 1
            flow_id_counter += 1
            
        print("="*62 + "\n")

    def get_route(self, src, dst):
        if self.routing_table is not None:
            try:
                return self.routing_table[src][dst]
            except KeyError:
                return []
        
        try:
            return nx.shortest_path(self.graph, source=src, target=dst, weight='weight')
        except nx.NetworkXNoPath:
            return []

    def run_simulation(self):
        print(f"Start Simulation: {Config.TOTAL_TIME} mins, Step: {Config.DT} mins")
        
        time_steps = np.arange(0, Config.TOTAL_TIME, Config.DT)
        total_steps = len(time_steps)
        
        for step, t in enumerate(time_steps):
            if step % 10 == 0:
                progress = (step / total_steps) * 100
                print(f"\n>>> Simulation Progress: {t:.1f} min ({progress:.1f}%) <<<")
            
            for sat in self.satellites:
                sat.update_position(t)
            
            new_packets = []
            for flow in self.traffic_sources:
                pkts = flow.generate_traffic(t, Config.DT)
                new_packets.extend(pkts)
            
            for pkt in new_packets:
                pkt.path = self.get_route(pkt.src, pkt.dst)
                
                if not pkt.path or len(pkt.path) < 2:
                    continue
                
                next_node = pkt.path[1]
                link_key = (pkt.src, next_node)
                
                if link_key in self.links:
                    self.links[link_key].enqueue(pkt)
            
            total_transferred = 0
            for (u, v), link in self.links.items():
                transferred = link.process_transmission(Config.DT)
                total_transferred += len(transferred)
                
                for pkt in transferred:
                    pkt.current_hop_idx += 1
                    if pkt.current_hop_idx >= len(pkt.path) - 1:
                        continue 
                    
                    current_node = pkt.path[pkt.current_hop_idx]
                    next_node = pkt.path[pkt.current_hop_idx + 1]
                    next_link_key = (current_node, next_node)
                    if next_link_key in self.links:
                        self.links[next_link_key].enqueue(pkt)
            
            self.collect_metrics(t)

    def collect_metrics(self, t):
        congested_count = 0
        total_links = len(self.links)
        total_dropped = 0
        total_arrived = 0
        
        for link in self.links.values():
            if link.current_buffer_load > 0.9 * link.limit:
                congested_count += 1
            total_dropped += link.dropped_bytes
            total_arrived += link.total_arrived_bytes
        
        congestion_ratio = congested_count / total_links if total_links > 0 else 0
        loss_rate = total_dropped / total_arrived if total_arrived > 0 else 0
        
        self.stats_time.append(t)
        self.stats_congestion_ratio.append(congestion_ratio)
        self.stats_loss_rate.append(loss_rate)

# ==========================================
# 7. 主程序入口与可视化
# ==========================================

if __name__ == "__main__":
    # 创建带时间戳和指定目录的日志文件
    output_dir = "SceneDepiction"
    os.makedirs(output_dir, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file_name = f"simulation_log_{timestamp}.txt"
    log_file_path = os.path.join(output_dir, log_file_name)

    original_stdout = sys.stdout
    
    print(f"Simulation started. All console output will be redirected to '{log_file_path}'")

    # 重定向所有print输出到日志文件
    with open(log_file_path, 'w', encoding='utf-8') as log_file:
        sys.stdout = log_file

        sim = NetworkSimulator()
        sim.build_topology()
        
        if os.path.exists("routing_table.pkl"):
            sim.load_routing_table("routing_table.pkl")
        else:
            print("[Warning] 'routing_table.pkl' not found. Using slower on-the-fly routing.")

        sim.init_traffic_flows(num_flows=5)
        
        sim.run_simulation()
        
        if sim.stats_congestion_ratio and sim.stats_loss_rate:
            print(f"\nFinal Congestion Ratio: {sim.stats_congestion_ratio[-1]:.4f}")
            print(f"Final Packet Loss Rate: {sim.stats_loss_rate[-1]:.4f}")

    # 恢复标准输出
    sys.stdout = original_stdout
    
    print(f"\nSimulation finished. All logs have been saved to '{log_file_path}'")
    
    # 可视化部分仍然在控制台显示，并弹出图表
    if sim.stats_time:
        plt.figure(figsize=(12, 5))
        plt.subplot(1, 2, 1)
        plt.plot(sim.stats_time, sim.stats_congestion_ratio, 'b-', label='Link Congestion Ratio')
        plt.xlabel('Time (min)')
        plt.ylabel('Ratio')
        plt.title('Network Congestion Evolution')
        plt.grid(True)
        plt.legend()
        
        plt.subplot(1, 2, 2)
        plt.plot(sim.stats_time, sim.stats_loss_rate, 'r-', label='System Packet Loss Rate')
        plt.xlabel('Time (min)')
        plt.ylabel('Loss Rate')
        plt.title('Packet Loss Rate Evolution')
        plt.grid(True)
        plt.legend()
        
        plt.tight_layout()
        plt.show()

        # 再次在控制台打印最终结果
        if sim.stats_congestion_ratio and sim.stats_loss_rate:
            print(f"Final Congestion Ratio: {sim.stats_congestion_ratio[-1]:.4f}")
            print(f"Final Packet Loss Rate: {sim.stats_loss_rate[-1]:.4f}")
    else:
        print("No simulation statistics were collected to generate plots.")

