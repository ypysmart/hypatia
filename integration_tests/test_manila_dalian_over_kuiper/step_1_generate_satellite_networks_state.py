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
NUM_SATELLITES = NUM_ORBS * NUM_SATS_PER_ORB  # 1156

local_shell = exputil.LocalShell()

# Clean slate start
local_shell.remove_force_recursive("temp/gen_data")
local_shell.make_full_dir("temp/gen_data")

# Both dynamic state algorithms should yield the same path and RTT
for dynamic_state_algorithm in [
    "algorithm_free_one_only_over_isls",
    "algorithm_free_gs_one_sat_many_only_over_isls"
]:

    # Specific outcomes
    output_generated_data_dir = "temp/gen_data"
    num_threads = 1
    time_step_ms = 100
    duration_s = 200

    # Add base name to setting
    name = "reduced_kuiper_630_" + dynamic_state_algorithm

    # Create output directories
    if not os.path.isdir(output_generated_data_dir):
        os.makedirs(output_generated_data_dir)
    if not os.path.isdir(output_generated_data_dir + "/" + name):
        os.makedirs(output_generated_data_dir + "/" + name)

    # Ground stations
    print("Generating ground stations...")
    with open(output_generated_data_dir + "/" + name + "/ground_stations.txt", "w+") as f_out:
        f_out.write("17, \"Manila, Philippines\", 14.583, 121.0, 0\n")
        f_out.write("18, \"Dalian, China\", 38.9, 121.6167, 0\n")

    # TLEs (generate full 1156 satellites)
    print("Generating TLEs...")
    satgen.generate_tles_from_scratch_manual(
        output_generated_data_dir + "/" + name + "/tles.txt",
        "Kuiper-630",
        num_orbits=NUM_ORBS,
        num_satellites_per_orbit=NUM_SATS_PER_ORB,
        phase_shift=0,
        inclination_degree=51.9,
        eccentricity=0.0000001,
        arg_of_perigee_degree=0.0,
        mean_anomaly_start_degree=0.0,
        altitude_m=ALTITUDE_M,
        earth_radius_m=EARTH_RADIUS
    )

    # ISLs (full +grid ISLs, no filtering)
    print("Generating ISLs...")
    satgen.generate_plus_grid_isls(
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

    # GSL interfaces
    if dynamic_state_algorithm == "algorithm_free_one_only_over_isls":
        gsl_interfaces_per_satellite = 1
        gsl_satellite_max_agg_bandwidth = 1.0
    elif dynamic_state_algorithm == "algorithm_free_gs_one_sat_many_only_over_isls":
        gsl_interfaces_per_satellite = len(ground_stations)
        gsl_satellite_max_agg_bandwidth = len(ground_stations)
    else:
        raise ValueError("Unknown dynamic state algorithm: " + dynamic_state_algorithm)
    print("Generating GSL interfaces info..")
    satgen.generate_simple_gsl_interfaces_info(
        output_generated_data_dir + "/" + name + "/gsl_interfaces_info.txt",
        NUM_SATELLITES,  # 1156 satellites
        len(ground_stations),
        gsl_interfaces_per_satellite,  # GSL interfaces per satellite
        1,  # (GSL) Interfaces per ground station
        gsl_satellite_max_agg_bandwidth,  # Aggregate max. bandwidth satellite (unit unspecified)
        1   # Aggregate max. bandwidth ground station (same unspecified unit)
    )

    # Forwarding state (this will be slow for full constellation!)
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