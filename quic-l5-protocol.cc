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
 *
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/object-vector.h"

#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
// #include "ns3/ipv4-route.h"
// #include "ns3/ipv6-route.h"

#include "quic-l5-protocol.h"
// #include "ns3/ipv4-end-point-demux.h"
// #include "ns3/ipv6-end-point-demux.h"
// #include "ns3/ipv4-end-point.h"
// #include "ns3/ipv6-end-point.h"
// #include "ns3/ipv4-l3-protocol.h"
// #include "ns3/ipv6-l3-protocol.h"
// #include "ns3/ipv6-routing-protocol.h"
#include "quic-socket-factory.h"
#include "quic-socket-base.h"
#include "quic-stream-base.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("QuicL5Protocol");

NS_OBJECT_ENSURE_REGISTERED (QuicL5Protocol);
//QUIC 协议的 L5 层实现，负责 QUIC 流的创建、管理、多路复用（multiplexing）和解复用（demultiplexing）
// #undef NS_LOG_APPEND_CONTEXT
// #define NS_LOG_APPEND_CONTEXT
// if (m_node and m_connectionId) { std::clog << " [node " << m_node->GetId () << " socket " << m_connectionId << "] "; }

TypeId
QuicL5Protocol::GetTypeId (void)
{
  static TypeId tid =
    TypeId ("ns3::QuicL5Protocol")
      .SetParent<QuicSocketBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<QuicL5Protocol> ()
      .AddAttribute ("StreamList", "The list of streams associated to this protocol.",
                     ObjectVectorValue (),
                     MakeObjectVectorAccessor (&QuicL5Protocol::m_streams),
                     MakeObjectVectorChecker<QuicStreamBase> ())
  ;
  return tid;
}

QuicL5Protocol::QuicL5Protocol ()
  : m_socket (0),
  m_node (0),
  m_connectionId ()
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC ("Made a QuicL5Protocol " << this);
  m_socket = 0;
  m_node = 0;
  m_connectionId = 0;
}

