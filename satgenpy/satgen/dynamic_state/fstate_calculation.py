import math
import networkx as nx


def calculate_fstate_shortest_path_without_gs_relaying(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        num_satellites,
        num_ground_stations,
        sat_net_graph_only_satellites_with_isls,
        num_isls_per_sat,
        gid_to_sat_gsl_if_idx,
        ground_station_satellites_in_range_candidates,
        sat_neighbor_to_if,
        prev_fstate,
        enable_verbose_logs
):

    # Calculate shortest path distances
    if enable_verbose_logs:
        print("  > Calculating Floyd-Warshall for graph without ground-station relays")
    # (Note: Numpy has a deprecation warning here because of how networkx uses matrices)
    dist_sat_net_without_gs = nx.floyd_warshall_numpy(sat_net_graph_only_satellites_with_isls)

    # Forwarding state
    fstate = {}

    # Now write state to file for complete graph
    output_filename = output_dynamic_state_dir + "/fstate_" + str(time_since_epoch_ns) + ".txt"
    if enable_verbose_logs:
        print("  > Writing forwarding state to: " + output_filename)
    with open(output_filename, "w+") as f_out:

        # Satellites to ground stations
        # From the satellites attached to the destination ground station,
        # select the one which promises the shortest path to the destination ground station (getting there + last hop)
        dist_satellite_to_ground_station = {}
        for curr in range(num_satellites):
            for dst_gid in range(num_ground_stations):
                dst_gs_node_id = num_satellites + dst_gid

                # Among the satellites in range of the destination ground station,
                # find the one which promises the shortest distance
                possible_dst_sats = ground_station_satellites_in_range_candidates[dst_gid]
                possibilities = []
                for b in possible_dst_sats:
                    if not math.isinf(dist_sat_net_without_gs[(curr, b[1])]):  # Must be reachable
                        possibilities.append(
                            (
                                dist_sat_net_without_gs[(curr, b[1])] + b[0],
                                b[1]
                            )
                        )
                possibilities = list(sorted(possibilities))

                # By default, if there is no satellite in range for the
                # destination ground station, it will be dropped (indicated by -1)
                next_hop_decision = (-1, -1, -1)
                distance_to_ground_station_m = float("inf")
                if len(possibilities) > 0:
                    dst_sat = possibilities[0][1]
                    distance_to_ground_station_m = possibilities[0][0]

                    # If the current node is not that satellite, determine how to get to the satellite
                    if curr != dst_sat:

                        # Among its neighbors, find the one which promises the
                        # lowest distance to reach the destination satellite
                        best_distance_m = 1000000000000000
                        for neighbor_id in sat_net_graph_only_satellites_with_isls.neighbors(curr):
                            distance_m = (
                                    sat_net_graph_only_satellites_with_isls.edges[(curr, neighbor_id)]["weight"]
                                    +
                                    dist_sat_net_without_gs[(neighbor_id, dst_sat)]
                            )
                            if distance_m < best_distance_m:
                                next_hop_decision = (
                                    neighbor_id,
                                    sat_neighbor_to_if[(curr, neighbor_id)],
                                    sat_neighbor_to_if[(neighbor_id, curr)]
                                )
                                best_distance_m = distance_m

                    else:
                        # This is the destination satellite, as such the next hop is the ground station itself
                        next_hop_decision = (
                            dst_gs_node_id,
                            num_isls_per_sat[dst_sat] + gid_to_sat_gsl_if_idx[dst_gid],
                            0
                        )

                # In any case, save the distance of the satellite to the ground station to re-use
                # when we calculate ground station to ground station forwarding
                dist_satellite_to_ground_station[(curr, dst_gs_node_id)] = distance_to_ground_station_m

                # Write to forwarding state
                if not prev_fstate or prev_fstate[(curr, dst_gs_node_id)] != next_hop_decision:
                    f_out.write("%d,%d,%d,%d,%d\n" % (
                        curr,
                        dst_gs_node_id,
                        next_hop_decision[0],
                        next_hop_decision[1],
                        next_hop_decision[2]
                    ))
                fstate[(curr, dst_gs_node_id)] = next_hop_decision

        # Ground stations to ground stations
        # Choose the source satellite which promises the shortest path
        for src_gid in range(num_ground_stations):
            for dst_gid in range(num_ground_stations):
                if src_gid != dst_gid:
                    src_gs_node_id = num_satellites + src_gid
                    dst_gs_node_id = num_satellites + dst_gid

                    # Among the satellites in range of the source ground station,
                    # find the one which promises the shortest distance
                    possible_src_sats = ground_station_satellites_in_range_candidates[src_gid]
                    possibilities = []
                    for a in possible_src_sats:
                        best_distance_offered_m = dist_satellite_to_ground_station[(a[1], dst_gs_node_id)]
                        if not math.isinf(best_distance_offered_m):
                            possibilities.append(
                                (
                                    a[0] + best_distance_offered_m,
                                    a[1]
                                )
                            )
                    possibilities = sorted(possibilities)

                    # By default, if there is no satellite in range for one of the
                    # ground stations, it will be dropped (indicated by -1)
                    next_hop_decision = (-1, -1, -1)
                    if len(possibilities) > 0:
                        src_sat_id = possibilities[0][1]
                        next_hop_decision = (
                            src_sat_id,
                            0,
                            num_isls_per_sat[src_sat_id] + gid_to_sat_gsl_if_idx[src_gid]
                        )

                    # Update forwarding state
                    if not prev_fstate or prev_fstate[(src_gs_node_id, dst_gs_node_id)] != next_hop_decision:
                        f_out.write("%d,%d,%d,%d,%d\n" % (
                            src_gs_node_id,
                            dst_gs_node_id,
                            next_hop_decision[0],
                            next_hop_decision[1],
                            next_hop_decision[2]
                        ))
                    fstate[(src_gs_node_id, dst_gs_node_id)] = next_hop_decision

    # Finally return result
    return fstate


