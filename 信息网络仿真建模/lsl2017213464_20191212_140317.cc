/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */

//
// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --distance=500"
// ./waf --run "wifi-simple-adhoc --distance=1000"
// ./waf --run "wifi-simple-adhoc --distance=1500"
//
// The source node and sink node can be changed like this:
//
// ./waf --run "wifi-simple-adhoc --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//
#include <ctime>
#include <sstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/energy-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-helper.h"//5.3.2，添加aodv协议
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/temp.h"
#include "ns3/netanim-module.h" //为了netanim可视化添加
using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");




void TxCallback(Ptr<CounterCalculator<uint32_t>>datac,
                std::string path,Ptr<const Packet> packet){
        NS_LOG_INFO("Sent frame counted in"<<datac->GetKey());
        datac->Update();
}





void ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet!");
    }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 2000;  // m提速，改2000
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;


  //uint32_t numNodes = 25;  // by default, 5x5
  uint32_t numNodes = 100;  //5.1.1修改数量，10*10
  
  
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  double interval = 1.0; // seconds
  bool verbose = false;
  bool tracing = false;



  std::string format("omnet");
  std::string experiment("NS3-test-third");
  std::string strategy("wifi-default");
  std::string input;
  std::string runID;



  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  //5.2 Wifi和信道参数设定 5.2.1基本组成
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (2.0));// Reception gain (dB).
  wifiPhy.Set ("TxGain", DoubleValue (0.0));  // Transmission gain (dB)
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-62.0));
  //CCA The energy of a non Wi-Fi received signal should be higher than this threshold (dbm) to allow the PHY layer to declare CCA BUSY state. This check is performed on the 20 MHz primary channel only.
  wifiPhy.Set ("TxPowerStart", DoubleValue(16.0206));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(16.0206));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-101.0));
  // The energy of a received signal should be higher than this threshold (dbm) to allow the PHY layer to detect the signal.

  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");


  /*change LossModle to expand the distance */
  /*
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel"),
                                                        "Exponent",DoubleValue(3.0),
                                                        "ReferenceDistance",DoubleValue(100),
                                                        "ReferenceLoss",DoubleValue(86.6779));
  */



  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  //5.1.2修改位置和运动模型
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",//网格位置分配
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),//间距
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (/*5*/10),//一行节点数量
                                 "LayoutType", StringValue ("RowFirst"));
  //mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  
  //mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");//（1）节点静止
 /* mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
		  		"Bounds",RectangleValue(Rectangle(-50000,50000,-50000,50000)),
				"Speed",StringValue("ns3::UniformRandomVariable[Min=200.0|Max=1000.0]" )
);//（2）节点运动RandomWalk2dMobilityModel*/
  
  mobility.Install (c);





  //set xieyi


  // Enable OLSR
  OlsrHelper olsr;
 // AodvHelper aodv;//5.3.2添加aodv协议
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);
 // list.Add(aodv,10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  //internet.SetRoutingHelper(olsr);//another method to set the "olsr" routing
  internet.Install (c);







  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (sinkNode), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
  source->Connect (remote);




//traffic_
//set send node and receive node0-9**90-99
NS_LOG_INFO("Create traffic source & sink.");
Ptr<Node>appSource=NodeList::GetNode(0);
Ptr<Sender>sender =CreateObject<Sender>();
appSource->AddApplication(sender);
sender->SetStartTime(Seconds(1));

Ptr<Node>appSink=NodeList::GetNode(90);
Ptr<Receiver>receiver1=CreateObject<Receiver>();
appSink->AddApplication(receiver1);
receiver1->SetStartTime(Seconds(0));

Config::Set("/NodeList/0/ApplicationList/*/$Sender/Destination",
            Ipv4AddressValue("10.1.1.91"));

NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource2 = NodeList::GetNode (1);
  Ptr<Sender> sender2 = CreateObject<Sender>();
  appSource2->AddApplication (sender2);
  sender2->SetStartTime (Seconds (3));

  Ptr<Node> appSink2 = NodeList::GetNode (91);
  Ptr<Receiver> receiver2 = CreateObject<Receiver>();
  appSink2->AddApplication (receiver2);
  receiver2->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/1/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.92"));

  NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource3 = NodeList::GetNode (2);
  Ptr<Sender> sender3 = CreateObject<Sender>();
  appSource3->AddApplication (sender3);
  sender3->SetStartTime (Seconds (5));

  Ptr<Node> appSink3 = NodeList::GetNode (92);
  Ptr<Receiver> receiver3 = CreateObject<Receiver>();
  appSink3->AddApplication (receiver3);
  receiver3->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/2/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.93"));

 NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource4 = NodeList::GetNode (3);
  Ptr<Sender> sender4 = CreateObject<Sender>();
  appSource4->AddApplication (sender4);
  sender4->SetStartTime (Seconds (7));

  Ptr<Node> appSink4 = NodeList::GetNode (93);
  Ptr<Receiver> receiver4 = CreateObject<Receiver>();
  appSink4->AddApplication (receiver4);
  receiver4->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/3/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.94"));

NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource5 = NodeList::GetNode (4);
  Ptr<Sender> sender5 = CreateObject<Sender>();
  appSource5->AddApplication (sender5);
  sender5->SetStartTime (Seconds (9));

  Ptr<Node> appSink5 = NodeList::GetNode (94);
  Ptr<Receiver> receiver5 = CreateObject<Receiver>();
  appSink5->AddApplication (receiver5);
  receiver5->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/4/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.95"));
  
NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource6 = NodeList::GetNode (5);
  Ptr<Sender> sender6 = CreateObject<Sender>();
  appSource6->AddApplication (sender6);
  sender6->SetStartTime (Seconds (11));

  Ptr<Node> appSink6 = NodeList::GetNode (95);
  Ptr<Receiver> receiver6 = CreateObject<Receiver>();
  appSink6->AddApplication (receiver6);
  receiver6->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/5/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.96"));

NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource7 = NodeList::GetNode (6);
  Ptr<Sender> sender7 = CreateObject<Sender>();
  appSource7->AddApplication (sender7);
  sender7->SetStartTime (Seconds (13));

  Ptr<Node> appSink7 = NodeList::GetNode (96);
  Ptr<Receiver> receiver7 = CreateObject<Receiver>();
  appSink7->AddApplication (receiver7);
  receiver7->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/6/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.97"));

NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource8 = NodeList::GetNode (7);
  Ptr<Sender> sender8 = CreateObject<Sender>();
  appSource8->AddApplication (sender8);
  sender8->SetStartTime (Seconds (15));

  Ptr<Node> appSink8 = NodeList::GetNode (97);
  Ptr<Receiver> receiver8 = CreateObject<Receiver>();
  appSink8->AddApplication (receiver8);
  receiver8->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/7/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.98"));

NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource9 = NodeList::GetNode (8);
  Ptr<Sender> sender9 = CreateObject<Sender>();
  appSource9->AddApplication (sender9);
  sender9->SetStartTime (Seconds (17));

  Ptr<Node> appSink9 = NodeList::GetNode (98);
  Ptr<Receiver> receiver9 = CreateObject<Receiver>();
  appSink9->AddApplication (receiver9);
  receiver9->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/8/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.99"));
  
 NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSource10 = NodeList::GetNode (9);
  Ptr<Sender> sender10 = CreateObject<Sender>();
  appSource10->AddApplication (sender10);
  sender10->SetStartTime (Seconds (19));

  Ptr<Node> appSink10 = NodeList::GetNode (99);
  Ptr<Receiver> receiver10 = CreateObject<Receiver>();
  appSink10->AddApplication (receiver10);
  receiver10->SetStartTime (Seconds (0));

  Config::Set ("/NodeList/9/ApplicationList/*/$Sender/Destination",
               Ipv4AddressValue ("10.1.1.100"));



DataCollector data;
data.DescribeRun(experiment,strategy,input,runID);
data.AddMetadata("author","lishuolin2017213464");








