# The MIT License (MIT)
#
# Copyright (c) 2020 ETH Zurich
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys
sys.path.append("../../satgenpy")
import satgen
import math
import os
import exputil


# WGS72 value; taken from https://geographiclib.sourceforge.io/html/NET/NETGeographicLib_8h_source.html
EARTH_RADIUS = 6378135.0

# Target altitude of ~630 km
ALTITUDE_M = 630000

# Considering an elevation angle of 30 degrees; possible values [1]: 20(min)/30/35/45
SATELLITE_CONE_RADIUS_M = ALTITUDE_M / math.tan(math.radians(30.0))

# Maximum GSL length
MAX_GSL_LENGTH_M = math.sqrt(math.pow(SATELLITE_CONE_RADIUS_M, 2) + math.pow(ALTITUDE_M, 2))

# ISLs are not allowed to dip below 80 km altitude in order to avoid weather conditions
MAX_ISL_LENGTH_M = 2 * math.sqrt(math.pow(EARTH_RADIUS + ALTITUDE_M, 2) - math.pow(EARTH_RADIUS + 80000, 2))

# Full shell sizes
NUM_ORBS = 34
NUM_SATS_PER_ORB = 34

##
# TO SELECT THE NODES SET FROM THE FULL SHELL RUN
#
# utilization_filename = "a_b/runs/kuiper_630_isls_1173_to_1241_with_TcpNewReno_at_10_Mbps/logs_ns3/isl_utilization.csv"
# active_nodes = set()
# with open(utilization_filename) as f_in:
#     for line in f_in:
#         x = line.split(",")
#         if float(x[4]) != 0:
#             active_nodes.add(int(x[0]))
#             active_nodes.add(int(x[1]))
# print(active_nodes)
#
# active_links = set()
# with open(utilization_filename) as f_in:
#     for line in f_in:
#         x = line.split(",")
#         if float(x[4]) != 0:
#             active_links.add((int(x[0]), int(x[1])))
# sorted_active_links = sorted(list(active_links))
# for a in sorted_active_links:
#     if int(a[0]) <= int(a[1]):
#         print(a)
#     elif (int(a[1]), int(a[0])) not in sorted_active_links:
#         print((int(a[1]), int(a[0])))
#

local_shell = exputil.LocalShell()

# Clean slate start
local_shell.remove_force_recursive("temp/gen_data")
local_shell.make_full_dir("temp/gen_data")

# Both dynamic state algorithms should yield the same path and RTT
# for dynamic_state_algorithm in [
#     "algorithm_free_one_only_over_isls",
#     "algorithm_free_gs_one_sat_many_only_over_isls"
# ]:
for dynamic_state_algorithm in [
    "algorithm_free_gs_many_sat_many_only_over_isls"
]:

    # Specific outcomes
    output_generated_data_dir = "temp/gen_data"
    num_threads = 8
    time_step_ms = 100
    duration_s = 200

    # Add base name to setting
    name = "reduced_kuiper_630_" + dynamic_state_algorithm

    # Path trace we base this test on:
    # 0,1173-184-183-217-1241
    # 18000000000,1173-218-217-1241
    # 27600000000,1173-648-649-650-616-1241
    # 74300000000,1173-218-217-216-250-1241
    # 125900000000,1173-647-648-649-650-616-1241
    # 128700000000,1173-647-648-649-615-1241

    # Nodes
    #
    # They were chosen based on selecting only the satellites which
    # saw any utilization during a run over the full Kuiper constellation
    # (which takes too long to generate forwarding state for)
    #
    # Original ID   Test ID
    # 183           0
    # 184           1
    # 215           2
    # 216           3
    # 217           4
    # 218           5
    # 249           6
    # 250           7
    # 615           8
    # 616           9
    # 647           10
    # 648           11
    # 649           12
    # 650           13
    # 682           14
    # 683           15
    # 684           16

    limited_satellite_set = {
        183, 184,
        215, 216, 217, 218, 249, 250,
        615, 616,
        647, 648, 649, 650,
        682, 683, 684
    }
    limited_satellite_idx_map = {
        183: 0,
        184: 1,
        215: 2,
        216: 3,
        217: 4,
        218: 5,
        249: 6,
        250: 7,
        615: 8,
        616: 9,
        647: 10,
        648: 11,
        649: 12,
        650: 13,
        682: 14,
        683: 15,
        684: 16
    }

    # Create output directories
    if not os.path.isdir(output_generated_data_dir):
        os.makedirs(output_generated_data_dir)
    if not os.path.isdir(output_generated_data_dir + "/" + name):
        os.makedirs(output_generated_data_dir + "/" + name)




