/* -*- Mode:C++;  -*- */
/*
 * Copyright (c) 2025 icfn
 *
 * This program is free software; you can redistribute it and/or modify
 * it 
 * Author: ypy
 */
#ifndef QUIC_FLOW_SCHEDULER_H
#define QUIC_FLOW_SCHEDULER_H

#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>

//可能重复包含

#include "ns3/command-line.h"

#include "ns3/basic-simulation.h"
#include "ns3/exp-util.h"
#include "ns3/topology.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/quic-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include "ns3/tcp-flow-schedule-reader.h"
#include "ns3/tcp-flow-send-helper.h"
#include "ns3/tcp-flow-send-application.h"
#include "ns3/tcp-flow-sink-helper.h"
#include "ns3/tcp-flow-sink.h"
namespace ns3 {

class QuicFlowScheduler
{

public:
    QuicFlowScheduler(Ptr<BasicSimulation> basicSimulation, Ptr<Topology> topology);
    //void WriteResults();

protected:
    void StartNextFlow(int i);
    Ptr<BasicSimulation> m_basicSimulation;
    int64_t m_simulation_end_time_ns;
    Ptr<Topology> m_topology = nullptr;
    bool m_enabled;

    std::vector<TcpFlowScheduleEntry> m_schedule;
    NodeContainer m_nodes;
    std::vector<ApplicationContainer> m_apps;
    std::set<int64_t> m_enable_logging_for_quic_flow_ids;
    uint32_t m_system_id;
    bool m_enable_distributed;
    std::vector<int64_t> m_distributed_node_system_id_assignment;
    std::string m_flows_csv_filename;
    std::string m_flows_txt_filename;

};

}

#endif /* QUIC_FLOW_SCHEDULER_H */