def calculate_fstate_shortest_path_with_gs_relaying(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        num_satellites,
        num_ground_stations,
        sat_net_graph,
        num_isls_per_sat,
        gid_to_sat_gsl_if_idx,
        sat_neighbor_to_if,
        prev_fstate,
        enable_verbose_logs
):

    # Calculate shortest paths
    if enable_verbose_logs:
        print("  > Calculating Floyd-Warshall for graph including ground-station relays")
    # (Note: Numpy has a deprecation warning here because of how networkx uses matrices)
    dist_sat_net = nx.floyd_warshall_numpy(sat_net_graph)

    # Forwarding state
    fstate = {}

    # Now write state to file for complete graph
    output_filename = output_dynamic_state_dir + "/fstate_" + str(time_since_epoch_ns) + ".txt"
    if enable_verbose_logs:
        print("  > Writing forwarding state to: " + output_filename)
    with open(output_filename, "w+") as f_out:

        # Satellites and ground stations to ground stations
        for current_node_id in range(num_satellites + num_ground_stations):
            for dst_gid in range(num_ground_stations):
                dst_gs_node_id = num_satellites + dst_gid

                # Cannot forward to itself
                if current_node_id != dst_gs_node_id:

                    # Among its neighbors, find the one which promises the
                    # lowest distance to reach the destination satellite
                    next_hop_decision = (-1, -1, -1)
                    best_distance_m = 1000000000000000
                    for neighbor_id in sat_net_graph.neighbors(current_node_id):

                        # Any neighbor must be reachable
                        if math.isinf(dist_sat_net[(current_node_id, neighbor_id)]):
                            raise ValueError("Neighbor cannot be unreachable")

                        # Calculate distance = next-hop + distance the next hop node promises
                        distance_m = (
                            sat_net_graph.edges[(current_node_id, neighbor_id)]["weight"]
                            +
                            dist_sat_net[(neighbor_id, dst_gs_node_id)]
                        )
                        if (
                                not math.isinf(dist_sat_net[(neighbor_id, dst_gs_node_id)])
                                and
                                distance_m < best_distance_m
                        ):

                            # Check node identifiers to determine what are the
                            # correct interface identifiers
                            if current_node_id >= num_satellites and neighbor_id < num_satellites:  # GS to sat.
                                my_if = 0
                                next_hop_if = (
                                    num_isls_per_sat[neighbor_id]
                                    +
                                    gid_to_sat_gsl_if_idx[current_node_id - num_satellites]
                                )

                            elif current_node_id < num_satellites and neighbor_id >= num_satellites:  # Sat. to GS
                                my_if = (
                                    num_isls_per_sat[current_node_id]
                                    +
                                    gid_to_sat_gsl_if_idx[neighbor_id - num_satellites]
                                )
                                next_hop_if = 0

                            elif current_node_id < num_satellites and neighbor_id < num_satellites:  # Sat. to sat.
                                my_if = sat_neighbor_to_if[(current_node_id, neighbor_id)]
                                next_hop_if = sat_neighbor_to_if[(neighbor_id, current_node_id)]

                            else:  # GS to GS
                                raise ValueError("GS-to-GS link cannot exist")

                            # Write the next-hop decision
                            next_hop_decision = (
                                neighbor_id,  # Next-hop node identifier
                                my_if,        # My outgoing interface id
                                next_hop_if   # Next-hop incoming interface id
                            )

                            # Update best distance found
                            best_distance_m = distance_m

                    # Write to forwarding state
                    if not prev_fstate or prev_fstate[(current_node_id, dst_gs_node_id)] != next_hop_decision:
                        f_out.write("%d,%d,%d,%d,%d\n" % (
                            current_node_id,
                            dst_gs_node_id,
                            next_hop_decision[0],
                            next_hop_decision[1],
                            next_hop_decision[2]
                        ))
                    fstate[(current_node_id, dst_gs_node_id)] = next_hop_decision

    # Finally return result
    return fstate

