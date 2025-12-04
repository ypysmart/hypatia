

void
QuicL4Protocol::SetRecvCallback (Callback<void, Ptr<Packet>, const QuicHeader&,  Address& > handler, Ptr<Socket> sock)
{

  NS_LOG_FUNCTION (this);

  m_socketHandlers.insert ( std::pair< Ptr<Socket>, Callback<void, Ptr<Packet>, const QuicHeader&, Address& > > (sock,handler));
  QuicUdpBindingList::iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == sock && item->m_budpSocket != 0)
        {
          item->m_budpSocket->SetRecvCallback (MakeCallback (&QuicL4Protocol::ForwardUp, this));
          break;
        }
      else if (item->m_quicSocket == sock && item->m_budpSocket6 != 0)
        {
          item->m_budpSocket6->SetRecvCallback (MakeCallback (&QuicL4Protocol::ForwardUp, this));
          break;
        }
      else if (item->m_quicSocket == sock)
        {
          NS_FATAL_ERROR ("The UDP socket for this QuicUdpBinding item is not set");
        }
    }
}

void
QuicL4Protocol::NotifyNewAggregate ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Node> node = this->GetObject<Node> ();

  if (m_node == 0)
    {
      if ((node != 0))
        {
          this->SetNode (node);
          Ptr<QuicSocketFactory> quicFactory = CreateObject<QuicSocketFactory> ();
          quicFactory->SetQuicL4 (this);
          node->AggregateObject (quicFactory);
        }
    }

  IpL4Protocol::NotifyNewAggregate ();
  if (!m_initialized) {  // 添加判断，避免重复
        m_initialized = true;
        Simulator::Schedule(Seconds(m_throughputInterval), &QuicL4Protocol::CalculateThroughput, this);
    }
}

int
QuicL4Protocol::GetProtocolNumber (void) const
{
  return PROT_NUMBER;
}

void
QuicL4Protocol::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_quicUdpBindingList.clear ();

  m_node = 0;
//  m_downTarget.Nullify ();
//  m_downTarget6.Nullify ();
  IpL4Protocol::DoDispose ();
}

Ptr<QuicSocketBase>
QuicL4Protocol::CloneSocket (Ptr<QuicSocketBase> oldsock)
{
  NS_LOG_FUNCTION (this);
  Ptr<QuicSocketBase> newsock = CopyObject<QuicSocketBase> (oldsock);
  NS_LOG_LOGIC (this << " cloned socket " << oldsock << " to socket " << newsock);
  Ptr<QuicUdpBinding> udpBinding = CreateObject<QuicUdpBinding> ();
  udpBinding->m_budpSocket = nullptr;
  udpBinding->m_budpSocket6 = nullptr;
  udpBinding->m_quicSocket = newsock;
  udpBinding->m_pathId = 0;
  m_quicUdpBindingList.insert (m_quicUdpBindingList.end (), udpBinding);

  return newsock;
}



Ptr<Socket>
QuicL4Protocol::CreateSocket ()
{
  return CreateSocket (m_congestionTypeId);
}

Ptr<Socket>
QuicL4Protocol::CreateSocket (TypeId congestionTypeId)
{
  NS_LOG_FUNCTION (this);
  ObjectFactory congestionAlgorithmFactory;
  congestionAlgorithmFactory.SetTypeId (m_congestionTypeId);

  // create the socket
  Ptr<QuicSocketBase> socket = CreateObject<QuicSocketBase> ();
  // create the congestion control algorithm
  Ptr<TcpCongestionOps> algo = congestionAlgorithmFactory.Create<TcpCongestionOps> ();
  socket->SetCongestionControlAlgorithm (algo);

  // TODO consider if rttFactory is needed
  // Ptr<RttEstimator> rtt = rttFactory.Create<RttEstimator> ();
  // socket->SetRtt (rtt);

  socket->SetNode (m_node);
  socket->SetQuicL4 (this);

  socket->InitializeScheduling ();

  // generate a random connection ID and check that has not been assigned to other
  // sockets associated to this L4 protocol
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();

  bool found = false;
  uint64_t connectionId;
  while (not found)
    {
      connectionId = uint64_t (rand->GetValue (0, pow (2, 64) - 1));
      found = true;
      for (auto it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
        {
          found = false;
          if (connectionId == (*it)->m_quicSocket->GetConnectionId ())
            {
              break;
            }
          found = true;
        }
    }
  socket->SetConnectionId (connectionId);
  Ptr<QuicUdpBinding> udpBinding = Create<QuicUdpBinding> ();
  udpBinding->m_budpSocket = nullptr;
  udpBinding->m_budpSocket6 = nullptr;
  udpBinding->m_quicSocket = socket;
  udpBinding->m_pathId = 0;
  m_quicUdpBindingList.insert (m_quicUdpBindingList.end (), udpBinding);

  return socket;
}

Ptr<Socket>
QuicL4Protocol::CreateUdpSocket ()
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_node != 0);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> udpSocket = Socket::CreateSocket (m_node, tid);

  return udpSocket;
}

