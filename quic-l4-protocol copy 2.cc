/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 SIGNET Lab, Department of Information Engineering, University of Padova
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
 * Authors: Alvise De Biasio <alvise.debiasio@gmail.com>
 *          Federico Chiariotti <chiariotti.federico@gmail.com>
 *          Michele Polese <michele.polese@gmail.com>
 *          Davide Marcato <davidemarcato@outlook.com>
 *          Wenjun Yang <wenjunyang@uvic.ca>
 *          Shengjie Shu <shengjies@uvic.ca>
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/object-vector.h"
#include "ns3/pointer.h"

#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv6-route.h"

#include "quic-l4-protocol.h"
#include "quic-header.h"
#include "ns3/ipv4-end-point-demux.h"
#include "ns3/ipv6-end-point-demux.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/ipv6-end-point.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/ipv6-routing-protocol.h"
#include "quic-socket-factory.h"
#include "ns3/tcp-congestion-ops.h"
#include "quic-congestion-ops.h"
#include "ns3/rtt-estimator.h"
#include "ns3/random-variable-stream.h"

#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <math.h>
#include <iostream>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (QuicL4Protocol);
NS_OBJECT_ENSURE_REGISTERED (QuicUdpBinding);
NS_LOG_COMPONENT_DEFINE ("QuicL4Protocol");

QuicUdpBinding::QuicUdpBinding ()
  : m_budpSocket (0),
  m_budpSocket6 (0),
  m_quicSocket (nullptr),
  m_listenerBinding (false),
  m_pathId(0)
{
  NS_LOG_FUNCTION (this);
}

QuicUdpBinding::~QuicUdpBinding ()
{
  NS_LOG_FUNCTION (this);
  m_budpSocket = 0;
  m_budpSocket6 = 0;
  m_quicSocket = nullptr;
  m_listenerBinding = false;
  m_pathId = 0;
}

TypeId
QuicUdpBinding::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QuicUdpBinding")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddConstructor<QuicUdpBinding> ()
    .AddAttribute ("QuicSocketBase", "The QuicSocketBase pointer.",
                   PointerValue (),
                   MakePointerAccessor (&QuicUdpBinding::m_quicSocket),
                   MakePointerChecker<QuicSocketBase> ())
  ;
  //NS_LOG_UNCOND("QuicUdpBinding");
  return tid;
}

TypeId
QuicUdpBinding::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}



#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_node) { std::clog << " [node " << m_node->GetId () << "] "; }

/* see http://www.iana.org/assignments/protocol-numbers */
const uint8_t QuicL4Protocol::PROT_NUMBER = 143;

TypeId
QuicL4Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QuicL4Protocol")
    .SetParent<IpL4Protocol> ()
    .SetGroupName ("Internet")
    .AddConstructor<QuicL4Protocol> ()
    .AddAttribute ("RttEstimatorType",
                   "Type of RttEstimator objects.",
                   TypeIdValue (RttMeanDeviation::GetTypeId ()),
                   MakeTypeIdAccessor (&QuicL4Protocol::m_rttTypeId),
                   MakeTypeIdChecker ())
    .AddAttribute ("0RTT-Handshake", "0RTT-Handshake start",
                   BooleanValue (false),
                   MakeBooleanAccessor (&QuicL4Protocol::m_0RTTHandshakeStart),
                   MakeBooleanChecker ())
    .AddAttribute ("SocketType",
                   "Socket type of QUIC objects.",
                   TypeIdValue (QuicCongestionOps::GetTypeId ()),
                   MakeTypeIdAccessor (&QuicL4Protocol::m_congestionTypeId),
                   MakeTypeIdChecker ())
    .AddAttribute ("SocketList", "The list of UDP and QUIC sockets associated to this protocol.",
                   ObjectVectorValue (),
                   MakeObjectVectorAccessor (&QuicL4Protocol::m_quicUdpBindingList),
                   MakeObjectVectorChecker<QuicUdpBinding> ())
    /*.AddAttribute ("AuthAddresses", "The list of Authenticated addresses associated to this protocol.",
                                           ObjectVectorValue (),
                                           MakeObjectVectorAccessor (&QuicL4Protocol::m_authAddresses),
                                           MakeObjectVectorChecker<Address> ())*/
  ;
  return tid;
}

