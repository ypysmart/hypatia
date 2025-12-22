import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime

def parse_file_and_create_subfiles(filename):
    """读取文件并创建六个子文件"""
    # 定义六个路径类型
    path_types = ['Path0', 'Path1', 'Path2', 'Path 0', 'Path 1', 'Path 2']
    
    # 创建文件句柄字典
    files = {}
    for path_type in path_types:
        files[path_type] = open(f'{path_type.replace(" ", "_")}.txt', 'w', encoding='utf-8')
    
    # 读取文件并分类
    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            # 检查是否包含路径信息
            for path_type in path_types:
                if path_type in line:
                    files[path_type].write(line)
                    break
    
    # 关闭所有文件
    for file in files.values():
        file.close()
    
    print("文件分类完成！")
    return [f'{path_type.replace(" ", "_")}.txt' for path_type in path_types]

def parse_path_data(filename):
    """解析路径数据文件"""
    times = []
    rtts = []
    cwnds = []
    inflights = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            # 提取时间
            time_match = re.search(r'at\s+([\d.]+)s', line)
            if time_match:
                times.append(float(time_match.group(1)))
            
            # 提取RTT
            rtt_match = re.search(r'RTT\s*=\s*([\d.]+)ms', line)
            if rtt_match:
                rtts.append(float(rtt_match.group(1)))
            
            # 提取cWnd
            cwnd_match = re.search(r'cWnd\s*=\s*([\d.]+)B', line)
            if cwnd_match:
                cwnds.append(float(cwnd_match.group(1)))
            
            # 提取InFlight
            inflight_match = re.search(r'InFlight\s*=\s*([\d.]+)B', line)
            if inflight_match:
                inflights.append(float(inflight_match.group(1)))
    
    return times, rtts, cwnds, inflights

def parse_send_data(filename):
    """解析发送数据文件"""
    times = []
    send_numbers = []
    available_windows = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            # 提取时间
            time_match = re.search(r'at\s+([\d.]+)s', line)
            if time_match:
                times.append(float(time_match.group(1)))
            
            # 提取sendNumber (格式如: Path0sendNumber > 0:0availableWindow > 0：2920)
            send_match = re.search(r'sendNumber\s*>\s*\d+:(\d+)', line)
            if send_match:
                send_numbers.append(float(send_match.group(1)))
            
            # 提取availableWindow (格式如: availableWindow > 0：2920)
            window_match = re.search(r'availableWindow\s*>\s*\d+：(\d+)', line)
            if window_match:
                available_windows.append(float(window_match.group(1)))
    
    return times, send_numbers, available_windows

def main():
    # 原始文件名
    input_file = 'AlgorithmLog/cwndconsole14.txt'  # 请替换为您的实际文件名
    
    # 第一步：分类文件
    print("开始分类文件...")
    subfiles = parse_file_and_create_subfiles(input_file)
    print(f"创建了 {len(subfiles)} 个子文件:")
    for file in subfiles:
        print(f"  - {file}")

if __name__ == "__main__":
    main()




import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime
import matplotlib.dates as mdates
import os

def parse_path_status(file_content):
    """解析路径状态信息（文档1）"""
    path_data = {0: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []},
                 1: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []},
                 2: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []}}
    
    lines = file_content.strip().split('\n')
    start_time = None
    
    for i, line in enumerate(lines):
        if not line.startswith('Path'):
            continue
            
        # 解析时间（如果没有时间戳，使用行号作为相对时间）
        if start_time is None:
            start_time = i
        current_time = i - start_time
        
        # 使用正则表达式提取路径信息
        match = re.search(r'Path (\d+)\s+proportion = [\d.]+%\s+RTT = ([\d.]+)ms\s+cWnd = ([\d.]+)B\s+InFlight = ([\d.]+)B', line)
        if match:
            path_num = int(match.group(1))
            rtt = float(match.group(2))
            cwnd = float(match.group(3).replace('B', ''))
            inflight = float(match.group(4).replace('B', ''))
            
            path_data[path_num]['time'].append(current_time)
            path_data[path_num]['rtt'].append(rtt)
            path_data[path_num]['cwnd'].append(cwnd)
            path_data[path_num]['inflight'].append(inflight)
    
    return path_data

def parse_send_info(file_content):
    """解析发送信息（文档2）"""
    send_data = {0: {'time': [], 'send_number': [], 'available_window': []},
                 1: {'time': [], 'send_number': [], 'available_window': []},
                 2: {'time': [], 'send_number': [], 'available_window': []}}
    
    lines = file_content.strip().split('\n')
    
    for line in lines:
        if not line.startswith('Path'):
            continue
            
        # 解析时间戳
        time_match = re.search(r'time ([\d.]+)', line)
        if not time_match:
            continue
        current_time = float(time_match.group(1))
        
        # 解析路径编号和发送信息
        send_match = re.search(r'Path(\d+)sendNumber > (\d+):(\d+)availableWindow > (\d+)：(\d+)', line)
        if send_match:
            path_num = int(send_match.group(1))
            send_number1 = int(send_match.group(2))
            send_number2 = int(send_match.group(3))
            available_window = int(send_match.group(5))
            
            # 合并两个sendNumber值
            total_send = send_number1 + send_number2
            
            send_data[path_num]['time'].append(current_time)
            send_data[path_num]['send_number'].append(total_send)
            send_data[path_num]['available_window'].append(available_window)
    
    return send_data