Ptr<Socket>
QuicL4Protocol::CreateUdpSocket6 ()
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_node != 0);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> udpSocket6 = Socket::CreateSocket (m_node, tid);

  return udpSocket6;
}

void
QuicL4Protocol::ReceiveIcmp (Ipv4Address icmpSource, uint8_t icmpTtl,
                             uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo,
                             Ipv4Address payloadSource,Ipv4Address payloadDestination,
                             const uint8_t payload[8])
{
  NS_LOG_FUNCTION (this << icmpSource << (uint16_t) icmpTtl << (uint16_t) icmpType << (uint16_t) icmpCode << icmpInfo
                        << payloadSource << payloadDestination);
}

void
QuicL4Protocol::ReceiveIcmp (Ipv6Address icmpSource, uint8_t icmpTtl,
                             uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo,
                             Ipv6Address payloadSource,Ipv6Address payloadDestination,
                             const uint8_t payload[8])
{
  NS_LOG_FUNCTION (this << icmpSource << (uint16_t) icmpTtl << (uint16_t) icmpType << (uint16_t) icmpCode << icmpInfo
                        << payloadSource << payloadDestination);

}

enum IpL4Protocol::RxStatus
QuicL4Protocol::Receive (Ptr<Packet> packet,
                         Ipv4Header const &incomingIpHeader,
                         Ptr<Ipv4Interface> incomingInterface)
{
  NS_LOG_FUNCTION (this << packet << incomingIpHeader << incomingInterface);
  NS_FATAL_ERROR ("This call should not be used: QUIC packets need to go through a UDP socket");
  return IpL4Protocol::RX_OK;
}

enum IpL4Protocol::RxStatus
QuicL4Protocol::Receive (Ptr<Packet> packet,
                         Ipv6Header const &incomingIpHeader,
                         Ptr<Ipv6Interface> interface)
{
  NS_LOG_FUNCTION (this << packet << incomingIpHeader.GetSourceAddress () <<
                   incomingIpHeader.GetDestinationAddress ());
  NS_FATAL_ERROR ("This call should not be used: QUIC packets need to go through a UDP socket");
  return IpL4Protocol::RX_OK;
}

void
QuicL4Protocol::SendPacket (Ptr<QuicSocketBase> socket, Ptr<Packet> pkt, const QuicHeader &outgoing) const
{
  NS_LOG_FUNCTION (this << socket);
  uint8_t pathId = outgoing.GetPathId (); 
  const_cast<QuicL4Protocol*>(this)->m_totalSentBytes += pkt->GetSize();  // 注意：const_cast去除const
    // std::cout
              // <<" 发送 "<<this <<"Send pkt#"<<outgoing.GetPacketNumber ()
              // <<"         path Id: "<<(int)uint8_t(pathId)
              // <<"         data size " <<  pkt->GetSize () 
              // <<"         发送总数据量 " << m_totalReceivedBytes
              // <<"        时间: " << Simulator::Now().GetNanoSeconds() 
              // <<"\n";
    // 新增：累加发送字节（pkt是数据部分，不包括头）
  //log for experiment
  // if (!IsServer()){
  //   std::cout<<"send\t"
  //            << (int)pathId <<"\t"
  //            << outgoing.GetPacketNumber () <<"\t"
  //            << Simulator::Now().GetSeconds()<< std::endl;
  // }

  
  NS_LOG_INFO ("Sending Packet Through UDP Socket");

  // Given the presence of multiple subheaders in pkt,
  // we create a new packet, add the new QUIC header and
  // then add pkt as payload
  Ptr<Packet> packetSent = Create<Packet> ();
  packetSent->AddHeader (outgoing);
  packetSent->AddAtEnd (pkt);

  QuicUdpBindingList::const_iterator it;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_quicSocket == socket && item->m_pathId == pathId)
        {
          UdpSend (item->m_budpSocket, packetSent, 0);
          break;
        }
    }
}


