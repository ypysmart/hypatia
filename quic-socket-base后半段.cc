
uint32_t
QuicSocketBase::GetInitialMaxStreamData () const
{
  return m_initial_max_stream_data;
}

uint32_t
QuicSocketBase::GetConnectionMaxData () const
{
  return m_max_data;
}

void
QuicSocketBase::SetConnectionMaxData (uint32_t maxData)
{
  m_max_data = maxData;
}

QuicSocket::QuicStates_t
QuicSocketBase::GetSocketState () const
{
  return m_socketState;
}

void
QuicSocketBase::SetState (TracedValue<QuicStates_t> newstate)
{
  NS_LOG_FUNCTION (this);

  if (m_quicl4->IsServer ())
    {
      NS_LOG_INFO (
        "Server " << QuicStateName[m_socketState] << " -> " << QuicStateName[newstate] << "");
    }
  else
    {
      NS_LOG_INFO (
        "Client " << QuicStateName[m_socketState] << " -> " << QuicStateName[newstate] << "");
    }

  m_socketState = newstate;
}

bool
QuicSocketBase::IsVersionSupported (uint32_t version)
{
  if (version == QUIC_VERSION || version == QUIC_VERSION_DRAFT_10
      || version == QUIC_VERSION_NS3_IMPL)
    {
      return true;
    }
  else
    {
      return false;
    }
}

void
QuicSocketBase::AbortConnection (uint16_t transportErrorCode,
                                 const char* reasonPhrase,
                                 bool applicationClose)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO (
    "Abort connection " << transportErrorCode << " because " << reasonPhrase);
  // //std::cout<< "Abort connection " << transportErrorCode << " because " << reasonPhrase <<std::endl;

  m_transportErrorCode = transportErrorCode;

  QuicSubheader quicSubheader;
  Ptr<Packet> frame = Create<Packet> ();
  if (!applicationClose)
    {
      quicSubheader = QuicSubheader::CreateConnectionClose (
        m_transportErrorCode, reasonPhrase);
    }
  else
    {
      quicSubheader = QuicSubheader::CreateApplicationClose (
        m_transportErrorCode, reasonPhrase);
    }
  frame->AddHeader (quicSubheader);

  QuicHeader quicHeader;
  switch (m_socketState)
    {
      case CONNECTING_CLT:
        quicHeader = QuicHeader::CreateInitial (m_connectionId, m_vers,
                                                m_subflows[0]->m_tcb->m_nextTxSequence++);
        break;
      case CONNECTING_SVR:
        quicHeader = QuicHeader::CreateHandshake (m_connectionId, m_vers,
                                                  m_subflows[0]->m_tcb->m_nextTxSequence++);
        break;
      case OPEN:
        quicHeader =
          !m_connected ?
          QuicHeader::CreateHandshake (m_connectionId, m_vers,
                                       m_subflows[0]->m_tcb->m_nextTxSequence++) :
          QuicHeader::CreateShort (m_connectionId,
                                   m_subflows[0]->m_tcb->m_nextTxSequence++,
                                   !m_omit_connection_id, m_keyPhase);
        break;
      case CLOSING:
        quicHeader = QuicHeader::CreateShort (m_connectionId,
                                              m_subflows[0]->m_tcb->m_nextTxSequence++,
                                              !m_omit_connection_id,
                                              m_keyPhase);
        break;
      default:
        NS_ABORT_MSG (
          "AbortConnection in unfeasible Socket State for the request");
        return;
    }
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddAtEnd (frame);
  uint32_t sz = packet->GetSize ();

  quicHeader.SetPathId(0);
  // m_subflows[0]->Add(m_subflows[0]->m_tcb->m_nextTxSequence);

  m_quicl4->SendPacket (this, packet, quicHeader);
  m_txTrace (packet, quicHeader, this);
  NotifyDataSent (sz);
   //std::cout<<"我怀疑就是这里6"<<std::endl;

  Close ();//这里调用了close函数
}

bool
QuicSocketBase::GetReceivedTransportParametersFlag () const
{
  return m_receivedTransportParameters;
}

