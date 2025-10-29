/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 SIGNET Lab, Department of Information Engineering, University of Padova
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
 *          Umberto Paro <umberto.paro@me.com>
 *          Wenjun Yang <wenjunyang@uvic.ca>
 *          Shengjie Shu <shengjies@uvic.ca>
 *
 */
/*
 #define NS_LOG_APPEND_CONTEXT \
  if (m_node and m_connectionId) { //std::clog << " [node " << m_node->GetId () << " socket " << m_connectionId << "] "; }
*/

#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"
#include "quic-socket-base.h"
#include "quic-congestion-ops.h"
#include "ns3/tcp-congestion-ops.h"
#include "quic-header.h"
#include "quic-l4-protocol.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/ipv6-end-point.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/tcp-header.h"
#include "ns3/tcp-option-winscale.h"
#include "ns3/tcp-option-ts.h"
#include "ns3/tcp-option-sack-permitted.h"
#include "ns3/tcp-option-sack.h"
#include "ns3/rtt-estimator.h"
#include "quic-socket-tx-edf-scheduler.h"
#include <math.h>
#include <algorithm>
#include <vector>
#include <sstream>
#include <ns3/core-module.h>

#include "mp-quic-scheduler.h"
#include "mp-quic-congestion-ops.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("QuicSocketBase");

NS_OBJECT_ENSURE_REGISTERED (QuicSocketBase);
NS_OBJECT_ENSURE_REGISTERED (QuicSocketState);

const uint16_t QuicSocketBase::MIN_INITIAL_PACKET_SIZE = 1200;