bool
QuicL4Protocol::RemoveSocket (Ptr<QuicSocketBase> socket)
{
  NS_LOG_FUNCTION (this);

  QuicUdpBindingList::iterator iter;
  bool found = false;
  bool closedListener = false;

  for (iter = m_quicUdpBindingList.begin (); iter != m_quicUdpBindingList.end (); ++iter)
    {
      Ptr<QuicUdpBinding> item = *iter;
      if (item->m_quicSocket == socket)
        {
          found = true;
          if (item->m_listenerBinding)
            {
              closedListener = true;
            }
          m_quicUdpBindingList.erase (iter);

          break;
        }
    }

  //if closing the listener, close all the clone ones
  if (closedListener)
    {
      NS_LOG_LOGIC (this << " Closing all the cloned sockets");
      iter = m_quicUdpBindingList.begin ();
      while (iter != m_quicUdpBindingList.end ())
        {
          std::cout<<"这里还藏了一个"<<std::endl;
          (*iter)->m_quicSocket->Close ();
          ++iter;
        }
    }

  return found;
}

Ipv4EndPoint *
QuicL4Protocol::Allocate (void)
{
  NS_LOG_FUNCTION (this);
  return m_endPoints->Allocate ();
}

Ipv4EndPoint *
QuicL4Protocol::Allocate (Ipv4Address address)
{
  NS_LOG_FUNCTION (this << address);
  return m_endPoints->Allocate (address);
}

Ipv4EndPoint *
QuicL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << port);
  return m_endPoints->Allocate (boundNetDevice, port);
}

Ipv4EndPoint *
QuicL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << address << port);
  return m_endPoints->Allocate (boundNetDevice, address, port);
}

Ipv4EndPoint *
QuicL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice,
                          Ipv4Address localAddress, uint16_t localPort,
                          Ipv4Address peerAddress, uint16_t peerPort)
{
  NS_LOG_FUNCTION (this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
  return m_endPoints->Allocate (boundNetDevice,
                                localAddress, localPort,
                                peerAddress, peerPort);
}

Ipv6EndPoint *
QuicL4Protocol::Allocate6 (void)
{
  NS_LOG_FUNCTION (this);
  return m_endPoints6->Allocate ();
}

Ipv6EndPoint *
QuicL4Protocol::Allocate6 (Ipv6Address address)
{
  NS_LOG_FUNCTION (this << address);
  return m_endPoints6->Allocate (address);
}

Ipv6EndPoint *
QuicL4Protocol::Allocate6 (Ptr<NetDevice> boundNetDevice, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << port);
  return m_endPoints6->Allocate (boundNetDevice, port);
}

Ipv6EndPoint *
QuicL4Protocol::Allocate6 (Ptr<NetDevice> boundNetDevice, Ipv6Address address, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << address << port);
  return m_endPoints6->Allocate (boundNetDevice, address, port);
}

