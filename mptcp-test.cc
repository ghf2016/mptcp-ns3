//
//  mptcp-test.cc
//  ns3
//
//  Created by Lynne Salameh on 17/6/16.
//  Copyright © 2016 University College London. All rights reserved.
//

#include <stdio.h>
#include <string>
#include <limits>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace ns3;

void TraceMacRx(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  PppHeader pppHeader;
  Ipv4Header ipheader;
  TcpHeader tcpHeader;
  
  Ptr<Packet> copy = packet->Copy();
  copy->RemoveHeader(pppHeader);
  
  if (pppHeader.GetProtocol() == 0x0021)
  {
    copy->RemoveHeader(ipheader);
    
    if (ipheader.GetProtocol() == TcpL4Protocol::PROT_NUMBER)
    {
      copy->RemoveHeader(tcpHeader);
      
      //Ignore SYN packets
      if (!(tcpHeader.GetFlags() & TcpHeader::SYN)){
        
        (*stream->GetStream()) << Simulator::Now().GetNanoSeconds() << " 0 1 "
        << tcpHeader.GetSequenceNumber() << " " << tcpHeader.GetAckNumber() << " " << copy->GetSize() << endl;
      }
    }
  }
}

void TraceMacTx(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  PppHeader pppHeader;
  Ipv4Header ipheader;
  TcpHeader tcpHeader;
  
  Ptr<Packet> copy = packet->Copy();
  copy->RemoveHeader(pppHeader);
  
  if (pppHeader.GetProtocol() == 0x0021)
  {
    copy->RemoveHeader(ipheader);
    
    if (ipheader.GetProtocol() == TcpL4Protocol::PROT_NUMBER)
    {
      copy->RemoveHeader(tcpHeader);
      
      //Ignore SYN packets
      if (!(tcpHeader.GetFlags() & TcpHeader::SYN)){
        
        (*stream->GetStream()) << Simulator::Now().GetNanoSeconds() << " 1 1 "
        << tcpHeader.GetSequenceNumber() << " " << tcpHeader.GetAckNumber() << " " << copy->GetSize() << endl;
      }
    }
  }
}


void TraceMacTxDrops(Ptr<OutputStreamWrapper> stream, Ptr<PointToPointNetDevice> device, Ptr<const Packet> packet)
{
  PppHeader pppHeader;
  Ipv4Header ipheader;
  TcpHeader tcpHeader;
  
  Ptr<Packet> copy = packet->Copy();
  copy->RemoveHeader(pppHeader);
  
  if (pppHeader.GetProtocol() == 0x0021)
  {
    copy->RemoveHeader(ipheader);
    
    if (ipheader.GetProtocol() == TcpL4Protocol::PROT_NUMBER)
    {
      copy->RemoveHeader(tcpHeader);
      
      //Ignore SYN packets
      if (!(tcpHeader.GetFlags() & TcpHeader::SYN)){
        
        (*stream->GetStream()) << Simulator::Now().GetNanoSeconds() << " " << device->GetNode()->GetId() << " " << device->GetIfIndex() << " "
        << tcpHeader.GetSequenceNumber() << " " << tcpHeader.GetAckNumber() << endl;
      }
    }
  }
}

void CheckAndCreateDirectory(string path)
{
  if(access(path.c_str(), F_OK ) == -1 ){
    const int error = mkdir(path.c_str(), S_IRWXU | S_IRWXG |  S_IROTH);
    
    if(error == -1){
      NS_FATAL_ERROR("Could not create directory " << path);
    }
  }
}


NetDeviceContainer PointToPointCreate(Ptr<Node> startNode,
                                      Ptr<Node> endNode,
                                      string rateMbps,
                                      string delayMs)
{
  
  NodeContainer linkedNodes;
  linkedNodes.Add(startNode);
  linkedNodes.Add(endNode);
  
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (rateMbps));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delayMs));
  
  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "MeanPktSize", UintegerValue(1496),
                           "LinkBandwidth", DataRateValue(rateMbps),
                           "LinkDelay", StringValue(delayMs));
  
  pointToPoint.SetQueue("ns3::DropTailQueue",
                        "MaxPackets", UintegerValue(1));
  
  NetDeviceContainer linkedDevices;
  linkedDevices = pointToPoint.Install (linkedNodes);
  
  tchRed.Install(linkedDevices);
  
  return linkedDevices;
}

