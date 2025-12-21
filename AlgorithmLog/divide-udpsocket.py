import re
from collections import defaultdict

# 输入文件名
input_file = 'cwndconsole11.txt'  # 替换为你的实际文件名

# 读取文件内容
with open(input_file, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 使用字典来分组行，key是0x地址，value是行的列表
groups = defaultdict(list)

# 正则表达式匹配0x开头的十六进制地址
address_pattern = re.compile(r'0x[0-9a-fA-F]+')

for line in lines:
    match = address_pattern.search(line)
    if match:
        address = match.group(0)
        groups[address].append(line.strip())
    else:
        # 如果行不包含地址，可以选择忽略或放到一个默认组，这里忽略
        pass

# 对于每个组，保存到一个单独的txt文件
for address, group_lines in groups.items():
    output_file = f'group_{address}.txt'
    with open(output_file, 'w', encoding='utf-8') as f:
        for line in group_lines:
            f.write(line + '\n')
    print(f'Saved group for {address} to {output_file}')