//count frame of send node 0-9
 Ptr<CounterCalculator<uint32_t> > totalTx1 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx1->SetKey ("wifi-tx-frames");
  totalTx1->SetContext ("node[0]");
  Config::Connect ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx1));
  data.AddDataCalculator (totalTx1);

  Ptr<CounterCalculator<uint32_t> > totalTx2 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx2->SetKey ("wifi-tx-frames");
  totalTx2->SetContext ("node[1]");
  Config::Connect ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx2));
  data.AddDataCalculator (totalTx2);

  Ptr<CounterCalculator<uint32_t> > totalTx3 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx3->SetKey ("wifi-tx-frames");
  totalTx3->SetContext ("node[2]");
  Config::Connect ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx3));
  data.AddDataCalculator (totalTx3);

  Ptr<CounterCalculator<uint32_t> > totalTx4 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx4->SetKey ("wifi-tx-frames");
  totalTx4->SetContext ("node[3]");
  Config::Connect ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx4));
  data.AddDataCalculator (totalTx4);

  Ptr<CounterCalculator<uint32_t> > totalTx5 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx5->SetKey ("wifi-tx-frames");
  totalTx5->SetContext ("node[4]");
  Config::Connect ("/NodeList/4/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx5));
  data.AddDataCalculator (totalTx5);


   Ptr<CounterCalculator<uint32_t> > totalTx6 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx6->SetKey ("wifi-tx-frames");
  totalTx6->SetContext ("node[5]");
  Config::Connect ("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx6));
  data.AddDataCalculator (totalTx6);


    Ptr<CounterCalculator<uint32_t> > totalTx7 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx7->SetKey ("wifi-tx-frames");
  totalTx7->SetContext ("node[6]");
  Config::Connect ("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx7));
  data.AddDataCalculator (totalTx7);


    Ptr<CounterCalculator<uint32_t> > totalTx8 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx8->SetKey ("wifi-tx-frames");
  totalTx8->SetContext ("node[7]");
  Config::Connect ("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx8));
  data.AddDataCalculator (totalTx8);


    Ptr<CounterCalculator<uint32_t> > totalTx9 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx9->SetKey ("wifi-tx-frames");
  totalTx9->SetContext ("node[8]");
  Config::Connect ("/NodeList/8/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx9));
  data.AddDataCalculator (totalTx9);

    Ptr<CounterCalculator<uint32_t> > totalTx10 =
    CreateObject<CounterCalculator<uint32_t> >();
  totalTx10->SetKey ("wifi-tx-frames");
  totalTx10->SetContext ("node[9]");
  Config::Connect ("/NodeList/9/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                   MakeBoundCallback (&TxCallback, totalTx10));
  data.AddDataCalculator (totalTx10);




//count frame of receive node 0-9
/*_______________________----------____________________*/


 Ptr<PacketCounterCalculator> totalRx1 =
    CreateObject<PacketCounterCalculator>();
  totalRx1->SetKey ("wifi-rx-frames");
  totalRx1->SetContext ("node[90]");
  Config::Connect ("/NodeList/90/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx1));
  data.AddDataCalculator (totalRx1);

  Ptr<PacketCounterCalculator> totalRx2 =
    CreateObject<PacketCounterCalculator>();
  totalRx2->SetKey ("wifi-rx-frames");
  totalRx2->SetContext ("node[91]");
  Config::Connect ("/NodeList/91/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx2));
  data.AddDataCalculator (totalRx2);

 Ptr<PacketCounterCalculator> totalRx3 =
    CreateObject<PacketCounterCalculator>();
  totalRx3->SetKey ("wifi-rx-frames");
  totalRx3->SetContext ("node[92]");
  Config::Connect ("/NodeList/92/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx3));
  data.AddDataCalculator (totalRx3);

 Ptr<PacketCounterCalculator> totalRx4 =
    CreateObject<PacketCounterCalculator>();
  totalRx4->SetKey ("wifi-rx-frames");
  totalRx4->SetContext ("node[93]");
  Config::Connect ("/NodeList/93/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx4));
  data.AddDataCalculator (totalRx4);

 Ptr<PacketCounterCalculator> totalRx5 =
    CreateObject<PacketCounterCalculator>();
  totalRx5->SetKey ("wifi-rx-frames");
  totalRx5->SetContext ("node[94]");
  Config::Connect ("/NodeList/94/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx5));
  data.AddDataCalculator (totalRx5);

  Ptr<PacketCounterCalculator> totalRx6 =
    CreateObject<PacketCounterCalculator>();
  totalRx6->SetKey ("wifi-rx-frames");
  totalRx6->SetContext ("node[95]");
  Config::Connect ("/NodeList/95/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx6));
  data.AddDataCalculator (totalRx6);

 Ptr<PacketCounterCalculator> totalRx7 =
    CreateObject<PacketCounterCalculator>();
  totalRx7->SetKey ("wifi-rx-frames");
  totalRx7->SetContext ("node[96]");
  Config::Connect ("/NodeList/96/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx7));
  data.AddDataCalculator (totalRx7);

Ptr<PacketCounterCalculator> totalRx8 =
    CreateObject<PacketCounterCalculator>();
  totalRx8->SetKey ("wifi-rx-frames");
  totalRx8->SetContext ("node[97]");
  Config::Connect ("/NodeList/97/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx8));
  data.AddDataCalculator (totalRx8);

Ptr<PacketCounterCalculator> totalRx9 =
    CreateObject<PacketCounterCalculator>();
  totalRx9->SetKey ("wifi-rx-frames");
  totalRx9->SetContext ("node[98]");
  Config::Connect ("/NodeList/98/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx9));
  data.AddDataCalculator (totalRx9);

Ptr<PacketCounterCalculator> totalRx10 =
    CreateObject<PacketCounterCalculator>();
  totalRx10->SetKey ("wifi-rx-frames");
  totalRx10->SetContext ("node[99]");
  Config::Connect ("/NodeList/99/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 totalRx10));
  data.AddDataCalculator (totalRx10);









//count packet of send  node 0..9
Ptr<PacketCounterCalculator> appTx1 =
    CreateObject<PacketCounterCalculator>();
  appTx1->SetKey ("sender-tx-packets");
  appTx1->SetContext ("node[0]");
  Config::Connect ("/NodeList/0/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx1));
  data.AddDataCalculator (appTx1);

  Ptr<PacketCounterCalculator> appTx2 =
    CreateObject<PacketCounterCalculator>();
  appTx2->SetKey ("sender-tx-packets");
  appTx2->SetContext ("node[1]");
  Config::Connect ("/NodeList/1/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx2));
  data.AddDataCalculator (appTx2);

  Ptr<PacketCounterCalculator> appTx3 =
    CreateObject<PacketCounterCalculator>();
  appTx3->SetKey ("sender-tx-packets");
  appTx3->SetContext ("node[2]");
  Config::Connect ("/NodeList/2/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx3));
  data.AddDataCalculator (appTx3);

  Ptr<PacketCounterCalculator> appTx4 =
    CreateObject<PacketCounterCalculator>();
  appTx4->SetKey ("sender-tx-packets");
  appTx4->SetContext ("node[3]");
  Config::Connect ("/NodeList/3/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx4));
  data.AddDataCalculator (appTx4);

  Ptr<PacketCounterCalculator> appTx5 =
    CreateObject<PacketCounterCalculator>();
  appTx5->SetKey ("sender-tx-packets");
  appTx5->SetContext ("node[4]");
  Config::Connect ("/NodeList/4/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx5));
  data.AddDataCalculator (appTx5);

  Ptr<PacketCounterCalculator> appTx6 =
    CreateObject<PacketCounterCalculator>();
  appTx6->SetKey ("sender-tx-packets");
  appTx6->SetContext ("node[5]");
  Config::Connect ("/NodeList/5/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx6));
  data.AddDataCalculator (appTx6);

  Ptr<PacketCounterCalculator> appTx7 =
    CreateObject<PacketCounterCalculator>();
  appTx7->SetKey ("sender-tx-packets");
  appTx7->SetContext ("node[6]");
  Config::Connect ("/NodeList/6/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx7));
  data.AddDataCalculator (appTx7);

  Ptr<PacketCounterCalculator> appTx8 =
    CreateObject<PacketCounterCalculator>();
  appTx8->SetKey ("sender-tx-packets");
  appTx8->SetContext ("node[7]");
  Config::Connect ("/NodeList/7/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx8));
  data.AddDataCalculator (appTx8);

  Ptr<PacketCounterCalculator> appTx9 =
    CreateObject<PacketCounterCalculator>();
  appTx9->SetKey ("sender-tx-packets");
  appTx9->SetContext ("node[8]");
  Config::Connect ("/NodeList/8/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx9));
  data.AddDataCalculator (appTx9);

  Ptr<PacketCounterCalculator> appTx10 =
    CreateObject<PacketCounterCalculator>();
  appTx10->SetKey ("sender-tx-packets");
  appTx10->SetContext ("node[9]");
  Config::Connect ("/NodeList/9/ApplicationList/*/$Sender/Tx",
                   MakeCallback (&PacketCounterCalculator::PacketUpdate,
                                 appTx10));
  data.AddDataCalculator (appTx10);





//count packets of receive node 
/*_____________________________________________________________*/



  Ptr<CounterCalculator<> > appRx1 =
    CreateObject<CounterCalculator<> >();
  appRx1->SetKey ("receiver-rx-packets");
  appRx1->SetContext ("node[90]");
  receiver1->SetCounter (appRx1);
  data.AddDataCalculator (appRx1);

Ptr<CounterCalculator<> > appRx2 =
    CreateObject<CounterCalculator<> >();
  appRx2->SetKey ("receiver-rx-packets");
  appRx2->SetContext ("node[91]");
  receiver2->SetCounter (appRx2);
  data.AddDataCalculator (appRx2);
  
Ptr<CounterCalculator<> > appRx3 =
    CreateObject<CounterCalculator<> >();
  appRx3->SetKey ("receiver-rx-packets");
  appRx3->SetContext ("node[92]");
  receiver3->SetCounter (appRx3);
  data.AddDataCalculator (appRx3);

Ptr<CounterCalculator<> > appRx4 =
    CreateObject<CounterCalculator<> >();
  appRx4->SetKey ("receiver-rx-packets");
  appRx4->SetContext ("node[93]");
  receiver4->SetCounter (appRx4);
  data.AddDataCalculator (appRx4);

Ptr<CounterCalculator<> > appRx5 =
    CreateObject<CounterCalculator<> >();
  appRx5->SetKey ("receiver-rx-packets");
  appRx5->SetContext ("node[94]");
  receiver5->SetCounter (appRx5);
  data.AddDataCalculator (appRx5);

Ptr<CounterCalculator<> > appRx6 =
    CreateObject<CounterCalculator<> >();
  appRx6->SetKey ("receiver-rx-packets");
  appRx6->SetContext ("node[95]");
  receiver6->SetCounter (appRx6);
  data.AddDataCalculator (appRx6);

Ptr<CounterCalculator<> > appRx7 =
    CreateObject<CounterCalculator<> >();
  appRx7->SetKey ("receiver-rx-packets");
  appRx7->SetContext ("node[96]");
  receiver7->SetCounter (appRx7);
  data.AddDataCalculator (appRx7);


Ptr<CounterCalculator<> > appRx8 =
    CreateObject<CounterCalculator<> >();
  appRx8->SetKey ("receiver-rx-packets");
  appRx8->SetContext ("node[97]");
  receiver8->SetCounter (appRx8);
  data.AddDataCalculator (appRx8);


Ptr<CounterCalculator<> > appRx9 =
    CreateObject<CounterCalculator<> >();
  appRx9->SetKey ("receiver-rx-packets");
  appRx9->SetContext ("node[98]");
  receiver9->SetCounter (appRx9);
  data.AddDataCalculator (appRx9);


Ptr<CounterCalculator<> > appRx10 =
    CreateObject<CounterCalculator<> >();
  appRx10->SetKey ("receiver-rx-packets");
  appRx10->SetContext ("node[99]");
  receiver10->SetCounter (appRx10);
  data.AddDataCalculator (appRx10);




//count delay 
Ptr<TimeMinMaxAvgTotalCalculator> delayStat1 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat1->SetKey ("delay");
  delayStat1->SetContext (".");
  receiver1->SetDelayTracker (delayStat1);
  data.AddDataCalculator (delayStat1);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat2 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat2->SetKey ("delay");
  delayStat2->SetContext (".");
  receiver2->SetDelayTracker (delayStat2);
  data.AddDataCalculator (delayStat2);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat3 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat3->SetKey ("delay");
  delayStat3->SetContext (".");
  receiver3->SetDelayTracker (delayStat3);
  data.AddDataCalculator (delayStat3);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat4 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat4->SetKey ("delay");
  delayStat4->SetContext (".");
  receiver4->SetDelayTracker (delayStat4);
  data.AddDataCalculator (delayStat4);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat5 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat5->SetKey ("delay");
  delayStat5->SetContext (".");
  receiver5->SetDelayTracker (delayStat5);
  data.AddDataCalculator (delayStat5);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat6 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat6->SetKey ("delay");
  delayStat6->SetContext (".");
  receiver6->SetDelayTracker (delayStat6);
  data.AddDataCalculator (delayStat6);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat7 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat7->SetKey ("delay");
  delayStat7->SetContext (".");
  receiver7->SetDelayTracker (delayStat7);
  data.AddDataCalculator (delayStat7);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat8 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat8->SetKey ("delay");
  delayStat8->SetContext (".");
  receiver8->SetDelayTracker (delayStat8);
  data.AddDataCalculator (delayStat8);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat9 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat9->SetKey ("delay");
  delayStat9->SetContext (".");
  receiver9->SetDelayTracker (delayStat9);
  data.AddDataCalculator (delayStat9);

Ptr<TimeMinMaxAvgTotalCalculator> delayStat10 =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat10->SetKey ("delay");
  delayStat10->SetContext (".");
  receiver10->SetDelayTracker (delayStat10);
  data.AddDataCalculator (delayStat10);





//count energy of node 

BasicEnergySourceHelper basicSourceHelper;
basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ",DoubleValue(30));
EnergySourceContainer Sources=basicSourceHelper.Install(c);
WifiRadioEnergyModelHelper radioEnergyHelper;
radioEnergyHelper.Set("TxCurrentA",DoubleValue(0.0174));

//radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.38));
//radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.313));

DeviceEnergyModelContainer deviceModels=radioEnergyHelper.Install(devices,Sources);








  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("wifi-simple-adhoc-grid.tr"));
      wifiPhy.EnablePcap ("wifi-simple-adhoc-grid", devices);
      // Trace routing tables
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.routes", std::ios::out);
    //  aodv.PrintRoutingTableAllEvery (Seconds (2), routingStream);


      olsr.PrintRoutingTableAllEvery (Seconds (2), routingStream);


      Ptr<OutputStreamWrapper> neighborStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.neighbors", std::ios::out);
     // aodv.PrintNeighborCacheAllEvery (Seconds (2), neighborStream);

      olsr.PrintNeighborCacheAllEvery (Seconds (2), neighborStream);


      // To do-- enable an IP-level trace that shows forwarding events only
    }

  // Give OLSR time to converge-- 30 seconds perhaps
  Simulator::Schedule (Seconds (30.0), &GenerateTraffic,
                       source, packetSize, numPackets, interPacketInterval);

  // Output what we are doing
  NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);





  
  AnimationInterface anim("LSLL2017213464.xml");//为了可视化添加
  //anim.SetMaxPktsPerTraceFile(99999999999999999);
  Simulator::Stop (Seconds (33.0));
  Simulator::Run ();




  
Ptr<DataOutputInterface>output=0;
if (format =="omnet"){
 NS_LOG_INFO("Createing omnet formatted data output.");
output = CreateObject<OmnetDataOutput>();
} else if (format=="db"){
NS_LOG_INFO("Creating sqlite formatted data output.");
 output = CreateObject<SqliteDataOutput>();}
else {NS_LOG_ERROR("Unknown output format"<<format);}
if(output!=0) output->Output(data);



//out energy
 std::fstream file;
  file.open("LSLenergy.csv",ios::out | ios::trunc);
  file.clear();
  uint32_t node_number=0;
//  for (DeviceEnergyModelContainer::Iterator iter = deviceModels.Begin (); iter != deviceModels.End (); iter ++)
//    {
//      double energyConsumed = (*iter)->GetTotalEnergyConsumption ();
/*      NS_LOG_UNCOND ("End of simulation (" << Simulator::Now ().GetSeconds ()
                     << "s) Total energy consumed by radio = " << energyConsumed << "J");
      // NS_ASSERT (energyConsumed <= 30.0);
      file <<node_number<< "," << energyConsumed << "\n";
      node_number++;
    }
*/



for (DeviceEnergyModelContainer::Iterator iter=deviceModels.Begin();iter!=deviceModels.End();iter++)
{
  double energyConsumed=(*iter)->GetTotalEnergyConsumption();
  NS_LOG_UNCOND("End of simulation("<<Simulator::Now().GetSeconds()
  <<"s)Total energy consumed by radio="<<energyConsumed<<"j");
  NS_ASSERT(energyConsumed<=30.01);
 file <<node_number<< "," << energyConsumed << "\n";
      node_number++;
}

  file.close();





Simulator::Destroy ();

  return 0;
}

