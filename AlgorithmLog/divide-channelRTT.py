import re
import matplotlib.pyplot as plt

# Read from txt file
file_path = 'AlgorithmLog/cwndconsole55.txt'  # Replace with your actual file path if different
with open(file_path, 'r', encoding='utf-8') as file:
    log_content = file.read()

# Split into lines
lines = log_content.splitlines()

# Dictionary to hold pkt-uid: {'sum_delay': float, 'first_line': int}
pkt_delays = {}

for i in range(len(lines)):
    line = lines[i].strip()
    if "卫星链路传播时延第一处" in line or "卫星链路传播时延第二处" in line:
        if i + 1 < len(lines):
            next_line = lines[i + 1].strip()
            # Extract pkt-uid and delay using regex
            match = re.match(r'(GSL|ISL) pkt-uid=(\d+) .* delay=([\d.]+) ms', next_line)
            if match:
                uid = int(match.group(2))
                delay = float(match.group(3))
                if uid not in pkt_delays:
                    pkt_delays[uid] = {'sum_delay': 0.0, 'first_line': i}
                pkt_delays[uid]['sum_delay'] += delay

# Sort by first_line to approximate time order
sorted_packets = sorted(pkt_delays.items(), key=lambda x: x[1]['first_line'])
time_indices = list(range(len(sorted_packets)))
total_delays = [v['sum_delay'] for k, v in sorted_packets]

# Plot the full graph
plt.figure(figsize=(10, 6))
plt.plot(time_indices, total_delays, marker='o')
plt.xlabel('Time Order (from start to end)')
plt.ylabel('Total Delay (ms)')
plt.title('Total Delay per Packet')
plt.grid(True)
plt.savefig('total_delay.png')
plt.close()

# Plot multiple graphs, each with at most 50 points
chunk_size = 50
for chunk_idx in range(0, len(time_indices), chunk_size):
    chunk_time = time_indices[chunk_idx:chunk_idx + chunk_size]
    chunk_delays = total_delays[chunk_idx:chunk_idx + chunk_size]
    
    plt.figure(figsize=(10, 6))
    plt.plot(chunk_time, chunk_delays, marker='o')
    plt.xlabel('Time Order (from start to end)')
    plt.ylabel('Total Delay (ms)')
    plt.title(f'Total Delay per Packet - Chunk {chunk_idx // chunk_size + 1}')
    plt.grid(True)
    plt.savefig(f'delay_chunk_{chunk_idx // chunk_size + 1}.png')
    plt.close()

print("Plots saved as 'total_delay.png' and 'delay_chunk_X.png' files.")