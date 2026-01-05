/* -*- Mode:C++;  -*- */
/*
 * Copyright (c) 2025 icfn
 *
 * This program is free software; you can redistribute it and/or modify
 * it 
 * Author: ypy
 */

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

#include "ns3/basic-simulation.h"
#include "ns3/tcp-flow-scheduler.h"
#include "ns3/udp-burst-scheduler.h"
#include "ns3/pingmesh-scheduler.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/tcp-optimizer.h"
#include "ns3/arbiter-single-forward-helper.h"
#include "ns3/ipv4-arbiter-routing-helper.h"
#include "ns3/gsl-if-bandwidth-helper.h"

#include "ns3/quic-flow-scheduler.h"
using namespace ns3;


// // 吞吐量监控函数
// void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Ptr<OutputStreamWrapper> stream) {
//     std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
//     Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
//     static bool first = true;

//     // 控制台输出部分保持原样（或可移除多余的cout）
//     if (first) {
//         std::cout << "Available flow IDs: ";
//         for (auto it = flowStats.begin(); it != flowStats.end(); ++it) {
//             std::cout << it->first << " ";
//         }
//         std::cout << std::endl;
//         first = false;

//         // 可选：第一次调用时，在文件加标题行
//         *stream->GetStream() << "flow_id\tlast_rx_time\tfirst_rx_time\trx_bytes\trx_packets\tlast_delay_ms\tthroughput_mbps" << std::endl;
//     }

//     for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats) {
//         // 计算时间差，避免除零
//         double time_diff = stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstRxPacket.GetSeconds();
//         double throughput = (time_diff > 0) ? (stats->second.rxBytes * 8.0 / 1024.0 / 1024.0 / time_diff) : 0.0;

//         // 修改为 key=value 格式，每行一个流，用空格分隔键值对（或用\t，如果你想保持tab分隔）
//         *stream->GetStream() << "flow_id=" << stats->first
//                              << " last_rx_time=" << stats->second.timeLastRxPacket.GetSeconds()
//                              << " first_rx_time=" << stats->second.timeFirstRxPacket.GetSeconds()
//                              << " rx_bytes=" << stats->second.rxBytes
//                              << " rx_packets=" << stats->second.rxPackets
//                              << " last_delay_ms=" << stats->second.lastDelay.GetMilliSeconds()
//                              << " throughput_mbps=" << throughput
//                              << std::endl;
//     }
// }
// 吞吐量监控函数
void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Ptr<OutputStreamWrapper> stream)
{
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    static bool first = true;
    if (first) {
        std::cout << "Available flow IDs: ";
        for (auto it = flowStats.begin(); it != flowStats.end(); ++it) {
            std::cout << it->first << " ";
        }
        std::cout << std::endl;
        first = false;
    }
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    {

        if (stats->first == 1 || stats->first == 2||(stats->first >= 3 && stats->first <= 5)||stats->first == 0){
            *stream->GetStream () << stats->first  << "\t" << stats->second.timeLastRxPacket.GetSeconds() << "\t" << stats->second.timeFirstRxPacket.GetSeconds() << "\t" <<stats->second.rxBytes << "\t" << stats->second.rxPackets << "\t" << stats->second.lastDelay.GetMilliSeconds() << "\t" << stats->second.rxBytes*8/1024/1024/(stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstRxPacket.GetSeconds())  << std::endl;
        }
    }
    Simulator::Schedule(Seconds(0.001),&ThroughputMonitor, fmhelper, flowMon, stream);
}


