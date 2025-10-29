
uint32_t
QuicSocketBase::SendDataPacket (SequenceNumber32 packetNumber, uint32_t maxSize, bool withAck, uint8_t pathId)
{
  NS_LOG_FUNCTION (this << packetNumber << maxSize << withAck);
  //std::cout << this << "packetNumber: " << packetNumber << " maxSize:" << maxSize << "withAck: " << withAck << std::endl;
  // maxSize = std::min (m_subflows[pathId]->m_tcb->m_cWnd.Get(), maxSize);

  if (!m_drainingPeriodEvent.IsRunning ())
    {
      m_idleTimeoutEvent.Cancel ();
      NS_LOG_LOGIC (this << " SendDataPacket Schedule Close at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + m_idleTimeout.Get ()).GetSeconds ());
      //  std::cout<<"取消if (!m_drainingPeriodEvent.IsRunning ())   m_idleTimeoutEvent"<<std::endl;

      m_idleTimeoutEvent = Simulator::Schedule (m_idleTimeout, &QuicSocketBase::Close, this);//这里调用了close函数
      // std::cout<<"SendDataPacket  if (!m_drainingPeriodEvent.IsRunning ())   就设置了"<<std::endl;

    }
  else
    {
      NS_LOG_INFO ("Draining period event running");
       //std::cout<<"Draining period event running"<<std::endl;
      return -1;
    }

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
      //std::cout << this << " SendDataPacket - sending packet " << packetNumber.GetValue () << " of size " << maxSize << " at time " << Simulator::Now ().GetSeconds () << std::endl;
      //  std::cout<<"SendDataPacket     if (m_txBuffer->GetNumFrameStream0InBuffer () > 0)设置m_idleTimeoutEvent"<<std::endl;
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

    }


  QuicHeader head;

  if (m_socketState == CONNECTING_SVR)
    {
      m_connected = true;
      head = QuicHeader::CreateHandshake (m_connectionId, m_vers, packetNumber);
    }
  else if (m_socketState == CONNECTING_CLT)
    {
      head = QuicHeader::CreateInitial (m_connectionId, m_vers, packetNumber);
    }
  else if (m_socketState == OPEN)
    {

      
      if (!m_connected and !m_quicl4->Is0RTTHandshakeAllowed ())
        {
          m_connected = true;
          head = QuicHeader::CreateHandshake (m_connectionId, m_vers, packetNumber);
        }
      else if (!m_connected and m_quicl4->Is0RTTHandshakeAllowed ())
        {
          head = QuicHeader::Create0RTT (m_connectionId, m_vers, packetNumber);
          m_connected = true;
          m_keyPhase == QuicHeader::PHASE_ONE ? m_keyPhase = QuicHeader::PHASE_ZERO :
            m_keyPhase = QuicHeader::PHASE_ONE;
        }
      else
        {
          head = QuicHeader::CreateShort (m_connectionId, packetNumber, !m_omit_connection_id, m_keyPhase);
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
         //std::cout<<"下一层发送核心函数"<<std::endl;
  m_quicl4->SendPacket (this, p, head);////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  m_txTrace (p, head, this);
  NotifyDataSent (sz);
  //调试点
  // m_txBuffer->UpdatePacketSent (packetNumber, sz, pathId, m_subflows[pathId]->m_tcb);
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
  std::cout << "重传大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
  uint32_t toRetx = m_txBuffer->Retransmission (next, pathId);
    std::cout << "重传大小变化 (移除数量): " <<m_txBuffer->AppSize ()<< std::endl;

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
        std::cout << "lostPackets大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
      std::vector<Ptr<QuicSocketTxItem> > lostPackets = m_txBuffer->DetectLostPackets (pathId);
        std::cout << "lostPackets大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;
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
          cc->OnPacketsLost (m_subflows[pathId]->m_tcb, lostPackets);
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
  // std::cout <<"*********************InFlight=" << inflight << ", Win////m_cWnd=" << win << "时间"<< Simulator::Now().GetNanoSeconds() << " ns - 被调用了" <<std::endl;

  if (inflight > win)
    {
      NS_LOG_INFO ("InFlight=" << inflight << ", Win=" << win << " availWin=0");
      // //std::cout <<"*********************InFlight=" << inflight << ", Win=" << win << " availWin=0" << std::endl;

      return 0;
    }

  NS_LOG_INFO ("InFlight=" << inflight << ", Win=" << win << " availWin=" << win - inflight);
  // //std::cout <<"InFlight=" << inflight << ", Win=" << win << " availWin=" << win - inflight<< std::endl;
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
        std::cout << "lbytesInFlight大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

  uint32_t bytesInFlight = m_txBuffer->BytesInFlight (pathId);
        std::cout << "bytesInFlight大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

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
    std::cout<<"m_idleTimeoutEvent.IsRunning () "<< m_idleTimeoutEvent.IsRunning ()<<"m_socketState！=0||5 "  <<m_socketState <<std::endl;
  if (m_idleTimeoutEvent.IsRunning () and m_socketState != IDLE
      and m_socketState != CLOSING)   //Connection Close from application signal
    {
      // std::cout<<"好了关键进入 if (m_idleTimeoutEvent.IsRunning () and m_socketState != IDLE      and m_socketState != CLOSING)   "<<std::endl;
      if(!m_txBuffer->SentListIsEmpty()) {
        m_appCloseSentListNoEmpty = true;
              // std::cout<<"好了 m_appCloseSentListNoEmpty = true;  "<<std::endl;
      } else {
        m_appCloseSentListNoEmpty = false;
        //std::cout<<"close设置closing1"<< " Close Schedule DoClose at time " << Simulator::Now ().GetNanoSeconds () <<std::endl;
        SetState (CLOSING);
        if (m_flushOnClose)
          {
            m_closeOnEmpty = true;
          }
        else
          {
            ScheduleCloseAndSendConnectionClosePacket ();
          }
      } 
    }
  else if (m_idleTimeoutEvent.IsExpired () and m_socketState != CLOSING
           and m_socketState != IDLE and m_socketState != LISTENING) //Connection Close due to Idle Period termination
    {
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
          // std::cout << "SetScheduler大小变化 (移除数量): 前面" << std::endl;

  m_txBuffer->SetScheduler (sched);
         std::cout << "SetScheduler？？？大小变化 (移除数量): 前面" <<m_txBuffer->AppSize ()<< std::endl;

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
std::cout<<"都有啥类型的："<<int(frameType)<<std::endl;
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
        // std::cout<<"都没调用怎么进来的?？"<<std::endl;
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

void
QuicSocketBase::OnReceivedAckFrame (QuicSubheader &sub)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Process ACK");

  std::cout << " OnReceivedAckFrame刚进来 " << m_txBuffer->AppSize() << " 字节在缓冲区中。" << std::endl;

  uint8_t pathId = sub.GetPathId();

   // Generate RateSample
  struct RateSample * rs = m_txBuffer->GetRateSample ();
  rs->m_priorInFlight = m_subflows[pathId]->m_tcb->m_bytesInFlight.Get ();

  uint32_t lostOut = m_txBuffer->GetLost (pathId);
  uint32_t delivered = m_subflows[pathId]->m_tcb->m_delivered;

  uint32_t previousWindow = m_txBuffer->BytesInFlight (pathId);

  std::vector<uint32_t> additionalAckBlocks = sub.GetAdditionalAckBlocks ();
  std::vector<uint32_t> gaps = sub.GetGaps ();
  uint32_t largestAcknowledged = sub.GetLargestAcknowledged ();
  m_subflows[pathId]->m_tcb->m_lastAckedSeq = largestAcknowledged;
  uint32_t ackBlockCount = sub.GetAckBlockCount ();

  
  NS_ABORT_MSG_IF (
    ackBlockCount != additionalAckBlocks.size ()
    and ackBlockCount != gaps.size (),
    "Received Corrupted Ack Frame.");

  std::vector<Ptr<QuicSocketTxItem> > ackedPackets = m_txBuffer->OnAckUpdate (
    m_subflows[pathId]->m_tcb, largestAcknowledged, additionalAckBlocks, gaps, pathId);

  

  // Count newly acked bytes
  uint32_t ackedBytes = previousWindow - m_txBuffer->BytesInFlight (pathId);
//调试点
  // m_txBuffer->GenerateRateSample (m_subflows[pathId]->m_tcb);
  rs->m_packetLoss = std::abs ((int) lostOut - (int) m_txBuffer->GetLost (pathId));
  m_subflows[pathId]->m_tcb->m_lastAckedSackedBytes = m_subflows[pathId]->m_tcb->m_delivered - delivered;
  // RTO packet acknowledged - IETF Draft QUIC Recovery, Sec. 4.3.3
  if (m_subflows[pathId]->m_tcb->m_rtoCount > 0)
    {
      // Packets after the RTO have been acknowledged
      if (m_subflows[pathId]->m_tcb->m_largestSentBeforeRto.GetValue () < largestAcknowledged)
        {
          uint32_t newPackets = (largestAcknowledged
                                 - m_subflows[pathId]->m_tcb->m_largestSentBeforeRto.GetValue ()) / GetSegSize ();
          uint32_t inFlightBeforeRto = m_txBuffer->BytesInFlight (pathId);
          std::cout<<"meiyong"<<newPackets<<std::endl;
          //调试点
          // m_txBuffer->ResetSentList (pathId, newPackets);
          std::vector<Ptr<QuicSocketTxItem> > lostPackets =
          
            m_txBuffer->DetectLostPackets (pathId);
          if (m_quicCongestionControlLegacy && !lostPackets.empty ())
            {
              // Reset congestion window and go into loss mode
                   //std::cout<<"m_kMinimumWindow大小："<<m_subflows[pathId]->m_tcb->m_kMinimumWindow<<std::endl;
              m_subflows[pathId]->m_tcb->m_cWnd = m_subflows[pathId]->m_tcb->m_kMinimumWindow;
              m_subflows[pathId]->m_tcb->m_endOfRecovery = m_subflows[pathId]->m_tcb->m_highTxMark;
              m_subflows[pathId]->m_tcb->m_ssThresh = m_congestionControl->GetSsThresh (
                m_subflows[pathId]->m_tcb, inFlightBeforeRto);
              m_subflows[pathId]->m_tcb->m_congState = TcpSocketState::CA_LOSS;
              m_congestionControl->CongestionStateSet (
                m_subflows[pathId]->m_tcb, TcpSocketState::CA_LOSS);
            }
        }
      else
        {
          m_subflows[pathId]->m_tcb->m_rtoCount = 0;
        }
    }

  // Tail loss probe packet acknowledged - IETF Draft QUIC Recovery, Sec. 4.3.2
  if (m_subflows[pathId]->m_tcb->m_tlpCount > 0 && !ackedPackets.empty ())
    {
      m_subflows[pathId]->m_tcb->m_tlpCount = 0;
    }

  // Find lost packets
  std::vector<Ptr<QuicSocketTxItem> > lostPackets = m_txBuffer->DetectLostPackets (pathId);
  std::cout << " OnReceivedAckFrame结束了 " << m_txBuffer->AppSize() << " 字节在缓冲区中。" << std::endl;

  if (m_appCloseSentListNoEmpty && m_txBuffer->SentListIsEmpty()){//m_appCloseSentListNoEmpty表示"应用层已请求关闭连接，但当时发送列表
    // std::cout<<"我怀疑就是这里4"<<std::endl;
    Close();//这里调用了close函数
  }


  // Recover from losses
  if (!lostPackets.empty ())
    {
      if (m_quicCongestionControlLegacy)
        {
          //Enter recovery (RFC 6675, Sec. 5)
          if (m_subflows[pathId]->m_tcb->m_congState != TcpSocketState::CA_RECOVERY)
            {
              m_subflows[pathId]->m_tcb->m_congState = TcpSocketState::CA_RECOVERY;
              m_subflows[pathId]->m_tcb->m_endOfRecovery = m_subflows[pathId]->m_tcb->m_highTxMark;
              m_congestionControl->CongestionStateSet (
                m_subflows[pathId]->m_tcb, TcpSocketState::CA_RECOVERY);
              m_subflows[pathId]->m_tcb->m_ssThresh = m_congestionControl->GetSsThresh (
                m_subflows[pathId]->m_tcb, BytesInFlight (pathId));
              m_subflows[pathId]->m_tcb->m_cWnd = m_subflows[pathId]->m_tcb->m_ssThresh;
            }
          NS_ASSERT (m_subflows[pathId]->m_tcb->m_congState == TcpSocketState::CA_RECOVERY);
        }
      else
        {
          DynamicCast<QuicCongestionOps> (m_congestionControl)->OnPacketsLost (
            m_subflows[pathId]->m_tcb, lostPackets);
        }
      DoRetransmit (lostPackets,pathId);
    }
  /* else */ 
  if (ackedBytes > 0)
    {
      Ptr<QuicSocketTxItem> lastAcked = ackedPackets.at (0); 
      if (!m_quicCongestionControlLegacy)
        {
          NS_LOG_INFO ("Update the variables in the congestion control (QUIC)");
          // Process the ACK
          if(m_enableMultipath && m_ccType == OLIA)
          {
            //std::cout<<"OLIA拥塞控制"<<std::endl;
            m_subflows[pathId]->m_tcb->m_bytesBeforeLost2 += ackedBytes;
            double alpha = GetOliaAlpha(pathId);
            double sum_rate = 0;
            for (uint16_t pid = 0; pid < m_subflows.size(); pid++)
            {
              sum_rate += m_subflows[pid]->GetRate();
            }
            // DynamicCast<QuicCongestionOps> (m_congestionControl)->OnAckReceived (m_subflows[pathId]->m_tcb, sub, ackedPackets, rs);

            DynamicCast<MpQuicCongestionOps> (m_congestionControl)->OnAckReceived (m_subflows[pathId]->m_tcb, sub, ackedPackets, rs, alpha, sum_rate);
          }
          else
          {
            //std::cout<<"quicNEWreno拥塞控制"<<std::endl;
            DynamicCast<QuicCongestionOps> (m_congestionControl)->OnAckReceived (m_subflows[pathId]->m_tcb, sub, ackedPackets, rs);
          
          }

          m_lastRtt = m_subflows[pathId]->m_tcb->m_lastRtt;
        }
      else
        {
          uint32_t ackedSegments = ackedBytes / GetSegSize ();

          NS_LOG_INFO ("Update the variables in the congestion control (legacy), ackedBytes "
                       << ackedBytes << " ackedSegments " << ackedSegments);
          // new acks are ordered from the highest packet number to the smalles
          

          NS_LOG_LOGIC ("Updating RTT estimate");
          // If the largest acked is newly acked, update the RTT.
          if (lastAcked->m_packetNumber >= m_subflows[pathId]->m_tcb->m_largestAckedPacket)
            {
              Time ackDelay = MicroSeconds (sub.GetAckDelay ());
              m_subflows[pathId]->m_tcb->m_lastRtt = Now () - lastAcked->m_lastSent - ackDelay;
              m_lastRtt = m_subflows[pathId]->m_tcb->m_lastRtt;
            }
          if (m_subflows[pathId]->m_tcb->m_congState != TcpSocketState::CA_RECOVERY
              && m_subflows[pathId]->m_tcb->m_congState != TcpSocketState::CA_LOSS)
            {
              // Increase the congestion window
              m_congestionControl->PktsAcked (m_subflows[pathId]->m_tcb, ackedSegments,
                                              m_subflows[pathId]->m_tcb->m_lastRtt);
              m_congestionControl->IncreaseWindow (m_subflows[pathId]->m_tcb, ackedSegments);
            }
          else
            {
              if (m_subflows[pathId]->m_tcb->m_endOfRecovery.GetValue () > largestAcknowledged)
                {
                  m_congestionControl->PktsAcked (m_subflows[pathId]->m_tcb, ackedSegments,
                                                  m_subflows[pathId]->m_tcb->m_lastRtt);
                  m_congestionControl->IncreaseWindow (m_subflows[pathId]->m_tcb, ackedSegments);
                }
              else
                {
                  m_subflows[pathId]->m_tcb->m_congState = TcpSocketState::CA_OPEN;
                  m_congestionControl->PktsAcked (m_subflows[pathId]->m_tcb, ackedSegments, m_subflows[pathId]->m_tcb->m_lastRtt);
                  m_congestionControl->CongestionStateSet (m_subflows[pathId]->m_tcb, TcpSocketState::CA_OPEN);
                }
            }
        }
      m_scheduler->PeekabooReward(pathId, lastAckTime);
      lastAckTime = Now();
    }
  else
    {
      NS_LOG_INFO ("Received an ACK to ack an ACK");
    }

  // notify the application that more data can be sent
  if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }


     //std::cout<<"还能进这里OnReceivedAckFrame"<<std::endl;

  // try to send more data
  SendPendingData (m_connected);

  // Compute timers
  SetReTxTimeout (pathId);
}

QuicTransportParameters
QuicSocketBase::OnSendingTransportParameters ()
{
  NS_LOG_FUNCTION (this);

  QuicTransportParameters transportParameters;
  transportParameters = transportParameters.CreateTransportParameters (
    m_initial_max_stream_data, m_max_data, m_initial_max_stream_id_bidi,
    (uint16_t) m_idleTimeout.Get ().GetSeconds (),
    (uint8_t) m_omit_connection_id, m_subflows[0]->m_tcb->m_segmentSize,
    m_ack_delay_exponent, m_initial_max_stream_id_uni);

  return transportParameters;
}

void
QuicSocketBase::OnReceivedTransportParameters (
  QuicTransportParameters transportParameters)
{
  NS_LOG_FUNCTION (this);

  if (m_receivedTransportParameters)
    {
      //std::cout<< "OnReceivedTransportParameters111调用了AbortConnection"  <<std::endl;
      AbortConnection (
        QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR,
        "Duplicate transport parameters reception");
      return;
    }
  m_receivedTransportParameters = true;

// TODO: A client MUST NOT include a stateless reset token. A server MUST treat receipt of a stateless_reset_token_transport
//   parameter as a connection error of type TRANSPORT_PARAMETER_ERROR

  uint32_t mask = transportParameters.GetInitialMaxStreamIdBidi ()
    & 0x00000003;
  if ((mask == 0) && m_socketState != CONNECTING_CLT)
    {
      // TODO AbortConnection(QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR, "Invalid Initial Max Stream Id Bidi value provided from Server");
      return;
    }
  else if ((mask == 1) && m_socketState != CONNECTING_SVR)
    {
      // TODO AbortConnection(QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR, "Invalid Initial Max Stream Id Bidi value provided from Client");
      return;
    }

  mask = transportParameters.GetInitialMaxStreamIdUni () & 0x00000003;
  if ((mask == 2) && m_socketState != CONNECTING_CLT)
    {
      // TODO AbortConnection(QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR, "Invalid Initial Max Stream Id Uni value provided from Server");
      return;
    }
  else if ((mask == 3) && m_socketState != CONNECTING_SVR)
    {
      // TODO AbortConnection(QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR, "Invalid Initial Max Stream Id Uni value provided from Client");
      return;
    }

  if (transportParameters.GetMaxPacketSize ()
      < QuicSocketBase::MIN_INITIAL_PACKET_SIZE
      or transportParameters.GetMaxPacketSize () > 65527)
    {
      //std::cout<< "OnReceivedTransportParameters222调用了AbortConnection"  <<std::endl;
      AbortConnection (
        QuicSubheader::TransportErrorCodes_t::TRANSPORT_PARAMETER_ERROR,
        "Invalid Max Packet Size value provided");
      return;
    }


  NS_LOG_DEBUG (
    "Before applying received transport parameters " << " m_initial_max_stream_data " << m_initial_max_stream_data << " m_max_data " << m_max_data << " m_initial_max_stream_id_bidi " << m_initial_max_stream_id_bidi << " m_idleTimeout " << m_idleTimeout << " m_omit_connection_id " << m_omit_connection_id << " m_tcb->m_segmentSize " << m_subflows[0]->m_tcb->m_segmentSize << " m_ack_delay_exponent " << m_ack_delay_exponent << " m_initial_max_stream_id_uni " << m_initial_max_stream_id_uni);

  m_initial_max_stream_data = std::min (
    transportParameters.GetInitialMaxStreamData (),
    m_initial_max_stream_data);
  m_quicl5->UpdateInitialMaxStreamData (m_initial_max_stream_data);

  m_max_data = std::min (transportParameters.GetInitialMaxData (),
                         m_max_data);

  m_initial_max_stream_id_bidi = std::min (
    transportParameters.GetInitialMaxStreamIdBidi (),
    m_initial_max_stream_id_bidi);

  m_idleTimeout = Time (
    std::min (transportParameters.GetIdleTimeout (),
              (uint16_t) m_idleTimeout.Get ().GetSeconds ()) * 1e9);

  m_omit_connection_id = std::min (transportParameters.GetOmitConnection (),
                                   (uint8_t) m_omit_connection_id);

  SetSegSize (
    std::min ((uint32_t) transportParameters.GetMaxPacketSize (),
              m_subflows[0]->m_tcb->m_segmentSize));

  m_ack_delay_exponent = std::min (transportParameters.GetAckDelayExponent (),
                                   m_ack_delay_exponent);

  m_initial_max_stream_id_uni = std::min (
    transportParameters.GetInitialMaxStreamIdUni (),
    m_initial_max_stream_id_uni);

  NS_LOG_DEBUG (
    "After applying received transport parameters " << " m_initial_max_stream_data " << m_initial_max_stream_data << " m_max_data " << m_max_data << " m_initial_max_stream_id_bidi " << m_initial_max_stream_id_bidi << " m_idleTimeout " << m_idleTimeout << " m_omit_connection_id " << m_omit_connection_id << " m_tcb->m_segmentSize " << m_subflows[0]->m_tcb->m_segmentSize << " m_ack_delay_exponent " << m_ack_delay_exponent << " m_initial_max_stream_id_uni " << m_initial_max_stream_id_uni);
}

int
QuicSocketBase::DoConnect (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socketState != IDLE and m_socketState != QuicSocket::LISTENING)
    {
      //m_errno = ERROR_INVAL;
      return -1;
    }

  if (m_socketState == LISTENING)
    {
      SetState (CONNECTING_SVR);
    }
  else if (m_socketState == IDLE)
    {
      SetState (CONNECTING_CLT);
      QuicHeader q;
      SendInitialHandshake (QuicHeader::INITIAL, q, 0);
    }
  return 0;
}

int
QuicSocketBase::DoFastConnect (void)
{
  NS_LOG_FUNCTION (this);
  NS_ABORT_MSG_IF (!IsVersionSupported (m_vers),
                   "0RTT Handshake requested with wrong Initial Version");

  if (m_socketState != IDLE)
    {
      //m_errno = ERROR_INVAL;
      return -1;
    }

  else if (m_socketState == IDLE)
    {
      SetState (OPEN);
      Simulator::ScheduleNow (&QuicSocketBase::ConnectionSucceeded, this);
      m_congestionControl->CongestionStateSet (m_subflows[0]->m_tcb,
                                               TcpSocketState::CA_OPEN);
      QuicHeader q;
      SendInitialHandshake (QuicHeader::ZRTT_PROTECTED, q, 0);
    }
  return 0;
}

void
QuicSocketBase::ConnectionSucceeded ()
{ // Wrapper to protected function NotifyConnectionSucceeded() so that it can
  // be called as a scheduled event
  NotifyConnectionSucceeded ();
  // The if-block below was moved from ProcessSynSent() to here because we need
  // to invoke the NotifySend() only after NotifyConnectionSucceeded() to
  // reflect the behaviour in the real world.
  if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }
}

