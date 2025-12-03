# Copyright (c) 2025 你可以写你名字
# 基于原版 algorithm_free_one_only_over_isls 修改而来

from .fstate_calculation import *
import math


def algorithm_free_one_only_over_isls(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        satellites,
        ground_stations,
        sat_net_graph_only_satellites_with_isls,
        ground_station_satellites_in_range,
        num_isls_per_sat,
        sat_neighbor_to_if,
        list_gsl_interfaces_info,
        prev_output,
        enable_verbose_logs
):
    """
    每个地面站最多使用 3 个不同的卫星进行上/下行
    每个接口独立带宽、独立连接，不会出现同一颗卫星占多个接口的情况
    """
    if enable_verbose_logs:
        print("\nALGORITHM: FREE THREE PER GS OVER ISLS (3 interfaces per GS)")

    num_satellites = len(satellites)
    num_ground_stations = len(ground_stations)

    # ==================== 1. 为每个地面站选最多 3 3 颗最近的卫星 ====================
    gs_to_selected_sats = []  # 长度 = num_ground_stations，每个元素是 [(dist, sid), ...] 最多3个
    for gid in range(num_ground_stations):
        candidates = ground_station_satellites_in_range[gid]  # [(dist, sid), ...]
        # 按距离排序，取前 3（如果不足 3 就全取）
        candidates_sorted = sorted(candidates, key=lambda x: x[0])[:3]
        gs_to_selected_sats.append(candidates_sorted)

        if enable_verbose_logs:
            print(f"  GS {gid} selected {len(candidates_sorted)} satellites: {[sid for _, sid in candidates_sorted]}")

    # ==================== 2. 计算每个被选中的 GSL 接口的带宽 ====================
    # 统计每颗卫星被多少个地面站选中（用于公平分配带宽）
    sat_selected_count = [0] * num_satellites
    for selected_list in gs_to_selected_sats:
        for _, sid in selected_list:
            sat_selected_count[sid] += 1

    # 写带宽文件
    output_bw_file = output_dynamic_state_dir + f"/gsl_if_bandwidth_{time_since_epoch_ns}.txt"
    if enable_verbose_logs:
        print("  > Writing gsl_if_bandwidth to:", output_bw_file)
    with open(output_bw_file, "w") as f:
        # 卫星侧：每个卫星的 1 个 GSL 接口带宽 = 1.0 / 被多少个 GS 选中
        for sid in range(num_satellites):
            bw = 1.0 / sat_selected_count[sid] if sat_selected_count[sid] > 0 else 1.0
            f.write(f"{sid},{num_isls_per_sat[sid]},{bw:.6f}\n")

        # 地面站侧：3 个接口平分 3.0 的聚合带宽 → 每个 1.0
        # 但只有被选中的接口才有带宽，未被选中的给 0（或者 1.0 用于清空队列）
        for gid in range(num_ground_stations):
            selected = gs_to_selected_sats[gid]
            for local_if_idx in range(3):
                if local_if_idx < len(selected):
                    f.write(f"{num_satellites + gid},{local_if_idx},1.000000\n")
                else:
                    f.write(f"{num_satellites + gid},{local_if_idx},0.000000\n")  # 或 1.0 用于快速清空

    # ==================== 3. 计算转发状态 ====================
    prev_fstate = prev_output["fstate"] if prev_output else None if prev_output else None

    # 关键：把 ground_station_satellites_in_range_candidates 替换成我们刚选好的最多 3 个
    # gid_to_sat_gsl_if_idx 仍然是 [0,1,2]（因为每个 GS 有 3 个接口，编号 0/1/2）
    gid_to_sat_gsl_if_idx = [0, 1, 2]  # 每个 GS 的 3 个接口依次对应第 0/1/2 个选中的卫星

    fstate = calculate_fstate_shortest_path_without_gs_relaying(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        num_satellites,
        num_ground_stations,
        sat_net_graph_only_satellites_with_isls,
        num_isls_per_sat,
        gid_to_sat_gsl_if_idx,
        gs_to_selected_sats,                 # ← 这里替换成我们精选的列表
        sat_neighbor_to_if,
        prev_fstate,
        enable_verbose_logs
    )

    return {"fstate": fstate}