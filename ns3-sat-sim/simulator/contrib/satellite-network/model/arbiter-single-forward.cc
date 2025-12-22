/*
 * Copyright (c) 2020 ETH Zurich
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
 * Author: Simon               2020
 */

#include "arbiter-single-forward.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ArbiterSingleForward);
TypeId ArbiterSingleForward::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::ArbiterSingleForward")
            .SetParent<ArbiterSatnet> ()
            .SetGroupName("BasicSim")
    ;
    return tid;
}

ArbiterSingleForward::ArbiterSingleForward(
        Ptr<Node> this_node,
        NodeContainer nodes,
        std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_list
) : ArbiterSatnet(this_node, nodes)
{
    m_next_hop_list = next_hop_list;
}

std::tuple<int32_t, int32_t, int32_t> ArbiterSingleForward::TopologySatelliteNetworkDecide(
        int32_t source_node_id,
        int32_t target_node_id,
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header,
        int32_t path_id)
{
    // 所有节点都优先查多路径表（无论是否地面站）
    auto key = std::make_pair(target_node_id, path_id);
    auto it = m_multi_path_hop.find(key);
    if (it != m_multi_path_hop.end()) {
        // int next = std::get<0>(it->second);
        // int own  = std::get<1>(it->second);
        // int next_if = std::get<2>(it->second);
        // std::cout << "Node " << m_node_id << " HIT multi-path entry: target=" << target_node_id
        //           << " path_id=" << path_id << " -> next_hop=" << next
        //           << " own_if=" << own << " next_if=" << next_if << std::endl;
        return it->second;
    }

    // fallback 到 path_id=0
    key = std::make_pair(target_node_id, 0);
    it = m_multi_path_hop.find(key);
    if (it != m_multi_path_hop.end()) {
        // int next = std::get<0>(it->second);
        // int own  = std::get<1>(it->second);
        // int next_if = std::get<2>(it->second);
        // std::cout << "Node " << m_node_id << " FALLBACK to path_id=0 for target=" << target_node_id
        //           << " -> next_hop=" << next << " own_if=" << own << " next_if=" << next_if << std::endl;
        return it->second;
    }

    // 再 fallback 到旧单路径表（兼容旧路由文件）
    if (target_node_id < (int32_t)m_next_hop_list.size()) {
        auto result = m_next_hop_list[target_node_id];
        // int next, own, next_if;
        // std::tie(next, own, next_if) = result;

        // if (next != -2 && next != -1) {
        //     std::cout << "Node " << m_node_id << " using LEGACY single-path table for target=" << target_node_id
        //               << " -> next_hop=" << next << " own_if=" << own << " next_if=" << next_if << std::endl;
        // } else if (next == -1) {
        //     std::cout << "Node " << m_node_id << " explicit DROP for target=" << target_node_id << std::endl;
        // } else {
        //     std::cout << "Node " << m_node_id << " no legacy route for target=" << target_node_id << " -> DROP" << std::endl;
        // }
        return result;
    }

    // 无路由
    return std::make_tuple(-1, -1, -1);
}

// SetSingleForwardState 保持不变（卫星节点仍会调用它）

void ArbiterSingleForward::SetSingleForwardState(int32_t target_node_id, int32_t next_node_id, int32_t own_if_id, int32_t next_if_id) {
    NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2, "Not permitted to set invalid (-2).");
    m_next_hop_list[target_node_id] = std::make_tuple(next_node_id, own_if_id, next_if_id);
}

std::string ArbiterSingleForward::StringReprOfForwardingState() {
    std::ostringstream res;
    res << "Single-forward state of node " << m_node_id << std::endl;
    for (size_t i = 0; i < m_nodes.GetN(); i++) {
        res << "  -> " << i << ": (" << std::get<0>(m_next_hop_list[i]) << ", "
            << std::get<1>(m_next_hop_list[i]) << ", "
            << std::get<2>(m_next_hop_list[i]) << ")" << std::endl;
    }
    return res.str();
}



void ArbiterSingleForward::SetMultiPathForwardState(int32_t target_node_id,
                                                    int32_t path_id,
                                                    int32_t next_node_id,
                                                    int32_t own_if_id,
                                                    int32_t next_if_id) {
    NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2,
                    "Not permitted to set invalid (-2).");

        auto key = std::make_pair(target_node_id, path_id);
    m_multi_path_hop[key] = std::make_tuple(next_node_id, own_if_id, next_if_id);

    // 同时写入单路径表 path_id=0，方便 fallback 和兼容
    if (path_id == 0) {
        m_next_hop_list[target_node_id] = std::make_tuple(next_node_id, own_if_id, next_if_id);
    }

    // // === 新增：调试输出，每次设置一条多路径就打印当前完整的多路径表 ===
    // std::cout << "=== Multi-path forwarding state UPDATED on node " << m_node_id 
    //           << " (Ground station: " << (IsGroundStation() ? "yes" : "no") << ") ===" << std::endl;

    // std::cout << "  Just set: target=" << target_node_id 
    //           << " path_id=" << path_id 
    //           << " -> next_hop=" << next_node_id 
    //           << " own_if=" << own_if_id 
    //           << " next_if=" << next_if_id << std::endl;

    // if (m_multi_path_hop.empty()) {
    //     std::cout << "  Multi-path table is currently EMPTY." << std::endl;
    // } else {
    //     std::cout << "  Current full multi-path table:" << std::endl;
    //     for (const auto& entry : m_multi_path_hop) {
    //         int32_t tgt = entry.first.first;
    //         int32_t pid = entry.first.second;

    //         // 替换结构化绑定
    //         int next, own, nextif;
    //         std::tie(next, own, nextif) = entry.second;

    //         std::cout << "    -> target " << tgt << " (path_id=" << pid << "): "
    //                   << "next_hop=" << next << ", own_if=" << own << ", next_if=" << nextif << std::endl;
    //     }
    // }
    // std::cout << "================================================================" << std::endl;
}

}