QuicL4Protocol::QuicL4Protocol ()
  : m_node (0),
  m_0RTTHandshakeStart (false),
  m_isServer (false),
  m_endPoints (new Ipv4EndPointDemux ()),
  m_endPoints6 (new Ipv6EndPointDemux ())
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC ("Created QuicL4Protocol object " << this);

  m_quicUdpBindingList = QuicUdpBindingList ();

  // 新增：初始化吞吐量统计变量
  // m_totalSentBytes = 0;
  m_initialized=false;
  // m_totalReceivedBytes = 0;
  m_lastThroughputTime = Time(0);
  m_throughputInterval = 5;  // 每1秒计算一次
  sum_totalSentBytes=0;       // 总发送字节数
  sum_totalReceivedBytes=0;   // 总接收字节数
  
}

QuicL4Protocol::~QuicL4Protocol ()
{
  NS_LOG_FUNCTION (this);
  m_quicUdpBindingList.clear ();
}

void
QuicL4Protocol::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_FUNCTION (this << node);

  m_node = node;
}

int
QuicL4Protocol::UdpBind (Ptr<QuicSocketBase> socket)
{
  NS_LOG_FUNCTION (this << socket);
// std::cout<<"-------------QuicL4Protocol::UdpBind"<<std::endl;
  int res = -1;
  QuicUdpBindingList::iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == socket and item->m_budpSocket == nullptr)
        {
          Ptr<Socket> udpSocket = CreateUdpSocket ();
          res = udpSocket->Bind ();
          item->m_budpSocket = udpSocket;
          break;
        }
    }

  return res;
}

int
QuicL4Protocol::UdpBind6 (Ptr<QuicSocketBase> socket)
{
  NS_LOG_FUNCTION (this << socket);

  int res = -1;
  QuicUdpBindingList::iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == socket and item->m_budpSocket6 == nullptr)
        {
          Ptr<Socket> udpSocket6 = CreateUdpSocket6 ();
          res = udpSocket6->Bind ();
          item->m_budpSocket6 = udpSocket6;
          break;
        }
    }

  return res;
}

int
QuicL4Protocol::UdpBind (const Address &address, Ptr<QuicSocketBase> socket)
{
  NS_LOG_FUNCTION (this << address << socket);

  int res = -1;
  if (InetSocketAddress::IsMatchingType (address))
    {
      QuicUdpBindingList::iterator it;
      for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          Ptr<QuicUdpBinding> item = *it;
          if (item->m_quicSocket == socket and  item->m_budpSocket == nullptr)
            {
              Ptr<Socket> udpSocket = CreateUdpSocket ();
              res = udpSocket->Bind (address);
              item->m_budpSocket = udpSocket;
              break;
            }
        }

      return res;
    }
  else if (Inet6SocketAddress::IsMatchingType (address))
    {
      QuicUdpBindingList::iterator it;
      for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          Ptr<QuicUdpBinding> item = *it;
          if (item->m_quicSocket == socket and item->m_budpSocket6 == nullptr)
            {
              Ptr<Socket> udpSocket6 = CreateUdpSocket ();
              res = udpSocket6->Bind (address);
              item->m_budpSocket6 = udpSocket6;
              break;
            }
        }

      return res;
    }
  return -1;
}

int
QuicL4Protocol::UdpConnect (const Address & address, Ptr<QuicSocketBase> socket)
{
  NS_LOG_FUNCTION (this << address << socket);
  if (InetSocketAddress::IsMatchingType (address) == true)
    {
      UdpBind (address, socket);

      QuicUdpBindingList::iterator it;
      for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          Ptr<QuicUdpBinding> item = *it;
          if (item->m_quicSocket == socket)
            {
              return item->m_budpSocket->Connect (address);
            }
        }

      NS_LOG_INFO ("UDP Socket: Connecting");

    }
  else if (Inet6SocketAddress::IsMatchingType (address) == true)
    {
      UdpBind (address, socket);

      QuicUdpBindingList::iterator it;
      for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          Ptr<QuicUdpBinding> item = *it;
          if (item->m_quicSocket == socket)
            {
              return item->m_budpSocket6->Connect (address);
            }
        }
      NS_LOG_INFO ("UDP Socket: Connecting");

    }
  NS_LOG_WARN ("UDP Connection Failed");
  return -1;
}

int
QuicL4Protocol::UdpSend (Ptr<Socket> udpSocket, Ptr<Packet> p, uint32_t flags) const
{
  NS_LOG_FUNCTION (this << udpSocket);

  return udpSocket->Send (p, flags);
}

Ptr<Packet>
QuicL4Protocol::UdpRecv (Ptr<Socket> udpSocket, uint32_t maxSize, uint32_t flags, Address &address)
{
  NS_LOG_FUNCTION (this);

  return udpSocket->RecvFrom (maxSize, flags, address);
}

