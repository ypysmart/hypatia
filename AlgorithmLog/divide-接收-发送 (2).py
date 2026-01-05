import re
import os
from collections import defaultdict

# 输入文件
input_file = "AlgorithmLog/cwndconsole19.txt"

# 输出文件夹
output_dir = "AlgorithmLog/quic_logs_by_device"

# 创建输出文件夹（如果不存在）
os.makedirs(output_dir, exist_ok=True)

# 正则表达式：匹配 “发送” 或 “接收” 后面跟着 0x 开头的地址，然后是 Send 或 Recv
pattern = re.compile(r'(发送|接收)\s+(0x[0-9a-fA-F]+)(Send|Recv)')

# 用于分组的字典：address -> 列表 of 行
logs_by_address = defaultdict(list)

print("正在读取日志文件并提取地址...")

with open(input_file, 'r', encoding='utf-8') as f:
    for line in f:
        match = pattern.search(line)
        if match:
            address = match.group(2)  # 提取 0x... 地址
            logs_by_address[address].append(line)  # 保留原始整行（包括空格和换行）

# 输出每个地址到单独文件
print(f"共找到 {len(logs_by_address)} 个不同的 QUIC 连接地址：")
for address, lines in logs_by_address.items():
    print(f"  - {address}  ({len(lines)} 条日志)")

    output_file = os.path.join(output_dir, f"device_{address}.log")
    with open(output_file, 'w', encoding='utf-8') as fout:
        fout.writelines(lines)
    
    print(f"  → 已保存到: {output_file}")

print("\n所有提取完成！结果保存在文件夹:", output_dir)