#py新增
#添加新函数 calculate_fstate_shortest_path_without_gs_relaying_multi_if（基于原有函数复制并修改）。
# 修改点：
# gid_to_sat_gsl_if_idx 改为 gid_to_connections: gid -> list of (sid, sat_if_idx, gs_if_idx)
# 在计算路径时，为src/dst GS选择最佳连接（例如，最短距离的接口组合）。
# def calculate_fstate_shortest_path_without_gs_relaying_multi_if(
#         output_dynamic_state_dir,
#         time_since_epoch_ns,
#         num_satellites,
#         num_ground_stations,
#         sat_net_graph_only_satellites_with_isls,
#         num_isls_per_sat,
#         gid_to_connections,  # 新: gid -> [(sid, sat_if_idx, gs_if_idx), ...]
#         ground_station_satellites_in_range_candidates,
#         sat_neighbor_to_if,
#         prev_fstate,
#         enable_verbose_logs
# ):

#     # Floyd-Warshall (保持不变)
#     if enable_verbose_logs:
#         print("  > Calculating Floyd-Warshall for graph without ground-station relays")
#     dist_sat_net_without_gs = nx.floyd_warshall_numpy(sat_net_graph_only_satellites_with_isls)

#     # Forwarding state
#     fstate = {}

#     output_filename = output_dynamic_state_dir + "/fstate_" + str(time_since_epoch_ns) + ".txt"
#     if enable_verbose_logs:
#         print("  > Writing forwarding state to: " + output_filename)
#     with open(output_filename, "w+") as f_out:

#         # Satellites to ground stations (类似原有，但使用多连接)
#         dist_satellite_to_ground_station = {}
#         for curr in range(num_satellites):
#             for dst_gid in range(num_ground_stations):
#                 dst_gs_node_id = num_satellites + dst_gid

