
void
QuicSocketBase::SetSegSize (uint32_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_pathManager->SetSegSize(size);
}

uint32_t
QuicSocketBase::GetSegSize (void) const
{
  return m_pathManager->GetSegSize();
}

void
QuicSocketBase::MaybeQueueAck (uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  ++m_subflows[pathId]->m_numPacketsReceivedSinceLastAckSent;
  NS_LOG_INFO ("m_numPacketsReceivedSinceLastAckSent " << m_subflows[pathId]->m_numPacketsReceivedSinceLastAckSent << " m_queue_ack " << m_subflows[pathId]->m_queue_ack);

  // handle the list of m_receivedPacketNumbers
  if (m_subflows[pathId]->m_receivedPacketNumbers.empty ())
    {
      NS_LOG_INFO ("Nothing to ACK");
      m_subflows[pathId]->m_queue_ack = false;
      return;
    }

  if (m_subflows[pathId]->m_numPacketsReceivedSinceLastAckSent > m_subflows[pathId]->m_tcb->m_kMaxPacketsReceivedBeforeAckSend)
    {
      NS_LOG_INFO ("immediately send ACK - max number of unacked packets reached");
      m_subflows[pathId]->m_queue_ack = true;
      if (!m_subflows[pathId]->m_sendAckEvent.IsRunning ())
        {
          m_subflows[pathId]->m_sendAckEvent = Simulator::Schedule (TimeStep (1), &QuicSocketBase::SendAck, this, pathId);
        }
    }

  if (HasReceivedMissing ())  // immediately queue the ACK
    {
      NS_LOG_INFO ("immediately send ACK - some packets have been received out of order");
      m_subflows[pathId]->m_queue_ack = true;
      if (!m_subflows[pathId]->m_sendAckEvent.IsRunning ())
        {
          m_subflows[pathId]->m_sendAckEvent = Simulator::Schedule (TimeStep (1), &QuicSocketBase::SendAck, this, pathId);
        }
    }

  if (!m_subflows[pathId]->m_queue_ack)
    {
      if (m_subflows[pathId]->m_numPacketsReceivedSinceLastAckSent > 2) // QUIC decimation option
        {
          NS_LOG_INFO ("immediately send ACK - more than 2 packets received");
          m_subflows[pathId]->m_queue_ack = true;
          if (!m_subflows[pathId]->m_sendAckEvent.IsRunning ())
            {
              m_subflows[pathId]->m_sendAckEvent = Simulator::Schedule (TimeStep (1), &QuicSocketBase::SendAck, this, pathId);
            }
        }
      else
        {
          if (!m_subflows[pathId]->m_delAckEvent.IsRunning ())
            {
              NS_LOG_INFO ("Schedule a delayed ACK");
              // schedule a delayed ACK
              m_subflows[pathId]->m_delAckEvent = Simulator::Schedule (
                m_subflows[pathId]->m_tcb->m_kDelayedAckTimeout, &QuicSocketBase::SendAck, this, pathId);
            }
          else
            {
              NS_LOG_INFO ("Delayed ACK timer already running");
            }
        }
    }
}

bool
QuicSocketBase::HasReceivedMissing ()
{
  // TODO implement this
  return false;
}

void
QuicSocketBase::SendAck (uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  m_subflows[pathId]->m_delAckEvent.Cancel ();
  m_subflows[pathId]->m_sendAckEvent.Cancel ();
  m_subflows[pathId]->m_queue_ack = false;

  m_subflows[pathId]->m_numPacketsReceivedSinceLastAckSent = 0;

  
  Ptr<Packet> p = Create<Packet> ();
  if (!m_subflows[pathId]->m_receivedPacketNumbers.empty())
  {
    // std::cout<<"SendAck调用"<<std::endl;

    p->AddAtEnd (OnSendingAckFrame (pathId));/***************************************************************** */
    SequenceNumber32 packetNumber = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
    QuicHeader head;
    head = QuicHeader::CreateShort (m_connectionId, packetNumber, !m_omit_connection_id, m_keyPhase);

    m_txBuffer->UpdateAckSent (packetNumber, p->GetSerializedSize () + head.GetSerializedSize (), m_subflows[pathId]->m_tcb);

    NS_LOG_INFO ("Send ACK packet with header " << head);

    head.SetPathId(pathId);
  //  std::cout<<"第一次出现"<<std::endl;
    m_quicl4->SendPacket (this, p, head);
    m_txTrace (p, head, this);
    // 🔥 新增：重置 Idle Timeout（和 SendDataPacket 一致）//不加这句服务器端只发送ack缓冲区为空不发送数据会认为是空闲导致连接服务器册申请连接关闭
    if (!m_drainingPeriodEvent.IsRunning()) {
        m_idleTimeoutEvent.Cancel();
        m_idleTimeoutEvent = Simulator::Schedule(m_idleTimeout, &QuicSocketBase::Close, this);
    }
  }
}

