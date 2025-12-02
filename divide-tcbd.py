import re
import os

# 读取文件内容
with open('cwndconsole6.txt', 'r', encoding='utf-8') as file:
    content = file.read()

# 使用正则表达式找到所有从"=== OnPacketAckedCC 被调用 ==="到"=== OnPacketAckedCC 变量打印结束 ==="的块
pattern = r'=== OnPacketAckedCC 被调用 ===\n(.*?)\n=== OnPacketAckedCC 变量打印结束 ==='
blocks = re.findall(pattern, content, re.DOTALL)

# 字典用于分组，键是tcbd指针地址，值是块列表
groups = {}

for block in blocks:
    # 在每个块中提取tcbd指针地址
    addr_match = re.search(r'tcbd \(QuicSocketState\) 指针地址: (0x[0-9a-fA-F]+)', block)
    if addr_match:
        addr = addr_match.group(1)
        if addr not in groups:
            groups[addr] = []
        # 添加完整的块，包括起始和结束标记
        full_block = "=== OnPacketAckedCC 被调用 ===\n" + block + "\n=== OnPacketAckedCC 变量打印结束 ===\n"
        groups[addr].append(full_block)

# 为每个地址创建一个txt文件
for addr, block_list in groups.items():
    filename = f"cwnd_{addr.replace('0x', '')}.txt"
    with open(filename, 'w') as outfile:
        outfile.write(''.join(block_list))

# 输出生成的文件的列表
generated_files = [f"cwnd_{addr.replace('0x', '')}.txt" for addr in groups.keys()]
print("Generated files:")
for f in generated_files:
    print(f)