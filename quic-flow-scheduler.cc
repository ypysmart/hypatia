/* -*- Mode:C++;  -*- */
/*
 * Copyright (c) 2025 icfn
 *
 * This program is free software; you can redistribute it and/or modify
 * it 
 * Author: ypy
 */

#include "quic-flow-scheduler.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/mpquic-bulk-send-helper.h"
#include "ns3/basic-sim-module.h"
#include "ns3/mpquic-bulk-send-application.h"
namespace ns3 {

void QuicFlowScheduler::StartNextFlow(int i) {

    // Fetch the flow to start
    TcpFlowScheduleEntry& entry = m_schedule[i];
    int64_t now_ns = Simulator::Now().GetNanoSeconds();
    NS_ASSERT(now_ns == entry.GetStartTimeNs());

    // Helper to install the source application

    // ========== 新增代码开始 ==========
    std::cout << "--- Debug Info for Node " << entry.GetFromNodeId() << " ---" << std::endl;
    Ptr<Node> fromNode = m_nodes.Get(entry.GetFromNodeId());
    Ptr<Ipv4> ipv4_from = fromNode->GetObject<Ipv4>();
    // 获取该节点上有多少个网络接口
    uint32_t nInterfaces_from = ipv4_from->GetNInterfaces();
    std::cout << "Number of interfaces on node " << entry.GetFromNodeId() << ": " << nInterfaces_from << std::endl;

    // 遍历每个接口
    for (uint32_t i = 0; i < nInterfaces_from; i++) {
        // 获取该接口上有多少个IP地址
        uint32_t nAddresses = ipv4_from->GetNAddresses(i);
        std::cout << "  Interface " << i << " has " << nAddresses << " address(es):" << std::endl;
        // 遍历该接口上的每个地址
        for (uint32_t j = 0; j < nAddresses; j++) {
            Ipv4Address addr = ipv4_from->GetAddress(i, j).GetLocal();
            std::cout << "    Address index " << j << ": " << addr << std::endl;
        }
    }
    std::cout << "---------------------------------试试看" << std::endl;

    std::cout << "--- Debug Info for Node " << entry.GetToNodeId() << " ---" << std::endl;
    Ptr<Node> toNode = m_nodes.Get(entry.GetToNodeId());
    Ptr<Ipv4> ipv4_to = toNode->GetObject<Ipv4>();
    uint32_t nInterfaces_to = ipv4_to->GetNInterfaces();
    std::cout << "Number of interfaces on node " << entry.GetToNodeId() << ": " << nInterfaces_to << std::endl;

    for (uint32_t i = 0; i < nInterfaces_to; i++) {
        uint32_t nAddresses = ipv4_to->GetNAddresses(i);
        std::cout << "  Interface " << i << " has " << nAddresses << " address(es):" << std::endl;
        for (uint32_t j = 0; j < nAddresses; j++) {
            Ipv4Address addr = ipv4_to->GetAddress(i, j).GetLocal();
            std::cout << "    Address index " << j << ": " << addr << std::endl;
        }
    }
    std::cout << "---------------------------------" << std::endl;

//     TcpFlowSendHelper source(
//             "ns3::TcpSocketFactory",
//             //选择哪一个tcpip网络接口地址和ipv4地址，这里配置的是发送端
//             InetSocketAddress(m_nodes.Get(entry.GetToNodeId())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1024),✅
//             entry.GetSizeByte(),
//             entry.GetTcpFlowId(),
//             m_enable_logging_for_tcp_flow_ids.find(entry.GetTcpFlowId()) != m_enable_logging_for_tcp_flow_ids.end(),//这里是关键是从流量csv读取到之后输入tcpip
//             m_basicSimulation->GetLogsDir(),
//             entry.GetAdditionalParameters()
//     );
//    // Install it on the node and start it right now
//     //发送端发包
//     ApplicationContainer app = source.Install(m_nodes.Get(entry.GetFromNodeId()));
    MpquicBulkSendHelper source ("ns3::QuicSocketFactory",
                            InetSocketAddress(m_nodes.Get(entry.GetToNodeId())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1024));
    // Set the amount of data to send in bytes.  Zero is unlimited.
    string myRandomNo = "0";
    uint32_t maxBytes = stoi(myRandomNo);
    source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    // m_factory.Set ("TcpFlowId", UintegerValue (flowId));//流 ID//为解决、作用是什么？是否需要增设QUIC flow id
    // 客户端
    ApplicationContainer sourceApps = source.Install (m_nodes.Get(entry.GetFromNodeId()));
    sourceApps.Start(NanoSeconds(0));
    m_apps.push_back(sourceApps);

  // 新增：Install sink (receiver) on target node
    PacketSinkHelper sink ("ns3::QuicSocketFactory",
                           InetSocketAddress (Ipv4Address::GetAny (), 1024));  // Listen on port 1024
    ApplicationContainer sinkApps = sink.Install (m_nodes.Get(entry.GetToNodeId()));
    sinkApps.Start (NanoSeconds (0));


    // If there is a next flow to start, schedule its start
    if (i + 1 != (int) m_schedule.size()) {
        int64_t next_flow_ns = m_schedule[i + 1].GetStartTimeNs();
        Simulator::Schedule(NanoSeconds(next_flow_ns - now_ns), &QuicFlowScheduler::StartNextFlow, this, i + 1);
    }

}
//调度并运行 TCP 流
QuicFlowScheduler::QuicFlowScheduler(Ptr<BasicSimulation> basicSimulation, Ptr<Topology> topology) {
    printf("ypyQuic FLOW SCHEDULER\n");

    m_basicSimulation = basicSimulation;
    m_topology = topology;

    // // Check if it is enabled explicitly
    m_enabled = parse_boolean(m_basicSimulation->GetConfigParamOrDefault("enable_quic_flow_scheduler", "true"));//这个使能我没找到在哪里开关我怀疑在tcp协议哪个类
    if (!m_enabled) {
        std::cout << "  > Not enabled explicitly, so disabled" << std::endl;

    } else {
        std::cout << "  >quic flow scheduler is enabled" << std::endl;

        // Properties we will use often
        m_nodes = m_topology->GetNodes();// 获取所有节点的 NodeContainer
        m_simulation_end_time_ns = m_basicSimulation->GetSimulationEndTimeNs();// 仿真结束时间(纳秒)
        m_enable_logging_for_quic_flow_ids = parse_set_positive_int64(// 需要详细日志的 流ID集合
                m_basicSimulation->GetConfigParamOrDefault("quic_flow_enable_logging_for_quic_flow_ids", "set()"));
        m_system_id = m_basicSimulation->GetSystemId();
        m_enable_distributed = m_basicSimulation->IsDistributedEnabled();
        m_distributed_node_system_id_assignment = m_basicSimulation->GetDistributedNodeSystemIdAssignment();

        // Read schedule
        std::vector<TcpFlowScheduleEntry> complete_schedule = read_tcp_flow_schedule(
                m_basicSimulation->GetRunDir() + "/" + m_basicSimulation->GetConfigParamOrFail("quic_flow_schedule_filename"),
                m_topology,
                m_simulation_end_time_ns
        );

// Check logging IDs (类似于 TCP)
        for (int64_t quic_flow_id : m_enable_logging_for_quic_flow_ids) {
            if ((size_t) quic_flow_id >= complete_schedule.size()) {
                throw std::invalid_argument("Invalid QUIC flow ID: " + std::to_string(quic_flow_id));
            }
        }

        // Filter the schedule to only have applications starting at nodes which are part of this system
        if (m_enable_distributed) {
            std::vector<TcpFlowScheduleEntry> filtered_schedule;//每个进程只负责仿真其上的节点。
            for (TcpFlowScheduleEntry &entry : complete_schedule) {
                if (m_distributed_node_system_id_assignment[entry.GetFromNodeId()] == m_system_id) {//m_distributed_node_system_id_assignment 是一个向量，其索引是节点ID，值是该节点由哪个系统ID负责。
                    filtered_schedule.push_back(entry);
                }
            }
            m_schedule = filtered_schedule;
        } else {
            m_schedule = complete_schedule;
        }

        // Schedule read
        printf("  > Read schedule (total flow start events: %lu)\n", m_schedule.size());
        m_basicSimulation->RegisterTimestamp("Read flow schedule");

        // Determine filenames (类似于 TCP)
        if (m_enable_distributed) {
            m_flows_csv_filename = m_basicSimulation->GetLogsDir() + "/system_" + std::to_string(m_system_id) + "_quic_flows.csv";
            m_flows_txt_filename = m_basicSimulation->GetLogsDir() + "/system_" + std::to_string(m_system_id) + "_quic_flows.txt";
        } else {
            m_flows_csv_filename = m_basicSimulation->GetLogsDir() + "/quic_flows.csv";
            m_flows_txt_filename = m_basicSimulation->GetLogsDir() + "/quic_flows.txt";
        }

        // Schedule the first flow start
        if (m_schedule.size() > 0) {
            int64_t first_flow_ns = m_schedule[0].GetStartTimeNs();
            Simulator::Schedule(NanoSeconds(first_flow_ns), &QuicFlowScheduler::StartNextFlow, this, 0);
        }

        // Register setup
        m_basicSimulation->RegisterTimestamp("Setup QUIC flows");
    }

    std::cout << std::endl;
}

// void QuicFlowScheduler::WriteResults() {
//     std::cout << "STORE ypyQuic FLOW RESULTS" << std::endl;

//     // Check if it is enabled explicitly
//     if (!m_enabled) {
//         std::cout << "  > Not enabled, so no ypyQuic flow results are written" << std::endl;

//     } else {

//         // Open files
//         std::cout << "  > Opening ypyQuic flow log files:" << std::endl;
//         FILE* file_csv = fopen(m_flows_csv_filename.c_str(), "w+");
//         std::cout << "    >> Opened: " << m_flows_csv_filename << std::endl;
//         FILE* file_txt = fopen(m_flows_txt_filename.c_str(), "w+");
//         std::cout << "    >> Opened: " << m_flows_txt_filename << std::endl;

//         // Header
//         std::cout << "  > Writing ypyQuic_flows.txt header" << std::endl;
//         fprintf(
//                 file_txt, "%-16s%-10s%-10s%-16s%-18s%-18s%-16s%-16s%-13s%-16s%-14s%s\n",
//                 "TCP Flow ID", "Source", "Target", "Size", "Start time (ns)",
//                 "End time (ns)", "Duration", "Sent", "Progress", "Avg. rate", "Finished?", "Metadata"
//         );

//         // Go over the schedule, write each flow's result
//         std::cout << "  > Writing ypyQuic log files line-by-line" << std::endl;
//         std::cout << "  > Total TCP flow log entries to write... " << m_apps.size() << std::endl;
//         uint32_t app_idx = 0;
//         for (TcpFlowScheduleEntry& entry : m_schedule) {

//             // Retrieve application*/**//**/*//*/**/*/*/*/*/*/*/ */ */ */
//             Ptr<MpquicBulkSendApplication> flowSendApp = m_apps.at(app_idx).Get(0)->GetObject<MpquicBulkSendApplication>();

//             // Finalize the detailed logs (if they are enabled)
//             flowSendApp->FinalizeDetailedLogs();

//             // Retrieve statistics
//             bool is_completed = flowSendApp->IsCompleted();
//             bool is_conn_failed = flowSendApp->IsConnFailed();
//             bool is_closed_err = flowSendApp->IsClosedByError();
//             bool is_closed_normal = flowSendApp->IsClosedNormally();
//             int64_t sent_byte = flowSendApp->GetAckedBytes();
//             int64_t fct_ns;
//             if (is_completed) {
//                 fct_ns = flowSendApp->GetCompletionTimeNs() - entry.GetStartTimeNs();
//             } else {
//                 fct_ns = m_simulation_end_time_ns - entry.GetStartTimeNs();
//             }
//             std::string finished_state;
//             if (is_completed) {
//                 finished_state = "YES";
//             } else if (is_conn_failed) {
//                 finished_state = "NO_CONN_FAIL";
//             } else if (is_closed_normal) {
//                 finished_state = "NO_BAD_CLOSE";
//             } else if (is_closed_err) {
//                 finished_state = "NO_ERR_CLOSE";
//             } else {
//                 finished_state = "NO_ONGOING";
//             }

//             // Write plain to the csv
//             fprintf(
//                     file_csv, "%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%s,%s\n",
//                     entry.GetTcpFlowId(), entry.GetFromNodeId(), entry.GetToNodeId(), entry.GetSizeByte(), entry.GetStartTimeNs(),
//                     entry.GetStartTimeNs() + fct_ns, fct_ns, sent_byte, finished_state.c_str(), entry.GetMetadata().c_str()
//             );

//             // Write nicely formatted to the text
//             char str_size_megabit[100];
//             sprintf(str_size_megabit, "%.2f Mbit", byte_to_megabit(entry.GetSizeByte()));
//             char str_duration_ms[100];
//             sprintf(str_duration_ms, "%.2f ms", nanosec_to_millisec(fct_ns));
//             char str_sent_megabit[100];
//             sprintf(str_sent_megabit, "%.2f Mbit", byte_to_megabit(sent_byte));
//             char str_progress_perc[100];
//             sprintf(str_progress_perc, "%.1f%%", ((double) sent_byte) / ((double) entry.GetSizeByte()) * 100.0);
//             char str_avg_rate_megabit_per_s[100];
//             sprintf(str_avg_rate_megabit_per_s, "%.1f Mbit/s", byte_to_megabit(sent_byte) / nanosec_to_sec(fct_ns));
//             //这里用tcp的继续去做完全没影响，entry就是一个记录id数据，application才是关键
//             fprintf(
//                     file_txt, "%-16" PRId64 "%-10" PRId64 "%-10" PRId64 "%-16s%-18" PRId64 "%-18" PRId64 "%-16s%-16s%-13s%-16s%-14s%s\n",
//                     entry.GetTcpFlowId(), entry.GetFromNodeId(), entry.GetToNodeId(), str_size_megabit, entry.GetStartTimeNs(),
//                     entry.GetStartTimeNs() + fct_ns, str_duration_ms, str_sent_megabit, str_progress_perc, str_avg_rate_megabit_per_s,
//                     finished_state.c_str(), entry.GetMetadata().c_str()
//             );

//             // Move on application index
//             app_idx += 1;

//         }

//         // Close files
//         std::cout << "  > Closing TCP flow log files:" << std::endl;
//         fclose(file_csv);
//         std::cout << "    >> Closed: " << m_flows_csv_filename << std::endl;
//         fclose(file_txt);
//         std::cout << "    >> Closed: " << m_flows_txt_filename << std::endl;

//         // Register completion
//         std::cout << "  > TCP flow log files have been written" << std::endl;
//         m_basicSimulation->RegisterTimestamp("Write TCP flow log files");

//     }

//     std::cout << std::endl;
// }

}
