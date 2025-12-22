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

#ifndef ARBITER_SINGLE_FORWARD_H
#define ARBITER_SINGLE_FORWARD_H

#include <tuple>
#include "ns3/arbiter-satnet.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/hash.h"
#include "ns3/abort.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"

namespace ns3 {

class ArbiterSingleForward : public ArbiterSatnet
{
public:
    static TypeId GetTypeId (void);

    // Constructor for single forward next-hop forwarding state
    ArbiterSingleForward(
            Ptr<Node> this_node,
            NodeContainer nodes,
            std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_list
    );

    // Single forward next-hop implementation
    // 修改后的决策函数（增加 path_id 参数，默认 0）
        std::tuple<int32_t, int32_t, int32_t> TopologySatelliteNetworkDecide(
            int32_t source_node_id,
            int32_t target_node_id,
            Ptr<const Packet> pkt,
            Ipv4Header const &ipHeader,
            bool is_request_for_source_ip_so_no_next_header,
            int32_t path_id = 0
        );  // 新增默认参数

    // Updating of forward state
    void SetSingleForwardState(int32_t target_node_id, int32_t next_node_id, int32_t own_if_id, int32_t next_if_id);

    // Static routing table
    std::string StringReprOfForwardingState();
    
    // 新增设置多路径状态的函数（仅地面站调用）
    void SetMultiPathForwardState(int32_t target_node_id,
                                  int32_t path_id,
                                  int32_t next_node_id,
                                  int32_t own_if_id,
                                  int32_t next_if_id);
private:
    std::vector<std::tuple<int32_t, int32_t, int32_t>> m_next_hop_list;
    std::map<std::pair<int32_t, int32_t>, std::tuple<int32_t, int32_t, int32_t>> m_multi_path_hop;
    // 判断当前节点是否是地面站（根据你的拓扑，这里写死 1156 和 1157，可改为配置文件或范围判断）
    bool IsGroundStation() const {
        return m_node_id == 1156 || m_node_id == 1157;
    }
};

}

#endif //ARBITER_SINGLE_FORWARD_H