uint32_t
QuicL4Protocol::GetTxAvailable (Ptr<QuicSocketBase> quicSocket) const
{
  NS_LOG_FUNCTION (this);
// std::cout<<"####QuicL4Protocol::GetTxAvailable()#####"<<std::endl;
  QuicUdpBindingList::const_iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == quicSocket)
        {
          return item->m_budpSocket->GetTxAvailable ();
        }
    }
  return 0;
}

uint32_t
QuicL4Protocol::GetRxAvailable (Ptr<QuicSocketBase> quicSocket) const
{
  NS_LOG_FUNCTION (this);

  QuicUdpBindingList::const_iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == quicSocket)
        {
          return item->m_budpSocket->GetRxAvailable ();
        }
    }
  return 0;
}

int
QuicL4Protocol::GetSockName (const ns3::QuicSocketBase* quicSocket, Address &address) const
{
  NS_LOG_FUNCTION (this);
  QuicUdpBindingList::const_iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == quicSocket)
        {
          return item->m_budpSocket->GetSockName (address);
        }
    }

  return -1;
}

int
QuicL4Protocol::GetPeerName (const ns3::QuicSocketBase* quicSocket, Address &address) const
{
  NS_LOG_FUNCTION (this);

  QuicUdpBindingList::const_iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == quicSocket)
        {
          return item->m_budpSocket->GetPeerName (address);
        }
    }

  return -1;
}

void
QuicL4Protocol::BindToNetDevice (Ptr<QuicSocketBase> quicSocket, Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (this);
  QuicUdpBindingList::iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == quicSocket)
        {
          item->m_budpSocket->BindToNetDevice (netdevice);
        }
    }
}

bool
QuicL4Protocol::SetListener (Ptr<QuicSocketBase> sock)
{
  NS_LOG_FUNCTION (this);

  if (sock != nullptr and m_quicUdpBindingList.size () == 1)
    {
      m_isServer = true;
      m_quicUdpBindingList.front ()->m_quicSocket = sock;
      m_quicUdpBindingList.front ()->m_listenerBinding = true;
      return true;
    }

  return false;
}

bool
QuicL4Protocol::IsServer (void)  const
{
  return m_isServer;
}

const std::vector<Address>&
QuicL4Protocol::GetAuthAddresses () const
{
  return m_authAddresses;
}

