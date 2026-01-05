# import re
# import matplotlib.pyplot as plt
# import os

# # Function to parse the log file and count OLIA occurrences per path per round
# def parse_log_and_count(file_path):
#     with open(file_path, 'r', encoding='utf-8') as file:
#         log_content = file.read()

#     # Regex to find round starts: "开始发送第 N 轮次"
#     round_pattern = re.compile(r'开始发送第\s*(\d+)\s*轮次')
#     # Regex to find OLIA block start
#     olia_start = "//////////////////////OLIA拥塞控制//////////////////////////"
#     # Regex to find pathId within OLIA block: "路径pathId= X"
#     path_pattern = re.compile(r'路径pathId=(\d+)')

#     rounds = {}  # Dictionary: round_num -> list of path_ids that had OLIA in that round
#     current_round = None
#     in_olia_block = False

#     for line in log_content.splitlines():
#         round_match = round_pattern.search(line)
#         if round_match:
#             current_round = int(round_match.group(1))
#             if current_round not in rounds:
#                 rounds[current_round] = []
#             continue

#         if olia_start in line and current_round is not None:
#             in_olia_block = True
#             continue

#         if in_olia_block:
#             path_match = path_pattern.search(line)
#             if path_match:
#                 path_id = int(path_match.group(1))
#                 rounds[current_round].append(path_id)
#                 in_olia_block = False  # Assume one path per OLIA block

#     # Now, count per path per round
#     # Find min and max round
#     if not rounds:
#         return None

#     min_round = min(rounds.keys())
#     max_round = max(rounds.keys())

#     # Assume paths 0,1,2
#     counts = {0: [0] * (max_round - min_round + 1),
#               1: [0] * (max_round - min_round + 1),
#               2: [0] * (max_round - min_round + 1)}

#     round_indices = list(range(min_round, max_round + 1))

#     for rnd, paths in rounds.items():
#         idx = rnd - min_round
#         for path in paths:
#             if path in counts:
#                 counts[path][idx] += 1

#     return round_indices, counts

# # Function to plot the counts in chunks of 100 points each, saving 100 figures
# def plot_counts_in_chunks(round_indices, counts, chunk_size=50, num_figures=200):
#     total_points = len(round_indices)
#     effective_chunk_size = total_points // num_figures
#     if effective_chunk_size == 0:
#         effective_chunk_size = total_points
#         num_figures = 1

#     for i in range(num_figures):
#         start_idx = i * effective_chunk_size
#         end_idx = min((i + 1) * effective_chunk_size, total_points)
        
#         chunk_rounds = round_indices[start_idx:end_idx]
#         chunk_counts_0 = counts[0][start_idx:end_idx]
#         chunk_counts_1 = counts[1][start_idx:end_idx]
#         chunk_counts_2 = counts[2][start_idx:end_idx]
        
#         plt.figure(figsize=(10, 6))
#         plt.plot(chunk_rounds, chunk_counts_0, label='Path 0', marker='o')
#         plt.plot(chunk_rounds, chunk_counts_1, label='Path 1', marker='s')
#         plt.plot(chunk_rounds, chunk_counts_2, label='Path 2', marker='^')
#         plt.xlabel('Round Number')
#         plt.ylabel('Number of OLIA Congestion Control Occurrences')
#         plt.title(f'OLIA Occurrences per Path (Rounds {chunk_rounds[0]}-{chunk_rounds[-1]})')
#         plt.legend()
#         plt.grid(True)
        
#         fig_path = f'plot_{i+1}.png'
#         plt.savefig(fig_path)
#         plt.close()
#         print(f'Saved {fig_path}')

# # Main execution
# if __name__ == "__main__":
#     file_path = 'AlgorithmLog/cwndconsole24.txt'  # Replace with your actual file path
#     result = parse_log_and_count(file_path)
#     if result:
#         round_indices, counts = result
#         plot_counts_in_chunks(round_indices, counts, 50,num_figures=200)
#     else:
#         print("No data found in the log.")