int main(int argc, char *argv[]) {



// //全局启用日志级别
    // LogComponentEnable("QuicL4Protocol", LOG_LEVEL_ALL);  // 启用所有级别日志
    // LogComponentEnable("QuicL4Protocol", LOG_PREFIX_FUNC);  // 添加函数名前缀
    // LogComponentEnable("MpquicBulkSendApplication", LOG_LEVEL_LOGIC);  // 启用INFO/LOGIC级别日志
    // LogComponentEnable("MpquicBulkSendApplication", LOG_LEVEL_DEBUG);  // 如果需要DEBUG日志
    // LogComponentEnable("MpquicBulkSendApplication", LOG_PREFIX_TIME);  // 添加时间前缀
    // LogComponentEnable("MpquicBulkSendApplication", LOG_PREFIX_NODE);  // 添加节点ID前缀

//     // Enable QUIC flow-level logging (for QuicStreamBase class)
// LogComponentEnable("QuicStreamBase", LOG_LEVEL_INFO);  // Basic info logs
// LogComponentEnable("QuicStreamBase", LOG_LEVEL_LOGIC); // More detailed (e.g., function entry/exit)
// LogComponentEnable("QuicStreamBase", LOG_PREFIX_TIME); // Add timestamps to logs
// LogComponentEnable("QuicStreamBase", LOG_PREFIX_NODE); // Add node ID to logs

// // Optionally enable other related components
// LogComponentEnable("QuicSocketBase", LOG_LEVEL_INFO);  // For socket-level (e.g., multi-path)
// LogComponentEnable("MpQuicScheduler", LOG_LEVEL_LOGIC); // For scheduler details
// LogComponentEnable("QuicL4Protocol", LOG_LEVEL_ALL);   // Already in your code, but confirm
// LogComponentEnable("QuicSocketBase", LOG_LEVEL_INFO);  // 基本信息日志
// LogComponentDisable("QuicSocketBase", LOG_LEVEL_ALL);
// LogComponentDisable("QuicL4Protocol", LOG_LEVEL_ALL);
// LogComponentDisable("MpQuicScheduler", LOG_LEVEL_ALL);
// LogComponentDisable("QuicStreamBase", LOG_LEVEL_ALL);
// LogComponentDisable("FlowMonitor", LOG_LEVEL_ALL);
// LogComponentDisable("QuicStreamBase", LOG_LEVEL_ALL);

// int  tcp_choose=0;
    // No buffering of printf
    setbuf(stdout, nullptr);
// LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);  // FlowMonitor 日志
    int schedulerType = MpQuicScheduler::ROUND_ROBIN;//多路径调度器（Scheduler）
    // double rate0 = 10.0;// 链路带宽下限 (Mbps)
    // double rate1 = 10.0;// 链路带宽上限 (Mbps)
    // double delay0 = 50.0;// 链路延迟下限 (ms)
    // double delay1 = 50.0;// 链路延迟上限 (ms)
    // string myRandomNo = "500000";// 要传输的数据总量 (Bytes)
    string lossrate = "0.0000";// 链路丢包率
    //BLEST (BLocking ESTimation) 调度算法的参数。旨在减少队头阻塞 (Head-of-Line Blocking) 的调度算法
    int bVar = 2;
    int bLambda = 100;
    //MAB (Multi-Armed Bandit) 调度算法，MAB 算法将路径选择建模为一个"多臂老虎机"问题，
    int mrate = 52428800;
    int ccType = QuicSocketBase::OLIA;//拥塞控制算法
    int mselect = 3;
    // int seed = 1;//随机数种子
    TypeId ccTypeId = MpQuicCongestionOps::GetTypeId ();//拥塞控制算法

    // Retrieve run directory
    CommandLine cmd;
    std::string run_dir = "";
    cmd.Usage("Usage: ./waf --run=\"main_satnet --run_dir='<path/to/run/directory>'\"");
    cmd.AddValue("run_dir",  "Run directory", run_dir);
    cmd.Parse(argc, argv);
    if (run_dir.compare("") == 0) {
        printf("Usage: ./waf --run=\"main_satnet --run_dir='<path/to/run/directory>'\"");
        return 0;
    }

    // Load basic simulation environment
    Ptr<BasicSimulation> basicSimulation = CreateObject<BasicSimulation>(run_dir);

    // // Setting socket type
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + basicSimulation->GetConfigParamOrFail("tcp_socket_type")));
    // 配置QUIC使用指定拥塞控制
    // 根据命令行参数选择拥塞控制算法
    // 拥塞控制算法选择
    if (ccType == QuicSocketBase::OLIA){
        ccTypeId = MpQuicCongestionOps::GetTypeId ();
            std::cout << "****************************当前设置拥塞控制算法是是OLIA***********************************" << std::endl;

    }
    if(ccType == QuicSocketBase::QuicNewReno){
        ccTypeId = QuicCongestionOps::GetTypeId ();
                    std::cout << "****************************当前设置拥塞控制算法是是QuicNewReno***********************************" << std::endl;
    }
        int BufSize = 50000000;

    Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize",UintegerValue   (BufSize));
    Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize",UintegerValue  (BufSize));
    Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize",UintegerValue    (BufSize));
    Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize",UintegerValue   (BufSize));
    std::cout << "当前设置的BufSize是" << (BufSize/1000000 )<<"MB"<< std::endl;

    // 多路径QUIC启用
    Config::SetDefault ("ns3::QuicSocketBase::EnableMultipath",BooleanValue(true));//可以考虑先不启用
    Config::SetDefault ("ns3::QuicSocketBase::CcType",IntegerValue(ccType));
    // 配置QUIC使用指定拥塞控制
    Config::SetDefault ("ns3::QuicL4Protocol::SocketType",TypeIdValue (ccTypeId));
    //多路径调度器（Scheduler）
    Config::SetDefault ("ns3::MpQuicScheduler::SchedulerType", IntegerValue(schedulerType));   
    Config::SetDefault ("ns3::MpQuicScheduler::BlestVar", UintegerValue(bVar));   
    Config::SetDefault ("ns3::MpQuicScheduler::BlestLambda", UintegerValue(bLambda));   
    Config::SetDefault ("ns3::MpQuicScheduler::MabRate", UintegerValue(mrate)); 
    Config::SetDefault ("ns3::MpQuicScheduler::Select", UintegerValue(mselect)); //又是一个拥塞算法选择配置？
     