bool
QuicSocketBase::CheckIfPacketOverflowMaxDataLimit (
  std::vector<std::pair<Ptr<Packet>, QuicSubheader> > disgregated)
{
  NS_LOG_FUNCTION (this);
  uint32_t validPacketSize = 0;
  for (auto frame_recv_it = disgregated.begin ();
       frame_recv_it != disgregated.end () and !disgregated.empty ();
       ++frame_recv_it)
    {
      QuicSubheader sub = (*frame_recv_it).second;
      // (*frame_recv_it)->PeekHeader (sub);

      if (sub.IsStream () and sub.GetStreamId () != 0)
        {
          validPacketSize += (*frame_recv_it).first->GetSize ();
        }
    }
  if ((m_max_data < m_rxBuffer->Size () + validPacketSize))
    {
      return true;
    }
  return false;
}

uint32_t
QuicSocketBase::GetMaxStreamId () const
{
  return std::max (m_initial_max_stream_id_bidi, m_initial_max_stream_id_uni);
}

uint32_t
QuicSocketBase::GetMaxStreamIdBidirectional () const
{
  return m_initial_max_stream_id_bidi;
}

uint32_t
QuicSocketBase::GetMaxStreamIdUnidirectional () const
{
  return m_initial_max_stream_id_uni;
}

bool
QuicSocketBase::CouldContainTransportParameters () const
{
  return m_couldContainTransportParameters;
}

void
QuicSocketBase::SetCongestionControlAlgorithm (Ptr<TcpCongestionOps> algo)
{
  NS_LOG_FUNCTION (this << algo);
  if (DynamicCast<QuicCongestionOps> (algo) != 0)
    {
      NS_LOG_INFO ("Non-legacy congestion control");
      m_quicCongestionControlLegacy = false;
    }
  else
    {
      NS_LOG_INFO (
        "Legacy congestion control, using only TCP standard functions");
      m_quicCongestionControlLegacy = true;
    }
  m_congestionControl = algo;
}

void
QuicSocketBase::SetSocketSndBufSize (uint32_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_socketTxBufferSize = size;
  //调试点
  // m_txBuffer->SetMaxBufferSize (size);
}

uint32_t
QuicSocketBase::GetSocketSndBufSize (void) const
{
  return m_txBuffer->GetMaxBufferSize ();
}

void
QuicSocketBase::SetSocketRcvBufSize (uint32_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_socketRxBufferSize = size;
  m_rxBuffer->SetMaxBufferSize (size);
}

uint32_t
QuicSocketBase::GetSocketRcvBufSize (void) const
{
  return m_rxBuffer->GetMaxBufferSize ();
}

void
QuicSocketBase::UpdateCwnd (uint32_t oldValue, uint32_t newValue)
{
  m_cWndTrace (oldValue, newValue);
}

void
QuicSocketBase::UpdateCwnd1 (uint32_t oldValue, uint32_t newValue)
{
  m_cWndTrace1 (oldValue, newValue);
}



void
QuicSocketBase::TraceRTT0 (Time oldValue, Time newValue)
{
  m_rttTrace0 (oldValue, newValue);
  // //std::cout<<"1"<<oldValue<<","<<newValue<<"\n";
}

void
QuicSocketBase::TraceRTT1 (Time oldValue, Time newValue)
{
  m_rttTrace1 (oldValue, newValue);
  // //std::cout<<"1"<<oldValue<<","<<newValue<<"\n";
}

void
QuicSocketBase::UpdateSsThresh (uint32_t oldValue, uint32_t newValue)
{
  m_ssThTrace (oldValue, newValue);
}

void
QuicSocketBase::UpdateSsThresh1 (uint32_t oldValue, uint32_t newValue)
{
  m_ssThTrace1 (oldValue, newValue);
}
void
QuicSocketBase::UpdateCongState (TcpSocketState::TcpCongState_t oldValue,
                                 TcpSocketState::TcpCongState_t newValue)
{
  m_congStateTrace (oldValue, newValue);
}

void
QuicSocketBase::UpdateNextTxSequence (SequenceNumber32 oldValue,
                                      SequenceNumber32 newValue)

{
  m_nextTxSequenceTrace (oldValue.GetValue (), newValue.GetValue ());
}

void
QuicSocketBase::UpdateHighTxMark (SequenceNumber32 oldValue, SequenceNumber32 newValue)
{
  m_highTxMarkTrace (oldValue.GetValue (), newValue.GetValue ());
}

void
QuicSocketBase::SetInitialSSThresh (uint32_t threshold)
{
  m_pathManager->SetInitialSSThresh(threshold);
}

uint32_t
QuicSocketBase::GetInitialSSThresh (void) const
{
  return m_pathManager->GetInitialSSThresh();
}