# import re
# import matplotlib.pyplot as plt
# import os

# # Function to parse the log file and count OnReceivedAckFrame occurrences per path per round
# def parse_log_and_count(file_path):
#     with open(file_path, 'r', encoding='utf-8') as file:
#         log_content = file.read()

#     # Regex to find round starts: "开始发送第 N 轮次"
#     round_pattern = re.compile(r'开始发送第\s*(\d+)\s*轮次')
#     # Regex to find OnReceivedAckFrame line: "进行拥塞控制 OnReceivedAckFrame pathId= X"
#     ack_pattern = re.compile(r'进行拥塞控制 OnReceivedAckFrame  pathId=(\d+)')

#     rounds = {}  # Dictionary: round_num -> list of path_ids that had Ack in that round
#     current_round = None

#     for line in log_content.splitlines():
#         round_match = round_pattern.search(line)
#         if round_match:
#             current_round = int(round_match.group(1))
#             if current_round not in rounds:
#                 rounds[current_round] = []
#             continue

#         if current_round is not None:
#             ack_match = ack_pattern.search(line)
#             if ack_match:
#                 path_id = int(ack_match.group(1))
#                 rounds[current_round].append(path_id)

#     # Now, count per path per round
#     # Find min and max round
#     if not rounds:
#         return None

#     min_round = min(rounds.keys())
#     max_round = max(rounds.keys())

#     # Assume paths 0,1,2
#     counts = {0: [0] * (max_round - min_round + 1),
#               1: [0] * (max_round - min_round + 1),
#               2: [0] * (max_round - min_round + 1)}

#     round_indices = list(range(min_round, max_round + 1))

#     for rnd, paths in rounds.items():
#         idx = rnd - min_round
#         for path in paths:
#             if path in counts:
#                 counts[path][idx] += 1

#     return round_indices, counts

# # Function to plot the counts in chunks, saving 100 figures
# def plot_counts_in_chunks(round_indices, counts, num_figures=100):
#     total_points = len(round_indices)
#     effective_chunk_size = total_points // num_figures
#     if effective_chunk_size == 0:
#         effective_chunk_size = total_points
#         num_figures = 1

#     for i in range(num_figures):
#         start_idx = i * effective_chunk_size
#         end_idx = min((i + 1) * effective_chunk_size, total_points)
        
#         if start_idx >= end_idx:
#             continue
        
#         chunk_rounds = round_indices[start_idx:end_idx]
#         chunk_counts_0 = counts[0][start_idx:end_idx]
#         chunk_counts_1 = counts[1][start_idx:end_idx]
#         chunk_counts_2 = counts[2][start_idx:end_idx]
        
#         plt.figure(figsize=(10, 6))
#         plt.plot(chunk_rounds, chunk_counts_0, label='Path 0', marker='o')
#         plt.plot(chunk_rounds, chunk_counts_1, label='Path 1', marker='s')
#         plt.plot(chunk_rounds, chunk_counts_2, label='Path 2', marker='^')
#         plt.xlabel('Round Number')
#         plt.ylabel('Number of OnReceivedAckFrame Occurrences')
#         plt.title(f'OnReceivedAckFrame Occurrences per Path (Rounds {chunk_rounds[0]}-{chunk_rounds[-1]})')
#         plt.legend()
#         plt.grid(True)
        
#         fig_path = f'ack_plot_{i+1}.png'
#         plt.savefig(fig_path)
#         plt.close()
#         print(f'Saved {fig_path}')

# # Main execution
# if __name__ == "__main__":
#     file_path = 'AlgorithmLog/cwndconsole24.txt'  # Replace with your actual file path
#     result = parse_log_and_count(file_path)
#     if result:
#         round_indices, counts = result
#         plot_counts_in_chunks(round_indices, counts, num_figures=200)
#     else:
#         print("No data found in the log.")
import re
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.lines import Line2D
import numpy as np
import os
from pathlib import Path