TypeId
QuicSocketBase::GetInstanceTypeId () const
{
  return QuicSocketBase::GetTypeId ();
}
//这段代码不是“运行”程序的逻辑，而是“配置”TCP套接字的选项。
TypeId
QuicSocketBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QuicSocketBase")
    .SetParent<QuicSocket> ()
    .SetGroupName ("Internet")
    .AddConstructor<QuicSocketBase> ()
    .AddAttribute ("InitialVersion",
                   "Quic Version. The default value starts a version negotiation procedure",
                   UintegerValue (QUIC_VERSION_NEGOTIATION),
                   MakeUintegerAccessor (&QuicSocketBase::m_vers),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IdleTimeout",
                   "Idle timeout value after which the socket is closed",
                   TimeValue (Seconds (300)),
                   MakeTimeAccessor (&QuicSocketBase::m_idleTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MaxStreamData",
                   "Stream Maximum Data",
                   UintegerValue (4294967295),      // according to the QUIC RFC this value should default to 0, and be increased by the client/server
                   MakeUintegerAccessor (&QuicSocketBase::m_initial_max_stream_data),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxData",
                   "Connection Maximum Data",
                   UintegerValue (4294967295),      // according to the QUIC RFC this value should default to 0, and be increased by the client/server
                   MakeUintegerAccessor (&QuicSocketBase::m_max_data),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxStreamIdBidi",
                   "Maximum StreamId for Bidirectional Streams",
                   UintegerValue (2),                   // according to the QUIC RFC this value should default to 0, and be increased by the client/server
                   MakeUintegerAccessor (&QuicSocketBase::m_initial_max_stream_id_bidi),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxStreamIdUni", "Maximum StreamId for Unidirectional Streams",
                   UintegerValue (2),                                  // according to the QUIC RFC this value should default to 0, and be increased by the client/server
                   MakeUintegerAccessor (&QuicSocketBase::m_initial_max_stream_id_uni),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxTrackedGaps", "Maximum number of gaps in an ACK",
                   UintegerValue (20),
                   MakeUintegerAccessor (&QuicSocketBase::m_maxTrackedGaps),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("OmitConnectionId", "Omit ConnectionId field in Short QuicHeader format",
                   BooleanValue (false),
                   MakeBooleanAccessor (&QuicSocketBase::m_omit_connection_id),
                   MakeBooleanChecker ())
    .AddAttribute ("MaxPacketSize", "Maximum Packet Size",
                   UintegerValue (1460),
                   MakeUintegerAccessor (&QuicSocketBase::GetSegSize,
                                         &QuicSocketBase::SetSegSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("SocketSndBufSize", "QuicSocketBase maximum transmit buffer size (bytes)",
                   UintegerValue (131072),                                  // 128k
                   MakeUintegerAccessor (&QuicSocketBase::GetSocketSndBufSize,
                                         &QuicSocketBase::SetSocketSndBufSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SocketRcvBufSize", "QuicSocketBase maximum receive buffer size (bytes)",
                   UintegerValue (131072),                                  // 128k
                   MakeUintegerAccessor (&QuicSocketBase::GetSocketRcvBufSize,
                                         &QuicSocketBase::SetSocketRcvBufSize),
                   MakeUintegerChecker<uint32_t> ())
    // .AddAttribute ("subSocket", "When true, this socket is subsocket",
    //                BooleanValue (false),
    //                MakeBooleanAccessor (&QuicSocketBase::m_subSocket),
    //                MakeBooleanChecker ())
    //	.AddAttribute ("StatelessResetToken, "Stateless Reset Token",
    //				   UintegerValue (0),
    //				   MakeUintegerAccessor (&QuicSocketBase::m_stateless_reset_token),
    //				   MakeUintegerChecker<uint128_t> ())
    .AddAttribute ("AckDelayExponent", "Ack Delay Exponent", 
                   UintegerValue (3),
                   MakeUintegerAccessor (&QuicSocketBase::m_ack_delay_exponent),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("FlushOnClose", "Determines the connection close behavior",
                   BooleanValue (true),
                   MakeBooleanAccessor (&QuicSocketBase::m_flushOnClose),
                   MakeBooleanChecker ())
    .AddAttribute ("kMaxTLPs",
                   "Maximum number of tail loss probes before an RTO fires",
                   UintegerValue (2),
                   MakeUintegerAccessor (&QuicSocketState::m_kMaxTLPs),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("kReorderingThreshold", "Maximum reordering in packet number space before FACK style loss detection considers a packet lost",
                   UintegerValue (3),
                   MakeUintegerAccessor (&QuicSocketState::m_kReorderingThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("kTimeReorderingFraction", "Maximum reordering in time space before time based loss detection considers a packet lost",
                   DoubleValue (9 / 8),
                   MakeDoubleAccessor (&QuicSocketState::m_kTimeReorderingFraction),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("kUsingTimeLossDetection", "Whether time based loss detection is in use", 
                   BooleanValue (false),
                   MakeBooleanAccessor (&QuicSocketState::m_kUsingTimeLossDetection),
                   MakeBooleanChecker ())
    .AddAttribute ("kMinTLPTimeout", "Minimum time in the future a tail loss probe alarm may be set for",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&QuicSocketState::m_kMinTLPTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kMinRTOTimeout", "Minimum time in the future an RTO alarm may be set for",
                   TimeValue (MilliSeconds (200)),
                   MakeTimeAccessor (&QuicSocketState::m_kMinRTOTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kDelayedAckTimeout", "The length of the peer's delayed ACK timer",
                   TimeValue (MilliSeconds (25)),
                   MakeTimeAccessor (&QuicSocketState::m_kDelayedAckTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kDefaultInitialRtt", "The default RTT used before an RTT sample is taken",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&QuicSocketState::m_kDefaultInitialRtt),
                   MakeTimeChecker ())
    .AddAttribute ("InitialSlowStartThreshold",
                   "QUIC initial slow start threshold (bytes)",
                   UintegerValue (INT32_MAX),
                   MakeUintegerAccessor (&QuicSocketBase::GetInitialSSThresh,
                                         &QuicSocketBase::SetInitialSSThresh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("InitialPacketSize",
                   "QUIC initial slow start threshold (bytes)",
                   UintegerValue (1200),
                   MakeUintegerAccessor (&QuicSocketBase::GetInitialPacketSize,
                                         &QuicSocketBase::SetInitialPacketSize),
                   MakeUintegerChecker<uint32_t> (
                     QuicSocketBase::MIN_INITIAL_PACKET_SIZE, UINT32_MAX))
    .AddAttribute ("SchedulingPolicy",
                   "Scheduling policy among streams",
                   TypeIdValue (QuicSocketTxScheduler::GetTypeId ()),
                   MakeTypeIdAccessor (&QuicSocketBase::m_schedulingTypeId),
                   MakeTypeIdChecker ())
    .AddAttribute ("DefaultLatency",
                   "Default latency bound for the EDF scheduler",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&QuicSocketBase::m_defaultLatency),
                   MakeTimeChecker ())
    .AddAttribute ("LegacyCongestionControl", "When true, use TCP implementations for the congestion control",
                   BooleanValue (false),
                   MakeBooleanAccessor (&QuicSocketBase::m_quicCongestionControlLegacy),
                   MakeBooleanChecker ())
    // .AddAttribute ("TCB",
    //                "The connection's QuicSocketState",
    //                PointerValue (),
    //                MakePointerAccessor (&QuicSocketBase::m_tcb),
    //                MakePointerChecker<QuicSocketState> ())
    .AddAttribute ("EnableMultipath",
                   "When true, multipath is supported",
                   BooleanValue (false),
                   MakeBooleanAccessor (&QuicSocketBase::m_enableMultipath),
                   MakeBooleanChecker ())
    // .AddAttribute ("StreamSize", "Maximum StreamId for Unidirectional Streams",
    //                UintegerValue (2),                                  // according to the QUIC RFC this value should default to 0, and be increased by the client/server
    //                MakeUintegerAccessor (&QuicSocketBase::m_streamSize),
    //                MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("CcType",
                   "define the type of the scheduler",
                   IntegerValue (QuicNewReno),
                   MakeIntegerAccessor (&QuicSocketBase::m_ccType),
                   MakeIntegerChecker<int16_t> ())
    .AddAttribute ("SubflowList", "The list of QUIC subflows associated to this socket.",
                   ObjectVectorValue (),
                   MakeObjectVectorAccessor (&QuicSocketBase::m_subflows),
                   MakeObjectVectorChecker<MpQuicSubFlow> ())
    // .AddTraceSource ("RTO", "Retransmission timeout",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_rto),
    //                  "ns3::Time::TracedValueCallback").AddTraceSource (
    //     "IdleTO", "Idle timeout",
    //     MakeTraceSourceAccessor (&QuicSocketBase::m_idleTimeout),
    //     "ns3::Time::TracedValueCallback").AddTraceSource (
    //     "DrainingPeriodTO", "Draining Period timeout",
    //     MakeTraceSourceAccessor (&QuicSocketBase::m_drainingPeriodTimeout),
    //     "ns3::Time::TracedValueCallback");
    .AddTraceSource ("RTO",
                     "Retransmission timeout",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_rto),
                     "ns3::Time::TracedValueCallback")
    .AddTraceSource ("RTT",
                     "Last RTT sample",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_lastRtt),
                     "ns3::Time::TracedValueCallback")
    .AddTraceSource ("NextTxSequence",
                     "Next sequence number to send (SND.NXT)",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_nextTxSequenceTrace),
                     "ns3::SequenceNumber32TracedValueCallback")
    .AddTraceSource ("HighestSequence",
                     "Highest sequence number ever sent in socket's life time",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_highTxMarkTrace),
                     "ns3::SequenceNumber32TracedValueCallback")
    // .AddTraceSource ("State",
    //                  "TCP state",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_state),
    //                  "ns3::TcpStatesTracedValueCallback")
    .AddTraceSource ("CongState",
                     "TCP Congestion machine state",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_congStateTrace),
                     "ns3::TcpSocketState::TcpCongStatesTracedValueCallback")
    // .AddTraceSource ("AdvWND",
    //                  "Advertised Window Size",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_advWnd),
    //                  "ns3::TracedValueCallback::Uint32")
    // .AddTraceSource ("RWND",
    //                  "Remote side's flow control window",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_rWnd),
    //                  "ns3::TracedValueCallback::Uint32")
    // .AddTraceSource ("BytesInFlight",
    //                  "Socket estimation of bytes in flight",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_bytesInFlight),
    //                  "ns3::TracedValueCallback::Uint32")
    // .AddTraceSource ("HighestRxSequence",
    //                  "Highest sequence number received from peer",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_highRxMark),
    //                  "ns3::SequenceNumber32TracedValueCallback")
    // .AddTraceSource ("HighestRxAck",
    //                  "Highest ack received from peer",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_highRxAckMark),
    //                  "ns3::SequenceNumber32TracedValueCallback")
    .AddTraceSource ("CongestionWindow",
                     "The QUIC connection's congestion window",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_cWndTrace),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("CongestionWindow1",
                     "The QUIC connection's congestion window",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_cWndTrace1),
                     "ns3::TracedValueCallback::Uint32")   
    .AddTraceSource ("SlowStartThreshold",
                     "TCP slow start threshold (bytes)",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_ssThTrace),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("SlowStartThreshold1",
                     "TCP slow start threshold (bytes)",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_ssThTrace1),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("RTT0",
                     "Last RTT sample",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_rttTrace0),
                     "ns3::Time::TracedValueCallback")
    .AddTraceSource ("RTT1",
                     "Last RTT sample",
                     MakeTraceSourceAccessor (&QuicSocketBase::m_rttTrace1),
                     "ns3::Time::TracedValueCallback")
     
    // .AddTraceSource ("Tx",
    //                  "Send QUIC packet to UDP protocol",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_txTrace),
    //                  "ns3::QuicSocketBase::QuicTxRxTracedCallback")
    // .AddTraceSource ("Rx",
    //                  "Receive QUIC packet from UDP protocol",
    //                  MakeTraceSourceAccessor (&QuicSocketBase::m_rxTrace),
    //                  "ns3::QuicSocketBase::QuicTxRxTracedCallback")
    
  ;
  return tid;
}

TypeId
QuicSocketState::GetTypeId (void)
{
  static TypeId tid =
    TypeId ("ns3::QuicSocketState")
    .SetParent<TcpSocketState> ()
    .SetGroupName ("Internet")
    .AddAttribute ("kMaxTLPs",
                   "Maximum number of tail loss probes before an RTO fires",
                   UintegerValue (2),
                   MakeUintegerAccessor (&QuicSocketState::m_kMaxTLPs),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("kReorderingThreshold",
                   "Maximum reordering in packet number space before FACK style loss detection considers a packet lost",
                   UintegerValue (3),
                   MakeUintegerAccessor (&QuicSocketState::m_kReorderingThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("kTimeReorderingFraction",
                   "Maximum reordering in time space before time based loss detection considers a packet lost",
                   DoubleValue (9 / 8),
                   MakeDoubleAccessor (&QuicSocketState::m_kTimeReorderingFraction),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("kUsingTimeLossDetection",
                   "Whether time based loss detection is in use", BooleanValue (false),
                   MakeBooleanAccessor (&QuicSocketState::m_kUsingTimeLossDetection),
                   MakeBooleanChecker ())
    .AddAttribute ("kMinTLPTimeout",
                   "Minimum time in the future a tail loss probe alarm may be set for",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&QuicSocketState::m_kMinTLPTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kMinRTOTimeout",
                   "Minimum time in the future an RTO alarm may be set for",
                   TimeValue (MilliSeconds (200)),
                   MakeTimeAccessor (&QuicSocketState::m_kMinRTOTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kDelayedAckTimeout", "The lenght of the peer's delayed ack timer",
                   TimeValue (MilliSeconds (25)),
                   MakeTimeAccessor (&QuicSocketState::m_kDelayedAckTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("kDefaultInitialRtt",
                   "The default RTT used before an RTT sample is taken",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&QuicSocketState::m_kDefaultInitialRtt),
                   MakeTimeChecker ())
    .AddAttribute ("kMaxPacketsReceivedBeforeAckSend",
                   "The maximum number of packets without sending an ACK",
                   UintegerValue (20),
                   MakeUintegerAccessor (&QuicSocketState::m_kMaxPacketsReceivedBeforeAckSend),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

QuicSocketState::QuicSocketState ()
  : TcpSocketState (),
    m_lossDetectionAlarm (),
    m_handshakeCount (0),
    m_tlpCount (
      0),
    m_rtoCount (0),
    m_largestSentBeforeRto (0),
    m_timeOfLastSentPacket (
      Seconds (0)),
    m_largestAckedPacket (0),
    m_smoothedRtt (Seconds (0)),
    m_rttVar (0),
    m_minRtt (
      Seconds (0)),
    m_maxAckDelay (Seconds (0)),
    m_lossTime (Seconds (0)),
    m_kMinimumWindow (
      10 * m_segmentSize),
    m_kLossReductionFactor (0.5),
    m_endOfRecovery (0),
    m_kMaxTLPs (
      2),
    m_kReorderingThreshold (3),
    m_kTimeReorderingFraction (9 / 8),
    m_kUsingTimeLossDetection (
      false),
    m_kMinTLPTimeout (MilliSeconds (10)),
    m_kMinRTOTimeout (
      MilliSeconds (200)),
    m_kDelayedAckTimeout (MilliSeconds (25)),
    m_alarmType (0),
    m_nextAlarmTrigger (Seconds (100)),
    m_kDefaultInitialRtt (
      MilliSeconds (100)),
    m_kMaxPacketsReceivedBeforeAckSend (20)
{
  m_lossDetectionAlarm.Cancel ();
}

QuicSocketState::QuicSocketState (const QuicSocketState &other)
  : TcpSocketState (other),
    m_lossDetectionAlarm (other.m_lossDetectionAlarm),
    m_handshakeCount (
      other.m_handshakeCount),
    m_tlpCount (other.m_tlpCount),
    m_rtoCount (
      other.m_rtoCount),
    m_largestSentBeforeRto (
      other.m_largestSentBeforeRto),
    m_timeOfLastSentPacket (
      other.m_timeOfLastSentPacket),
    m_largestAckedPacket (
      other.m_largestAckedPacket),
    m_smoothedRtt (
      other.m_smoothedRtt),
    m_rttVar (other.m_rttVar),
    m_minRtt (
      other.m_minRtt),
    m_maxAckDelay (other.m_maxAckDelay),
    m_lossTime (
      other.m_lossTime),
    m_kMinimumWindow (other.m_kMinimumWindow),
    m_kLossReductionFactor (
      other.m_kLossReductionFactor),
    m_endOfRecovery (
      other.m_endOfRecovery),
    m_kMaxTLPs (other.m_kMaxTLPs),
    m_kReorderingThreshold (
      other.m_kReorderingThreshold),
    m_kTimeReorderingFraction (
      other.m_kTimeReorderingFraction),
    m_kUsingTimeLossDetection (
      other.m_kUsingTimeLossDetection),
    m_kMinTLPTimeout (
      other.m_kMinTLPTimeout),
    m_kMinRTOTimeout (other.m_kMinRTOTimeout),
    m_kDelayedAckTimeout (
      other.m_kDelayedAckTimeout),
    m_kDefaultInitialRtt (
      other.m_kDefaultInitialRtt),
    m_kMaxPacketsReceivedBeforeAckSend (other.m_kMaxPacketsReceivedBeforeAckSend)
{
  m_lossDetectionAlarm.Cancel ();
}

QuicSocketBase::QuicSocketBase (void)
  :  QuicSocket (),
    m_endPoint (0),
    m_endPoint6 (0),
    m_node (0),
    m_quicl4 (0),
    m_quicl5 (0),
    m_socketState (
      IDLE),
    m_transportErrorCode (
      QuicSubheader::TransportErrorCodes_t::NO_ERROR),
    m_serverBusy (false),
    m_errno (
      ERROR_NOTERROR),
    m_connected (false),
    m_connectionId (0),
    m_vers (
      QUIC_VERSION_NS3_IMPL),
    m_keyPhase (QuicHeader::PHASE_ZERO),
    m_lastReceived (Seconds (0.0)),
    m_initial_max_stream_data (
      0),
    m_max_data (0),
    m_initial_max_stream_id_bidi (0),
    m_idleTimeout (
      Seconds (300.0)),
    m_omit_connection_id (false),
    m_ack_delay_exponent (
      3),
    m_initial_max_stream_id_uni (0),
    m_maxTrackedGaps (20),
    m_receivedTransportParameters (
      false),
    m_couldContainTransportParameters (true),
    m_rto (
      Seconds (30.0)),
    m_drainingPeriodTimeout (Seconds (90.0)),
    m_closeOnEmpty (false),
    m_congestionControl (
      0),
    m_lastRtt (Seconds (0.0)),
    m_queue_ack (false),
    m_numPacketsReceivedSinceLastAckSent (0),
    m_pacingTimer (Timer::REMOVE_ON_DESTROY),
    m_enableMultipath(false),
    m_pathManager(0),
    m_scheduler (0),
    m_subflows (0)
{
  NS_LOG_FUNCTION (this);


  m_rxBuffer = CreateObject<QuicSocketRxBuffer> ();
  m_txBuffer = CreateObject<QuicSocketTxBuffer> ();
  m_receivedPacketNumbers = std::vector<SequenceNumber32> ();

  m_quicCongestionControlLegacy = false;
  m_pacingTimer.SetFunction (&QuicSocketBase::NotifyPacingPerformed, this);

  // /**
  //  * [IETF DRAFT 10 - Quic Transport: sec 5.7.1]
  //  *
  //  * The initial number for a packet number MUST be selected randomly from a range between
  //  * 0 and 2^32 -1025 (inclusive).
  //  * However, in this implementation, we set the sequence number to 0
  //  *
  //  */
  // if (!m_quicCongestionControlLegacy)
  //   {
  //     Ptr<UniformRandomVariable> rand =
  //       CreateObject<UniformRandomVariable> ();
  //     m_tcb->m_nextTxSequence = SequenceNumber32 (0);
  //     // (uint32_t) rand->GetValue (0, pow (2, 32) - 1025));
  //   }
  m_subflows = std::vector <Ptr<MpQuicSubFlow>> ();
  CreatePathManager();
  CreateScheduler();

}

QuicSocketBase::QuicSocketBase (const QuicSocketBase& sock)   // Copy constructor
  : QuicSocket (sock),
    m_endPoint (0),
    m_endPoint6 (0),
    m_node (sock.m_node),
    m_quicl4 (sock.m_quicl4),
    m_quicl5 (0),
    m_socketState (LISTENING),
    m_transportErrorCode (sock.m_transportErrorCode),
    m_serverBusy (sock.m_serverBusy),
    m_errno (sock.m_errno),
    m_connected (sock.m_connected),
    m_connectionId (0),
    m_vers (sock.m_vers),
    m_keyPhase (QuicHeader::PHASE_ZERO),
    m_lastReceived (sock.m_lastReceived),
    m_initial_max_stream_data (sock.m_initial_max_stream_data),
    m_max_data (sock.m_max_data),
    m_initial_max_stream_id_bidi (sock.m_initial_max_stream_id_bidi),
    m_idleTimeout (sock.m_idleTimeout),
    m_omit_connection_id (sock.m_omit_connection_id),
    m_ack_delay_exponent (sock.m_ack_delay_exponent),
    m_initial_max_stream_id_uni (sock.m_initial_max_stream_id_uni),
    m_maxTrackedGaps (sock.m_maxTrackedGaps),
    m_receivedTransportParameters (sock.m_receivedTransportParameters),
    m_couldContainTransportParameters (sock.m_couldContainTransportParameters),
    m_rto (sock.m_rto),
    m_drainingPeriodTimeout (sock.m_drainingPeriodTimeout),
    m_closeOnEmpty (sock.m_closeOnEmpty),
    m_lastRtt (sock.m_lastRtt),
    m_quicCongestionControlLegacy (sock.m_quicCongestionControlLegacy),
    m_queue_ack (sock.m_queue_ack),
    m_numPacketsReceivedSinceLastAckSent (sock.m_numPacketsReceivedSinceLastAckSent),
    m_lastMaxData(0),
    m_maxDataInterval(10),
    m_pacingTimer (Timer::REMOVE_ON_DESTROY),
    m_txTrace (sock.m_txTrace),
    m_rxTrace (sock.m_rxTrace),
    m_enableMultipath(sock.m_enableMultipath),
    m_pathManager(sock.m_pathManager),
    m_scheduler (sock.m_scheduler),
    m_subflows (sock.m_subflows)
{
  NS_LOG_FUNCTION (this);

  m_txBuffer = CopyObject (sock.m_txBuffer);
  m_rxBuffer = CopyObject (sock.m_rxBuffer);

  m_receivedPacketNumbers = std::vector<SequenceNumber32> ();


  // m_tcb = CopyObject (sock.m_tcb);
  if (sock.m_congestionControl)
    {
      m_congestionControl = sock.m_congestionControl->Fork ();
    }
  m_quicCongestionControlLegacy = sock.m_quicCongestionControlLegacy;
  // m_txBuffer->SetQuicSocketState (m_tcb);

  // m_tcb->m_pacingRate = m_tcb->m_maxPacingRate;
  m_pacingTimer.SetFunction (&QuicSocketBase::NotifyPacingPerformed, this);

  m_pathManager->SetSocket(this);
}

QuicSocketBase::~QuicSocketBase (void)
{
  NS_LOG_FUNCTION (this);

  m_node = 0;
  if (m_endPoint != nullptr)
    {
      NS_ASSERT (m_quicl4 != nullptr);
      NS_ASSERT (m_endPoint != nullptr);
      m_quicl4->DeAllocate (m_endPoint);
      NS_ASSERT (m_endPoint == nullptr);
    }
  if (m_endPoint6 != nullptr)
    {
      NS_ASSERT (m_quicl4 != nullptr);
      NS_ASSERT (m_endPoint6 != nullptr);
      m_quicl4->DeAllocate (m_endPoint6);
      NS_ASSERT (m_endPoint6 == nullptr);
    }
  m_quicl4 = 0;
  m_subflows.clear();
  //CancelAllTimers ();
  m_pacingTimer.Cancel ();
}

/* Inherit from Socket class: Bind socket to an end-point in QuicL4Protocol */
int
QuicSocketBase::Bind (void)
{
  //NS_LOG_FUNCTION (this);
  m_endPoint = m_quicl4->Allocate ();
  if (0 == m_endPoint)
    {
      m_errno = ERROR_ADDRNOTAVAIL;
      return -1;
    }

  m_quicl4->UdpBind (this);
  return SetupCallback ();
}

int
QuicSocketBase::Bind (const Address &address)
{
  NS_LOG_FUNCTION (this);
  if (InetSocketAddress::IsMatchingType (address))
    {
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      Ipv4Address ipv4 = transport.GetIpv4 ();
      uint16_t port = transport.GetPort ();
      //SetIpTos (transport.GetTos ());
      if (ipv4 == Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_quicl4->Allocate ();
        }
      else if (ipv4 == Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_quicl4->Allocate (GetBoundNetDevice (), port);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_quicl4->Allocate (ipv4);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_quicl4->Allocate (GetBoundNetDevice (), ipv4, port);
        }
      if (0 == m_endPoint)
        {
          m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
          return -1;
        }
    }
  else if (Inet6SocketAddress::IsMatchingType (address))
    {
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address ipv6 = transport.GetIpv6 ();
      uint16_t port = transport.GetPort ();
      if (ipv6 == Ipv6Address::GetAny () && port == 0)
        {
          m_endPoint6 = m_quicl4->Allocate6 ();
        }
      else if (ipv6 == Ipv6Address::GetAny () && port != 0)
        {
          m_endPoint6 = m_quicl4->Allocate6 (GetBoundNetDevice (), port);
        }
      else if (ipv6 != Ipv6Address::GetAny () && port == 0)
        {
          m_endPoint6 = m_quicl4->Allocate6 (ipv6);
        }
      else if (ipv6 != Ipv6Address::GetAny () && port != 0)
        {
          m_endPoint6 = m_quicl4->Allocate6 (GetBoundNetDevice (), ipv6, port);
        }
      if (0 == m_endPoint6)
        {
          m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
          return -1;
        }
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }

  m_quicl4->UdpBind (address, this);
  return SetupCallback ();
}

int
QuicSocketBase::Bind6 (void)
{
  NS_LOG_FUNCTION (this);
  m_endPoint6 = m_quicl4->Allocate6 ();
  if (0 == m_endPoint6)
    {
      m_errno = ERROR_ADDRNOTAVAIL;
      return -1;
    }

  m_quicl4->UdpBind6 (this);
  return SetupCallback ();
}

/* Inherit from Socket class: Bind this socket to the specified NetDevice */
void
QuicSocketBase::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (this);

  m_quicl4->BindToNetDevice (this, netdevice);
}

int
QuicSocketBase::Listen (void)
{
  NS_LOG_FUNCTION (this);
  if (m_socketType == NONE)
    {
      m_socketType = SERVER;
    }

  if (m_socketState != IDLE and m_socketState != QuicSocket::CONNECTING_SVR)
    {
      //m_errno = ERROR_INVAL;
      return -1;
    }

  bool res = m_quicl4->SetListener (this);
  NS_ASSERT (res);

  SetState (LISTENING);

  return 0;
}


int
QuicSocketBase::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this);


  if (InetSocketAddress::IsMatchingType (address))
    {
      if (m_endPoint == nullptr)
        {
          if (Bind () == -1)
            {
              NS_ASSERT (m_endPoint == nullptr);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint != nullptr);
        }
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      m_endPoint->SetPeer (transport.GetIpv4 (), transport.GetPort ());
      m_endPoint6 = nullptr;

      // For multipath implementation
      Ptr<MpQuicSubFlow> subflow0 = m_pathManager->InitialSubflow0(InetSocketAddress(m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()), address);
      
      
      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      /*if (SetupEndpoint () != 0)
        {
          NS_LOG_ERROR ("Route to destination does not exist ?!");
          return -1;
        }*/
    }
  else if (Inet6SocketAddress::IsMatchingType (address))
    {
      // If we are operating on a v4-mapped address, translate the address to
      // a v4 address and re-call this function
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address v6Addr = transport.GetIpv6 ();
      if (v6Addr.IsIpv4MappedAddress () == true)
        {
          Ipv4Address v4Addr = v6Addr.GetIpv4MappedAddress ();
          return Connect (InetSocketAddress (v4Addr, transport.GetPort ()));
        }

      if (m_endPoint6 == nullptr)
        {
          if (Bind6 () == -1)
            {
              NS_ASSERT (m_endPoint6 == nullptr);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint6 != nullptr);
        }
      m_endPoint6->SetPeer (v6Addr, transport.GetPort ());
      m_endPoint = nullptr;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      /*if (SetupEndpoint6 () != 0)
        {
          NS_LOG_ERROR ("Route to destination does not exist ?!");
          return -1;
        }*/
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }


  if (m_socketType == NONE)
    {
      m_socketType = CLIENT;
    }

  if (m_quicl5 == 0)
    {
      m_quicl5 = CreateStreamController ();
      m_quicl5->CreateStream (QuicStream::BIDIRECTIONAL, 0);   // Create Stream 0 (necessary)
    }

  // check if the address is in a list of known and authenticated addresses
  auto result = std::find (
    m_quicl4->GetAuthAddresses ().begin (), m_quicl4->GetAuthAddresses ().end (),
    InetSocketAddress::ConvertFrom (address).GetIpv4 ());

  if (result != m_quicl4->GetAuthAddresses ().end ()
      || m_quicl4->Is0RTTHandshakeAllowed ())
    {
      NS_LOG_INFO (
        "CONNECTION AUTHENTICATED Client found the Server " << InetSocketAddress::ConvertFrom (address).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (address).GetPort () << " in authenticated list");
      // connect the underlying UDP socket
      m_quicl4->UdpConnect (address, this);
      return DoFastConnect ();
    }
  else
    {
      NS_LOG_INFO (
        "CONNECTION not authenticated: cannot perform 0-RTT Handshake");
      m_quicl4->UdpConnect (address, this);
      return DoConnect ();
    }

}

/* Inherit from Socket class: Invoked by upper-layer application */
int
QuicSocketBase::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION (this << flags);
  int data = 0;

  if (m_drainingPeriodEvent.IsRunning ())
    {
      NS_LOG_INFO ("Socket in draining state, cannot send packets");
      return 0;
    }

  if (flags == 0)
    {
      data = Send (p);
    }
  else
    {
      data = m_quicl5->DispatchSend (p, flags);
    }
  return data;
}

int
QuicSocketBase::Send (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);

  if (m_drainingPeriodEvent.IsRunning ())
    {
      NS_LOG_INFO ("Socket in draining state, cannot send packets");
      return 0;
    }

  int data = m_quicl5->DispatchSend (p);

  return data;
}

int
QuicSocketBase::AppendingTx (Ptr<Packet> frame)//QuicL5Protocol::Send (Ptr<Packet> frame)调用
{
  NS_LOG_FUNCTION (this);
   std::cout <<"补充数据---------ing真的吗？"<<"AppendingTx 时间: " << Simulator::Now().GetNanoSeconds() << " ns - 被调用了" << std::endl;
  if (m_socketState != IDLE)
    {
      bool done = m_txBuffer->Add (frame);
      std::cout <<"补充数据--------已完成" <<"AppendingTx中的m_txBuffer->AppSize ()：    " << m_txBuffer->AppSize ()<<"  时间: " << Simulator::Now().GetNanoSeconds() <<std::endl;
      if (!done)
        {
          NS_LOG_INFO ("Exceeding Socket Tx Buffer Size");
          m_errno = ERROR_MSGSIZE;
        }
      else
        {
          uint32_t win;
          for (uint16_t i = 0; i < GetActiveSubflows().size(); i++){
            win += AvailableWindow (i);
          }
          
        NS_LOG_DEBUG ("Added packet to the buffer - txBufSize = " << m_txBuffer->AppSize ()
                        << " AvailableWindow = " << win << " state " << QuicStateName[m_socketState]);
        }
          //std::cout<<"m_socketState=  "<<m_socketState<<std::endl;
      if (m_socketState != IDLE)
        {
          //std::cout<<"还能进这里m_socketState != IDLE   "<<std::endl;
          if (!m_sendPendingDataEvent.IsRunning ())
            {
                 //std::cout<<"还能进这里m_sendPendingDataEvent"<<std::endl;
                m_sendPendingDataEvent = Simulator::Schedule (
                  TimeStep (1), &QuicSocketBase::SendPendingData, this,
                  m_connected);
              
            }
        }
      if (done)
        {
          return frame->GetSize ();
        }
      return -1;
    }
  else
    {

      NS_ABORT_MSG ("Sending in state" << QuicStateName[m_socketState]);
      return -1;
    }
}


uint32_t
QuicSocketBase::SendPendingData (bool withAck)//调用OnReceivedAckFrame
{
  NS_LOG_FUNCTION (this << withAck);
  //std::cout <<"SendPendingData被调用了" << m_txBuffer->AppSize ()<<"SendPendingData 时间: " << Simulator::Now().GetNanoSeconds() << " ns - 被调用了" << std::endl;

  if (m_txBuffer->AppSize () == 0)
    {
      //std::cout <<"SendPendingData中的m_txBuffer：   " << m_txBuffer->AppSize () <<std::endl;
      //std::cout <<"缓冲区空了？" << m_txBuffer->AppSize () <<std::endl;
      if (m_closeOnEmpty)
        {
          m_drainingPeriodEvent.Cancel ();
          //std::cout <<"总不能是这里发的停止帧吧" << std::endl;
          SendConnectionClosePacket (0, "Scheduled connection close - no error");
        }
      NS_LOG_INFO ("Nothing to send");
      //std::cout <<"缓冲区空了   被return了"<<std::endl;
      return false;
    }

  uint32_t nPacketsSent = 0;
//优先处理流0（stream 0）的帧（QUIC中流0通常用于控制或握手数据）。
  // prioritize stream 0
  //检查缓冲区中是否有流0的帧。如果有，继续循环。
        //std::cout <<"SendPendingData经过前期检查" << m_txBuffer->AppSize () <<std::endl;

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while (m_txBuffer->GetNumFrameStream0InBuffer () > 0)
  {
    //std::cout <<"第一个while-stream0: " << std::endl;
    // check pacing timer
    //检查步调定时器（pacing timer）：如果启用步调（m_pacing为真），且定时器正在运行，则跳过发送（break），以避免过快发送导致拥塞（QUIC标准中步调用于平滑流量
    if (m_subflows[0]->m_tcb->m_pacing)
    {
      NS_LOG_DEBUG ("Pacing is enabled");
      if (m_pacingTimer.IsRunning ())
        {
          NS_LOG_INFO ("Skipping Packet due to pacing - for " << m_pacingTimer.GetDelayLeft ());
          break;
        }
      NS_LOG_DEBUG ("Pacing Timer is not running");
    }
//计算窗口：AvailableWindow(0)（子流0的可用拥塞窗口）、ConnectionWindow(0)（连接级接收窗口）、BytesInFlight(0)（子流0飞行中字节数）。这些是拥塞控制的关键指标。
    uint32_t win = AvailableWindow (0); //just use first subflow to deal with stream 0
    uint32_t connWin = ConnectionWindow (0);
    uint32_t bytesInFlight = BytesInFlight (0);
    
    NS_LOG_DEBUG (
    "BEFORE stream 0 Available Window " << win
                                      << " Connection RWnd " << connWin
                                      << " BytesInFlight " << bytesInFlight
                                      << " BufferedSize " << m_txBuffer->AppSize ()
                                      << " MaxPacketSize " << GetSegSize ());


    NS_LOG_DEBUG ("Send a frame for stream 0");
    //生成下一个序列号next
    SequenceNumber32 next = ++m_subflows[0]->m_tcb->m_nextTxSequence;
    NS_LOG_INFO ("on path 0 SN " << next);
//调用SendDataPacket(next, 0, m_queue_ack, 0)：发送流0的数据包。参数：序列号、最大大小0（表示自动计算）、是否带ACK、路径ID=0。

    SendDataPacket (next, 0, m_subflows[0]->m_queue_ack, 0);
    // 重新计算窗口并日志记录发送后状态（AFTER...）。

    win = AvailableWindow (0);
    connWin = ConnectionWindow (0);
    bytesInFlight = BytesInFlight (0);
    NS_LOG_DEBUG (
      "AFTER stream 0 Available Window " << win
                                          << " Connection RWnd " << connWin
                                          << " BytesInFlight " << bytesInFlight
                                          << " BufferedSize " << m_txBuffer->AppSize ()
                                          << " MaxPacketSize " << GetSegSize ());
// 递增nPacketsSent。
    ++nPacketsSent;
  }
        //std::cout <<"SendPendingData经过stram0的while" << m_txBuffer->AppSize () <<std::endl;

//调用调度器（m_scheduler）获取下一个路径ID的概率向量sendP
  std::vector<double> sendP = m_scheduler->GetNextPathIdToUse();
    std::cout <<"我看看有多离谱sendP.size(); " <<sendP.size()<<std::endl;
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  for (uint8_t sendingPathId = 0; sendingPathId < sendP.size(); sendingPathId++)
  {
      //std::cout <<"就没有进入过处理其他流这里" << m_txBuffer->AppSize () <<std::endl;
    uint32_t availableWindow = AvailableWindow (sendingPathId);//计算每个路径的可用窗口availableWindow。
    uint32_t sendSize = m_txBuffer->AppSize () * sendP[sendingPathId];//计算发送大小sendSize：缓冲区总大小乘以路径比例sendP[sendingPathId]
    uint32_t sendNumber = sendSize/GetSegSize();//计算包数sendNumber：发送大小除以段大小（GetSegSize()，QUIC的分段大小，通常接近MTU）。
    // //std::cout <<"for循环 "<<"  sendingPathId "<<sendingPathId <<"  sendP.size()"<<sendP.size() << std::endl;
    // //std::cout <<"                "<<"availableWindow "<<availableWindow <<"    sendSize"<<sendSize << std::endl;
    if (sendSize > availableWindow)
    {
      sendNumber = availableWindow/GetSegSize();
    } 
//只要有包可发（sendNumber>0）、窗口可用、缓冲区非空：
//处理其他流
      //std::cout <<"其他流内循环while "<<"sendNumber > 0:"<<sendNumber <<"      availableWindow > 0："<<availableWindow <<"        m_txBuffer->AppSize ():"<< m_txBuffer->AppSize() << " SendDataPacket Schedule Close at time " << Simulator::Now ().GetSeconds ()  <<std::endl;
    while (sendNumber > 0 and availableWindow > 0 and m_txBuffer->AppSize () > 0)
      {
             //std::cout <<"                内循环 "<<"sendNumber > 0："<<sendNumber <<"    availableWindow > 0："<<availableWindow << " SendDataPacket Schedule Close at time " << Simulator::Now ().GetSeconds ()  << std::endl;
            // //std::cout <<"                                "<<"availableWindow "<<availableWindow <<"    sendSize"<<sendSize << std::endl;
        // check draining period检查排水期：如果运行中，返回false（不能发送）。
        if (m_drainingPeriodEvent.IsRunning ())
          {
             //std::cout <<"Draining period: no packets can be sent"<< std::endl;
            NS_LOG_INFO ("Draining period: no packets can be sent");
            return false;
          }

        // check pacing timer检查步调定时器：类似流0处理，如果运行中，break。
        if (m_subflows[sendingPathId]->m_tcb->m_pacing)
          {
            NS_LOG_DEBUG ("Pacing is enabled");
            if (m_pacingTimer.IsRunning ())
              {
                NS_LOG_INFO ("Skipping Packet due to pacing - for " << m_pacingTimer.GetDelayLeft ());
                 //std::cout <<"Skipping Packet due to pacing - for "<< std::endl;
                break;
              }
            NS_LOG_DEBUG ("Pacing Timer is not running");
          }

        // check the state of the socket!检查socket状态：如果在连接中（CONNECTING_CLT/SVR），不发送数据，break。
        if (m_socketState == CONNECTING_CLT || m_socketState == CONNECTING_SVR)
          {
            NS_LOG_INFO ("CONNECTING_CLT and CONNECTING_SVR state; no data to transmit");
             //std::cout <<"CONNECTING_CLT and CONNECTING_SVR state; no data to transmit "<< std::endl;

            break;
          }

        uint32_t availableData = m_txBuffer->AppSize ();
//如果可用数据<窗口且非关闭模式，通知应用层提供更多数据
        if (availableData < availableWindow and !m_closeOnEmpty)
          {
            NS_LOG_INFO ("Ask the app for more data before trying to send");
            NotifySend (GetTxAvailable ());
          }
//防止Silly Window Syndrome（愚蠢窗口综合征，TCP/QUIC常见问题）：如果窗口<段大小 且 数据>窗口，等待更大窗口，break。
        if (availableWindow < GetSegSize () and availableData > availableWindow and !m_closeOnEmpty)
          {
            //std::cout <<"availableData: "<<availableData<< std::endl;
            NS_LOG_INFO ("Preventing Silly Window Syndrome. Wait to Send.");
            //std::cout <<"Preventing Silly Window Syndrome. Wait to Send. "<< std::endl;
            break;
          }
//生成下一个序列号next。
        SequenceNumber32 next = ++m_subflows[sendingPathId]->m_tcb->m_nextTxSequence;
//计算发送大小s：min(窗口, 段大小)。
        uint32_t s = std::min (availableWindow, GetSegSize ());

        uint32_t win = AvailableWindow (sendingPathId); // mark: to be AvailableWindow (m_lastUsedsFlowIdx)
        uint32_t connWin = ConnectionWindow (sendingPathId);
        uint32_t bytesInFlight = BytesInFlight (sendingPathId);

        NS_LOG_DEBUG (
          "BEFORE Available Window " << win
                                    << " Connection RWnd " << connWin
                                    << " BytesInFlight " << bytesInFlight
                                    << " BufferedSize " << m_txBuffer->AppSize ()
                                    << " MaxPacketSize " << GetSegSize ());

        NS_LOG_INFO ("on path " << sendingPathId << " SN " << next);
        // uint32_t sz =
        //调用SendDataPacket(next, s, withAck, sendingPathId)：核心发送函数，发送数据包。
        //std::cout <<"核心发送位置"<< std::endl;

        SendDataPacket (next, s, withAck, sendingPathId);///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        win = AvailableWindow (sendingPathId);
        connWin = ConnectionWindow (sendingPathId);
        bytesInFlight = BytesInFlight (sendingPathId);
        NS_LOG_DEBUG (
          "AFTER Available Window " << win
                                    << " Connection RWnd " << connWin
                                    << " BytesInFlight " << bytesInFlight
                                    << " BufferedSize " << m_txBuffer->AppSize ()
                                    << " MaxPacketSize " << GetSegSize ());
//递增nPacketsSent，更新窗口，递减sendNumber
        ++nPacketsSent;

        availableWindow = AvailableWindow(sendingPathId);
        sendNumber--;
      }
  }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (nPacketsSent > 0)
    {
      NS_LOG_INFO ("SendPendingData sent " << nPacketsSent << " packets");
    }
  else
    {
      NS_LOG_INFO ("SendPendingData no packets sent");
    }

  return nPacketsSent;
}

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
    p->AddAtEnd (OnSendingAckFrame (pathId));/***************************************************************** */
    SequenceNumber32 packetNumber = ++m_subflows[pathId]->m_tcb->m_nextTxSequence;
    QuicHeader head;
    head = QuicHeader::CreateShort (m_connectionId, packetNumber, !m_omit_connection_id, m_keyPhase);
//调试点
    // m_txBuffer->UpdateAckSent (packetNumber, p->GetSerializedSize () + head.GetSerializedSize (), m_subflows[pathId]->m_tcb);

    NS_LOG_INFO ("Send ACK packet with header " << head);

    head.SetPathId(pathId);
    m_quicl4->SendPacket (this, p, head);
    m_txTrace (p, head, this);
  }
  
  
  
}