#                 # 选择dst GS的最佳出口连接（最短 dist + last hop）
#                 possibilities = []
#                 for (dst_sid, dst_sat_if, dst_gs_if) in gid_to_connections[dst_gid]:
#                     if not math.isinf(dist_sat_net_without_gs[(curr, dst_sid)]):
#                         # last_hop_dist 从 ground_station_satellites_in_range_candidates 取
#                         last_hop_dist = next(d for d, s in ground_station_satellites_in_range_candidates[dst_gid] if s == dst_sid)
#                         possibilities.append(
#                             (
#                                 dist_sat_net_without_gs[(curr, dst_sid)] + last_hop_dist,
#                                 dst_sid,
#                                 dst_sat_if,
#                                 dst_gs_if
#                             )
#                         )
#                 possibilities = sorted(possibilities)

#                 next_hop_decision = (-1, -1, -1)
#                 distance_to_ground_station_m = float("inf")
#                 if possibilities:
#                     dist, dst_sat, dst_sat_if, dst_gs_if = possibilities[0]
#                     distance_to_ground_station_m = dist
#                     if curr != dst_sat:
#                         # 找最佳邻居 (原有)
#                         best_distance_m = float("inf")
#                         for neighbor_id in sat_net_graph_only_satellites_with_isls.neighbors(curr):
#                             distance_m = (
#                                 sat_net_graph_only_satellites_with_isls.edges[(curr, neighbor_id)]["weight"]
#                                 + dist_sat_net_without_gs[(neighbor_id, dst_sat)]
#                             )
#                             if distance_m < best_distance_m:
#                                 next_hop_decision = (
#                                     neighbor_id,
#                                     sat_neighbor_to_if[(curr, neighbor_id)],
#                                     sat_neighbor_to_if[(neighbor_id, curr)]
#                                 )
#                                 best_distance_m = distance_m
#                     else:
#                         # 直接到GS
#                         next_hop_decision = (
#                             dst_gs_node_id,
#                             num_isls_per_sat[dst_sat] + dst_sat_if,
#                             dst_gs_if  # GS侧接口
#                         )

#                 dist_satellite_to_ground_station[(curr, dst_gs_node_id)] = distance_to_ground_station_m

#                 if not prev_fstate or prev_fstate[(curr, dst_gs_node_id)] != next_hop_decision:
#                     f_out.write("%d,%d,%d,%d,%d\n" % (
#                         curr, dst_gs_node_id, next_hop_decision[0], next_hop_decision[1], next_hop_decision[2]
#                     ))
#                 fstate[(curr, dst_gs_node_id)] = next_hop_decision

#         # Ground stations to ground stations (类似，但选择src GS的最佳入口和dst的最佳出口)
#         for src_gid in range(num_ground_stations):
#             for dst_gid in range(num_ground_stations):
#                 if src_gid != dst_gid:
#                     src_gs_node_id = num_satellites + src_gid
#                     dst_gs_node_id = num_satellites + dst_gid

#                     # 选择src GS的最佳入口卫星（最短 first_hop + dist_to_dst）
#                     possibilities = []
#                     for (src_sid, src_sat_if, src_gs_if) in gid_to_connections[src_gid]:
#                         # first_hop_dist
#                         first_hop_dist = next(d for d, s in ground_station_satellites_in_range_candidates[src_gid] if s == src_sid)
#                         best_dist_to_dst = dist_satellite_to_ground_station[(src_sid, dst_gs_node_id)]
#                         if not math.isinf(best_dist_to_dst):
#                             possibilities.append(
#                                 (
#                                     first_hop_dist + best_dist_to_dst,
#                                     src_sid,
#                                     src_sat_if,
#                                     src_gs_if
#                                 )
#                             )
#                     possibilities = sorted(possibilities)

#                     next_hop_decision = (-1, -1, -1)
#                     if possibilities:
#                         _, src_sat_id, src_sat_if, src_gs_if = possibilities[0]
#                         next_hop_decision = (
#                             src_sat_id,
#                             src_gs_if,  # GS侧出接口
#                             num_isls_per_sat[src_sat_id] + src_sat_if  # Sat侧入接口
#                         )