int main(int argc, char* argv[])
{
  
  //LogComponentEnable("TcpSocketBase", LOG_LEVEL_ALL);
  
  string linkRate = "10Mbps";
  string linkDelay = "10ms";
  
  //The bandwidth delay product
  uint32_t bdp = DataRate(linkRate).GetBitRate() * Time(linkDelay).GetSeconds() * 4;
  uint32_t bdpBytes = bdp/8;
  
  //For now let's assume the maximum size of mptcp option is 30 bytes
  uint32_t tcpOptionSize = 30;
  uint32_t segmentSizeWithoutHeaders = 1452 - tcpOptionSize;
  uint32_t segmentSize = 1500;
  
  //TCP configuration
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (segmentSizeWithoutHeaders));
  
  //Slow start threshold
  Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(numeric_limits<uint32_t>::max()));
  
  //Set the receive window size to the maximum possible
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(1<<30));
  
  //Disable the timestamp option
  Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(false));
  
  //Set the mptcp option
  Config::SetDefault("ns3::TcpSocketBase::EnableMpTcp", BooleanValue(true));
  
  //Set the initial congestion window to be larger than duplicate ack threshold
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(4));
  
  uint32_t queueSize = max<uint32_t>(bdpBytes, 10 * segmentSize);
  
  //Config::SetDefault("ns3::Queue::Mode", EnumValue(Queue::QUEUE_MODE_BYTES));
  //Config::SetDefault("ns3::Queue::MaxBytes", UintegerValue(queueSize));
  
  Config::SetDefault("ns3::RedQueueDisc::Mode", EnumValue(Queue::QUEUE_MODE_BYTES));
  Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(queueSize * 0.8));
  Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(queueSize));
  Config::SetDefault("ns3::RedQueueDisc::QueueLimit", UintegerValue(queueSize));
  
  //Set the send buffer to be larger than double the bdp
  //Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (queueSize * 3));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (numeric_limits<uint32_t>::max()));
  
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue(Seconds(2.0)));
  Config::SetDefault("ns3::ArpCache::AliveTimeout", TimeValue(Seconds(120 + 1)));
  
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpCubic::GetTypeId()));
  
  
  NodeContainer client;
  client.Create(1);
  
  //Create the internet stack helper, and install the internet stack on the client node
  InternetStackHelper stackHelper;
  //Set the routing protocol to global routing only (we don't use static routing for now)
  Ipv4ListRoutingHelper listRoutingHelper;
  Ipv4GlobalRoutingHelper globalRoutingHelper;
  listRoutingHelper.Add(globalRoutingHelper, 10);
  
  stackHelper.SetRoutingHelper(listRoutingHelper);
  //stackHelper.SetTcp("ns3::MptcpL4Protocol");
  
  stackHelper.Install(client);
  
  NodeContainer aSwitch;
  aSwitch.Create(1);
  stackHelper.Install(aSwitch);
  
  //Create the address helper
  Ipv4AddressHelper addressHelper;
  addressHelper.SetBase("10.10.0.0", "255.255.255.0");
  
  
  //Create a link between the switch and the client, assign IP addresses
  NetDeviceContainer linkedDevices = PointToPointCreate(client.Get(0), aSwitch.Get(0), "10Mbps", "10ms");
  Ipv4InterfaceContainer clientFacingInterfaces = addressHelper.Assign(linkedDevices);
  
  //Create the servers and install the internet stack on them
  NodeContainer server;
  server.Create(1);
  stackHelper.Install(server);
  
  NetDeviceContainer devices = PointToPointCreate(server.Get(0), aSwitch.Get(0), "20Mbps", "10ms");
  Ipv4InterfaceContainer interfaces = addressHelper.Assign(devices);
  
  int portNum = 4000;
  Address remoteAddress(InetSocketAddress(clientFacingInterfaces.GetAddress(0), portNum));
  
  //Install the bulk send
  OnOffHelper onOff("ns3::TcpSocketFactory", remoteAddress);
  
  //BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
  
  onOff.SetConstantRate(DataRate("20Mbps"), segmentSizeWithoutHeaders);
  //onOff.SetConstantRate(DataRate("200Kbps"), 2565);
  //bulkSend.SetAttribute("SendSize", UintegerValue(10e6));
  //bulkSend.SetAttribute("MaxBytes", UintegerValue(100e6));
  //bulkSend.Install(server);
  onOff.Install(server);
  
  PacketSinkHelper packetSink("ns3::TcpSocketFactory", remoteAddress);
  packetSink.Install(client);
  
  //Create an output directory
  string outputDir = "mptcp_test";
  CheckAndCreateDirectory(outputDir);
  
  stringstream devicePath;
  devicePath << "/NodeList/" << client.Get(0)->GetId() << "/DeviceList/1/$ns3::PointToPointNetDevice/";
  //devicePath << "/NodeList/0/DeviceList/*/$ns3::PointToPointNetDevice/";
  
  stringstream tfile;
  tfile << outputDir << "/mptcp_client";
  Ptr<OutputStreamWrapper> throughputFile = Create<OutputStreamWrapper>(tfile.str(), std::ios::out);
  //Write the column labels into the file
  *(throughputFile->GetStream()) << "timestamp send connection seqno ackno size" << endl;
  
  Config::ConnectWithoutContext(devicePath.str() + "PhyRxEnd", MakeBoundCallback(TraceMacRx, throughputFile));
  Config::ConnectWithoutContext(devicePath.str() + "PhyTxBegin", MakeBoundCallback(TraceMacTx, throughputFile));
  
  
  uint32_t serverId = server.Get(0)->GetId();
  
  
  devicePath.str("");
  devicePath << "/NodeList/" << serverId << "/DeviceList/*/$ns3::PointToPointNetDevice/";
  
  //Config::Set(devicePath.str() + "TxQueue/MinTh", DoubleValue(bdpBytes * 2 * 0.8));
  //Config::Set(devicePath.str() + "TxQueue/MaxTh", DoubleValue(bdpBytes * 2));
  //Config::Set(devicePath.str() + "TxQueue/QueueLimit", UintegerValue(bdpBytes * 2));
  
  stringstream sfile;
  sfile << outputDir << "/mptcp_server";
  Ptr<OutputStreamWrapper> serverFile = Create<OutputStreamWrapper>(sfile.str(), std::ios::out);
  //Write the column labels into the file
  *(serverFile->GetStream()) << "timestamp send connection seqno ackno size" << endl;
  Config::ConnectWithoutContext(devicePath.str() + "PhyTxBegin", MakeBoundCallback(TraceMacTx, serverFile));
  Config::ConnectWithoutContext(devicePath.str() + "PhyRxEnd", MakeBoundCallback(TraceMacRx, serverFile));
  
  /*stringstream dfile;
  dfile << outputDir << "/mptcp_drops";
  Ptr<OutputStreamWrapper> dropsFile = Create<OutputStreamWrapper>(dfile.str(), std::ios::out);
  //Write the column labels into the file
  *(dropsFile->GetStream()) << "timestamp node device seqno ackno" << endl;
  
  devicePath.str("");*/
 // devicePath << "/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/";
  //Config::ConnectWithoutContext(devicePath.str() + "PhyTxDrop", MakeBoundCallback(TraceMacTxDrops, dropsFile));
  
  
  uint32_t clientId = client.Get(0)->GetId();
  uint32_t switchId = aSwitch.Get(0)->GetId();
  
  
  cout << "client node is " << clientId << " switch id " << switchId << " server id " << serverId << endl;
  
  //Populate the IP routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  
  //LogComponentEnable("MptcpCongestionOps", LOG_LEVEL_ALL);
  //LogComponentEnable("MptcpL4Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable("MptcpMetaSocket", LOG_LEVEL_ALL);
  //LogComponentEnable("MpTcpMetaSocket", LOG_LEVEL_ALL);
  //LogComponentEnable("MpTcpSubflow", LOG_LEVEL_ALL);
  
  //Set the simulator stop time
  Simulator::Stop (Seconds(20.0));
  
  //Begin the simulation
  Simulator::Run ();
  Simulator::Destroy ();
  
  return 0;
}