QuicL5Protocol::~QuicL5Protocol ()
{
  NS_LOG_FUNCTION (this);
}
//创建单个单个单个单个单个新流，ID 为当前流数量。
void
QuicL5Protocol::CreateStream (
  const QuicStreamBase::QuicStreamDirectionTypes_t streamDirectionType)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Create the stream with ID " << m_streams.size ());
  Ptr<QuicStreamBase> stream = CreateObject<QuicStreamBase> ();

  stream->SetQuicL5 (this);

  stream->SetNode (m_node);

  stream->SetConnectionId (m_connectionId);

  stream->SetStreamId ((uint64_t) m_streams.size ());

  uint64_t mask = 0x00000003;
  if ((m_streams.size () & mask) == QuicStream::CLIENT_INITIATED_BIDIRECTIONAL
      or (m_streams.size () & mask)
      == QuicStream::SERVER_INITIATED_BIDIRECTIONAL)
    {
      stream->SetStreamDirectionType (QuicStream::BIDIRECTIONAL);

    }
  else
    {
      stream->SetStreamDirectionType (streamDirectionType);
    }

  if (stream->GetStreamId () > 0)
    {
      stream->SetMaxStreamData (m_socket->GetInitialMaxStreamData ());
    }
  else
    {
      stream->SetMaxStreamData (UINT32_MAX);
    }


  m_streams.push_back (stream);

}
//创建多个多个多个多个多个多个流，至指定数量。
void
QuicL5Protocol::CreateStream (
  const QuicStream::QuicStreamDirectionTypes_t streamDirectionType,
  uint64_t streamNum)
{

  NS_LOG_FUNCTION (this << m_streams.size () << streamNum);


  if (streamNum > m_socket->GetMaxStreamId ())   // TODO separate unidirectional and bidirectional
    {
      NS_LOG_INFO ("MaxStreamId " << m_socket->GetMaxStreamId ());
      SignalAbortConnection (
        QuicSubheader::TransportErrorCodes_t::STREAM_ID_ERROR,
        "Initiating Stream with higher StreamID with respect to what already negotiated");
      return;
    }

  // create streamNum streams
  while (m_streams.size () <= streamNum)
    {
      NS_LOG_INFO ("Create stream " << m_streams.size ());
      CreateStream (streamDirectionType);
    }

}
//关联 QUIC 套接字到该协议栈，用于后续发送/接收操作。
void
QuicL5Protocol::SetSocket (Ptr<QuicSocketBase> sock)
{
  NS_LOG_FUNCTION (this);
  m_socket = sock;
}
//将数据包分发到多个流上发送，实现多路复用。默认在所有流上均衡分担负载（除流 0）。两个函数一样//////////////////////////////////////////////////有点意思，这个负载均衡和路径调度什么关系
int
QuicL5Protocol::DispatchSend (Ptr<Packet> data)
{
  NS_LOG_FUNCTION (this);

  int sentData = 0;

  // if the streams are not created yet, open the streams
  if (m_streams.size () != m_socket->GetMaxStreamId ())
    {
      NS_LOG_INFO ("Create the missing streams");
      CreateStream (QuicStream::SENDER, m_socket->GetMaxStreamId ());   // TODO open up to max_stream_uni and max_stream_bidi
    }

  std::vector<Ptr<Packet> > disgregated = DisgregateSend (data);

  std::vector<Ptr<QuicStreamBase> >::iterator jt = m_streams.begin () + 1;   // Avoid Send on stream <0>, which is used only for handshake

  for (std::vector<Ptr<Packet> >::iterator it = disgregated.begin ();
       it != disgregated.end (); ++jt)
    {
      if (jt == m_streams.end ())             // Sending Remaining Load
        {
          jt = m_streams.begin () + 1;
        }
      NS_LOG_LOGIC (
        this << " " << (uint64_t)(*jt)->GetStreamDirectionType () << (uint64_t) QuicStream::SENDER << (uint64_t) QuicStream::BIDIRECTIONAL);

      if ((*jt)->GetStreamDirectionType () == QuicStream::SENDER
          or (*jt)->GetStreamDirectionType () == QuicStream::BIDIRECTIONAL)
        {
          NS_LOG_INFO (
            "Sending data on stream " << (*jt)->GetStreamId ());
          sentData = (*jt)->Send ((*it));
          ++it;
        }
    }

  return sentData;
}
//将数据包发送到指定流，实现针对性多路复用。两个函数一样
int
QuicL5Protocol::DispatchSend (Ptr<Packet> data, uint64_t streamId)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Send packet on (specified) stream " << streamId);

  Ptr<QuicStreamBase> stream = SearchStream (streamId);

  if (stream == nullptr)
    {
      CreateStream (QuicStream::SENDER, streamId);
    }

  stream = SearchStream (streamId);
  int sentData = 0;

  if (stream->GetStreamDirectionType () == QuicStream::SENDER
      or stream->GetStreamDirectionType () == QuicStream::BIDIRECTIONAL)
    {
      sentData = stream->Send (data);
    }

  return sentData;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//处理接收到的数据包，实现解复用。将包分解成帧，分发到流或套接字。