def merge_path_data(existing_data, new_data):
    """合并路径数据"""
    for path_num in [0, 1, 2]:
        for key in ['time', 'rtt', 'cwnd', 'inflight']:
            existing_data[path_num][key].extend(new_data[path_num][key])
    return existing_data

def merge_send_data(existing_data, new_data):
    """合并发送数据"""
    for path_num in [0, 1, 2]:
        for key in ['time', 'send_number', 'available_window']:
            existing_data[path_num][key].extend(new_data[path_num][key])
    return existing_data

def plot_path_curves(path_data, send_data, save_dir="AlgorithmLog/plots4"):
    """绘制所有曲线并保存为PNG文件"""
    # 创建保存目录
    if not os.path.exists(save_dir):
        os.makedirs(save_dir)
    
    # 路径颜色
    colors = {0: 'blue', 1: 'red', 2: 'green'}
    path_names = {0: 'Path 0', 1: 'Path 1', 2: 'Path 2'}
    
    # 绘制路径性能分析图表
    fig, axes = plt.subplots(3, 3, figsize=(18, 12))
    fig.suptitle('Network Path Performance Analysis', fontsize=16, fontweight='bold')
    
    # 绘制每个路径的曲线
    for path_num in [0, 1, 2]:
        color = colors[path_num]
        path_name = path_names[path_num]
        
        # RTT曲线
        if path_data[path_num]['time'] and path_data[path_num]['rtt']:
            ax1 = axes[0, path_num]
            ax1.plot(path_data[path_num]['time'], path_data[path_num]['rtt'], 
                    color=color, linewidth=2, label=f'{path_name} RTT')
            ax1.set_title(f'{path_name} - RTT')
            ax1.set_ylabel('RTT (ms)')
            ax1.grid(True, alpha=0.3)
            ax1.legend()
        
        # CWND曲线
        if path_data[path_num]['time'] and path_data[path_num]['cwnd']:
            ax2 = axes[1, path_num]
            ax2.plot(path_data[path_num]['time'], path_data[path_num]['cwnd'], 
                    color=color, linewidth=2, label=f'{path_name} CWND')
            ax2.set_title(f'{path_name} - Congestion Window')
            ax2.set_ylabel('CWND (Bytes)')
            ax2.grid(True, alpha=0.3)
            ax2.legend()
        
        # InFlight曲线
        if path_data[path_num]['time'] and path_data[path_num]['inflight']:
            ax3 = axes[2, path_num]
            ax3.plot(path_data[path_num]['time'], path_data[path_num]['inflight'], 
                    color=color, linewidth=2, label=f'{path_name} InFlight')
            ax3.set_title(f'{path_name} - InFlight Data')
            ax3.set_ylabel('InFlight (Bytes)')
            ax3.set_xlabel('Time (relative units)')
            ax3.grid(True, alpha=0.3)
            ax3.legend()
    
    plt.tight_layout()
    # 保存第一个图表
    plt.savefig(os.path.join(save_dir, 'network_path_performance.png'), dpi=300, bbox_inches='tight')
    plt.close(fig)  # 关闭图表以释放内存
    print("已保存图表: network_path_performance.png")
    
    # 绘制发送信息分析图表
    fig2, axes2 = plt.subplots(2, 3, figsize=(18, 8))
    fig2.suptitle('Send Information Analysis', fontsize=16, fontweight='bold')
    
    for path_num in [0, 1, 2]:
        color = colors[path_num]
        path_name = path_names[path_num]
        
        # Send Number曲线
        if send_data[path_num]['time'] and send_data[path_num]['send_number']:
            ax1 = axes2[0, path_num]
            ax1.plot(send_data[path_num]['time'], send_data[path_num]['send_number'], 
                    color=color, linewidth=2, label=f'{path_name} Send Number')
            ax1.set_title(f'{path_name} - Send Number')
            ax1.set_ylabel('Send Number')
            ax1.grid(True, alpha=0.3)
            ax1.legend()
        
        # Available Window曲线
        if send_data[path_num]['time'] and send_data[path_num]['available_window']:
            ax2 = axes2[1, path_num]
            ax2.plot(send_data[path_num]['time'], send_data[path_num]['available_window'], 
                    color=color, linewidth=2, label=f'{path_name} Available Window')
            ax2.set_title(f'{path_name} - Available Window')
            ax2.set_ylabel('Available Window')
            ax2.set_xlabel('Time (seconds)')
            ax2.grid(True, alpha=0.3)
            ax2.legend()
    
    plt.tight_layout()
    # 保存第二个图表
    plt.savefig(os.path.join(save_dir, 'send_information_analysis.png'), dpi=300, bbox_inches='tight')
    plt.close(fig2)  # 关闭图表以释放内存
    print("已保存图表: send_information_analysis.png")
    
    # 为每个路径单独创建图表
    for path_num in [0, 1, 2]:
        if path_data[path_num]['time']:  # 只有有数据时才创建图表
            color = colors[path_num]
            path_name = path_names[path_num]
            
            # 创建单个路径的详细图表
            fig3, axes3 = plt.subplots(2, 2, figsize=(12, 8))
            fig3.suptitle(f'{path_name} - Detailed Analysis', fontsize=14, fontweight='bold')
            
            # RTT
            if path_data[path_num]['rtt']:
                ax1 = axes3[0, 0]
                ax1.plot(path_data[path_num]['time'], path_data[path_num]['rtt'], 
                        color=color, linewidth=2)
                ax1.set_title(f'{path_name} - RTT')
                ax1.set_ylabel('RTT (ms)')
                ax1.grid(True, alpha=0.3)
            
            # CWND
            if path_data[path_num]['cwnd']:
                ax2 = axes3[0, 1]
                ax2.plot(path_data[path_num]['time'], path_data[path_num]['cwnd'], 
                        color=color, linewidth=2)
                ax2.set_title(f'{path_name} - Congestion Window')
                ax2.set_ylabel('CWND (Bytes)')
                ax2.grid(True, alpha=0.3)
            
            # InFlight
            if path_data[path_num]['inflight']:
                ax3 = axes3[1, 0]
                ax3.plot(path_data[path_num]['time'], path_data[path_num]['inflight'], 
                        color=color, linewidth=2)
                ax3.set_title(f'{path_name} - InFlight Data')
                ax3.set_ylabel('InFlight (Bytes)')
                ax3.set_xlabel('Time (relative units)')
                ax3.grid(True, alpha=0.3)
            
            # Send Number
            if send_data[path_num]['send_number']:
                ax4 = axes3[1, 1]
                ax4.plot(send_data[path_num]['time'], send_data[path_num]['send_number'], 
                        color=color, linewidth=2)
                ax4.set_title(f'{path_name} - Send Number')
                ax4.set_ylabel('Send Number')
                ax4.set_xlabel('Time (seconds)')
                ax4.grid(True, alpha=0.3)
            
            plt.tight_layout()
            # 保存单个路径的图表
            plt.savefig(os.path.join(save_dir, f'{path_name.lower().replace(" ", "_")}_detailed.png'), 
                       dpi=300, bbox_inches='tight')
            plt.close(fig3)  # 关闭图表以释放内存
            print(f"已保存图表: {path_name.lower().replace(' ', '_')}_detailed.png")