def parse_log_file(file_path):
    """
    解析日志文件，提取轮次、路径信息和cwnd值
    """
    # 确保文件存在
    if not os.path.exists(file_path):
        print(f"错误: 文件 '{file_path}' 不存在")
        return {}
    
    # 初始化数据结构
    rounds_data = {}
    current_round = 0
    in_round = False
    in_olia_block = False
    current_path_id = None
    current_cwnd = None
    current_state = None
    last_state_line = ""
    
    # 状态关键词
    state_keywords = {
        "In Congestion Avoidance": "congestion_avoidance",
        "In recovery": "recovery", 
        "In slow start": "slow_start",
        "In MinimumWindow": "minimum_window"
    }
    
    print(f"开始解析文件: {file_path}")
    
    with open(file_path, 'r', encoding='utf-8') as file:
        for line_num, line in enumerate(file, 1):
            line = line.strip()
            
            # 检测是否进入新轮次
            round_match = re.search(r'开始发送第\s*(\d+)\s*轮次', line)
            if round_match:
                current_round = int(round_match.group(1))
                in_round = True
                
                # 初始化当前轮次的数据结构
                if current_round not in rounds_data:
                    rounds_data[current_round] = {
                        'olia_count': {0: 0, 1: 0, 2: 0},  # 每个路径的OLIA出现次数
                        'cwnd_values': {0: None, 1: None, 2: None},  # 每个路径的cwnd值
                        'states': {0: None, 1: None, 2: None}  # 每个路径的状态
                    }
                continue
            
            # 检测OLIA拥塞控制块开始
            if '//////////////////////OLIA拥塞控制//////////////////////////' in line:
                in_olia_block = True
                continue
            
            # 如果在OLIA块中，查找状态信息
            if in_olia_block:
                # 检查是否有状态关键词
                for keyword, state in state_keywords.items():
                    if keyword in line:
                        current_state = state
                        last_state_line = line
                        break
                
                # 查找路径信息
                path_match = re.search(r'路径pathId=(\d+)', line)
                if path_match:
                    current_path_id = int(path_match.group(1))
                    
                    # 查找cwnd值
                    cwnd_match = re.search(r'cWnd=\s*(\d+)', line)
                    if cwnd_match:
                        current_cwnd = int(cwnd_match.group(1))
                    
                    # 记录信息到当前轮次
                    if current_round in rounds_data and current_path_id is not None:
                        # 增加OLIA计数
                        rounds_data[current_round]['olia_count'][current_path_id] += 1
                        
                        # 记录cwnd值
                        if current_cwnd is not None:
                            rounds_data[current_round]['cwnd_values'][current_path_id] = current_cwnd
                        
                        # 记录状态（从上一行获取）
                        if current_state:
                            rounds_data[current_round]['states'][current_path_id] = current_state
                        
                        # 重置当前变量
                        current_cwnd = None
                
                # 如果遇到空行或新的OLIA块结束，重置OLIA块状态
                if not line or line.startswith('//////////////////////////////////'):
                    in_olia_block = False
                    current_state = None
    
    return rounds_data