int
QuicL5Protocol::DispatchRecv (Ptr<Packet> data, Address &address)
{
  NS_LOG_FUNCTION (this);
  //使用 DisgregateRecv 分解 data 成帧-子头对
  auto disgregated = DisgregateRecv (data);
//检查是否溢出 maxData（流控制）；若溢出，触发 AbortConnection。触发 AbortConnection?
  if (m_socket->CheckIfPacketOverflowMaxDataLimit (disgregated))
    {
      NS_LOG_WARN ("Maximum data limit overflow");
      // put here this check instead of in QuicSocketBase due to framework mismatch in packet->Copy()
      SignalAbortConnection (
        QuicSubheader::TransportErrorCodes_t::FLOW_CONTROL_ERROR,
        "Received more data w.r.t. Max Data limit");
      return -1;
    }
//检测是否纯 ACK 帧，并创建缺失流。
  bool onlyAckFrames = true;
  uint64_t currStreamNum = m_streams.size () - 1;
  for (auto &elem : disgregated)
    {
      QuicSubheader sub = elem.second;

      // check if this is an ack frame
      if (!sub.IsAck ())
        {
          onlyAckFrames = false;
        }

      if (sub.GetStreamId () > currStreamNum)
        {
          currStreamNum = sub.GetStreamId ();
        }
    }

  CreateStream (QuicStream::RECEIVER, currStreamNum);

  for (auto it = disgregated.begin (); it != disgregated.end (); ++it)
    {
      QuicSubheader sub = (*it).second;

      if (sub.IsRstStream () or sub.IsMaxStreamData ()//迭代帧：流相关帧（如 RST_STREAM）转发到 stream->Recv；
          or sub.IsStreamBlocked () or sub.IsStopSending ()
          or sub.IsStream ())
        {
          Ptr<QuicStreamBase> stream = SearchStream (sub.GetStreamId ());

          if (stream != nullptr
              and (stream->GetStreamDirectionType () == QuicStream::RECEIVER
                   or stream->GetStreamDirectionType ()
                   == QuicStream::BIDIRECTIONAL))
            {
              NS_LOG_INFO (
                "Receiving frame on stream " << stream->GetStreamId () <<
                  " trigger stream");
              stream->Recv ((*it).first, sub, address);
            }
        }
      else//其他（如 ACK）转发到 m_socket->OnReceivedFrame。
        {
          NS_LOG_INFO (
            "Receiving frame on stream " << sub.GetStreamId () <<
              " trigger socket");
          m_socket->OnReceivedFrame (sub);
        }
    }

  // trigger ACK TX if the received packet was not ACK-only
  return !onlyAckFrames;
}

int
QuicL5Protocol::Send (Ptr<Packet> frame)//在quic-stream中调用 1、 int size = m_quicl5->Send (frame);           2、   m_quicl5->Send (maxStream);
{
  NS_LOG_FUNCTION (this);

  return m_socket->AppendingTx (frame);
}
//由流调用，将接收帧追加到套接字的接收队列。
int
QuicL5Protocol::Recv (Ptr<Packet> frame, Address &address)
{
  NS_LOG_FUNCTION (this);

  m_socket->AppendingRx (frame, address);

  return frame->GetSize ();
}
//将发送数据包分片成多个片段，均衡分配到流。
std::vector<Ptr<Packet> >
QuicL5Protocol::DisgregateSend (Ptr<Packet> data)
{
  NS_LOG_FUNCTION (this);

  uint32_t dataSizeByte = data->GetSize ();
  std::vector< Ptr<Packet> > disgregated;
  //data->Print(std::cout);

  // Equally distribute load on all streams except on stream 0
  uint32_t loadPerStream = dataSizeByte / (m_streams.size () - 1);
  uint32_t remainingLoad = dataSizeByte - loadPerStream * (m_streams.size () - 1);
  if (loadPerStream < 1)
    {
      loadPerStream = 1;
    }

  for (uint32_t start = 0; start < dataSizeByte; start += loadPerStream)
    {
      if (remainingLoad > 0 && start + remainingLoad == dataSizeByte)
        {
          Ptr<Packet> remainingfragment = data->CreateFragment (
            start, remainingLoad);
          disgregated.push_back (remainingfragment);
        }
      else
        {
          Ptr<Packet> fragment = data->CreateFragment (start, loadPerStream);
          disgregated.push_back (fragment);
        }

    }

  return disgregated;
}
//将接收数据包分解成帧-子头对。
std::vector< std::pair<Ptr<Packet>, QuicSubheader> >
QuicL5Protocol::DisgregateRecv (Ptr<Packet> data)
{
  NS_LOG_FUNCTION (this);

  uint32_t dataSizeByte = data->GetSize ();
  std::vector< std::pair<Ptr<Packet>, QuicSubheader> > disgregated;
  NS_LOG_INFO ("DisgregateRecv for a packet with size " << dataSizeByte);
  //data->Print(std::cout);

  // the packet could contain multiple frames
  // each of them starts with a subheader
  // cycle through the data packet and extract the frames
  for (uint32_t start = 0; start < dataSizeByte; )
    {
      QuicSubheader sub;
      data->RemoveHeader (sub);
      NS_LOG_INFO ("subheader " << sub << " dataSizeByte " << dataSizeByte
                                << " remaining " << data->GetSize () << " frame size " << sub.GetLength ());
      Ptr<Packet> remainingfragment = data->CreateFragment (0, sub.GetLength ());
      NS_LOG_INFO ("fragment size " << remainingfragment->GetSize ());

      // remove the first portion of the packet
      data->RemoveAtStart (sub.GetLength ());
      start += sub.GetSerializedSize () + sub.GetLength ();
      disgregated.push_back (std::make_pair (remainingfragment, sub));
    }


  return disgregated;
}