void
QuicSocketBase::SetInitialPacketSize (uint32_t size)
{
  NS_ABORT_MSG_IF (size < 1200, "The size of the initial packet should be at least 1200 bytes");
  m_initialPacketSize = size;
}

uint32_t
QuicSocketBase::GetInitialPacketSize () const
{
  return m_initialPacketSize;
}

void QuicSocketBase::SetLatency (uint32_t streamId, Time latency)
{
  m_txBuffer->SetLatency (streamId, latency);
}

Time QuicSocketBase::GetLatency (uint32_t streamId)
{
  return m_txBuffer->GetLatency (streamId);
}

void QuicSocketBase::SetDefaultLatency (Time latency)
{
  m_txBuffer->SetDefaultLatency (latency);
}

Time QuicSocketBase::GetDefaultLatency ()
{
  return m_txBuffer->GetDefaultLatency ();
}

void
QuicSocketBase::NotifyPacingPerformed (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Pacing timer expired, try sending a packet");
  //std::cout<<"还能进这里NotifyPacingPerformed"<<std::endl;
  SendPendingData (m_connected);
}



void
QuicSocketBase::CreateScheduler ()
{
  NS_LOG_FUNCTION (this);
  m_scheduler = CreateObject<MpQuicScheduler> ();
  m_scheduler->SetSocket(this);
}

void
QuicSocketBase::CreatePathManager()
{
  NS_LOG_FUNCTION (this);
  m_pathManager = CreateObject<MpQuicPathManager> ();
  m_pathManager->SetSocket(this);
}

void 
QuicSocketBase::CreateNewSubflows ()
{
  NS_LOG_FUNCTION (this);
  int16_t addrNum = m_node->GetObject<Ipv4>()->GetNInterfaces();
  if (m_enableMultipath && addrNum > 2)
  {
    m_quicl4->Allow0RTTHandshake(true);
    for(int16_t num = 2; num < addrNum; num++)
    {
      Ptr<MpQuicSubFlow> subflow = m_pathManager->AddSubflow(InetSocketAddress(m_node->GetObject<Ipv4>()->GetAddress(num,0).GetLocal(), m_endPoint->GetLocalPort()+num-1), m_currentFromAddress, num-1);
    }
  }
}


void 
QuicSocketBase::SubflowInsert(Ptr<MpQuicSubFlow> sflow)
{
  NS_LOG_FUNCTION (this);
  m_subflows.insert(m_subflows.end(), sflow);
}

void
QuicSocketBase::AddPath(Address address, Address from, uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  m_quicl4->AddPath(pathId, this, address, from);
}

void
QuicSocketBase::SendAddAddress(Address address, uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  QuicSubheader sub = QuicSubheader::CreateAddAddress (address, pathId);
  Ptr<Packet> frame = Create<Packet> ();
  frame->AddHeader (sub);
  Ptr<Packet> p = Create<Packet> ();
  p->AddAtEnd(frame);
  SequenceNumber32 packetNumber = ++m_subflows[0]->m_tcb->m_nextTxSequence;
  QuicHeader head;
  head = QuicHeader::CreateShort (m_connectionId, packetNumber,!m_omit_connection_id, m_keyPhase);
  head.SetPathId(0);
  NS_LOG_INFO ("Send ADD_ADDRESS packet with header " << head);
  // m_subflows[0]->Add(packetNumber);
  m_quicl4->SendPacket (this, p, head);

}

void
QuicSocketBase::OnReceivedAddAddressFrame (QuicSubheader &sub)
{
  NS_LOG_FUNCTION (this);
  uint8_t pathId = sub.GetPathId();

  InetSocketAddress transport = InetSocketAddress::ConvertFrom (sub.GetAddress());
  Ipv4Address ipv4 = transport.GetIpv4 ();
  uint16_t port = transport.GetPort ();
  Address peerAddr = InetSocketAddress(ipv4, port);
  Address localAddr = InetSocketAddress(m_node->GetObject<Ipv4>()->GetAddress(pathId+1,0).GetLocal(), port);

  m_quicl4->AddPath(pathId, this, localAddr, peerAddr);
  m_quicl4->Allow0RTTHandshake(true);
  m_pathManager->AddSubflowWithPeerAddress(localAddr, peerAddr, pathId);
}