#                     if not prev_fstate or prev_fstate[(src_gs_node_id, dst_gs_node_id)] != next_hop_decision:
#                         f_out.write("%d,%d,%d,%d,%d\n" % (
#                             src_gs_node_id, dst_gs_node_id, next_hop_decision[0], next_hop_decision[1], next_hop_decision[2]
#                         ))
#                     fstate[(src_gs_node_id, dst_gs_node_id)] = next_hop_decision

#     return fstate


# # ===================== 新增：真正支持多接口的版本 =====================
# def calculate_fstate_shortest_path_without_gs_relaying_multi_if(
#         output_dynamic_state_dir,
#         time_since_epoch_ns,
#         num_satellites,
#         num_ground_stations,
#         sat_net_graph_only_satellites_with_isls,
#         num_isls_per_sat,
#         gid_to_connections,                    # gid → [(sid, sat_if_idx, gs_if_idx), ...]  长度最多3
#         ground_station_satellites_in_range_candidates,
#         sat_neighbor_to_if,
#         prev_fstate,
#         enable_verbose_logs
# ):
#     if enable_verbose_logs:
#         print("  > 计算 Floyd-Warshall（卫星间最短路径）")
#     dist_sat = nx.floyd_warshall_numpy(sat_net_graph_only_satellites_with_isls)

#     fstate = {}
#     output_filename = output_dynamic_state_dir + "/fstate_" + str(time_since_epoch_ns) + ".txt"
#     if enable_verbose_logs:
#         print("  > 写入转发状态文件:", output_filename)

#     with open(output_filename, "w+") as f_out:

#         # ====================== 1. 卫星 → 地面站（下行）======================
#         # 每个卫星到每个GS，选择当前最优的那个出口卫星（原来的逻辑已经最优，不需要多路径）
#         for curr_sat in range(num_satellites):
#             for dst_gid in range(num_ground_stations):
#                 dst_gs_node = num_satellites + dst_gid
#                 best = (float("inf"), -1, -1, -1)   # (距离, dst_sid, sat_if, gs_if)

#                 for (sid, sat_if, gs_if) in gid_to_connections[dst_gid]:
#                     if sid not in sat_net_graph_only_satellites_with_isls:
#                         continue
#                     d = dist_sat[curr_sat, sid]
#                     if math.isinf(d):
#                         continue
#                     last_hop = next(dist for dist, s in ground_station_satellites_in_range_candidates[dst_gid] if s == sid)
#                     total = d + last_hop
#                     if total < best[0]:
#                         best = (total, sid, sat_if, gs_if)

#                 if best[1] == -1:  # 不可达
#                     next_hop = (-1, -1, -1)
#                 elif curr_sat == best[1]:  # 自己就是出口卫星
#                     next_hop = (dst_gs_node, num_isls_per_sat[curr_sat] + best[2], best[3])
#                 else:
#                     # 找去往出口卫星的最佳邻居
#                     best_nh = (-1, -1, -1)
#                     best_dist = float("inf")
#                     for nb in sat_net_graph_only_satellites_with_isls.neighbors(curr_sat):
#                         tmp = (sat_net_graph_only_satellites_with_isls.edges[curr_sat, nb]["weight"] +
#                                dist_sat[nb, best[1]])
#                         if tmp < best_dist:
#                             best_dist = tmp
#                             best_nh = (nb,
#                                         sat_neighbor_to_if[(curr_sat, nb)],
#                                         sat_neighbor_to_if[(nb, curr_sat)])
#                     next_hop = best_nh

#                 if not prev_fstate or prev_fstate.get((curr_sat, dst_gs_node)) != next_hop:
#                     f_out.write(f"{curr_sat},{dst_gs_node},{next_hop[0]},{next_hop[1]},{next_hop[2]}\n")
#                 fstate[(curr_sat, dst_gs_node)] = next_hop