Ptr<QuicStreamBase>
QuicL5Protocol::SearchStream (uint64_t streamId)
{
  NS_LOG_FUNCTION (this);
  std::vector<Ptr<QuicStreamBase> >::iterator it = m_streams.begin ();
  Ptr<QuicStreamBase> stream;
  while (it != m_streams.end ())
    {
      if ((*it)->GetStreamId () == streamId)
        {
          stream = *it;
          break;
        }
      ++it;
    }
  return stream;
}
/*///////////////////////////////////////////////////////////////////////////////////

SET函数群
SetNode
SetConnectionId//设置连接 ID，用于标识 QUIC 连接。
GetMaxPacketSize获取最大包大小
ContainsTransportParameters//检查最近接收包是否含传输参数。
OnReceivedTransportParameters//转发接收的传输参数到套接字。
SignalAbortConnection//信号中断连接。
*/
//关联 QUIC 套接字到该协议栈，用于后续发送/接收操作。
void
QuicL5Protocol::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_node = node;
}
//设置连接 ID，用于标识 QUIC 连接。
void
QuicL5Protocol::SetConnectionId (uint64_t connId)
{
  NS_LOG_FUNCTION (this << connId);
  m_connectionId = connId;
}
//获取最大包大小
uint16_t
QuicL5Protocol::GetMaxPacketSize () const
{
  return m_socket->GetSegSize ();
}
//检查最近接收包是否含传输参数。
bool
QuicL5Protocol::ContainsTransportParameters ()
{
  return m_socket->CouldContainTransportParameters ();
}
//转发接收的传输参数到套接字。
void
QuicL5Protocol::OnReceivedTransportParameters (
  QuicTransportParameters transportParameters)
{
  m_socket->OnReceivedTransportParameters (transportParameters);
}
//信号中断连接。
void
QuicL5Protocol::SignalAbortConnection (uint16_t transportErrorCode,
                                       const char* reasonPhrase)
{
  NS_LOG_FUNCTION (this);
  m_socket->AbortConnection (transportErrorCode, reasonPhrase);
}
/*///////////////////////////////////////////////////////////////////////////////////

*////////////////////////////////////////////////////////////////////////////////////
//更新所有流的初始 maxStreamData。
void
QuicL5Protocol::UpdateInitialMaxStreamData (uint32_t newMaxStreamData)
{
  NS_LOG_FUNCTION (this << newMaxStreamData);

  // TODO handle in a different way bidirectional and unidirectional streams
  for (auto stream : m_streams)
    {
      if (stream->GetStreamId () > 0) // stream 0 is set to UINT32_MAX and not modified
        {
          stream->SetMaxStreamData (newMaxStreamData);
        }
    }
}
//GetMaxData ()
uint64_t
QuicL5Protocol::GetMaxData ()
{
  NS_LOG_FUNCTION (this);

  uint64_t maxData = 0;
  for (auto stream : m_streams)
    {
      maxData += stream->SendMaxStreamData ();
    }
  return maxData;
}

} // namespace ns3

