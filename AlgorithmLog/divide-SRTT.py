import re
import matplotlib.pyplot as plt
import os

# Ensure the directory exists
os.makedirs('AlgorithmLog/SRTT', exist_ok=True)

# 函数：从日志中提取数据
def extract_data(log_content):
    # 正则表达式匹配包含 " //////////当前更新" 的行
    pattern = r'(\d+\.\d+) //////////当前更新path=(\d+) srtt=\+(\d+\.\d+)ns ms lastRtt=\+(\d+\.\d+)nsms minRtt=(\d+) ms rttVar=(\d+) ms cWnd=(\d+) inFlight=(\d+)'
    
    path0_times = []
    path0_srtt = []
    path0_lastRtt = []
    
    path1_times = []
    path1_srtt = []
    path1_lastRtt = []
    
    matches = re.findall(pattern, log_content)
    for match in matches:
        time = float(match[0])  # 时间 (s)
        path = int(match[1])    # path ID
        srtt = float(match[2])  # srtt 值 (假设单位 ms，忽略 ns 标签错误)
        lastRtt = float(match[3])  # lastRtt 值 (假设单位 ms)
        
        if path == 0:
            path0_times.append(time)
            path0_srtt.append(srtt)
            path0_lastRtt.append(lastRtt)
        elif path == 1:
            path1_times.append(time)
            path1_srtt.append(srtt)
            path1_lastRtt.append(lastRtt)
    
    return (path0_times, path0_srtt, path0_lastRtt), (path1_times, path1_srtt, path1_lastRtt)

# 第一个任务：在一张图上绘制四条曲线
def plot_task1(path0_data, path1_data):
    path0_times, path0_srtt, path0_lastRtt = path0_data
    path1_times, path1_srtt, path1_lastRtt = path1_data
    
    plt.figure(figsize=(12, 6))
    # path=0
    if path0_times:
        plt.plot(path0_times, path0_srtt, label='Path 0 SRTT (ms)', color='blue')
        plt.plot(path0_times, path0_lastRtt, label='Path 0 Last RTT (ms)', color='cyan', linestyle='--')
    # path=1
    if path1_times:
        plt.plot(path1_times, path1_srtt, label='Path 1 SRTT (ms)', color='red')
        plt.plot(path1_times, path1_lastRtt, label='Path 1 Last RTT (ms)', color='orange', linestyle='--')
    
    plt.xlabel('Time (s)')
    plt.ylabel('RTT (ms)')
    plt.title('SRTT and Last RTT for Path 0 and Path 1')
    plt.legend()
    plt.grid(True)
    plt.savefig('AlgorithmLog/SRTT/task1_all_curves.png')
    # plt.show()  # 移除显示

# 第二个任务：为每个path的SRTT生成多个图，每个图最多50点，用圆圈标注
def plot_task2(path_data, path_id):
    times, srtt, _ = path_data
    if not times:
        print(f"No data for Path {path_id}")
        return
    
    points_per_plot = 50
    num_plots = (len(times) + points_per_plot - 1) // points_per_plot
    
    for i in range(num_plots):
        start = i * points_per_plot
        end = min((i + 1) * points_per_plot, len(times))
        
        plt.figure(figsize=(10, 5))
        plt.scatter(times[start:end], srtt[start:end], label=f'Path {path_id} SRTT (ms)', color='green' if path_id == 0 else 'purple', marker='o')
        plt.plot(times[start:end], srtt[start:end], color='gray', linestyle='-', alpha=0.5)  # 连接线以便查看趋势
        
        plt.xlabel('Time (s)')
        plt.ylabel('SRTT (ms)')
        plt.title(f'Path {path_id} SRTT - Part {i+1}/{num_plots} (Points {start+1} to {end})')
        plt.legend()
        plt.grid(True)
        plt.savefig(f'AlgorithmLog/SRTT/task2_path{path_id}_part{i+1}.png')
        # plt.show()  # 移除显示

