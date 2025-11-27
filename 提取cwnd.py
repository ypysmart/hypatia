# ========== 一键运行版：直接复制粘贴运行 ==========
input_file  = "cwnd-console2.txt"          # 你的原始日志文件（保持文件名不变或改成完整路径）
output_file = "cwnd_only.txt"        # 提取结果保存到这个新文件

with open(input_file, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 只保留包含 tcbd->m_cWnd 的行
cwnd_lines = [line.rstrip('\n') for line in lines if 'tcbd->m_cWnd' in line]

# 保存到新文件
with open(output_file, 'w', encoding='utf-8') as f:
    for line in cwnd_lines:
        f.write(line + '\n')

print(f"提取完成！")
print(f"共提取了 {len(cwnd_lines)} 行拥塞窗口记录")
print(f"已保存到：{output_file}")


import re
import matplotlib.pyplot as plt

# 文件路径（请改成你的实际路径）
filename = "cwnd_only.txt"

# 读取文件并提取所有 tcbd->m_cWnd: 后面的数字
values = []
with open(filename, 'r', encoding='utf-8') as f:
    for line in f:
        # 使用正则匹配 "tcbd->m_cWnd:" 后面的数字
        match = re.search(r'tcbd->m_cWnd:(\d+)', line)
        if match:
            cwnd = int(match.group(1))
            values.append(cwnd / 1380.0)   # 除以 1380

# 如果没有提取到数据就报错退出
if not values:
    raise ValueError("文件中没有找到任何 tcbd->m_cWnd: 的数值")

# 绘制图像
plt.figure(figsize=(14, 7))
plt.plot(values, linewidth=1.2, color='#1f77b4')
plt.title('Congestion Window (cwnd / 1380)', fontsize=16)
plt.xlabel('ACK 序号（时间顺序）', fontsize=14)
plt.ylabel('cwnd (单位：1380 bytes ≈ 1 个常见 MSS)', fontsize=14)
plt.grid(True, which="both", ls="--", alpha=0.5)
plt.tight_layout()

# 保存为 PNG（分辨率 300dpi，适合报告/论文使用）
output_png = "cwnd_div_1380.png"
plt.savefig(output_png, dpi=300, bbox_inches='tight')
print(f"图像已保存：{output_png}")

# 可选：同时显示图像（在本地运行时会弹窗）
plt.show()