def main():
    # 初始化数据结构
    all_path_data = {0: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []},
                     1: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []},
                     2: {'time': [], 'rtt': [], 'cwnd': [], 'inflight': []}}
    
    all_send_data = {0: {'time': [], 'send_number': [], 'available_window': []},
                     1: {'time': [], 'send_number': [], 'available_window': []},
                     2: {'time': [], 'send_number': [], 'available_window': []}}
    
    # 路径状态文件列表
    path_status_files = ['Path_0.txt', 'Path_1.txt', 'Path_2.txt']
    # 发送信息文件列表
    send_info_files = ['path0.txt', 'path1.txt', 'path2.txt']
    
    # 解析路径状态文件
    print("正在解析路径状态文件...")
    for file_path in path_status_files:
        if os.path.exists(file_path):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                path_data = parse_path_status(content)
                all_path_data = merge_path_data(all_path_data, path_data)
                print(f"成功解析文件: {file_path}")
            except Exception as e:
                print(f"解析文件 {file_path} 时出错: {e}")
        else:
            print(f"文件不存在: {file_path}")
    
    # 解析发送信息文件
    print("正在解析发送信息文件...")
    for file_path in send_info_files:
        if os.path.exists(file_path):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                send_data = parse_send_info(content)
                all_send_data = merge_send_data(all_send_data, send_data)
                print(f"成功解析文件: {file_path}")
            except Exception as e:
                print(f"解析文件 {file_path} 时出错: {e}")
        else:
            print(f"文件不存在: {file_path}")
    
    # 打印数据统计
    print("\n数据统计:")
    for path_num in [0, 1, 2]:
        print(f"Path {path_num} - 状态数据点: {len(all_path_data[path_num]['time'])}")
        print(f"Path {path_num} - 发送数据点: {len(all_send_data[path_num]['time'])}")
    
    # 检查是否有数据
    has_data = False
    for path_num in [0, 1, 2]:
        if all_path_data[path_num]['time'] or all_send_data[path_num]['time']:
            has_data = True
            break
    
    if not has_data:
        print("没有找到有效数据，请检查文件路径和内容格式")
        return
    
    # 绘制曲线并保存为PNG文件
    print("\n正在生成并保存图表...")
    plot_path_curves(all_path_data, all_send_data)
    print("\n所有图表已保存到 'plots' 目录中")

if __name__ == "__main__":
    main()