#18,Rio-de-Janeiro,-22.902780,-43.207500,0.000000,4284573.241489,-4024537.642061,-2466804.550393
#73,Sankt-Peterburg-(Saint-Petersburg),59.929858,30.326228,0.000000,2765465.818184,1617706.410247,5496564.164974
# 0,Manila,14.6042,120.9822,0
# 1,Dalian,38.913811,121.602322,0
    # Ground stations
    print("Generating ground stations...")
    with open(output_generated_data_dir + "/" + name + "/ground_stations.basic.txt", "w+") as f_out:
        f_out.write("0,Manila,14.6042,120.9822,0\n")  # Originally no. 17 in top 100
        f_out.write("1,Dalian,38.913811,121.602322,0\n")  # Originally no. 85 in top 100
    satgen.extend_ground_stations(
        output_generated_data_dir + "/" + name + "/ground_stations.basic.txt",
        output_generated_data_dir + "/" + name + "/ground_stations.txt"
    )

    # TLEs (taken from Kuiper-610 first shell)
    print("Generating TLEs...")
   # ==================== TLEs：改成完整 1156 颗 ====================
    # 你说你已经有完整的 tle.txt，直接把它拷贝进来
    os.makedirs(output_generated_data_dir + "/" + name, exist_ok=True)
    
    # 直接复制你当前的完整 tle.txt（必须放在本目录下，文件名叫 tle_full_1156.txt 也可以改名）
    local_shell.copy_file(
        "templates/tles.txt",   # ←←←← 这里改成你的完整 TLE 文件名
        output_generated_data_dir + "/" + name + "/tles.txt"
    )
    
    # ISLs
    print("Generating ISLs...")
    complete_list_isls = satgen.generate_plus_grid_isls(
        output_generated_data_dir + "/" + name + "/isls.txt",
        NUM_ORBS,
        NUM_SATS_PER_ORB,
        isl_shift=0,
        idx_offset=0
    )

    # Description
    print("Generating description...")
    satgen.generate_description(
        output_generated_data_dir + "/" + name + "/description.txt",
        MAX_GSL_LENGTH_M,
        MAX_ISL_LENGTH_M
    )

    # Extended ground stations
    ground_stations = satgen.read_ground_stations_extended(
        output_generated_data_dir + "/" + name + "/ground_stations.txt"
    )
    
    #py修改
    # GSL interfaces
    if dynamic_state_algorithm == "algorithm_free_one_only_over_isls":
        gsl_interfaces_per_satellite = 1
        gsl_satellite_max_agg_bandwidth = 1.0
        gsl_gs_max_agg_bandwidth = 1.0  # GS原1.0
        gsl_interfaces_per_gs = 1  # GS原1
    elif dynamic_state_algorithm == "algorithm_free_gs_one_sat_many_only_over_isls":
        gsl_interfaces_per_satellite = len(ground_stations)
        gsl_satellite_max_agg_bandwidth = len(ground_stations)
        gsl_gs_max_agg_bandwidth = 1.0
        gsl_interfaces_per_gs = 1
    else:  # 新算法或其他
        gsl_interfaces_per_satellite = len(ground_stations) * 2  # 修改为 *2（例如2 GS * 2 = 4 接口）
        gsl_satellite_max_agg_bandwidth = len(ground_stations) * 2  # 聚合带宽匹配接口数
        gsl_gs_max_agg_bandwidth = 2.0  # 每个GS聚合2.0
        gsl_interfaces_per_gs = 2  # 每个GS有2个接口

    print("Generating GSL interfaces info..")
    satgen.generate_simple_gsl_interfaces_info(
        output_generated_data_dir + "/" + name + "/gsl_interfaces_info.txt",
        1156,  # 卫星数量
        len(ground_stations),
        gsl_interfaces_per_satellite,  # 每个卫星的GSL接口数
        gsl_interfaces_per_gs,  # 每个地面站的接口数（改为3）
        gsl_satellite_max_agg_bandwidth,  # 卫星聚合带宽
        gsl_gs_max_agg_bandwidth  # 地面站聚合带宽（改为2.0）
    )

    # Forwarding state
    print("Generating forwarding state...")
    satgen.help_dynamic_state(
        output_generated_data_dir,
        num_threads,
        name,
        time_step_ms,
        duration_s,
        MAX_GSL_LENGTH_M,
        MAX_ISL_LENGTH_M,
        dynamic_state_algorithm,
        False
    )

    # TODO: Add parameter to specify where plotting files are
    # # Clean slate start
    # local_shell.remove_force_recursive("temp/analysis_data")
    # local_shell.make_full_dir("temp/analysis_data")
    # output_analysis_data_dir = "temp/analysis_data"
    # satgen.post_analysis.print_routes_and_rtt(
    #     output_analysis_data_dir + "/" + name,
    #     output_generated_data_dir + "/" + name,
    #     time_step_ms,
    #     duration_s,
    #     17,
    #     18
    # )
