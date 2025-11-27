# split_quic_logs_by_this.py

import re
from collections import defaultdict

def split_logs(input_file="console.txt"):
    # 用于存储每个 this 指针对应的日志行
    logs_by_this = defaultdict(list)
    
    # 正则匹配 发送 或 接收 行，并提取 this 指针
    pattern = re.compile(r"(发送|接收)\s+(0x[0-9a-fA-F]+)(Send|Recv)\s+pkt")

    with open(input_file, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            match = pattern.search(line)
            if match:
                action = match.group(1)  # 发送 或 接收
                this_ptr = match.group(2)  # 0x55bf543d34e0
                func = match.group(3)     # Send 或 Recv
                key = f"{this_ptr}{func}"  # 完整标识：0x55bf543d34e0Send
                logs_by_this[key].append(line)

    # 输出到独立文件
    for key, lines in logs_by_this.items():
        # 美化文件名
        if "Send" in key:
            role = "Sender"
        else:
            role = "Receiver"
        filename = f"quic_log_{key}_{role}.txt"
        with open(filename, "w", encoding="utf-8") as out:
            for line in lines:
                out.write(line + "\n")
        print(f"已生成: {filename}  ({len(lines)} 行)")

    print(f"\n拆分完成！共生成 {len(logs_by_this)} 个文件。")

if __name__ == "__main__":
    split_logs("console.txt")