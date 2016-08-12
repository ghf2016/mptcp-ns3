/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Sussex
 * Copyright (c) 2015 Université Pierre et Marie Curie (UPMC)
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
 * Author:  Matthieu Coudron <matthieu.coudron@lip6.fr>
 *          Morteza Kheirkhah <m.kheirkhah@sussex.ac.uk>
 */
#include "ns3/mptcp-scheduler-fastest-rtt.h"
#include "ns3/mptcp-subflow.h"
#include "ns3/mptcp-meta-socket.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("MpTcpSchedulerFastestRTT");

namespace ns3
{
  
TypeId
MpTcpSchedulerFastestRTT::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MpTcpSchedulerFastestRTT")
                              .SetParent<MpTcpScheduler> ()
                              .AddConstructor<MpTcpSchedulerFastestRTT> ()
                      ;
  
  return tid;
}


MpTcpSchedulerFastestRTT::MpTcpSchedulerFastestRTT() :  MpTcpScheduler()
                                                      , m_metaSock(0)
{
  NS_LOG_FUNCTION(this);
}

MpTcpSchedulerFastestRTT::~MpTcpSchedulerFastestRTT (void)
{
  NS_LOG_FUNCTION(this);
}


void
MpTcpSchedulerFastestRTT::SetMeta(Ptr<MpTcpMetaSocket> metaSock)
{
  NS_ASSERT(metaSock);
  NS_ASSERT_MSG(m_metaSock == 0, "SetMeta already called");
  m_metaSock = metaSock;
  //  m_firstUnmappedDsn = m_metaSock->m_txBuffer->HeadSequence();
}

int
MpTcpSchedulerFastestRTT::FindFastestSubflowWithFreeWindow() const
{
  NS_LOG_FUNCTION_NOARGS();
  int id = -1;
  Time lowestEstimate = Time::Max();
  
  for(int i = 0; i < (int) m_metaSock->GetNActiveSubflows() ; i++ )
  {
    Ptr<MpTcpSubflow> sf = m_metaSock->GetActiveSubflow(i);
    if(sf->AvailableWindow() <= 0)
    {
      continue;
    }
    
    if(sf->GetRttEstimator()->GetEstimate () < lowestEstimate)
    {
      //!
      lowestEstimate = sf->GetRttEstimator()->GetEstimate ();
      id = i;
    }
  }
  return id;
}

Ptr<MpTcpSubflow>
MpTcpSchedulerFastestRTT::GetAvailableControlSubflow()
{
  NS_ASSERT(m_metaSock->GetNActiveSubflows() > 0 );
  return  m_metaSock->GetActiveSubflow(0);
  //  m_lastUsedFlowId = (m_lastUsedFlowId + 1) % m_metaSock->GetNActiveSubflows();
  //  return m_metaSock->GetSubFlow(m_lastUsedFlowId);
}
  
Ptr<MpTcpSubflow> MpTcpSchedulerFastestRTT::GetAvailableSubflow (uint32_t dataToSend, uint32_t metaWindow)
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT(m_metaSock);
  
  //int nbOfSubflows = m_metaSock->GetNActiveSubflows();
  //int attempt = 0;
  
  int id = FindFastestSubflowWithFreeWindow();
  
  if(id < 0)
  {
    NS_LOG_DEBUG("No valid subflow");
    return nullptr;
  }
  
  //TODO: need to get next available subflow if this one cannot send data
  
  Ptr<MpTcpSubflow> subflow = m_metaSock->GetActiveSubflow(id);
  uint32_t subflowWindow = subflow->AvailableWindow();
  
  NS_LOG_DEBUG("subflow AvailableWindow  [" << subflowWindow << "]");
  uint32_t canSend = std::min( subflowWindow, metaWindow);
  //uint32_t canSend = subflowWindow;
  
  if(canSend > 0 && subflow->CanSendPendingData(canSend))
  {
    return subflow;
  }
  
  NS_LOG_DEBUG("No subflow available");
  return nullptr;
  
}



// TODO this will be solved
// We assume scheduler can't send data on subflows, so it can just generate mappings
//std::vector< std::pair<uint8_t, MappingList>
// std::pair< start,size , subflow>
// ca génère les mappings ensuite
bool
MpTcpSchedulerFastestRTT::GenerateMapping(int& activeSubflowArrayId, SequenceNumber64& dsn, uint16_t& length)
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT(m_metaSock);
  
  //!
  size_t nbOfSubflows = m_metaSock->GetNActiveSubflows();
  int attempt = 0;
  uint32_t amountOfDataToSend = 0;
  
  //! Tx data not sent to subflows yet
  SequenceNumber32 metaNextTxSeq = m_metaSock->GetNextTxSequence();
  
  amountOfDataToSend = m_metaSock->GetTxBuffer()->SizeFromSequence(metaNextTxSeq);
  
  uint32_t metaWindow = m_metaSock->AvailableWindow();
  
  NS_LOG_DEBUG("TxData to send=" << amountOfDataToSend  << "; Available meta window=" << metaWindow);
  if(amountOfDataToSend <= 0)
  {
    NS_LOG_DEBUG("Nothing to send from meta");
    return false;
  }
  
  
  if(metaWindow <= 0)
  {
    NS_LOG_DEBUG("No meta window available (TODO should be in persist state ?)");
    return false; // TODO ?
  }
  
  int id = FindFastestSubflowWithFreeWindow();
  
  if(id < 0)
  {
    NS_LOG_DEBUG("No valid subflow");
    return false;
  }
  //    while(attempt < nbOfSubflows)
  //    {
  //        attempt++;
  //        m_lastUsedFlowId = (m_lastUsedFlowId + 1) % nbOfSubflows;
  Ptr<MpTcpSubflow> subflow = m_metaSock->GetActiveSubflow(id);
  uint32_t subflowWindow = subflow->AvailableWindow();
  
  NS_LOG_DEBUG("subflow AvailableWindow  [" << subflowWindow << "]");
  //uint32_t canSend = std::min( subflowWindow, metaWindow);
  uint32_t canSend = subflowWindow;
  
  //! Can't send more than SegSize
  //metaWindow en fait on s'en fout du SegSize ?
  if(canSend > 0 && subflow->CanSendPendingData(canSend))
  {
    
    // activeSubflowArrayId = m_lastUsedFlowId;
    dsn = metaNextTxSeq;
    canSend = std::min(canSend, amountOfDataToSend);
    // For now we limit ourselves to a per packet basis
    UintegerValue segmentSize;
    subflow->GetAttribute("SegmentSize", segmentSize);
    length = std::min(canSend, uint32_t(segmentSize.Get()));
    return true;
  }
  //    }
  NS_LOG_DEBUG("");
  return false;
}
  
} // namespace ns3