uint32_t
QuicSocketBase::SendDataPacket (SequenceNumber32 packetNumber, uint32_t maxSize, bool withAck, uint8_t pathId)
{
  NS_LOG_FUNCTION (this << packetNumber << maxSize << withAck);
  //std::cout << this << "packetNumber: " << packetNumber << " maxSize:" << maxSize << "withAck: " << withAck << std::endl;
  // maxSize = std::min (m_subflows[pathId]->m_tcb->m_cWnd.Get(), maxSize);
std::cout<<"Sending data on path" << (int)pathId << " withAck=" << withAck<<std::endl;
  if (!m_drainingPeriodEvent.IsRunning ())
    {
      m_idleTimeoutEvent.Cancel ();
      NS_LOG_LOGIC (this << " SendDataPacket Schedule Close at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + m_idleTimeout.Get ()).GetSeconds ());
      //  //std::cout<<"取消if (!m_drainingPeriodEvent.IsRunning ())   m_idleTimeoutEvent"<<std::endl;

      m_idleTimeoutEvent = Simulator::Schedule (m_idleTimeout, &QuicSocketBase::Close, this);//这里调用了close函数
      // //std::cout<<"SendDataPacket  if (!m_drainingPeriodEvent.IsRunning ())   就设置了"<<std::endl;

    }
  else
    {
      //ypy
      NS_LOG_INFO ("Draining period event running");
      //  std::cout<<"Draining period event running"<<this<<std::endl;
      return -1;
      //ypy
      // NS_LOG_INFO("DRAINING: ACK-only allowed");
    }
// if (m_drainingPeriodEvent.IsRunning()) {
//     if (!withAck || m_txBuffer->AppSize() > 0) {  // 有数据？禁止！
//       NS_LOG_INFO("DRAINING: Block data, allow ACK-only");
//       return 0;
//     }
//   }
  Ptr<Packet> p;

  if (m_txBuffer->GetNumFrameStream0InBuffer () > 0)
    {
      p = m_txBuffer->NextStream0Sequence (packetNumber);
      NS_ABORT_MSG_IF (p == 0, "No packet for stream 0 in the buffer!");
      if (p == 0) 
      {
          //std::cout << "No packet for stream 0 in the buffer!" << std::endl;
      }
      else
      {
          //std::cout << "No packet for stream 0 in the buffer!"  << " p = m_txBuffer:" << p << std::endl;
      }
    }
  else
    {
      NS_LOG_LOGIC (this << " SendDataPacket - sending packet " << packetNumber.GetValue () << " of size " << maxSize << " at time " << Simulator::Now ().GetSeconds ());
      // std::cout << this << " SendDataPacket - sending packet " << packetNumber.GetValue () << " of size " << maxSize << " at time " << Simulator::Now ().GetSeconds () << std::endl;
      //  //std::cout<<"SendDataPacket     if (m_txBuffer->GetNumFrameStream0InBuffer () > 0)设置m_idleTimeoutEvent"<<std::endl;
      m_idleTimeoutEvent = Simulator::Schedule (m_idleTimeout, &QuicSocketBase::Close, this);//这里调用了close函数
      p = m_txBuffer->NextSequence (maxSize, packetNumber, pathId);
    }

  uint32_t sz = p->GetSize ();

  // check whether the connection is appLimited, i.e. not enough data to fill a packet
  if (sz < maxSize and m_txBuffer->AppSize () == 0 and m_subflows[pathId]->m_tcb->m_bytesInFlight.Get () < m_subflows[pathId]->m_tcb->m_cWnd)
    {
      NS_LOG_LOGIC ("Connection is Application-Limited. sz = " << sz << " < maxSize = " << maxSize);
      m_subflows[pathId]->m_tcb->m_appLimitedUntil = m_subflows[pathId]->m_tcb->m_delivered + m_subflows[pathId]->m_tcb->m_bytesInFlight.Get () ? : 1U;
    }

  // perform pacing
  if (m_subflows[pathId]->m_tcb->m_pacing)
    {
      NS_LOG_DEBUG ("Pacing is enabled");
      if (m_pacingTimer.IsExpired ())
        {
          NS_LOG_DEBUG ("Current Pacing Rate " << m_subflows[pathId]->m_tcb->m_pacingRate);
          NS_LOG_DEBUG ("Pacing Timer is in expired state, activate it. Expires in " <<
                        m_subflows[pathId]->m_tcb->m_pacingRate.Get ().CalculateBytesTxTime (sz));
          m_pacingTimer.Schedule (m_subflows[pathId]->m_tcb->m_pacingRate.Get ().CalculateBytesTxTime (sz));
        }
      else
        {
          NS_LOG_INFO ("Pacing Timer is already in running state");
        }
    }

  bool isAckOnly = ((sz == 0) & (withAck));


  if (withAck && !m_subflows[pathId]->m_receivedPacketNumbers.empty ())
    {
      p->AddAtEnd (OnSendingAckFrame (pathId));/***************************************************************** */
      
    // std::cout<<"Senddatapacket调用"<<std::endl;

    }


  QuicHeader head;

  if (m_socketState == CONNECTING_SVR)
    {
      m_connected = true;
      head = QuicHeader::CreateHandshake (m_connectionId, m_vers, packetNumber);
                                                // std::cout << this << "看发送状态" <<  "CreateHandshake"  << std::endl;

    }
  else if (m_socketState == CONNECTING_CLT)
    {
      head = QuicHeader::CreateInitial (m_connectionId, m_vers, packetNumber);
                                          // std::cout << this << "看发送状态" <<  "CreateInitial"  << std::endl;

    }
  else if (m_socketState == OPEN)
    {

      
      if (!m_connected and !m_quicl4->Is0RTTHandshakeAllowed ())
        {
          m_connected = true;
          head = QuicHeader::CreateHandshake (m_connectionId, m_vers, packetNumber);
                                    // std::cout << this << "看发送状态" <<  "CreateHandshake"  << std::endl;

        }
      else if (!m_connected and m_quicl4->Is0RTTHandshakeAllowed ())
        {
          head = QuicHeader::Create0RTT (m_connectionId, m_vers, packetNumber);
                          // std::cout << this << "看发送状态" <<  "Create0RTT"  << std::endl;

          m_connected = true;
          m_keyPhase == QuicHeader::PHASE_ONE ? m_keyPhase = QuicHeader::PHASE_ZERO :
            m_keyPhase = QuicHeader::PHASE_ONE;
        }
      else
        {
          head = QuicHeader::CreateShort (m_connectionId, packetNumber, !m_omit_connection_id, m_keyPhase);
                // std::cout << this << "看发送状态" <<  "CreateShort"  << std::endl;

        }
    }
  else
    {
      // 0 bytes sent - the socket is closed!
      return 0;
    }

  NS_LOG_INFO ("SendDataPacket of size " << p->GetSize ());
  

  head.SetPathId(pathId);
  // m_subflows[pathId]->Add(packetNumber);
//  std::cout<<"第二次出现"<<std::endl;
  m_quicl4->SendPacket (this, p, head);////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  m_txTrace (p, head, this);
  NotifyDataSent (sz);

  m_txBuffer->UpdatePacketSent (packetNumber, sz, pathId, m_subflows[pathId]->m_tcb);
  // DynamicCast<MpQuicCongestionOps> (m_congestionControl)->OnPacketSent (m_subflows[pathId]->m_tcb, packetNumber, isAckOnly);

  if (!m_quicCongestionControlLegacy)
    {
      DynamicCast<QuicCongestionOps> (m_congestionControl)->OnPacketSent (
        m_subflows[pathId]->m_tcb, packetNumber, isAckOnly);
    }
  if (!isAckOnly)
    {
      SetReTxTimeout (pathId);
    }

  return sz;
}

void
QuicSocketBase::SetReTxTimeout (uint8_t pathId)
{
  //TODO check for special packets
  NS_LOG_FUNCTION (this);

  // Don't arm the alarm if there are no packets with retransmittable data in flight.
  //if (numRetransmittablePacketsOutstanding == 0)
  if (false)
    {
      m_subflows[pathId]->m_tcb->m_lossDetectionAlarm.Cancel ();
      return;
    }

  if (m_subflows[pathId]->m_tcb->m_kUsingTimeLossDetection)
    {
      m_subflows[pathId]->m_tcb->m_lossTime = Simulator::Now () + m_subflows[pathId]->m_tcb->m_kTimeReorderingFraction * m_subflows[pathId]->m_tcb->m_smoothedRtt;
    }

  Time alarmDuration;
  // Handshake packets are outstanding
  if (m_socketState == CONNECTING_CLT || m_socketState == CONNECTING_SVR)
    {
      NS_LOG_INFO ("Connecting, set alarm");
      // Handshake retransmission alarm.
      if (m_subflows[pathId]->m_tcb->m_smoothedRtt == Seconds (0))
        {
          alarmDuration = 2 * m_subflows[pathId]->m_tcb->m_kDefaultInitialRtt;
        }
      else
        {
          alarmDuration = 2 * m_subflows[pathId]->m_tcb->m_smoothedRtt;
        }
      alarmDuration = std::max (alarmDuration + m_subflows[pathId]->m_tcb->m_maxAckDelay,
                                m_subflows[pathId]->m_tcb->m_kMinTLPTimeout);
      alarmDuration = alarmDuration * (2 ^ m_subflows[pathId]->m_tcb->m_handshakeCount);
      m_subflows[pathId]->m_tcb->m_alarmType = 0;
    }
  else if (m_subflows[pathId]->m_tcb->m_lossTime != Seconds (0))
    {
      NS_LOG_INFO ("Early retransmit timer");
      // Early retransmit timer or time loss detection.
      alarmDuration = m_subflows[pathId]->m_tcb->m_lossTime - m_subflows[pathId]->m_tcb->m_timeOfLastSentPacket;
      m_subflows[pathId]->m_tcb->m_alarmType = 1;
    }
  else if (m_subflows[pathId]->m_tcb->m_tlpCount < m_subflows[pathId]->m_tcb->m_kMaxTLPs)
    {
      NS_LOG_LOGIC ("m_subflows[pathId]->m_tcb->m_tlpCount < m_subflows[pathId]->m_tcb->m_kMaxTLPs");
      // Tail Loss Probe
      alarmDuration = std::max ((3 / 2) * m_subflows[pathId]->m_tcb->m_smoothedRtt + m_subflows[pathId]->m_tcb->m_maxAckDelay,
                                m_subflows[pathId]->m_tcb->m_kMinTLPTimeout);
      m_subflows[pathId]->m_tcb->m_alarmType = 2;
    }
  else
    {
      NS_LOG_LOGIC ("RTO");
      alarmDuration = m_subflows[pathId]->m_tcb->m_smoothedRtt + 4 * m_subflows[pathId]->m_tcb->m_rttVar
                    + m_subflows[pathId]->m_tcb->m_maxAckDelay;
      alarmDuration = std::max (alarmDuration, m_subflows[pathId]->m_tcb->m_kMinRTOTimeout);
      alarmDuration = alarmDuration * (2 ^ m_subflows[pathId]->m_tcb->m_rtoCount);
      m_subflows[pathId]->m_tcb->m_alarmType = 3;
    }
  NS_LOG_INFO ("Schedule ReTxTimeout at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + alarmDuration).GetSeconds ());
  NS_LOG_INFO ("Alarm after " << alarmDuration.GetSeconds () << " seconds");
  // pass pathId to &QuicSocketBase::ReTxTimeout
  m_subflows[pathId]->m_tcb->m_lossDetectionAlarm = Simulator::Schedule (alarmDuration, &QuicSocketBase::ReTxTimeout, this, pathId);
  m_subflows[pathId]->m_tcb->m_nextAlarmTrigger = Simulator::Now () + alarmDuration;
}

void
QuicSocketBase::DoRetransmit (std::vector<Ptr<QuicSocketTxItem> > lostPackets, uint8_t pathId)
{
  NS_LOG_FUNCTION (this);
  // Get packets to retransmit
  SequenceNumber32 next = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
  //std::cout << "重传大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
  uint32_t toRetx = m_txBuffer->Retransmission (next, pathId);
    //std::cout << "重传大小变化 (移除数量): " <<m_txBuffer->AppSize ()<< std::endl;

  NS_LOG_INFO (toRetx << " bytes to retransmit");
  NS_LOG_DEBUG ("Send the retransmitted frame");
  uint32_t win = AvailableWindow (pathId);
  uint32_t connWin = ConnectionWindow (pathId);
  uint32_t bytesInFlight = BytesInFlight (pathId);
  NS_LOG_DEBUG ("BEFORE Available Window " << win
                               << " Connection RWnd " << connWin
                               << " BytesInFlight " << bytesInFlight
                               << " BufferedSize " << m_txBuffer->AppSize ()
                               << " MaxPacketSize " << GetSegSize ());

  // Send the retransmitted data
  NS_LOG_INFO ("Retransmitted packet, next sequence number " << m_subflows[pathId]->m_tcb->m_nextTxSequence);
  SendDataPacket (next, toRetx, m_connected,pathId);
}

void
QuicSocketBase::ReTxTimeout (uint8_t pathId)
{
  if (Simulator::Now () < m_subflows[pathId]->m_tcb->m_nextAlarmTrigger)
    {
      NS_LOG_INFO ("Canceled alarm");
      return;
    }
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  
  // Handshake packets are outstanding)
  if (m_subflows[pathId]->m_tcb->m_alarmType == 0 && (m_socketState == CONNECTING_CLT || m_socketState == CONNECTING_SVR))
    {
      // Handshake retransmission alarm.
      //TODO retransmit handshake packets
      //RetransmitAllHandshakePackets();
      m_subflows[pathId]->m_tcb->m_handshakeCount++;
    }
  else if (m_subflows[pathId]->m_tcb->m_alarmType == 1 && m_subflows[pathId]->m_tcb->m_lossTime != Seconds (0))
    {
        //std::cout << "lostPackets大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
      std::vector<Ptr<QuicSocketTxItem> > lostPackets = m_txBuffer->DetectLostPackets (pathId);
        //std::cout << "lostPackets大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
      NS_LOG_INFO ("RTO triggered: early retransmit");
      // Early retransmit or Time Loss Detection.
      if (m_quicCongestionControlLegacy)
        {
          // TCP early retransmit logic [RFC 5827]: enter recovery (RFC 6675, Sec. 5)
          if (m_subflows[pathId]->m_tcb->m_congState != TcpSocketState::CA_RECOVERY)
            {
              m_subflows[pathId]->m_tcb->m_congState = TcpSocketState::CA_RECOVERY;
              m_subflows[pathId]->m_tcb->m_cWnd = m_subflows[pathId]->m_tcb->m_ssThresh;
              m_subflows[pathId]->m_tcb->m_endOfRecovery = m_subflows[pathId]->m_tcb->m_highTxMark;
              m_congestionControl->CongestionStateSet (
                m_subflows[pathId]->m_tcb, TcpSocketState::CA_RECOVERY);
              m_subflows[pathId]->m_tcb->m_ssThresh = m_congestionControl->GetSsThresh (
                m_subflows[pathId]->m_tcb, BytesInFlight (pathId));
            }
        }
      else
        {
          Ptr<QuicCongestionOps> cc = dynamic_cast<QuicCongestionOps*> (&(*m_congestionControl));
          cc->OnPacketsLost (m_subflows[pathId]->m_tcb, lostPackets);////////////////////////////////////////////乘以损失减少因子，通常小于 1，导致 cwnd 减小，例如减半
        }
      // Retransmit all lost packets immediately
      // m_subflows[pathId]->UpdateCwndOnPacketLost();
      DoRetransmit (lostPackets, pathId);
    }
  else if (m_subflows[pathId]->m_tcb->m_alarmType == 2 && m_subflows[pathId]->m_tcb->m_tlpCount < m_subflows[pathId]->m_tcb->m_kMaxTLPs)
    {
      // Tail Loss Probe. Send one new data packet, do not retransmit - IETF Draft QUIC Recovery, Sec. 4.3.2
      SequenceNumber32 next = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
      NS_LOG_INFO ("TLP triggered");
      uint32_t s = std::min (ConnectionWindow (pathId), GetSegSize ());
      // cancel pacing to send packet immediately
      m_pacingTimer.Cancel ();

      SendDataPacket (next, s, m_connected,pathId);
      m_subflows[pathId]->m_tcb->m_tlpCount++;
    }
  else if (m_subflows[pathId]->m_tcb->m_alarmType == 3)
    {
      // RTO.
      if (m_subflows[pathId]->m_tcb->m_rtoCount == 0)
        {
          m_subflows[pathId]->m_tcb->m_largestSentBeforeRto = m_subflows[pathId]->m_tcb->m_highTxMark;
        }
      // RTO. Send two new data packets, do not retransmit - IETF Draft QUIC Recovery, Sec. 4.3.3
      NS_LOG_INFO ("RTO triggered");
      SequenceNumber32 next = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
      uint32_t s = std::min (AvailableWindow (pathId), GetSegSize ());

      // cancel pacing to send packet immediately
      m_pacingTimer.Cancel ();

      SendDataPacket (next, s, m_connected,pathId);
      next = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;

      s = std::min (AvailableWindow (pathId), GetSegSize ());

      // cancel pacing, again
      m_pacingTimer.Cancel ();

      SendDataPacket (next, s, m_connected,pathId);

      m_subflows[pathId]->m_tcb->m_rtoCount++;
    } 
}

uint32_t
QuicSocketBase::AvailableWindow (uint8_t pathId) 
{
  NS_LOG_FUNCTION (this);

  NS_LOG_DEBUG ("m_max_data " << m_max_data << " m_tcb->m_cWnd.Get () " << m_subflows[pathId]->m_tcb->m_cWnd.Get ());
  
  uint32_t win = std::min (m_max_data, m_subflows[pathId]->m_tcb->m_cWnd.Get()); // Number of bytes allowed to be outstanding
  uint32_t inflight = BytesInFlight (pathId);   // Number of outstanding bytes
  // std::cout <<"咱瞅瞅他便不变id："<<this <<"   CWND"<< m_subflows[pathId]->m_tcb->m_cWnd.Get() <<std::endl;

  if (inflight > win)
    {
      NS_LOG_INFO ("InFlight=" << inflight << ", Win=" << win << " availWin=0");
      // std::cout <<"*********************InFlight=" << inflight << ", Win=" << win << " availWin=0" << std::endl;

      return 0;
    }

  NS_LOG_INFO ("InFlight=" << inflight << ", Win=" << win << " availWin=" << win - inflight);
    // std::cout <<"InFlight=" << inflight << ", Win=" << win << " availWin=" << win - inflight<< std::endl;
  return win - inflight;

}

uint32_t
QuicSocketBase::ConnectionWindow (uint8_t pathId)
{
  NS_LOG_FUNCTION (this);

  uint32_t inFlight = BytesInFlight (pathId);

  NS_LOG_INFO ("Returning calculated Connection: MaxData " << m_max_data << " InFlight: " << inFlight);

  return (inFlight > m_max_data) ? 0 : m_max_data - inFlight;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//计算飞行字节
uint32_t
QuicSocketBase::BytesInFlight (uint8_t pathId) 
{
  NS_LOG_FUNCTION (this);
        //std::cout << "lbytesInFlight大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

  uint32_t bytesInFlight = m_txBuffer->BytesInFlight (pathId);
  //  std::cout << "bytesInFlight大小变化 (移除数量): 前面" <<bytesInFlight<< std::endl;

  NS_LOG_INFO ("Returning calculated bytesInFlight: " << bytesInFlight);
  m_subflows[pathId]->m_tcb->m_bytesInFlight = bytesInFlight;
  return bytesInFlight;
}

/* Inherit from Socket class: In QuicSocketBase, it is same as Send() call */
int
QuicSocketBase::SendTo (Ptr<Packet> p, uint32_t flags, const Address &address)
{
  NS_LOG_FUNCTION (this);

  return Send (p, flags);
}

/* Inherit from Socket class: Return data to upper-layer application. Parameter flags
 is not used. Data is returned as a packet of size no larger than maxSize */
Ptr<Packet>
QuicSocketBase::Recv (uint32_t maxSize, uint32_t flags)
{
  NS_LOG_FUNCTION (this);
  NS_ABORT_MSG_IF (flags, "use of flags is not supported in QuicSocketBase::Recv()");

  if (m_rxBuffer->Size () == 0 && m_socketState == CLOSING)
    {
      return Create<Packet> ();
    }
  Ptr<Packet> outPacket = m_rxBuffer->Extract (maxSize);
  return outPacket;
}

/* Inherit from Socket class: Recv and return the remote's address */
Ptr<Packet>
QuicSocketBase::RecvFrom (uint32_t maxSize, uint32_t flags, Address &fromAddress)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet = m_rxBuffer->Extract (maxSize);

  if (packet != nullptr && packet->GetSize () != 0)
    {
      if (m_endPoint != nullptr)
        {
          fromAddress = InetSocketAddress (m_endPoint->GetPeerAddress (), m_endPoint->GetPeerPort ());
        }
      else if (m_endPoint6 != nullptr)
        {
          fromAddress = Inet6SocketAddress (m_endPoint6->GetPeerAddress (), m_endPoint6->GetPeerPort ());
        }
      else
        {
          fromAddress = InetSocketAddress (Ipv4Address::GetZero (), 0);
        }
    }
  return packet;
}

void
QuicSocketBase::ScheduleCloseAndSendConnectionClosePacket ()
{
  m_drainingPeriodEvent.Cancel ();
  NS_LOG_LOGIC (this << " Close Schedule DoClose at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + m_drainingPeriodTimeout.Get ()).GetSeconds ());
   //std::cout<<"等待m_drainingPeriodTimeout"<<std::endl;
  m_drainingPeriodEvent = Simulator::Schedule (m_drainingPeriodTimeout, &QuicSocketBase::DoClose, this);
  SendConnectionClosePacket (0, "Scheduled connection close - no error");
}


int
QuicSocketBase::Close (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO (this << " Close at time " << Simulator::Now ().GetSeconds ());
  // std::cout<<"总调用Close"<<std::endl;

  //std::cout << this << " Close at time " << Simulator::Now ().GetSeconds ()<<std::endl;
  m_receivedTransportParameters = false;
//ypy版本，删除SetState (CLOSING);
  // std::cout<<"m_idleTimeoutEvent.IsRunning () "<<m_idleTimeoutEvent.IsRunning () <<std::endl;
  // if (m_idleTimeoutEvent.IsRunning () and m_socketState != IDLE
  //     and m_socketState != CLOSING)   //Connection Close from application signal
  //   {
  //     if(!m_txBuffer->SentListIsEmpty() ) //这里是ypy改的整个if都改了
  //     {
  //       m_appCloseSentListNoEmpty = true;
  //     } 
  //     else  if(m_txBuffer->SentListIsEmpty() and m_txBuffer->AppSize() == 0)
  //     {
  //       m_appCloseSentListNoEmpty = false;
  //       //std::cout<<"close设置closing1"<<std::endl;
  //       //SetState (CLOSING);
  //       if (m_flushOnClose)
  //         {
  //           m_closeOnEmpty = true;
  //            //std::cout<<"我怀疑就是这里"<<std::endl;
  //         }
  //       else
  //         {
  //           //std::cout<<"进入ScheduleCloseAndSendConnectionClosePacket"<<std::endl;
  //           ScheduleCloseAndSendConnectionClosePacket ();
  //         }
  //     }
  //     else
  //     {
  //        m_appCloseSentListNoEmpty = false;
  //       // SetState (CLOSING);
  //       if (m_flushOnClose)
  //         {
  //           m_closeOnEmpty = true;
  //            //std::cout<<"我怀疑就是这里"<<std::endl;
  //         }
  //       else
  //         {
  //           //std::cout<<"进入ScheduleCloseAndSendConnectionClosePacket"<<std::endl;
  //           ScheduleCloseAndSendConnectionClosePacket ();
  //         }
  //     } 
  //   }
//原版
  // std::cout<<"this"<<this<<"m_idleTimeoutEvent.IsRunning () "<< m_idleTimeoutEvent.IsRunning ()<<"m_socketState！=0||5 "  <<m_socketState <<std::endl;
  if (m_idleTimeoutEvent.IsRunning () and m_socketState != IDLE
      and m_socketState != CLOSING)   //Connection Close from application signal
    {
      // //std::cout<<"好了关键进入 if (m_idleTimeoutEvent.IsRunning () and m_socketState != IDLE      and m_socketState != CLOSING)   "<<std::endl;
      if(!m_txBuffer->SentListIsEmpty()) {
        m_appCloseSentListNoEmpty = true;
        // std::cout<<"好了 m_appCloseSentListNoEmpty = true;  "<<std::endl;
      } else {
        m_appCloseSentListNoEmpty = false;
        // std::cout<<"close设置closing1"<< " Close Schedule DoClose at time " << Simulator::Now ().GetNanoSeconds () <<std::endl;
        SetState (CLOSING);
        if (m_flushOnClose)
          {
            m_closeOnEmpty = true;
          }
        else
          {
            // std::cout<<"从这里进入1"<<std::endl;
            ScheduleCloseAndSendConnectionClosePacket ();
          }
      } 
    }
  else if (m_idleTimeoutEvent.IsExpired () and m_socketState != CLOSING
           and m_socketState != IDLE and m_socketState != LISTENING) //Connection Close due to Idle Period termination
    {
              // std::cout<<"close设置closing2"<< " Close Schedule DoClose at time " << Simulator::Now ().GetNanoSeconds () <<std::endl;

      SetState (CLOSING);
      m_drainingPeriodEvent.Cancel ();
      NS_LOG_LOGIC (
        this << " Close Schedule DoClose at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + m_drainingPeriodTimeout.Get ()).GetSeconds ());
      m_drainingPeriodEvent = Simulator::Schedule (m_drainingPeriodTimeout,
                                                   &QuicSocketBase::DoClose,
                                                   this);
    }
  else if (m_idleTimeoutEvent.IsExpired ()
           and m_drainingPeriodEvent.IsExpired () and m_socketState != CLOSING
           and m_socketState != IDLE) //close last listening sockets
    {
      NS_LOG_LOGIC (this << " Closing listening socket");
      DoClose ();
    }
  else if (m_idleTimeoutEvent.IsExpired ()
           and m_drainingPeriodEvent.IsExpired () and m_socketState == IDLE)
    {
      NS_LOG_LOGIC (this << " Has already been closed");
    }
//  std::cout<<"m_appCloseSentListNoEmpty"<< m_appCloseSentListNoEmpty<<std::endl;
  return 0;
}

/* Send a CONNECTION_CLOSE frame */
uint32_t
QuicSocketBase::SendConnectionClosePacket (uint16_t errorCode, std::string phrase)//这个函数包上一个外壳就约等于ScheduleCloseAndSendConnectionClosePacket
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p = Create<Packet> ();
  SequenceNumber32 packetNumber = ++m_subflows[0]->m_tcb->m_nextTxSequence;

  QuicSubheader qsb = QuicSubheader::CreateConnectionClose (errorCode, phrase.c_str ());
  p->AddHeader (qsb);


  QuicHeader head;

  head = QuicHeader::CreateShort (m_connectionId, packetNumber, !m_omit_connection_id, m_keyPhase);


  NS_LOG_DEBUG ("Send Connection Close packet with header " << head);
  
  head.SetPathId(0);
  // m_subflows[0]->Add(packetNumber);
  //  std::cout<<"第三次出现"<<std::endl;
  m_quicl4->SendPacket (this, p, head);
  m_txTrace (p, head, this);

  return 0;
}

/* Inherit from Socket class: Signal a termination of send */
int
QuicSocketBase::ShutdownSend (void)
{
  NS_LOG_FUNCTION (this);



  return 0;
}

/* Inherit from Socket class: Signal a termination of receive */
int
QuicSocketBase::ShutdownRecv (void)
{
  NS_LOG_FUNCTION (this);

  return 0;
}

void
QuicSocketBase::SetNode (Ptr<Node> node)
{
//NS_LOG_FUNCTION (this);

  m_node = node;
}

Ptr<Node>
QuicSocketBase::GetNode (void) const
{
//NS_LOG_FUNCTION_NOARGS ();

  return m_node;
}

/* Inherit from Socket class: Return local address:port */
int
QuicSocketBase::GetSockName (Address &address) const
{
  NS_LOG_FUNCTION (this);

  return m_quicl4->GetSockName (this, address);
}

int
QuicSocketBase::GetPeerName (Address &address) const
{
  NS_LOG_FUNCTION (this);

  return m_quicl4->GetPeerName (this, address);
}

/* Inherit from Socket class: Get the max number of bytes an app can send */
uint32_t
QuicSocketBase::GetTxAvailable (void) const
{
  NS_LOG_FUNCTION (this);

  return m_txBuffer->Available ();
}

/* Inherit from Socket class: Get the max number of bytes an app can read */
uint32_t
QuicSocketBase::GetRxAvailable (void) const
{
  NS_LOG_FUNCTION (this);

  return m_rxBuffer->Available ();
}

/* Inherit from Socket class: Returns error code */
enum Socket::SocketErrno
QuicSocketBase::GetErrno (void) const
{
  return m_errno;
}

/* Inherit from Socket class: Returns socket type, NS3_SOCK_STREAM */
enum Socket::SocketType
QuicSocketBase::GetSocketType (void) const
{
  return NS3_SOCK_STREAM;
}

//////////////////////////////////////////////////////////////////////////////////////

/* Clean up after Bind. Set up callback functions in the end-point. */
int
QuicSocketBase::SetupCallback (void)
{
  NS_LOG_FUNCTION (this);

  if (m_quicl4 == 0)
    {
      return -1;
    }
  else
    {
      m_quicl4->SetRecvCallback (
        MakeCallback (&QuicSocketBase::ReceivedData, this), this);
    }

  return 0;
}

int
QuicSocketBase::AppendingRx (Ptr<Packet> frame, Address &address)
{

  NS_LOG_FUNCTION (this);

  if (!m_rxBuffer->Add (frame))
    {
      // Insert failed: No data or RX buffer full
      NS_LOG_INFO ("Dropping packet due to full RX buffer");
      return 0;
    }
  else
    {
      NS_LOG_INFO ("Notify Data Recv");
      NotifyDataRecv ();   // trigger the application method
    }

  return frame->GetSize ();
}

void
QuicSocketBase::SetQuicL4 (Ptr<QuicL4Protocol> quic)
{
  NS_LOG_FUNCTION (this);

  m_quicl4 = quic;
}

void
QuicSocketBase::SetConnectionId (uint64_t connectionId)
{
  NS_LOG_FUNCTION_NOARGS ();

  m_connectionId = connectionId;
}

void
QuicSocketBase::InitializeScheduling ()
{
  ObjectFactory schedulerFactory;
  schedulerFactory.SetTypeId (m_schedulingTypeId);
  Ptr<QuicSocketTxScheduler> sched = schedulerFactory.Create<QuicSocketTxScheduler> ();
          // //std::cout << "SetScheduler大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

  m_txBuffer->SetScheduler (sched);
         //std::cout << "SetScheduler大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

  SetDefaultLatency (m_defaultLatency);
}

uint64_t
QuicSocketBase::GetConnectionId (void) const
{
  NS_LOG_FUNCTION_NOARGS ();

  return m_connectionId;
}

void
QuicSocketBase::SetVersion (uint32_t version)
{
  NS_LOG_FUNCTION (this);

  m_vers = version;
  return;
}

//////////////////////////////////////////////////////////////////////////////////////

bool
QuicSocketBase::SetAllowBroadcast (bool allowBroadcast)
{
  NS_LOG_FUNCTION (this);

  return (!allowBroadcast);
}

bool
QuicSocketBase::GetAllowBroadcast (void) const
{
  return false;
}

Ptr<QuicL5Protocol>
QuicSocketBase::CreateStreamController ()
{
  NS_LOG_FUNCTION (this);

  Ptr<QuicL5Protocol> quicl5 = CreateObject<QuicL5Protocol> ();

  quicl5->SetSocket (this);
  quicl5->SetNode (m_node);
  quicl5->SetConnectionId (m_connectionId);

  return quicl5;
}

void
QuicSocketBase::SendInitialHandshake (uint8_t type,
                                      const QuicHeader &quicHeader,
                                      Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << m_vers);

  if (type == QuicHeader::VERSION_NEGOTIATION)
    {
      NS_LOG_INFO ("Create VERSION_NEGOTIATION");
      m_receivedTransportParameters = false;
      m_couldContainTransportParameters = true;

      std::vector<uint32_t> supportedVersions;
      supportedVersions.push_back (QUIC_VERSION);
      supportedVersions.push_back (QUIC_VERSION_DRAFT_10);
      supportedVersions.push_back (QUIC_VERSION_NS3_IMPL);

      uint8_t *buffer = new uint8_t[4 * supportedVersions.size ()];

      Ptr<Packet> payload = Create<Packet> (buffer,
                                            4 * supportedVersions.size ());

      for (uint8_t i = 0; i < (uint8_t) supportedVersions.size (); i++)
        {

          buffer[4 * i] = (supportedVersions[i]);
          buffer[4 * i + 1] = (supportedVersions[i] >> 8);
          buffer[4 * i + 2] = (supportedVersions[i] >> 16);
          buffer[4 * i + 3] = (supportedVersions[i] >> 24);

        }

      Ptr<Packet> p = Create<Packet> (buffer, 4 * supportedVersions.size ());
      QuicHeader head = QuicHeader::CreateVersionNegotiation (
        quicHeader.GetConnectionId (),
        QUIC_VERSION_NEGOTIATION,
        supportedVersions);

      // Set initial congestion window and Ssthresh
      m_subflows[0]->m_tcb->m_cWnd = m_subflows[0]->m_tcb->m_initialCWnd;
      m_subflows[0]->m_tcb->m_ssThresh = m_subflows[0]->m_tcb->m_initialSsThresh;

      //server (receiver)
      head.SetPathId(0);
  //  std::cout<<"第四次出现"<<std::endl;
      m_quicl4->SendPacket (this, p, head);
      m_txTrace (p, head, this);
      NotifyDataSent (p->GetSize ());

    }
  else if (type == QuicHeader::INITIAL)
    {
      //client(sender)
      // Set initial congestion window and Ssthresh
      m_subflows[0]->m_tcb->m_cWnd = m_subflows[0]->m_tcb->m_initialCWnd;
      m_subflows[0]->m_tcb->m_ssThresh = m_subflows[0]->m_tcb->m_initialSsThresh;
      
      NS_LOG_INFO ("Create INITIAL");
      Ptr<Packet> p = Create<Packet> ();
      p->AddHeader (OnSendingTransportParameters ());
      // the RFC says that
      // "Clients MUST ensure that the first Initial packet they
      // send is sent in a UDP datagram that is at least 1200 octets."
      Ptr<Packet> payload = Create<Packet> (
        GetInitialPacketSize () - p->GetSize ());
      p->AddAtEnd (payload);

      m_quicl5->DispatchSend (p, 0);

    }
  else if (type == QuicHeader::RETRY)
    {
      NS_LOG_INFO ("Create RETRY");
      Ptr<Packet> p = Create<Packet> ();
      p->AddHeader (OnSendingTransportParameters ());
      Ptr<Packet> payload = Create<Packet> (
        GetInitialPacketSize () - p->GetSize ());
      p->AddAtEnd (payload);

      m_quicl5->DispatchSend (p, 0);
    }
  else if (type == QuicHeader::HANDSHAKE)
    {
      NS_LOG_INFO ("Create HANDSHAKE");
      Ptr<Packet> p = Create<Packet> ();
      if (m_socketState == CONNECTING_SVR)
        {
          p->AddHeader (OnSendingTransportParameters ());
        }

      Ptr<Packet> payload = Create<Packet> (
        GetInitialPacketSize () - p->GetSize ());
      p->AddAtEnd (payload);

      m_quicl5->DispatchSend (p, 0);
      m_congestionControl->CongestionStateSet (m_subflows[0]->m_tcb,
                                               TcpSocketState::CA_OPEN);
    }
  else if (type == QuicHeader::ZRTT_PROTECTED)
    {
      NS_LOG_INFO ("Create ZRTT_PROTECTED");
      Ptr<Packet> p = Create<Packet> ();
      p->AddHeader (OnSendingTransportParameters ());

      m_quicl5->DispatchSend (p, 0);

    }
  else
    {

      NS_LOG_INFO ("Wrong Handshake Type");

      return;

    }
}

void
QuicSocketBase::OnReceivedFrame (QuicSubheader &sub)
{
  NS_LOG_FUNCTION (this << (uint64_t)sub.GetFrameType ());

  uint8_t frameType = sub.GetFrameType ();
//std::cout<<"都有啥类型的："<<int(frameType)<<std::endl;
  switch (frameType)
    {

      case QuicSubheader::ACK:
        NS_LOG_INFO ("Received ACK frame");
        // OnReceivedAckFrame (sub);
        break;

      case QuicSubheader::CONNECTION_CLOSE:
        NS_LOG_INFO ("Received CONNECTION_CLOSE frame");
          //std::cout<<"我怀疑就是这里3"<<std::endl;

        Close ();//这里调用了close函数
        break;

      case QuicSubheader::APPLICATION_CLOSE:
        NS_LOG_INFO ("Received APPLICATION_CLOSE frame");
        DoClose ();
        break;

      case QuicSubheader::PADDING:
        NS_LOG_INFO ("Received PADDING frame");
        // no need to do anything
        break;

      case QuicSubheader::MAX_DATA:
        // set the maximum amount of data that can be sent
        // on this connection
        NS_LOG_INFO ("Received MAX_DATA frame");
        SetConnectionMaxData (sub.GetMaxData ());
        break;

      case QuicSubheader::MAX_STREAM_ID:
        // TODO update the maximum stream ID
        NS_LOG_INFO ("Received MAX_STREAM_ID frame");
        break;

      case QuicSubheader::PING:
        // TODO
        NS_LOG_INFO ("Received PING frame");
        break;

      case QuicSubheader::BLOCKED:
        // TODO
        NS_LOG_INFO ("Received BLOCKED frame");
        break;

      case QuicSubheader::STREAM_ID_BLOCKED:
        // TODO
        NS_LOG_INFO ("Received STREAM_ID_BLOCKED frame");
        break;

      case QuicSubheader::NEW_CONNECTION_ID:
        // TODO
        NS_LOG_INFO ("Received NEW_CONNECTION_ID frame");
        break;

      case QuicSubheader::PATH_CHALLENGE:
        // TODO reply with a PATH_RESPONSE with the same value
        // as that carried by the PATH_CHALLENGE
        NS_LOG_INFO ("Received PATH_CHALLENGE frame");
        std::cout<<"Received PATH_CHALLENGE OnReceivedPathChallengeFrame " <<std::endl;

        OnReceivedPathChallengeFrame(sub);
        break;

      case QuicSubheader::PATH_RESPONSE:
        // TODO check if it matches what was sent in a PATH_CHALLENGE
        // otherwise abort with a UNSOLICITED_PATH_RESPONSE error
        NS_LOG_INFO ("Received PATH_RESPONSE frame");
        OnReceivedPathResponseFrame(sub);
        break;

      case QuicSubheader::ADD_ADDRESS:
        // TODO check if it matches what was sent in a PATH_CHALLENGE
        // otherwise abort with a UNSOLICITED_PATH_RESPONSE error
        NS_LOG_INFO ("Received ADD_ADDRESS frame");
          std::cout<<"Received ADD_ADDRESS OnReceivedAddAddressFrame " <<std::endl;
        OnReceivedAddAddressFrame (sub);
        break;

      case QuicSubheader::REMOVE_ADDRESS:
        // TODO check if it matches what was sent in a PATH_CHALLENGE
        // otherwise abort with a UNSOLICITED_PATH_RESPONSE error
        NS_LOG_INFO ("Received REMOVE_ADDRESS frame");
        break;

      case QuicSubheader::MP_ACK:
        // TODO check if it matches what was sent in a PATH_CHALLENGE
        // otherwise abort with a UNSOLICITED_PATH_RESPONSE error
        NS_LOG_INFO ("Received MP_ACK frame");
        // //std::cout<<"都没调用怎么进来的?？"<<std::endl;
        OnReceivedAckFrame (sub);
        break;

      default:
        //std::cout<< "OnReceivedFrame调用了AbortConnection"  <<std::endl;
        AbortConnection (
          QuicSubheader::TransportErrorCodes_t::PROTOCOL_VIOLATION,
          "Received Corrupted Frame");
        return;
    }

}

Ptr<Packet>
QuicSocketBase::OnSendingAckFrame (uint8_t pathId)
{
  NS_LOG_FUNCTION (this);

  NS_ABORT_MSG_IF (m_subflows[pathId]->m_receivedPacketNumbers.empty (),
                   " Sending Ack Frame without packets to acknowledge");
  NS_LOG_INFO ("Attach an ACK frame to the packet");

  std::sort (m_subflows[pathId]->m_receivedPacketNumbers.begin (), m_subflows[pathId]->m_receivedPacketNumbers.end (),
             std::greater<SequenceNumber32> ());

  SequenceNumber32 largestAcknowledged = *(m_subflows[pathId]->m_receivedPacketNumbers.begin ());

  uint32_t ackBlockCount = 0;
  std::vector<uint32_t> additionalAckBlocks;
  std::vector<uint32_t> gaps;

  std::vector<SequenceNumber32>::const_iterator curr_rec_it =
    m_subflows[pathId]->m_receivedPacketNumbers.begin ();
  std::vector<SequenceNumber32>::const_iterator next_rec_it =
    m_subflows[pathId]->m_receivedPacketNumbers.begin () + 1;

  for (; next_rec_it != m_subflows[pathId]->m_receivedPacketNumbers.end ();
       ++curr_rec_it, ++next_rec_it)
    {
      if (((*curr_rec_it) - (*next_rec_it) - 1 > 0)
          and ((*curr_rec_it) != (*next_rec_it)))
        {
          //std::clog << "curr " << (*curr_rec_it) << " next " << (*next_rec_it) << " ";
          additionalAckBlocks.push_back ((*next_rec_it).GetValue ());
          gaps.push_back ((*curr_rec_it).GetValue () - 1);
          ackBlockCount++;
        }
      // Limit the number of gaps that are sent in an ACK (older packets have already been retransmitted)
      if (ackBlockCount >= m_maxTrackedGaps)
        {
          break;
        }
    }


  Time delay = Simulator::Now () - m_lastReceived;
  uint64_t ack_delay = delay.GetMicroSeconds ();
  QuicSubheader sub = QuicSubheader::CreateMpAck (
    largestAcknowledged.GetValue (), ack_delay, largestAcknowledged.GetValue (),
    gaps, additionalAckBlocks, pathId);// m_subflows[pathId]->m_receivedSeqNumbers.back().GetValue());

  Ptr<Packet> ackFrame = Create<Packet> ();
  
  ackFrame->AddHeader (sub);

  if (m_subflows[pathId]->m_lastMaxData < m_subflows[pathId]->m_maxDataInterval)
    {
      m_subflows[pathId]->m_lastMaxData++;
    }
  else
    {
      QuicSubheader maxData = QuicSubheader::CreateMaxData (m_quicl5->GetMaxData ());
      ackFrame->AddHeader (maxData);
      m_subflows[pathId]->m_lastMaxData = 0;
    }
  // //std::cout<<"subheader pathid "<<sub.GetPathId()<<"\n";
  return ackFrame;
}
