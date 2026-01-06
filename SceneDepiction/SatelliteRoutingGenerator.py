import networkx as nx
import numpy as np
import pickle
import time
import os

# ==========================================
# 1. 配置参数 (保持与原代码一致)
# ==========================================
class Config:
    # --- 星座参数 (Walker简化) ---
    NUM_PLANES = 6           # P: 轨道平面数
    SATS_PER_PLANE = 11      # S: 每个平面卫星数
    TOTAL_SATS = 66          # N = P * S
    
    # --- 链路参数 ---
    LINK_DELAY_MS = 20.0     # 链路延迟/权重

# ==========================================
# 2. 卫星与拓扑构建类
# ==========================================
class TopologyManager:
    def __init__(self):
        self.satellites = []
        self.graph = nx.Graph() # 无向图

    def build_topology(self):
        """
        构建Walker星座拓扑，逻辑与原代码完全一致
        """
        P = Config.NUM_PLANES
        S = Config.SATS_PER_PLANE
        
        print(f"[-] Initializing Topology: {P} Planes, {S} Sats/Plane...")

        # 添加节点
        for p in range(P):
            for s in range(S):
                sat_id = p * S + s
                self.graph.add_node(sat_id)
        
        # 添加链路 (每个卫星4条边)
        edge_count = 0
        for p in range(P):
            for s in range(S):
                u = p * S + s
                
                # 邻居列表
                neighbors = []
                
                # 1. 平面内前向: (p, (s+1)%S)
                v_intra_f = p * S + (s + 1) % S
                neighbors.append(v_intra_f)
                
                # 2. 平面内后向: (p, (s-1)%S)
                v_intra_b = p * S + (s - 1 + S) % S
                neighbors.append(v_intra_b)
                
                # 3. 相邻平面右向: ((p+1)%P, s)
                v_inter_r = ((p + 1) % P) * S + s
                neighbors.append(v_inter_r)
                
                # 4. 相邻平面左向: ((p-1)%P, s)
                v_inter_l = ((p - 1 + P) % P) * S + s
                neighbors.append(v_inter_l)
                
                for v in neighbors:
                    # 避免重复添加边 (无向图 u-v 和 v-u 是一样的)
                    if not self.graph.has_edge(u, v):
                        self.graph.add_edge(u, v, weight=Config.LINK_DELAY_MS)
                        edge_count += 1
        
        print(f"[+] Topology Built. Nodes: {self.graph.number_of_nodes()}, Edges: {edge_count}")

# ==========================================
# 3. 路由计算引擎
# ==========================================
def generate_and_save_routes(topology_manager, filename="routing_table.pkl"):
    """
    计算所有点对点的最短路径并保存
    格式: dict[source_id][target_id] = [node_path_list]
    """
    graph = topology_manager.graph
    all_nodes = list(graph.nodes())
    total_nodes = len(all_nodes)
    
    # 最终的大表
    routing_table = {}

    print("\n[Start] Calculating shortest paths for all nodes...")
    start_time = time.time()

    # 遍历每一个源节点
    for i, src in enumerate(all_nodes):
        # 使用 Dijkstra 算法计算从 src 到图中所有其他节点的最短路径
        # networkx 的 single_source_dijkstra_path 会返回一个字典: {target: [path_nodes]}
        paths = nx.single_source_dijkstra_path(graph, src, weight='weight')
        
        routing_table[src] = paths
        
        # 计算一步输出一个日志
        elapsed = time.time() - start_time
        print(f" -> [Step {i+1}/{total_nodes}] Computed routes for Node {src} (Found {len(paths)} targets) | Elapsed: {elapsed:.2f}s")

    print("\n[Saving] Writing routing table to file...")
    
    # 保存为 Pickle 文件
    try:
        with open(filename, 'wb') as f:
            pickle.dump(routing_table, f)
        
        file_size = os.path.getsize(filename) / 1024  # KB
        print(f"[Success] Routing table saved to '{filename}' ({file_size:.2f} KB)")
        print("Done.")
        
    except Exception as e:
        print(f"[Error] Failed to save file: {e}")

# ==========================================
# 4. 主程序入口
# ==========================================
if __name__ == "__main__":
    # 1. 建立拓扑
    topo = TopologyManager()
    topo.build_topology()
    
    # 2. 计算并保存
    generate_and_save_routes(topo)