int
QuicSocketBase::DoClose (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO (this << " DoClose at time " << Simulator::Now ().GetSeconds ());
//std::cout<<"执行了DoClose"<<std::endl;
  if (m_socketState != IDLE)
    {
      SetState (IDLE);
    }

  SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  return m_quicl4->RemoveSocket (this);
}


void
QuicSocketBase::ReceivedData (Ptr<Packet> p, const QuicHeader& quicHeader,
                              Address &address)
{
  NS_LOG_FUNCTION (this);
  m_rxTrace (p, quicHeader, this);

  //For multipath Implementation
  uint8_t pathId = quicHeader.GetPathId();
  m_currentPathId = pathId;//更新当前路径ID成员变量m_currentPathId，记录这个包来自哪个路径。
  m_currentFromAddress = address;//发送方的地址（用于IPv4/IPv6等）。
  // //std::cout<< "发送方的地址==address"  <<address<<std::endl;

  NS_LOG_INFO ("Received packet of size " << p->GetSize ());
  //如果不在排水期，则进入if块重置空闲超时；否则，直接返回丢弃包。
  if (!m_drainingPeriodEvent.IsRunning ())
    {
      m_idleTimeoutEvent.Cancel ();   // 每次发送数据包（SendDataPacket被调用时），都会重置这个定时器。意思是“只要有数据发送，就延长空闲超时”。
      NS_LOG_LOGIC (
        this << " ReceivedData Schedule Close at time " << Simulator::Now ().GetSeconds () << " to expire at time " << (Simulator::Now () + m_idleTimeout.Get ()).GetSeconds ());
        // std::cout<<"ReceivedData取消设置   if (!m_drainingPeriodEvent.IsRunning ())   m_idleTimeoutEvent"<<std::endl;

        m_idleTimeoutEvent = Simulator::Schedule (m_idleTimeout, &QuicSocketBase::Close, this);//这句原来没注释
        // std::cout<<"ReceivedData启动 if (!m_drainingPeriodEvent.IsRunning ())   m_idleTimeoutEvent"<<std::endl;
       //std::cout<<"执行完receivedata的close    m_socketState ==   "<<m_socketState<<std::endl;

    }
  else   // If the socket is in Draining Period, discard the packets
    {
      return;
    }

  int onlyAckFrames = 0;//用于标记包是否只包含ACK帧
  bool unsupportedVersion = false;//标记版本是否不支持
//检查头部是否为0-RTT（零往返时间）保护包，且socket处于LISTENING状态（服务器监听中）////////////////////////////////////////////////////////////////////////////////////////////////
  if (quicHeader.IsORTT () and m_socketState == LISTENING)
    {
  //std::cout<< "接收m_socketState==LISTENING"  <<std::endl;

      if (m_serverBusy)
        {
          //如果服务器忙碌（m_serverBusy为true），调用AbortConnection中止连接，发送错误码"SERVER_BUSY"并返回。防止服务器过载。
          //std::cout<< "quicHeader.IsORTT ()调用了AbortConnection"  <<std::endl;
          AbortConnection (QuicSubheader::TransportErrorCodes_t::SERVER_BUSY,
                           "Server too busy to accept new connections");
          return;
        }

      m_couldContainTransportParameters = true;
    //调用m_quicl5->DispatchRecv分派接收数据（m_quicl5是QuicL5Protocol对象，处理应用层）。返回值为onlyAckFrames
      onlyAckFrames = m_quicl5->DispatchRecv (p, address);
      //将接收到的包号添加到对应子流的m_receivedPacketNumbers向量中，用于跟踪和生成ACK帧
      m_subflows[pathId]->m_receivedPacketNumbers.push_back (quicHeader.GetPacketNumber ());

      m_connected = true;
      //切换密钥阶段（key phase），用于加密保护。QUIC使用密钥阶段来轮换加密密钥。如果当前是PHASE_ONE，则切换到PHASE_ZERO，反之亦然。
      m_keyPhase == QuicHeader::PHASE_ONE ? m_keyPhase =
        QuicHeader::PHASE_ZERO :
        m_keyPhase =
          QuicHeader::PHASE_ONE;
          //将socket状态设置为OPEN（打开），表示连接就绪可传输数据。
      SetState (OPEN);
      //立即调度ConnectionSucceeded函数，通知上层连接成功（可能触发回调）。
      Simulator::ScheduleNow (&QuicSocketBase::ConnectionSucceeded, this);
      m_congestionControl->CongestionStateSet (m_subflows[pathId]->m_tcb,TcpSocketState::CA_OPEN);
      m_couldContainTransportParameters = false;

    }
//检查头部是否为INITIAL（初始握手包），且socket处于CONNECTING_SVR（服务器连接中）。这是服务器处理客户端初始握手的逻辑。////////////////////////////////////////////////////////////////////////////////////////////////
  else if (quicHeader.IsInitial () and m_socketState == CONNECTING_SVR)
    {
        //std::cout<< "接收m_socketState==CONNECTING_SVR  &  IsInitial"  <<std::endl;

      NS_LOG_INFO ("Server receives INITIAL");
      if (m_serverBusy)
        {
          //std::cout<< "quicHeader.IsInitial () 调用了AbortConnection"  <<std::endl;
          AbortConnection (QuicSubheader::TransportErrorCodes_t::SERVER_BUSY,
                           "Server too busy to accept new connections");//同上，如果服务器忙碌，中止连接。
          return;
        }
        //检查初始包大小是否小于最小要求（1200字节，QUIC规范要求以防止放大攻击）。如果太小，构建错误消息并中止连接（协议违规）。
      if (p->GetSize () < QuicSocketBase::MIN_INITIAL_PACKET_SIZE)
        {
          std::stringstream error;
          error << "Initial Packet smaller than "
                << QuicSocketBase::MIN_INITIAL_PACKET_SIZE << " octects";
          //std::cout<< "p->GetSize () < QuicSocketBase::MIN_INITIAL_PACKET_SIZE 调用了AbortConnection"  <<std::endl;
          AbortConnection (QuicSubheader::TransportErrorCodes_t::PROTOCOL_VIOLATION,
            error.str ().c_str ());
          return;
        }
       // 同上，分派接收数据。
      onlyAckFrames = m_quicl5->DispatchRecv (p, address);
      //同上，记录包号。
      m_subflows[pathId]->m_receivedPacketNumbers.push_back (quicHeader.GetPacketNumber ());
        //检查版本是否支持（调用IsVersionSupported）。如果支持，重置参数标志，并发送握手响应（HANDSHAKE类型）。
      if (IsVersionSupported (quicHeader.GetVersion ()))
        {
          m_couldContainTransportParameters = false;
          SendInitialHandshake (QuicHeader::HANDSHAKE, quicHeader, p);
          
        }
      else//如果版本不支持，设置unsupportedVersion为true，并发送版本协商包。
        {
          NS_LOG_INFO (this << " WRONG VERSION " << quicHeader.GetVersion ());
          unsupportedVersion = true;
          SendInitialHandshake (QuicHeader::VERSION_NEGOTIATION, quicHeader,p);
        }
      
      return;
    }
//检查头部是否为HANDSHAKE（握手包），且socket处于CONNECTING_CLT（客户端连接中）。这是客户端处理服务器握手响应的逻辑。注释提到传输参数接收可能导致未定义行为。
  else if (quicHeader.IsHandshake () and m_socketState == CONNECTING_CLT)   // Undefined compiler behaviour if i try to receive transport parameters
    {
      //std::cout<< "接收m_socketState==CONNECTING_CLT  &  IsHandshake"  <<std::endl;

      NS_LOG_INFO ("Client receives HANDSHAKE");

      onlyAckFrames = m_quicl5->DispatchRecv (p, address);
      m_subflows[pathId]->m_receivedPacketNumbers.push_back (quicHeader.GetPacketNumber ());

      SetState (OPEN);
      Simulator::ScheduleNow(&QuicSocketBase::ConnectionSucceeded, this);
      m_congestionControl->CongestionStateSet (m_subflows[pathId]->m_tcb,
                                               TcpSocketState::CA_OPEN);
      m_couldContainTransportParameters = false;

      SendInitialHandshake (QuicHeader::HANDSHAKE, quicHeader, p);

      return;
    }
//服务器接收握手包，且处于CONNECTING_SVR状态。
  else if (quicHeader.IsHandshake () and m_socketState == CONNECTING_SVR)
    {
      //std::cout<< "接收m_socketState==CONNECTING_SVR  &  IsHandshake"  <<std::endl;

      NS_LOG_INFO ("Server receives HANDSHAKE");

      //For multipath implementation
      CreateNewSubflows();

      onlyAckFrames = m_quicl5->DispatchRecv (p, address);
      m_subflows[pathId]->m_receivedPacketNumbers.push_back (quicHeader.GetPacketNumber ());
      SetState (OPEN);
      Simulator::ScheduleNow (&QuicSocketBase::ConnectionSucceeded, this);
      m_congestionControl->CongestionStateSet (m_subflows[pathId]->m_tcb,TcpSocketState::CA_OPEN);
      //发送挂起的待发数据（带ACK）。
      //std::cout<<"还能进这里ReceivedData"<<std::endl;
      SendPendingData (true);

      return;
    }
//客户端接收版本协商包，且处于CONNECTING_CLT状态。
  else if (quicHeader.IsVersionNegotiation () and m_socketState == CONNECTING_CLT)
    {
      //std::cout<< "接收m_socketState==CONNECTING_CLT  &  IsVersionNegotiation"  <<std::endl;

      NS_LOG_INFO ("Client receives VERSION_NEGOTIATION");
      //分配缓冲区并复制包数据，用于解析版本列表。
      uint8_t *buffer = new uint8_t[p->GetSize ()];
      p->CopyData (buffer, p->GetSize ());
        //解析包数据，每4字节一个版本号，转换为uint32_t并存入向量receivedVersions（大端序转换）。
      std::vector<uint32_t> receivedVersions;
      for (uint8_t i = 0; i < p->GetSize (); i = i + 4)
        {
          receivedVersions.push_back (
            buffer[i] + (buffer[i + 1] << 8) + (buffer[i + 2] << 16)
            + (buffer[i + 3] << 24));
        }

      std::vector<uint32_t> supportedVersions;
      supportedVersions.push_back (QUIC_VERSION);
      supportedVersions.push_back (QUIC_VERSION_DRAFT_10);
      supportedVersions.push_back (QUIC_VERSION_NS3_IMPL);

      uint32_t foundVersion = 0;
      for (uint8_t i = 0; i < receivedVersions.size (); i++)
        {
          for (uint8_t j = 0; j < supportedVersions.size (); j++)
            {
              if (receivedVersions[i] == supportedVersions[j])
                {
                  foundVersion = receivedVersions[i];
                }
            }
        }
        //在接收版本中查找匹配的支持版本。如果找到，设置foundVersion
      if (foundVersion != 0)
        {
          NS_LOG_INFO ("A matching supported version is found " << foundVersion << " re-send initial");
          m_vers = foundVersion;
          SendInitialHandshake (QuicHeader::INITIAL, quicHeader, p);
        }
      else
        {
          //std::cout<< "quicHeader.IsVersionNegotiation () 调用了AbortConnection"  <<std::endl;
          AbortConnection (
            QuicSubheader::TransportErrorCodes_t::VERSION_NEGOTIATION_ERROR,
            "No supported Version found by the Client");
          return;
        }
      return;
    }
  //注释表示这里可能需要处理ACK（待办）。提到如果包只含ACK，不能显式ACK它，并检查延迟ACK。
  else if (quicHeader.IsShort () and m_socketState == OPEN)
    {
       //std::cout<< "接收m_socketState==OPEN  &  IsShort"  <<std::endl;

      // TODOACK here?
      // we need to check if the packet contains only an ACK frame
      // in this case we cannot explicitely ACK it!
      // check if delayed ACK is used
      
      m_subflows[pathId]->m_receivedPacketNumbers.push_back (quicHeader.GetPacketNumber ());
      onlyAckFrames = m_quicl5->DispatchRecv (p, address);

    }
  //如果状态为CLOSING（关闭中）。
  else if (m_socketState == CLOSING)
    {
     //std::cout<< "接收m_socketState==CLOSING "  <<std::endl;
      AbortConnection (m_transportErrorCode,
                       "Received packet in Closing state");
    }
  //
  else
    {
                  //std::cout<< "接收m_socketState==else "  <<std::endl;

      return;
    }

  // trigger the process for ACK handling if the received packet was not ACK only
  NS_LOG_DEBUG ("onlyAckFrames " << onlyAckFrames << " unsupportedVersion " << unsupportedVersion);
  if (onlyAckFrames == 1 && !unsupportedVersion)
    {
      m_lastReceived = Simulator::Now ();
      NS_LOG_DEBUG ("Call MaybeQueueAck");
      MaybeQueueAck (pathId);
    }

}

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
