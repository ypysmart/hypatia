import re
import matplotlib.pyplot as plt
import os

# Create the directory if it doesn't exist
directory = 'AlgorithmLog/RTT1/'
os.makedirs(directory, exist_ok=True)

# Read the document content from file
with open('AlgorithmLog/cwndconsole13.txt', 'r', encoding='utf-8') as file:
    document_content = file.read()

# Define patterns for start and end markers
start_marker = r"/////////////////////////////////////////////////////RTT开始/////////////////////////////////////"
end_marker = r"/////////////////////////////////////////////////////RTT结束/////////////////////////////////////"

# Use regex to find all RTT blocks
rtt_blocks = re.findall(f"{start_marker}(.*?){end_marker}", document_content, re.DOTALL | re.MULTILINE)

# Dictionary to group by subflow_tcb, and also collect rtt values for plotting
groups = {}
rtt_groups = {}  # To store lists of rtt in ms for each subflow_tcb

for block in rtt_blocks:
    # Clean the block
    block = block.strip()
    
    # Extract pathId, subflow_tcb, cWnd from first line
    first_line_match = re.search(r"pathId=(\d+) subflow_tcb=(0x[0-9a-fA-F]+) cWnd=(\d+)", block)
    if first_line_match:
        path_id = first_line_match.group(1)
        subflow_tcb = first_line_match.group(2)
        cwnd = first_line_match.group(3)
        
        # Extract m_lastRtt from second line if present
        second_line_match = re.search(r"tcbd->m_lastRtt=\+([\d.]+)ns", block)
        last_rtt_ns = float(second_line_match.group(1)) if second_line_match else None
        
        # Create a record string
        record = f"pathId={path_id} subflow_tcb={subflow_tcb} cWnd={cwnd}\n"
        if last_rtt_ns is not None:
            record += f"tcbd->m_lastRtt=+{last_rtt_ns}ns\n"
        
        # Group records
        if subflow_tcb not in groups:
            groups[subflow_tcb] = []
            rtt_groups[subflow_tcb] = []
        groups[subflow_tcb].append(record)
        
        # Collect rtt in ms
        if last_rtt_ns is not None:
            last_rtt_ms = last_rtt_ns / 1e6
            rtt_groups[subflow_tcb].append(last_rtt_ms)

# Sort the subflow_tcb keys to have consistent ordering, or use enumeration for incrementing
# Here we enumerate over the groups to assign incremental names
for idx, (subflow_tcb, records) in enumerate(groups.items(), start=1):
    base_name = f"RTT{idx}"
    txt_filename = os.path.join(directory, f"{base_name}.txt")
    with open(txt_filename, 'w', encoding='utf-8') as f:
        f.write("\n".join(records))
    print(f"Written group for {subflow_tcb} to {txt_filename}")
    
    # Plot the RTT curve if there are RTT values
    rtt_values = rtt_groups.get(subflow_tcb, [])
    if rtt_values:
        plt.figure()
        plt.plot(range(len(rtt_values)), rtt_values, marker='o')
        plt.title(f"RTT over time for {base_name}")
        plt.xlabel("Sequence")
        plt.ylabel("RTT (ms)")
        plt.grid(True)
        png_filename = os.path.join(directory, f"{base_name}.png")
        plt.savefig(png_filename)
        plt.close()
        print(f"Saved RTT plot for {subflow_tcb} to {png_filename}")