void
QuicSocketBase::SendPathChallenge(uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  QuicSubheader sub = QuicSubheader::CreatePathChallenge (pathId);
  Ptr<Packet> frame = Create<Packet> ();
  frame->AddHeader (sub);
  Ptr<Packet> p = Create<Packet> ();
  p->AddAtEnd(frame);
  SequenceNumber32 packetNumber = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
  QuicHeader head;
  head = QuicHeader::CreateShort (m_connectionId, packetNumber,!m_omit_connection_id, m_keyPhase);
  head.SetPathId(pathId);

  NS_LOG_INFO ("Send PATH_CHALLENGE packet with header " << head);
  m_quicl4->SendPacket (this, p, head);
}

void
QuicSocketBase::OnReceivedPathChallengeFrame (QuicSubheader &sub)
{
  NS_LOG_FUNCTION (this);
  m_subflows[m_currentPathId]->m_peerAddr = m_currentFromAddress;
  m_subflows[m_currentPathId]->m_subflowState = MpQuicSubFlow::Active;
  m_quicl4->ReDoUdpConnect(m_currentPathId, m_currentFromAddress);
  //调试点
  // m_txBuffer->AddSentList(m_currentPathId);
  std::cout << " OnReceivedPathChallengeFrame结束了 " << m_txBuffer->AppSize() << " 字节在缓冲区中。" << std::endl;
  SendPathResponse(m_currentPathId);
}

void
QuicSocketBase::SendPathResponse (uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  QuicSubheader sub = QuicSubheader::CreatePathResponse (pathId);
  Ptr<Packet> frame = Create<Packet> ();
  frame->AddHeader (sub);
  Ptr<Packet> p = Create<Packet> ();
  p->AddAtEnd(frame);
  SequenceNumber32 packetNumber = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
  QuicHeader head;
  head = QuicHeader::CreateShort (m_connectionId, packetNumber,!m_omit_connection_id, m_keyPhase);
  head.SetPathId(pathId);
  NS_LOG_INFO ("Send PATH_RESPONSE packet with header " << head);

  m_quicl4->SendPacket (this, p, head);
}

void
QuicSocketBase::OnReceivedPathResponseFrame (QuicSubheader &sub)
{
  NS_LOG_FUNCTION (this);
  m_subflows[m_currentPathId]->m_subflowState = MpQuicSubFlow::Active;
  //调试点
  // m_txBuffer->AddSentList(m_currentPathId);
}


std::vector<Ptr<MpQuicSubFlow>>
QuicSocketBase::GetActiveSubflows()
{
  NS_LOG_FUNCTION(this);
  std::vector<Ptr<MpQuicSubFlow>> sflows;
  for (uint16_t i = 0; i < m_subflows.size(); i++)
  {
    if (m_subflows[i]->m_subflowState == MpQuicSubFlow::Active){
      sflows.insert(sflows.end(), m_subflows[i]);
    }
  }
  return sflows;
}

double
QuicSocketBase::GetOliaAlpha(uint8_t pathId)
{
  std::vector<uint8_t> B;
  std::vector<uint8_t> M;
  uint32_t maxCwnd = 0;
  double maxr = 0;
  for (uint8_t i = 0; i < m_subflows.size(); i++)
  {
      if (m_subflows[i]->m_tcb->m_cWnd > maxCwnd)
      {
        maxCwnd = m_subflows[i]->m_tcb->m_cWnd;
        M.push_back(i);
      }
      double rate = std::max(m_subflows[i]->m_tcb->m_bytesBeforeLost1,m_subflows[i]->m_tcb->m_bytesBeforeLost2)/pow(m_subflows[i]->m_tcb->m_lastRtt.Get().GetSeconds(),2);
      if (rate > maxr)
      {
        maxr = rate;
        B.push_back(i);
      }
  }
  std::vector<uint8_t> B_M;
  for (int x: B)
  {
    if(std::find(M.begin(), M.end(), x) == M.end()) {
      B_M.push_back(x);
    }
  }

  if(std::find(B_M.begin(), B_M.end(), pathId) != B_M.end())
  {
    return 0.5/B_M.size();
  } 
  else if(!B_M.empty() && std::find(M.begin(), M.end(), pathId) != M.end())
  {
    return -0.5/M.size();
  }
  else
  {
    return 0;
  }

}

uint32_t 
QuicSocketBase::GetBytesInBuffer()
{
  return m_txBuffer->AppSize();
}


} // namespace ns3