void
QuicL4Protocol::ForwardUp (Ptr<Socket> sock)
{
  NS_LOG_FUNCTION (this);
  Address from;
  Ptr<Packet> packet;

  while ((packet = sock->RecvFrom (from)))
    {
      NS_LOG_INFO ("Receiving packet on UDP socket");

      QuicHeader header;
      packet->RemoveHeader (header);
      NS_LOG_INFO(" Recv pkt " << header.GetPacketNumber () 
                <<" pathId: "<<header.GetPathId());
        //闫沛阳输出日志
      m_totalReceivedBytes += packet->GetSize();
      // uint8_t pathIdypy = header.GetPathId();  // 假设返回 uint8_t
    // std::cout << "Debug: raw pathId value = " << static_cast<int>(pathIdypy) << "\n";
      //  std::cout
      //         <<"         接收 "<<this <<"Recv pkt#"<< header.GetPacketNumber () 
      //         <<"         pathId: "<<(int)uint8_t(header.GetPathId())  
      //         <<"         data size " << packet->GetSize () 
      //         <<"        接收总数据量 " << m_totalReceivedBytes
      //         <<"        时间: " << Simulator::Now().GetNanoSeconds() 
      //         <<"\n";
      uint64_t connectionId;
      if (header.HasConnectionId ())
        {
          connectionId = header.GetConnectionId ();
        }
      /*else if (m_sockets.size () <= 2) // Rivedere
        {
          if (m_sockets[0]->GetSocketState () != QuicSocket::LISTENING)
            {
              connectionId = m_sockets[0]->GetConnectionId ();
            }
          else if (m_sockets.size () == 2 && m_sockets[1]->GetSocketState () != QuicSocket::LISTENING)
            {
              connectionId = m_sockets[1]->GetConnectionId ();
            }
          else
            {
              NS_FATAL_ERROR ("The Connection ID can only be omitted by means of m_omit_connection_id transport parameter"
                              " if source and destination IP address and port are sufficient to identify a connection");
            }

        }*/
      else
        {
          NS_FATAL_ERROR ("The Connection ID can only be omitted by means of m_omit_connection_id transport parameter"
                          " if source and destination IP address and port are sufficient to identify a connection");
        }

      QuicUdpBindingList::iterator it;
      Ptr<QuicSocketBase> socket;
      for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          Ptr<QuicUdpBinding> item = *it;
          if (item->m_quicSocket->GetConnectionId () == connectionId)
            {
              socket = item->m_quicSocket;
              break;
            }
        }

      NS_LOG_LOGIC ((socket == nullptr));
      /*NS_LOG_INFO ("Initial " << header.IsInitial ());
      NS_LOG_INFO ("Handshake " << header.IsHandshake ());
      NS_LOG_INFO ("Short " << header.IsShort ());
      NS_LOG_INFO ("Version Negotiation " << header.IsVersionNegotiation ());
      NS_LOG_INFO ("Retry " << header.IsRetry ());
      NS_LOG_INFO ("0Rtt " << header.IsORTT ());*/

      if (header.IsInitial () and m_isServer and socket == nullptr)
        {
          NS_LOG_LOGIC (this << " Cloning listening socket " << m_quicUdpBindingList.front ()->m_quicSocket);
          socket = CloneSocket (m_quicUdpBindingList.front ()->m_quicSocket);
          socket->SetConnectionId (connectionId);
          socket->Connect (from);
          socket->SetupCallback ();

        }
      else if (header.IsHandshake () and m_isServer and socket != nullptr)
        {
          NS_LOG_LOGIC ("CONNECTION AUTHENTICATED - Server authenticated Client " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                        InetSocketAddress::ConvertFrom (from).GetPort () << "");
          m_authAddresses.push_back (InetSocketAddress::ConvertFrom (from).GetIpv4 ()); //add to the list of authenticated sockets
        }
      else if (header.IsHandshake () and !m_isServer and socket != nullptr)
        {
          NS_LOG_LOGIC ("CONNECTION AUTHENTICATED - Client authenticated Server " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                        InetSocketAddress::ConvertFrom (from).GetPort () << "");
          m_authAddresses.push_back (InetSocketAddress::ConvertFrom (from).GetIpv4 ()); //add to the list of authenticated sockets
        }
      else if (header.IsORTT () and m_isServer)
        {
          auto result = std::find (m_authAddresses.begin (), m_authAddresses.end (), InetSocketAddress::ConvertFrom (from).GetIpv4 ());
          // check if a 0-RTT is allowed with this endpoint - or if the attribute m_0RTTHandshakeStart has been forced to be true
          if (result == m_authAddresses.end () && m_0RTTHandshakeStart)
            {
              m_authAddresses.push_back (InetSocketAddress::ConvertFrom (from).GetIpv4 ()); //add to the list of authenticated sockets
            }
          else if (result == m_authAddresses.end () && !m_0RTTHandshakeStart)
            {
              NS_LOG_WARN ( this << " CONNECTION ABORTED: 0RTT Packet from unauthenticated address " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                            InetSocketAddress::ConvertFrom (from).GetPort ());
              continue;
            }

          NS_LOG_LOGIC ("CONNECTION AUTHENTICATED - Server authenticated Client " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                        InetSocketAddress::ConvertFrom (from).GetPort () << "");
          NS_LOG_LOGIC ( this << " Cloning listening socket " << m_quicUdpBindingList.front ()->m_quicSocket);
          socket = CloneSocket (m_quicUdpBindingList.front ()->m_quicSocket);
          socket->SetConnectionId (connectionId);
          socket->Connect (from);
          socket->SetupCallback ();

        }
      else if (header.IsShort ())
        {
          auto result = std::find (m_authAddresses.begin (), m_authAddresses.end (), InetSocketAddress::ConvertFrom (from).GetIpv4 ());

          if (result == m_authAddresses.end () && m_0RTTHandshakeStart)
            {
              m_authAddresses.push_back (InetSocketAddress::ConvertFrom (from).GetIpv4 ()); //add to the list of authenticated sockets
            }
          else if (result == m_authAddresses.end () && !m_0RTTHandshakeStart)
            {
              NS_LOG_WARN ( this << " CONNECTION ABORTED: Short Packet from unauthenticated address " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                            InetSocketAddress::ConvertFrom (from).GetPort ());
              continue;
            }
        }

      // Handle callback for the correct socket
      if (!m_socketHandlers[socket].IsNull ())
        {
          NS_LOG_LOGIC (this << " waking up handler of socket " << socket);
          m_socketHandlers[socket] (packet, header, from);
        }
      else
        {
          NS_FATAL_ERROR ( this << " no handler for socket " << socket);
        }
    }
}