#         # ====================== 2. 地面站 → 地面站（上行）← 重点！真正用满3个口 =====================
#         for src_gid in range(num_ground_stations):
#             for dst_gid in range(num_ground_stations):
#                 if src_gid == dst_gid:
#                     continue

#                 src_gs_node  = num_satellites + src_gid
#                 dst_gs_node  = num_satellites + dst_gid

#                 # 找出当前时刻所有可用的入口卫星（最多3个）
#                 candidates = []
#                 for (entry_sid, sat_if_on_sat, gs_if_on_gs) in gid_to_connections[src_gid]:
#                     # 入口第一跳距离
#                     first_hop_dist = next(d for d,s in ground_station_satellites_in_range_candidates[src_gid] if s == entry_sid)

#                     # 这颗入口卫星到目的GS的最短距离（已经提前算好）
#                     to_dst_dist = float("inf")
#                     for (exit_sid, _, _) in gid_to_connections[dst_gid]:
#                         if not math.isinf(dist_sat[entry_sid, exit_sid]):
#                             last_hop = next(d for d,s in ground_station_satellites_in_range_candidates[dst_gid] if s == exit_sid)
#                             to_dst_dist = min(to_dst_dist, dist_sat[entry_sid, exit_sid] + last_hop)

#                     if to_dst_dist < float("inf"):
#                         total_path_len = first_hop_dist + to_dst_dist
#                         candidates.append((total_path_len, entry_sid, sat_if_on_sat, gs_if_on_gs))

#                 if not candidates:
#                     continue

#                 # 策略：全部可达的入口都用上！（这就是真·3端口并行）
#                 # 即使路径长度差10%，我们也全用，换取3倍带宽（LEO场景完全值得）
#                 for _, entry_sid, sat_if, gs_if in candidates:
#                     # 从地面站src_gid的 gs_if 口直接发给卫星 entry_sid 的 sat_if 接口
#                     line = f"{src_gs_node},{dst_gs_node},{entry_sid},{gs_if},{num_isls_per_sat[entry_sid] + sat_if}\n"
#                     f_out.write(line)

#                     # 同时也要写反向（如果你要双向对称，下面再补一行）
#                     # 这里我们不写反向，因为下行已经覆盖了