def generate_plots(rounds_data, output_dir='AlgorithmLog/plots41'):#/////////////////////////////////////////////////////////////////////////////////////////////////////
    """
    生成可视化图表，每50个轮次一张图
    """
    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)
    
    # 获取所有轮次
    all_rounds = sorted(rounds_data.keys())
    if not all_rounds:
        print("未找到任何轮次数据")
        return
    
    max_round = max(all_rounds)
    
    # 状态颜色映射
    state_colors = {
        'slow_start': 'green',
        'congestion_avoidance': 'blue',
        'recovery': 'orange',
        'minimum_window': 'red',
        None: 'gray'
    }
    
    # 路径颜色
    path_colors = {0: 'blue', 1: 'green', 2: 'red'}
    
    # 每50个轮次一组
    for start_round in range(1, max_round + 1, 50):
        end_round = min(start_round + 49, max_round)
        
        # 筛选当前组的轮次
        group_rounds = [r for r in range(start_round, end_round + 1) if r in rounds_data]
        
        if not group_rounds:
            continue
        
        # 创建图形
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 10))
        
        # 准备数据
        path0_olia_counts = []
        path1_olia_counts = []
        path2_olia_counts = []
        
        path0_cwnd = []
        path1_cwnd = []
        path2_cwnd = []
        
        path0_states = []
        path1_states = []
        path2_states = []
        
        valid_rounds = []
        
        for round_num in group_rounds:
            if round_num not in rounds_data:
                continue
                
            data = rounds_data[round_num]
            valid_rounds.append(round_num)
            
            # OLIA计数数据
            path0_olia_counts.append(data['olia_count'][0])
            path1_olia_counts.append(data['olia_count'][1])
            path2_olia_counts.append(data['olia_count'][2])
            
            # CWND数据
            path0_cwnd.append(data['cwnd_values'][0])
            path1_cwnd.append(data['cwnd_values'][1])
            path2_cwnd.append(data['cwnd_values'][2])
            
            # 状态数据
            path0_states.append(data['states'][0])
            path1_states.append(data['states'][1])
            path2_states.append(data['states'][2])
        
        # 图1：OLIA出现次数
        ax1.plot(valid_rounds, path0_olia_counts, 'b-', label='Path 0', linewidth=2, marker='o')
        ax1.plot(valid_rounds, path1_olia_counts, 'g-', label='Path 1', linewidth=2, marker='s')
        ax1.plot(valid_rounds, path2_olia_counts, 'r-', label='Path 2', linewidth=2, marker='^')
        
        ax1.set_title(f'OLIA拥塞控制出现次数 (轮次 {start_round}-{end_round})', fontsize=14, fontweight='bold')
        ax1.set_xlabel('轮次', fontsize=12)
        ax1.set_ylabel('OLIA出现次数', fontsize=12)
        ax1.legend(loc='best')
        ax1.grid(True, alpha=0.3)
        ax1.set_xticks(valid_rounds)
        
        # 图2：CWND值，用不同颜色标记状态
        # 为每个路径创建散点图，根据状态着色
        for path_id, (cwnd_values, states, color) in enumerate([
            (path0_cwnd, path0_states, 'blue'),
            (path1_cwnd, path1_states, 'green'),
            (path2_cwnd, path2_states, 'red')
        ]):
            # 创建散点，根据状态着色
            for i, (round_num, cwnd, state) in enumerate(zip(valid_rounds, cwnd_values, states)):
                if cwnd is not None:  # 只绘制有cwnd值的点
                    state_color = state_colors.get(state, 'gray')
                    ax2.scatter(round_num, cwnd, color=state_color, s=100, 
                               edgecolor=color, linewidth=2, zorder=5)
            
            # 创建连线（只连接有值的点）
            valid_points = [(r, c) for r, c in zip(valid_rounds, cwnd_values) if c is not None]
            if len(valid_points) > 1:
                x_vals, y_vals = zip(*sorted(valid_points))
                ax2.plot(x_vals, y_vals, color=color, linewidth=1, linestyle='--', alpha=0.7)
        
        ax2.set_title(f'CWND值及状态 (轮次 {start_round}-{end_round})', fontsize=14, fontweight='bold')
        ax2.set_xlabel('轮次', fontsize=12)
        ax2.set_ylabel('CWND值', fontsize=12)
        ax2.grid(True, alpha=0.3)
        ax2.set_xticks(valid_rounds)
        
        # 创建图例
        # 路径图例
        path_legend = [
            Line2D([0], [0], color='blue', linewidth=2, label='Path 0'),
            Line2D([0], [0], color='green', linewidth=2, label='Path 1'),
            Line2D([0], [0], color='red', linewidth=2, label='Path 2')
        ]
        
        # 状态图例
        state_patches = [
            mpatches.Patch(color='green', label='慢启动 (Slow Start)'),
            mpatches.Patch(color='blue', label='拥塞避免 (Congestion Avoidance)'),
            mpatches.Patch(color='orange', label='恢复阶段 (Recovery)'),
            mpatches.Patch(color='red', label='最小窗口 (Minimum Window)'),
            mpatches.Patch(color='gray', label='未知状态')
        ]
        
        # 添加图例
        ax2.legend(handles=path_legend + state_patches, loc='upper left', bbox_to_anchor=(1.05, 1), borderaxespad=0.)
        
        # 调整布局
        plt.tight_layout()
        
        # 保存图表
        filename = os.path.join(output_dir, f'rounds_{start_round}_{end_round}.png')
        plt.savefig(filename, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"已生成图表: {filename}")
    
    # 生成汇总数据文件
    generate_summary_file(rounds_data, output_dir)

