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

#include "ns3/arbiter-satnet.h"
#include "ns3/quic-header.h"
namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("ArbiterSatnet");
NS_OBJECT_ENSURE_REGISTERED (ArbiterSatnet);
TypeId ArbiterSatnet::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::ArbiterSatnet")
            .SetParent<Arbiter> ()
            .SetGroupName("BasicSim")
    ;
    return tid;
}

ArbiterSatnet::ArbiterSatnet(
        Ptr<Node> this_node,
        NodeContainer nodes
) : Arbiter(this_node, nodes) {
    // Intentionally left empty
}

ArbiterResult ArbiterSatnet::Decide(
        int32_t source_node_id,
        int32_t target_node_id,
        ns3::Ptr<const ns3::Packet> pkt,
        ns3::Ipv4Header const &ipHeader,
        bool is_socket_request_for_source_ip
) {
    int32_t path_id = 0;  // 默认 path_id = 0

    // 关键：根据目的 IP 地址确定 path_id
    Ipv4Address dest_ip = ipHeader.GetDestination();

    // 你需要维护一个映射：IP -> path_id（或直接 -> 接口索引）
    // 推荐方式：在 ArbiterSingleForward 构造时传入或读取所有本地接口 IP

    // 示例映射（你可以做成成员变量 m_ip_to_pathid）
    // 假设地面站节点 1156 和 1157 有多个 GSL 接口
    if (m_node_id == 1156 || m_node_id == 1157) {
        if (dest_ip == Ipv4Address("10.36.32.1")) path_id = 0;
        else if (dest_ip == Ipv4Address("10.36.33.1")) path_id = 1;
        else if (dest_ip == Ipv4Address("10.36.34.1")) path_id = 2;
        else if (dest_ip == Ipv4Address("10.36.35.1")) path_id = 0;
        else if (dest_ip == Ipv4Address("10.36.36.1")) path_id = 1;
        else if (dest_ip == Ipv4Address("10.36.37.1")) path_id = 2;
        // ... 继续添加其他接口
        else {
            // 未知 IP，走默认 path 0（安全 fallback）
            // std::cout << "Node " << m_node_id << " unknown dest IP " << dest_ip
            //           << ", using default path_id=0未知 IP，走默认 path 0" << std::endl;
            path_id = 0;
        }

        // std::cout << "Node " << m_node_id << " routing to dest IP " << dest_ip
        //           << " -> using path_id = " << path_id << std::endl;
    }
    // 卫星节点仍然用单路径（path_id 无意义，或固定为 0）
    // 直接用 path_id = 0 即可

    // 调用决策（path_id 只对地面站有效）
    auto next = TopologySatelliteNetworkDecide(
        source_node_id, target_node_id, pkt, ipHeader,
        is_socket_request_for_source_ip, path_id
    );

    int32_t next_node_id = std::get<0>(next);
    int32_t own_if_id    = std::get<1>(next);
    int32_t next_if_id   = std::get<2>(next);

    NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2,
                    "Forwarding state invalid (-2)");

    if (next_node_id != -1) {
        uint32_t gateway = m_nodes.Get(next_node_id)->GetObject<Ipv4>()
                           ->GetAddress(next_if_id, 0).GetLocal().Get();
        return ArbiterResult(false, own_if_id, gateway);
    } else {
        return ArbiterResult(true, 0, 0);
    }
}
}