#     if enable_verbose_logs:
#         print("  > 地面站→地面站 转发规则已写入，所有3个端口全部利用！")
#     return fstate
# ===================== 新增：支持多接口 + path_id 的版本 =====================
def calculate_fstate_shortest_path_without_gs_relaying_multi_if(
        output_dynamic_state_dir,
        time_since_epoch_ns,
        num_satellites,
        num_ground_stations,
        sat_net_graph_only_satellites_with_isls,
        num_isls_per_sat,
        gid_to_connections,                    # gid → [(sid, sat_if_idx, gs_if_idx), ...]  长度最多3
        ground_station_satellites_in_range_candidates,
        sat_neighbor_to_if,
        prev_fstate,
        enable_verbose_logs
):
    if enable_verbose_logs:
        print("  > 计算 Floyd-Warshall（卫星间最短路径）")
    dist_sat = nx.floyd_warshall_numpy(sat_net_graph_only_satellites_with_isls)

    fstate = {}
    output_filename = output_dynamic_state_dir + "/fstate_" + str(time_since_epoch_ns) + ".txt"
    if enable_verbose_logs:
        print("  > 写入转发状态文件（新增path_id列）:", output_filename)

    with open(output_filename, "w+") as f_out:

        # ====================== 1. 卫星 → 地面站（下行）======================
        for curr_sat in range(num_satellites):
            for dst_gid in range(num_ground_stations):
                dst_gs_node = num_satellites + dst_gid
                best = (float("inf"), -1, -1, -1)   # (距离, dst_sid, sat_if, gs_if)

                for (sid, sat_if, gs_if) in gid_to_connections[dst_gid]:
                    if sid not in sat_net_graph_only_satellites_with_isls:
                        continue
                    d = dist_sat[curr_sat, sid]
                    if math.isinf(d):
                        continue
                    last_hop = next(dist for dist, s in ground_station_satellites_in_range_candidates[dst_gid] if s == sid)
                    total = d + last_hop
                    if total < best[0]:
                        best = (total, sid, sat_if, gs_if)

                path_id = 0  # 默认星间链路用0
                if best[1] == -1:  # 不可达
                    next_hop = (-1, -1, -1)
                elif curr_sat == best[1]:  # 自己就是出口卫星 → 直接下行到GS
                    next_hop = (dst_gs_node, num_isls_per_sat[curr_sat] + best[2], best[3])
                    path_id = best[3]  # 使用GS侧接口索引作为path_id（0,1,2）
                else:
                    # 转发给邻居卫星 → 星间链路
                    best_nh = (-1, -1, -1)
                    best_dist = float("inf")
                    for nb in sat_net_graph_only_satellites_with_isls.neighbors(curr_sat):
                        tmp = (sat_net_graph_only_satellites_with_isls.edges[curr_sat, nb]["weight"] +
                               dist_sat[nb, best[1]])
                        if tmp < best_dist:
                            best_dist = tmp
                            best_nh = (nb,
                                       sat_neighbor_to_if[(curr_sat, nb)],
                                       sat_neighbor_to_if[(nb, curr_sat)])
                    next_hop = best_nh
                    path_id = 0  # 星间链路

                # 写入：增加path_id列
                if not prev_fstate or prev_fstate.get((curr_sat, dst_gs_node)) != next_hop:
                    f_out.write(f"{curr_sat},{dst_gs_node},{next_hop[0]},{next_hop[1]},{next_hop[2]},{path_id}\n")
                fstate[(curr_sat, dst_gs_node)] = next_hop

        # ====================== 2. 地面站 → 地面站（上行）======================
        for src_gid in range(num_ground_stations):
            for dst_gid in range(num_ground_stations):
                if src_gid == dst_gid:
                    continue

                src_gs_node = num_satellites + src_gid
                dst_gs_node = num_satellites + dst_gid

                # 所有当前可达的入口卫星（最多3个）
                candidates = []
                for (entry_sid, sat_if_on_sat, gs_if_on_gs) in gid_to_connections[src_gid]:
                    first_hop_dist = next(d for d, s in ground_station_satellites_in_range_candidates[src_gid] if s == entry_sid)

                    # 计算这颗入口卫星能否到达目的GS（通过任意一个出口）
                    to_dst_dist = float("inf")
                    for (exit_sid, _, _) in gid_to_connections[dst_gid]:
                        if not math.isinf(dist_sat[entry_sid, exit_sid]):
                            last_hop = next(d for d, s in ground_station_satellites_in_range_candidates[dst_gid] if s == exit_sid)
                            to_dst_dist = min(to_dst_dist, dist_sat[entry_sid, exit_sid] + last_hop)

                    if to_dst_dist < float("inf"):
                        total_path_len = first_hop_dist + to_dst_dist
                        candidates.append((total_path_len, entry_sid, sat_if_on_sat, gs_if_on_gs))

                # 为每一个可达的入口接口都生成一条转发规则（充分利用3个端口）
                for _, entry_sid, sat_if, gs_if in candidates:
                    next_hop_node = entry_sid
                    my_if = gs_if                          # GS侧出接口 (0,1,2)
                    next_if = num_isls_per_sat[entry_sid] + sat_if
                    path_id = gs_if                         # 关键：path_id = GS接口索引 → 天然 0,1,2

                    line = f"{src_gs_node},{dst_gs_node},{next_hop_node},{my_if},{next_if},{path_id}\n"
                    f_out.write(line)

                    # 可选：如果你后续需要对称路由，也可以写反向（但通常不需要，因为下行已覆盖）

    if enable_verbose_logs:
        print("  > 转发状态生成完成，已添加 path_id 列：星间=0，星地=GS接口索引(0,1,2)")
    return fstate