def generate_summary_file(rounds_data, output_dir):
    """
    生成汇总数据文件
    """
    summary_file = os.path.join(output_dir, 'summary.txt')
    
    with open(summary_file, 'w', encoding='utf-8') as f:
        f.write("轮次数据分析汇总\n")
        f.write("=" * 50 + "\n\n")
        
        for round_num in sorted(rounds_data.keys()):
            data = rounds_data[round_num]
            f.write(f"轮次 {round_num}:\n")
            
            for path_id in [0, 1, 2]:
                olia_count = data['olia_count'][path_id]
                cwnd_value = data['cwnd_values'][path_id]
                state = data['states'][path_id]
                
                if olia_count > 0 or cwnd_value is not None:
                    f.write(f"  路径 {path_id}: ")
                    f.write(f"OLIA出现次数={olia_count}, ")
                    
                    if cwnd_value is not None:
                        f.write(f"CWND={cwnd_value}, ")
                    
                    if state:
                        f.write(f"状态={state}")
                    
                    f.write("\n")
            
            f.write("\n")
        
        # 统计信息
        f.write("统计信息:\n")
        f.write("-" * 30 + "\n")
        
        total_olia = {0: 0, 1: 0, 2: 0}
        cwnd_changes = {0: 0, 1: 0, 2: 0}
        
        for round_data in rounds_data.values():
            for path_id in [0, 1, 2]:
                total_olia[path_id] += round_data['olia_count'][path_id]
                if round_data['cwnd_values'][path_id] is not None:
                    cwnd_changes[path_id] += 1
        
        for path_id in [0, 1, 2]:
            f.write(f"路径 {path_id}: 总OLIA次数={total_olia[path_id]}, CWND赋值次数={cwnd_changes[path_id]}\n")
    
    print(f"已生成汇总文件: {summary_file}")

def main():
    # 使用你的文件路径
    # 方法1：使用原始字符串
    log_file = r'E:\ypy_part_hypatia\hypatia\AlgorithmLog\cwndconsole41.txt'#/////////////////////////////////////////////////////////////////////////////////////////////////////
    
    # 或者使用方法2：Path对象
    # from pathlib import Path
    # log_file = Path(r'E:\ypy_part_hypatia\hypatia\AlgorithmLog\cwndconsole25.txt')
    
    # 或者如果你想手动输入
    # log_file = input("请输入日志文件路径: ").strip()
    
    if not os.path.exists(log_file):
        print(f"错误: 文件 '{log_file}' 不存在")
        return
    
    print(f"正在解析日志文件: {log_file}")
    rounds_data = parse_log_file(log_file)
    
    print(f"解析完成，共找到 {len(rounds_data)} 个轮次的数据")
    
    # 生成图表
    print("正在生成图表...")
    generate_plots(rounds_data)
    
    print("完成!")

if __name__ == "__main__":
    main()