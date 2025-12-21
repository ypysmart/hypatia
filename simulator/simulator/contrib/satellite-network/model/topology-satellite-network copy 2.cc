/*
 * Copyright (c) 2019 ETH Zurich
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jens Eirik Saethre  June 2019
 *         Andre Aguas         March 2020
 *         Simon               2020
 */

#include "topology-satellite-network.h"

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED (TopologySatelliteNetwork);
    TypeId TopologySatelliteNetwork::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::TopologySatelliteNetwork")
                .SetParent<Object> ()
                .SetGroupName("SatelliteNetwork")
        ;
        return tid;
    }

    TopologySatelliteNetwork::TopologySatelliteNetwork(Ptr<BasicSimulation> basicSimulation, const Ipv4RoutingHelper& ipv4RoutingHelper) {
        m_basicSimulation = basicSimulation;
        ReadConfig();
        Build(ipv4RoutingHelper);
    }

    void TopologySatelliteNetwork::ReadConfig() {
        m_satellite_network_dir = m_basicSimulation->GetRunDir() + "/" + m_basicSimulation->GetConfigParamOrFail("satellite_network_dir");
        m_satellite_network_routes_dir =  m_basicSimulation->GetRunDir() + "/" + m_basicSimulation->GetConfigParamOrFail("satellite_network_routes_dir");
        m_satellite_network_force_static = parse_boolean(m_basicSimulation->GetConfigParamOrDefault("satellite_network_force_static", "false"));
    }

    void
    TopologySatelliteNetwork::Build(const Ipv4RoutingHelper& ipv4RoutingHelper) {
        std::cout << "SATELLITE NETWORK" << std::endl;

        // Initialize satellites
        ReadSatellites();
        std::cout << "  > Number of satellites........ " << m_satelliteNodes.GetN() << std::endl;

        // Initialize ground stations
        ReadGroundStations();
        std::cout << "  > Number of ground stations... " << m_groundStationNodes.GetN() << std::endl;

        // Only ground stations are valid endpoints
        for (uint32_t i = 0; i < m_groundStations.size(); i++) {
            m_endpoints.insert(m_satelliteNodes.GetN() + i);
        }

        // All nodes
        m_allNodes.Add(m_satelliteNodes);
        m_allNodes.Add(m_groundStationNodes);
        std::cout << "  > Number of nodes............. " << m_allNodes.GetN() << std::endl;

        // Install internet stacks on all nodes
        InstallInternetStacks(ipv4RoutingHelper);
        std::cout << "  > Installed Internet stacks" << std::endl;

        // IP helper
        m_ipv4_helper.SetBase ("10.0.0.0", "255.255.255.0");

        // Link settings
        m_isl_data_rate_megabit_per_s = parse_positive_double(m_basicSimulation->GetConfigParamOrFail("isl_data_rate_megabit_per_s"));
        m_gsl_data_rate_megabit_per_s = parse_positive_double(m_basicSimulation->GetConfigParamOrFail("gsl_data_rate_megabit_per_s"));
        m_isl_max_queue_size_pkts = parse_positive_int64(m_basicSimulation->GetConfigParamOrFail("isl_max_queue_size_pkts"));
        m_gsl_max_queue_size_pkts = parse_positive_int64(m_basicSimulation->GetConfigParamOrFail("gsl_max_queue_size_pkts"));

        // Utilization tracking settings
        m_enable_isl_utilization_tracking = parse_boolean(m_basicSimulation->GetConfigParamOrFail("enable_isl_utilization_tracking"));
        if (m_enable_isl_utilization_tracking) {
            m_isl_utilization_tracking_interval_ns = parse_positive_int64(m_basicSimulation->GetConfigParamOrFail("isl_utilization_tracking_interval_ns"));
        }

        // Create ISLs
        std::cout << "  > Reading and creating ISLs" << std::endl;
        ReadISLs();

        // Create GSLs
        std::cout << "  > Creating GSLs" << std::endl;
        CreateGSLs();

        // ARP caches
        std::cout << "  > Populating ARP caches" << std::endl;
        PopulateArpCaches();

        std::cout << std::endl;

    }

    void
    TopologySatelliteNetwork::ReadSatellites()
    {

        // Open file
        std::ifstream fs;
        fs.open(m_satellite_network_dir + "/tles.txt");
        NS_ABORT_MSG_UNLESS(fs.is_open(), "File tles.txt could not be opened");

        // First line:
        // <orbits> <satellites per orbit>
        std::string orbits_and_n_sats_per_orbit;
        std::getline(fs, orbits_and_n_sats_per_orbit);
        std::vector<std::string> res = split_string(orbits_and_n_sats_per_orbit, " ", 2);
        int64_t num_orbits = parse_positive_int64(res[0]);
        int64_t satellites_per_orbit = parse_positive_int64(res[1]);

        // Create the nodes
        m_satelliteNodes.Create(num_orbits * satellites_per_orbit);

        // Associate satellite mobility model with each node
        int64_t counter = 0;
        std::string name, tle1, tle2;
        while (std::getline(fs, name)) {
            std::getline(fs, tle1);
            std::getline(fs, tle2);

            // Format:
            // <name>
            // <TLE line 1>
            // <TLE line 2>

            // Create satellite
            Ptr<Satellite> satellite = CreateObject<Satellite>();
            satellite->SetName(name);
            satellite->SetTleInfo(tle1, tle2);

            // Decide the mobility model of the satellite
            MobilityHelper mobility;
            if (m_satellite_network_force_static) {

                // Static at the start of the epoch
                mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                mobility.Install(m_satelliteNodes.Get(counter));
                Ptr<MobilityModel> mobModel = m_satelliteNodes.Get(counter)->GetObject<MobilityModel>();
                mobModel->SetPosition(satellite->GetPosition(satellite->GetTleEpoch()));

            } else {

                // Dynamic
                mobility.SetMobilityModel(
                        "ns3::SatellitePositionMobilityModel",
                        "SatellitePositionHelper",
                        SatellitePositionHelperValue(SatellitePositionHelper(satellite))
                );
                mobility.Install(m_satelliteNodes.Get(counter));

            }

            // Add to all satellites present
            m_satellites.push_back(satellite);

            counter++;
        }

        // Check that exactly that number of satellites has been read in
        if (counter != num_orbits * satellites_per_orbit) {
            throw std::runtime_error("Number of satellites defined in the TLEs does not match");
        }

        fs.close();
    }

    void
    TopologySatelliteNetwork::ReadGroundStations()
    {

        // Create a new file stream to open the file
        std::ifstream fs;
        fs.open(m_satellite_network_dir + "/ground_stations.txt");
        NS_ABORT_MSG_UNLESS(fs.is_open(), "File ground_stations.txt could not be opened");

        // Read ground station from each line
        std::string line;
        while (std::getline(fs, line)) {

            std::vector<std::string> res = split_string(line, ",", 8);

            // All eight values
            uint32_t gid = parse_positive_int64(res[0]);
            std::string name = res[1];
            double latitude = parse_double(res[2]);
            double longitude = parse_double(res[3]);
            double elevation = parse_double(res[4]);
            double cartesian_x = parse_double(res[5]);
            double cartesian_y = parse_double(res[6]);
            double cartesian_z = parse_double(res[7]);
            Vector cartesian_position(cartesian_x, cartesian_y, cartesian_z);

            // Create ground station data holder
            Ptr<GroundStation> gs = CreateObject<GroundStation>(
                    gid, name, latitude, longitude, elevation, cartesian_position
            );
            m_groundStations.push_back(gs);

            // Create the node
            m_groundStationNodes.Create(1);
            if (m_groundStationNodes.GetN() != gid + 1) {
                throw std::runtime_error("GID is not incremented each line");
            }

            // Install the constant mobility model on the node
            MobilityHelper mobility;
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(m_groundStationNodes.Get(gid));
            Ptr<MobilityModel> mobilityModel = m_groundStationNodes.Get(gid)->GetObject<MobilityModel>();
            mobilityModel->SetPosition(cartesian_position);

        }

        fs.close();
    }

    void
    TopologySatelliteNetwork::InstallInternetStacks(const Ipv4RoutingHelper& ipv4RoutingHelper) {
    QuicHelper stack;
	stack.InstallQuicAndIpv4(m_allNodes, ipv4RoutingHelper);
    }

    void
    TopologySatelliteNetwork::ReadISLs()
    {

        // Link helper
        PointToPointLaserHelper p2p_laser_helper;
        std::string max_queue_size_str = format_string("%" PRId64 "p", m_isl_max_queue_size_pkts);
        p2p_laser_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
        p2p_laser_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_isl_data_rate_megabit_per_s) + "Mbps")));
        std::cout << "    >> ISL data rate........ " << m_isl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
        std::cout << "    >> ISL max queue size... " << m_isl_max_queue_size_pkts << " packets" << std::endl;

        // Traffic control helper
        TrafficControlHelper tch_isl;
        tch_isl.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("1p"))); // Will be removed later any case

        // Open file
        std::ifstream fs;
        fs.open(m_satellite_network_dir + "/isls.txt");
        NS_ABORT_MSG_UNLESS(fs.is_open(), "File isls.txt could not be opened");

        // Read ISL pair from each line
        std::string line;
        int counter = 0;
        while (std::getline(fs, line)) {
            std::vector<std::string> res = split_string(line, " ", 2);

            // Retrieve satellite identifiers
            int32_t sat0_id = parse_positive_int64(res.at(0));
            int32_t sat1_id = parse_positive_int64(res.at(1));
            Ptr<Satellite> sat0 = m_satellites.at(sat0_id);
            Ptr<Satellite> sat1 = m_satellites.at(sat1_id);

            // Install a p2p laser link between these two satellites
            NodeContainer c;
            c.Add(m_satelliteNodes.Get(sat0_id));
            c.Add(m_satelliteNodes.Get(sat1_id));
            NetDeviceContainer netDevices = p2p_laser_helper.Install(c);

            // Install traffic control helper
            tch_isl.Install(netDevices.Get(0));
            tch_isl.Install(netDevices.Get(1));

            // Assign some IP address (nothing smart, no aggregation, just some IP address)
            m_ipv4_helper.Assign(netDevices);
            m_ipv4_helper.NewNetwork();

            // Remove the traffic control layer (must be done here, else the Ipv4 helper will assign a default one)
            TrafficControlHelper tch_uninstaller;
            tch_uninstaller.Uninstall(netDevices.Get(0));
            tch_uninstaller.Uninstall(netDevices.Get(1));

            // Utilization tracking
            if (m_enable_isl_utilization_tracking) {
                netDevices.Get(0)->GetObject<PointToPointLaserNetDevice>()->EnableUtilizationTracking(m_isl_utilization_tracking_interval_ns);
                netDevices.Get(1)->GetObject<PointToPointLaserNetDevice>()->EnableUtilizationTracking(m_isl_utilization_tracking_interval_ns);

                m_islNetDevices.Add(netDevices.Get(0));
                m_islFromTo.push_back(std::make_pair(sat0_id, sat1_id));
                m_islNetDevices.Add(netDevices.Get(1));
                m_islFromTo.push_back(std::make_pair(sat1_id, sat0_id));
            }

            counter += 1;
        }
        fs.close();

        // Completed
        std::cout << "    >> Created " << std::to_string(counter) << " ISL(s)" << std::endl;

    }