Ipv6EndPoint *
QuicL4Protocol::Allocate6 (Ptr<NetDevice> boundNetDevice,
                           Ipv6Address localAddress, uint16_t localPort,
                           Ipv6Address peerAddress, uint16_t peerPort)
{
  NS_LOG_FUNCTION (this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
  return m_endPoints6->Allocate (boundNetDevice,
                                 localAddress, localPort,
                                 peerAddress, peerPort);
}

void
QuicL4Protocol::DeAllocate (Ipv4EndPoint *endPoint)
{
  NS_LOG_FUNCTION (this << endPoint);
  m_endPoints->DeAllocate (endPoint);
}

void
QuicL4Protocol::DeAllocate (Ipv6EndPoint *endPoint)
{
  NS_LOG_FUNCTION (this << endPoint);
  m_endPoints6->DeAllocate (endPoint);
}

void
QuicL4Protocol::SetDownTarget (IpL4Protocol::DownTargetCallback callback)
{
  NS_LOG_FUNCTION (this);
  m_downTarget = callback;
}

IpL4Protocol::DownTargetCallback
QuicL4Protocol::GetDownTarget (void) const
{
  NS_LOG_FUNCTION (this);
  return m_downTarget;
}

void
QuicL4Protocol::SetDownTarget6 (IpL4Protocol::DownTargetCallback6 callback)
{
  NS_LOG_FUNCTION (this);
  m_downTarget6 = callback;
}

IpL4Protocol::DownTargetCallback6
QuicL4Protocol::GetDownTarget6 (void) const
{
  return m_downTarget6;
}

bool
QuicL4Protocol::Is0RTTHandshakeAllowed () const
{
  return m_0RTTHandshakeStart;
}

//For multipath implementation

int
QuicL4Protocol::AddPath(uint8_t pathId, Ptr<QuicSocketBase> socket, Address localAddress, Address peerAddress)
{
  NS_LOG_FUNCTION (this);
  int res = -1;
  if (InetSocketAddress::IsMatchingType (localAddress))
    {
      Ptr<QuicUdpBinding> udpBinding = CreateObject<QuicUdpBinding> ();
      Ptr<Socket> udpSocket = CreateUdpSocket ();
      res = udpSocket->Bind (localAddress);
      udpSocket->Connect(peerAddress);
      udpSocket->SetRecvCallback (MakeCallback (&QuicL4Protocol::ForwardUp, this));
      udpBinding->m_budpSocket = udpSocket;
      udpBinding->m_budpSocket6 = nullptr;
      udpBinding->m_quicSocket = socket;
      udpBinding->m_pathId = pathId;
      m_quicUdpBindingList.insert(m_quicUdpBindingList.end (),udpBinding);
      return res;
    }
  else if (Inet6SocketAddress::IsMatchingType (localAddress))
    {
      Ptr<QuicUdpBinding> udpBinding = CreateObject<QuicUdpBinding> ();
      Ptr<Socket> udpSocket = CreateUdpSocket ();
      res = udpSocket->Bind (localAddress);
      udpSocket->Connect(peerAddress);
      udpSocket->SetRecvCallback (MakeCallback (&QuicL4Protocol::ForwardUp, this));
      udpBinding->m_budpSocket = nullptr;
      udpBinding->m_budpSocket6 = udpSocket;
      udpBinding->m_quicSocket = socket;
      udpBinding->m_pathId = pathId;
      m_quicUdpBindingList.insert(m_quicUdpBindingList.end (),udpBinding);
      return res;
    }
  return -1;
}

void
QuicL4Protocol::Allow0RTTHandshake (bool allow0RTT)
{
  m_0RTTHandshakeStart = allow0RTT;
}

int
QuicL4Protocol::ReDoUdpConnect(uint8_t pathId, Address peerAddress)
{
  QuicUdpBindingList::iterator it;
  Ptr<QuicSocketBase> socket;
  for (it = m_quicUdpBindingList.begin (); it != m_quicUdpBindingList.end (); ++it)
    {
      Ptr<QuicUdpBinding> item = *it;
      if (item->m_pathId == pathId)
        {
          return item->m_budpSocket->Connect(peerAddress);
        }
    }
    return -1;
}
void
QuicL4Protocol::CalculateThroughput()
{
//   Time now = Simulator::Now();  // 当前模拟时间
//   double timeDiff = (now - m_lastThroughputTime).GetSeconds();  // 时间差（秒）

//   if (timeDiff > 0) {  // 避免除零
//     sum_totalSentBytes=sum_totalSentBytes+m_totalSentBytes * 8.0 / 1024.0 / 1024.0;
// sum_totalReceivedBytes=sum_totalReceivedBytes+m_totalReceivedBytes* 8.0 / 1024.0 / 1024.0;
//     double sentThroughputMbps = (m_totalSentBytes * 8.0 / 1024.0 / 1024.0) / timeDiff;  // 字节转Mbps
//     double recvThroughputMbps = (m_totalReceivedBytes * 8.0 / 1024.0 / 1024.0) / timeDiff;

//     // 输出到控制台（你可以改成写文件）
//     NS_LOG_UNCOND("QUIC Total Sent Throughput: " << sentThroughputMbps << " Mbps");
//     NS_LOG_UNCOND("QUIC Total Received Throughput: " << recvThroughputMbps << " Mbps");

//     NS_LOG_UNCOND("累计发送流量" << sum_totalSentBytes << " Mbps");
//     NS_LOG_UNCOND("累计接收流量" << sum_totalReceivedBytes << " Mbps");
//   }

//   // 重置累加器（可选，如果你想每间隔重算一次累计）
//   m_totalSentBytes = 0;
//   m_totalReceivedBytes = 0;
//   m_lastThroughputTime = now;

//   // 调度下一次计算（每m_throughputInterval秒）
//   Simulator::Schedule(Seconds(m_throughputInterval), &QuicL4Protocol::CalculateThroughput, this);
}
} // namespace ns3

