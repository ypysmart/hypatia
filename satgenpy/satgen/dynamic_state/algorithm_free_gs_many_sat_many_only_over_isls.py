# The MIT License (MIT)
#
# Copyright (c) 2020 ETH Zurich
#
# ... (保持原有版权声明)

from .fstate_calculation import *


def algorithm_free_gs_many_sat_many_only_over_isls(
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
    FREE GROUND STATION (MANY) SATELLITE (MANY) ONLY OVER ISLS ALGORITHM

    支持每个GS有多个(3)接口，每个接口连接不同的卫星，避免一个卫星连接同一个GS的多个接口。
    """

    if enable_verbose_logs:
        print("\nALGORITHM: FREE GROUND STATION MANY SATELLITE MANY ONLY OVER ISLS")

    # 检查图
    if sat_net_graph_only_satellites_with_isls.number_of_nodes() != len(satellites):
        raise ValueError("Number of nodes in the graph does not match the number of satellites")
    for sid in range(len(satellites)):
        for n in sat_net_graph_only_satellites_with_isls.neighbors(sid):
            if n >= len(satellites):
                raise ValueError("Graph cannot contain satellite-to-ground-station links")

    # 检查接口条件：卫星接口 >= GS数量 * 3, GS接口 == 3, GS聚合带宽 == 3.0//改为2了
    num_gs_interfaces = 2  # 固定为2
    for i in range(len(list_gsl_interfaces_info)):
        if i < len(satellites):
            if list_gsl_interfaces_info[i]["number_of_interfaces"] < len(ground_stations) * num_gs_interfaces:
                raise ValueError("Satellites must have at least len(GS) * 3 GSL interfaces")
        else:
            if list_gsl_interfaces_info[i]["number_of_interfaces"] != num_gs_interfaces:
                raise ValueError("Ground stations must have exactly 3 interfaces")
            if list_gsl_interfaces_info[i]["aggregate_max_bandwidth"] != num_gs_interfaces:
                raise ValueError("Ground station aggregate max. bandwidth must be 3.0")
    if enable_verbose_logs:
        print("  > Interface conditions are met")

    #################################
    # 选择连接：为每个GS选择3个不同的卫星（距离最近的3个）
    #

    ground_station_satellites_in_range_selected = []  # 每个GS的选中卫星列表：[(dist, sid), ...] 长度3
    satellite_gsl_ifs_assigned = [{} for _ in range(len(satellites))]  # sid -> {gid: [if_idx_list]} 确保每个GS只分配1个if

    for gid in range(len(ground_stations)):
        # 按距离排序候选卫星
        candidates = sorted(ground_station_satellites_in_range[gid])

        # 选择前3个（或少于3个如果不足）
        selected = candidates[:num_gs_interfaces]
        ground_station_satellites_in_range_selected.append(selected)

        # 为每个选中卫星分配接口，确保同一个卫星不分配多个接口给同一个GS
        for _, sid in selected:
            if gid not in satellite_gsl_ifs_assigned[sid]:
                # 分配一个新接口：从卫星的GSL接口池中取一个未用的（假设接口足够）
                assigned_if_idx = len(satellite_gsl_ifs_assigned[sid])  # 简单递增分配
                satellite_gsl_ifs_assigned[sid][gid] = [assigned_if_idx]  # 只分配1个
            else:
                raise ValueError(f"Satellite {sid} already assigned to GS {gid} - violation")

    #################################
    # BANDWIDTH STATE：均匀分配带宽
    #

    output_filename = output_dynamic_state_dir + "/gsl_if_bandwidth_" + str(time_since_epoch_ns) + ".txt"
    if enable_verbose_logs:
        print("  > Writing interface bandwidth state to: " + output_filename)
    with open(output_filename, "w+") as f_out:
        if time_since_epoch_ns == 0:
            # 卫星接口：每个分配的接口带宽 = 聚合 / 总分配接口数
            for sid in range(len(satellites)):
                num_assigned_ifs = len(satellite_gsl_ifs_assigned[sid])
                if_bandwidth = list_gsl_interfaces_info[sid]["aggregate_max_bandwidth"] / max(1, num_assigned_ifs)
                for gid, if_list in satellite_gsl_ifs_assigned[sid].items():
                    for if_idx in if_list:
                        abs_if_idx = num_isls_per_sat[sid] + if_idx
                        f_out.write("%d,%d,%f\n" % (sid, abs_if_idx, if_bandwidth))

            # GS接口：每个GS的3个接口均匀分聚合带宽
            for gid in range(len(ground_stations)):
                gs_node_id = len(satellites) + gid
                if_bandwidth = list_gsl_interfaces_info[gs_node_id]["aggregate_max_bandwidth"] / num_gs_interfaces
                for if_idx in range(num_gs_interfaces):
                    f_out.write("%d,%d,%f\n" % (gs_node_id, if_idx, if_bandwidth))

    #################################
    # FORWARDING STATE：修改以支持多接口
    #

    # 前一个fstate（delta）
    prev_fstate = None
    if prev_output is not None:
        prev_fstate = prev_output["fstate"]

    # gid到卫星GSL接口索引的映射：现在是列表，因为多接口
    # 但为了兼容原有fstate计算，我们可以为每个GS-Sat连接分配独特的if_idx
    # 这里简化：使用卫星侧的assigned_if_idx作为gid_to_sat_gsl_if_idx[gid]，但需扩展为per-connection
    # 注意：fstate计算需要调整以支持多接口选择（例如，选择最佳路径时考虑多个入口/出口）

    # 为简单起见，我们扩展gid_to_sat_gsl_if_idx为字典：gid -> [(sid, sat_if_idx, gs_if_idx)]
    gid_to_connections = {}
    for gid in range(len(ground_stations)):
        connections = []
        gs_if_idx = 0
        for _, sid in ground_station_satellites_in_range_selected[gid]:
            sat_if_idx = satellite_gsl_ifs_assigned[sid][gid][0]  # 只一个
            connections.append((sid, sat_if_idx, gs_if_idx))
            gs_if_idx += 1
        gid_to_connections[gid] = connections

    # 修改fstate计算：需要更新calculate_fstate_shortest_path_without_gs_relaying以支持多接口
    # （见步骤3）
    print("被调用了")
    fstate = calculate_fstate_shortest_path_without_gs_relaying_multi_if(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        len(satellites),
        len(ground_stations),
        sat_net_graph_only_satellites_with_isls,
        num_isls_per_sat,
        gid_to_connections,                                      # 关键：传多连接
        ground_station_satellites_in_range_selected,             # 注意这里原来是 _selected
        sat_neighbor_to_if,
        prev_fstate,
        enable_verbose_logs
    )

    if enable_verbose_logs:
        print("")

    return {
        "fstate": fstate
    }