# 第三个任务：任务一的基础上剔除全部的srtt和lastRtt中的0值
def plot_task3(path0_data, path1_data):
    path0_times, path0_srtt, path0_lastRtt = path0_data
    path1_times, path1_srtt, path1_lastRtt = path1_data
    
    # 过滤 path0 srtt 非零
    filtered_path0_srtt_times = [t for t, v in zip(path0_times, path0_srtt) if v > 0]
    filtered_path0_srtt = [v for v in path0_srtt if v > 0]
    
    # 过滤 path0 lastRtt 非零
    filtered_path0_lastRtt_times = [t for t, v in zip(path0_times, path0_lastRtt) if v > 0]
    filtered_path0_lastRtt = [v for v in path0_lastRtt if v > 0]
    
    # 过滤 path1 srtt 非零
    filtered_path1_srtt_times = [t for t, v in zip(path1_times, path1_srtt) if v > 0]
    filtered_path1_srtt = [v for v in path1_srtt if v > 0]
    
    # 过滤 path1 lastRtt 非零
    filtered_path1_lastRtt_times = [t for t, v in zip(path1_times, path1_lastRtt) if v > 0]
    filtered_path1_lastRtt = [v for v in path1_lastRtt if v > 0]
    
    plt.figure(figsize=(12, 6))
    # path=0 srtt
    if filtered_path0_srtt_times:
        plt.plot(filtered_path0_srtt_times, filtered_path0_srtt, label='Path 0 SRTT (ms, non-zero)', color='blue')
    # path=0 lastRtt
    if filtered_path0_lastRtt_times:
        plt.plot(filtered_path0_lastRtt_times, filtered_path0_lastRtt, label='Path 0 Last RTT (ms, non-zero)', color='cyan', linestyle='--')
    # path=1 srtt
    if filtered_path1_srtt_times:
        plt.plot(filtered_path1_srtt_times, filtered_path1_srtt, label='Path 1 SRTT (ms, non-zero)', color='red')
    # path=1 lastRtt
    if filtered_path1_lastRtt_times:
        plt.plot(filtered_path1_lastRtt_times, filtered_path1_lastRtt, label='Path 1 Last RTT (ms, non-zero)', color='orange', linestyle='--')
    
    plt.xlabel('Time (s)')
    plt.ylabel('RTT (ms)')
    plt.title('SRTT and Last RTT for Path 0 and Path 1 (Non-Zero Values)')
    plt.legend()
    plt.grid(True)
    plt.savefig('AlgorithmLog/SRTT/task3_non_zero_curves.png')
    # plt.show()  # 移除显示
# 第四个任务：任务二的基础上剔除全部的srtt中的0值（任务二只绘制srtt）
def plot_task4(path_data, path_id):
    times, srtt, _ = path_data
    if not times:
        print(f"No data for Path {path_id}")
        return
    
    # 过滤非零 srtt
    filtered_times = [t for t, v in zip(times, srtt) if v > 0]
    filtered_srtt = [v for v in srtt if v > 0]
    
    if not filtered_times:
        print(f"No non-zero data for Path {path_id}")
        return
    
    points_per_plot = 50
    num_plots = (len(filtered_times) + points_per_plot - 1) // points_per_plot
    
    for i in range(num_plots):
        start = i * points_per_plot
        end = min((i + 1) * points_per_plot, len(filtered_times))
        
        plt.figure(figsize=(10, 5))
        plt.scatter(filtered_times[start:end], filtered_srtt[start:end], label=f'Path {path_id} SRTT (ms, non-zero)', color='green' if path_id == 0 else 'purple', marker='o')
        plt.plot(filtered_times[start:end], filtered_srtt[start:end], color='gray', linestyle='-', alpha=0.5)  # 连接线以便查看趋势
        
        plt.xlabel('Time (s)')
        plt.ylabel('SRTT (ms)')
        plt.title(f'Path {path_id} SRTT (Non-Zero) - Part {i+1}/{num_plots} (Points {start+1} to {end})')
        plt.legend()
        plt.grid(True)
        plt.savefig(f'AlgorithmLog/SRTT/task4_path{path_id}_part{i+1}.png')
        # plt.show()  # 移除显示
# 主函数
def main():
    with open('AlgorithmLog/cwndconsole54.txt', 'r', encoding='utf-8') as f:
        log_content = f.read()
    path0_data, path1_data = extract_data(log_content)
    
    # 第一个任务
    plot_task1(path0_data, path1_data)
    
    # 第二个任务
    plot_task2(path0_data, 0)
    plot_task2(path1_data, 1)
    
    # 第三个任务
    plot_task3(path0_data, path1_data)
    
    # 第四个任务
    plot_task4(path0_data, 0)
    plot_task4(path1_data, 1)

if __name__ == '__main__':
    main()