//     int ccType = basicSimulation->GetConfigParamOrFail("quic_cc_type");
// int schedulerType = basicSimulation->GetConfigParamOrFail("quic_scheduler_type");
// 如果尚未获取，则获取其他键，例如：
std::string loggingIds = basicSimulation->GetConfigParamOrFail("quic_flow_enable_logging_for_quic_flow_ids");
std::string scheduleFile = basicSimulation->GetConfigParamOrFail("quic_flow_schedule_filename");
    
    // Optimize TCP

     TcpOptimizer::OptimizeBasic(basicSimulation);

    

    // Read topology, and install routing arbiters
    Ptr<TopologySatelliteNetwork> topology = CreateObject<TopologySatelliteNetwork>(basicSimulation, Ipv4ArbiterRoutingHelper());
    ArbiterSingleForwardHelper arbiterHelper(basicSimulation, topology->GetNodes());
    GslIfBandwidthHelper gslIfBandwidthHelper(basicSimulation, topology->GetNodes());

    // Schedule flows
    // TcpFlowScheduler tcpFlowScheduler(basicSimulation, topology); // Requires enable_tcp_flow_scheduler=true
    QuicFlowScheduler quicFlowScheduler(basicSimulation, topology);
    // Schedule UDP bursts
    //UdpBurstScheduler udpBurstScheduler(basicSimulation, topology); // Requires enable_udp_burst_scheduler=true

    // Schedule pings
    // PingmeshScheduler pingmeshScheduler(basicSimulation, topology); // Requires enable_pingmesh_scheduler=true
    // AsciiTraceHelper asciiTraceHelper;
    // std::ostringstream fileName;
    // Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (fileName.str ());
  

    // FlowMonitorHelper flowmon;
    // Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    // ThroughputMonitor(&flowmon, monitor, stream); 
    // Run schedulerType 
    basicSimulation->Run();
  //  ThroughputMonitor(&flowmon, monitor, stream); 

    // Write flow results
    // quicFlowScheduler.WriteResults();

    // Write UDP burst results
    //udpBurstScheduler.WriteResults();

    // Write pingmesh results
    //pingmeshScheduler.WriteResults();

    // Collect utilization statistics
    topology->CollectUtilizationStatistics();

    // Finalize the simulation
    basicSimulation->Finalize();

    return 0;

}
