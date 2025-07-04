/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2017 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *   Author: Biljana Bojovic <bbojovic@cttc.es>
 */

/**
 * \file cttc-3gpp-channel-example.cc
 * \ingroup examples
 * \brief Channel Example
 *
 * This example describes how to setup a simulation using the 3GPP channel model
 * from TR 38.900. Topology consists by default of 2 UEs and 2 gNbs, and can be
 * configured to be either mobile or static scenario.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/mmwave-helper.h"
#include <ns3/buildings-helper.h>
#include "ns3/log.h"
#include <ns3/buildings-module.h>
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int pckRecCounterBestEffort = 0;
int pckRecCounterPrio = 0;
const int pcktSize = 1204;

void ReceivePacketBestEffort (Ptr<Socket> socket)
{
  pckRecCounterBestEffort += 1;
  Ptr<Packet> packet = socket->Recv ();
  SocketIpTosTag tosTag;
  /**
  if (packet->RemovePacketTag (tosTag))
    {
      NS_LOG_INFO (" TOS = " << (uint32_t)tosTag.GetTos ());
    }
  SocketIpTtlTag ttlTag;
  if (packet->RemovePacketTag (ttlTag))
    {
      NS_LOG_INFO (" TTL = " << (uint32_t)ttlTag.GetTtl ());
    }
  **/
}

static void SendPacketBestEffort (Ptr<Socket> socket, uint32_t pktSize, 
                        uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      //socket->Send (Create<Packet> (pktSize));
      std::string pktStr = std::to_string(pktCount);
      Time ts = Simulator::Now();
      const std::string msg = std::to_string(ts.ToDouble(Time::NS));
      //Ptr<Packet> pkt1 = Create<Packet> (reinterpret_cast<const uint8_t*> (&msg), 5);
      Ptr<Packet> pkt1 = Create<Packet> (reinterpret_cast<const uint8_t*> (msg.c_str()), pktSize);
      //int sizePckt = pkt1->GetSize();
      socket->Send (pkt1);
      Simulator::Schedule (pktInterval, &SendPacketBestEffort, socket, pktSize, pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

// TODO: check out other traces (in the interface implementations)
/**
 * Prints out to file buildings.txt the locations of the buildings.
 */
void
TraceBuildingLoc()
{
  //Write the location of buildings.
  static bool firstTitme = true;
  if(firstTitme)
    {
      firstTitme = false;
      std::ofstream outFile;
      outFile.open ("building.txt");
      if (!outFile.is_open ())
        {
          NS_FATAL_ERROR ("Can't open file building.txt");
        }
      for (BuildingList::Iterator bit = BuildingList::Begin (); bit != BuildingList::End (); ++bit)
        {
          Box boundaries = (*bit)->GetBoundaries ();
          outFile << boundaries.xMin<< "\t";
          outFile << boundaries.xMax<< "\t";
          outFile << boundaries.yMin<< "\t";
          outFile << boundaries.yMax<< "\t";
          outFile << boundaries.zMin<< "\t";
          outFile << boundaries.zMax << std::endl;
        }
      outFile.close ();
    }
}

// parameters that can be set from the command line

/**
 * \brief Global variable used to configure the type of 3gpp scenario. It is accessible as "--scenario" from CommandLine.
 */
static ns3::GlobalValue g_scenario("scenario",
                                   "The scenario for the simulation. Choose among 'RMa', 'UMa', 'UMi-StreetCanyon', 'InH-OfficeMixed', 'InH-OfficeOpen', 'InH-ShoppingMall'",
                                   ns3::StringValue("UMa"), ns3::MakeStringChecker());
/**
 * \brief Global variable used to configure whether the buildings are used in the scenario. It is accessible as "--enableBuildings" from CommandLine.
 */
static ns3::GlobalValue g_enableBuildings("enableBuildings", "If true, use MmWave3gppBuildingsPropagationLossModel, else use MmWave3gppPropagationLossModel",
                                          ns3::BooleanValue(true), ns3::MakeBooleanChecker());

/**
 * \brief Global variable used to configure LOS condition. It is accessible as "--losCondition" from CommandLine.
 */
static ns3::GlobalValue g_losCondition("losCondition",
                                       "The LOS condition for the simulation, if MmWave3gppPropagationLossModel is used. Choose 'l' for LOS only, 'n' for NLOS only, 'a' for the probabilistic model",
                                       ns3::StringValue("a"), ns3::MakeStringChecker());

/**
 * \brief Global variable used to configure optional NLOS equation from 3GPP TR 38.900. It is accessible as "--optionNlos" from CommandLine.
 */
static ns3::GlobalValue g_optionNlos("optionNlos", "If true, use the optional NLOS pathloss equation from 3GPP TR 38.900",
                                     ns3::BooleanValue(false), ns3::MakeBooleanChecker());

/**
 * \brief Global variable used to configure central carrier frequency. It is accessible as "--frequency" from CommandLine.
 */
static ns3::GlobalValue g_frequency("frequency",
                                    "The frequency in GHz",
                                     ns3::DoubleValue(28e9),
                                     ns3::MakeDoubleChecker<double>());

int 
main (int argc, char *argv[])
{

  CommandLine cmd;
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();
  // parse again so you can override input file default values via command line
  cmd.Parse (argc, argv);

  // enable logging or not
  bool logging = true;
  if(logging)
    {
      //LogComponentEnable ("MmWave3gppPropagationLossModel", LOG_LEVEL_ALL);
      //LogComponentEnable ("MmWave3gppBuildingsPropagationLossModel", LOG_LEVEL_ALL);
      //LogComponentEnable ("MmWave3gppChannel", LOG_LEVEL_ALL);
      //LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      //LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      //LogComponentEnable ("LteRlcUm", LOG_LEVEL_LOGIC);
      //LogComponentEnable ("LteRlcUm", LOG_LEVEL_LOGIC);
      //LogComponentEnable ("LtePdcp", LOG_LEVEL_INFO);
    }

  // set simulation time and mobility
  double simTime = 1; // s
  double speed = 1; // 1 m/s for walking UT.

  // parse the command line options
  BooleanValue booleanValue;
  StringValue stringValue;
  IntegerValue integerValue;
  DoubleValue doubleValue;
  GlobalValue::GetValueByName("scenario", stringValue); // set the scenario
  std::string scenario = stringValue.Get();
  GlobalValue::GetValueByName("enableBuildings", booleanValue); // use buildings or not
  bool enableBuildings = booleanValue.Get();
  GlobalValue::GetValueByName("losCondition", stringValue); // set the losCondition
  std::string condition = stringValue.Get();
  GlobalValue::GetValueByName("optionNlos", booleanValue); // use optional NLOS equation
  bool optionNlos = booleanValue.Get();

  GlobalValue::GetValueByName("frequency", doubleValue); //
  double frequencyInGHz = doubleValue.Get();

  // attributes that can be set for this channel model
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::ChannelCondition", StringValue(condition));
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::Scenario", StringValue(scenario));
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::OptionalNlos", BooleanValue(optionNlos));

  // important to set frequency into the 3gpp model
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::Frequency", DoubleValue(frequencyInGHz));
  Config::SetDefault ("ns3::MmWave3gppBuildingsPropagationLossModel::Frequency", DoubleValue(frequencyInGHz));

  // other parameters of buildings model
  Config::SetDefault ("ns3::MmWave3gppPropagationLossModel::Shadowing", BooleanValue(true)); // enable or disable the shadowing effect
  Config::SetDefault ("ns3::MmWave3gppBuildingsPropagationLossModel::UpdateCondition", BooleanValue(true)); // enable or disable the LOS/NLOS update when the UE moves

  Config::SetDefault ("ns3::MmWave3gppChannel::UpdatePeriod", TimeValue(MilliSeconds(100))); // interval after which the channel for a moving user is updated, 
  // with spatial consistency procedure. If 0, spatial consistency is not used
  Config::SetDefault ("ns3::MmWave3gppChannel::CellScan", BooleanValue(false)); // Set true to use cell scanning method, false to use the default power method.
  Config::SetDefault ("ns3::MmWave3gppChannel::Blockage", BooleanValue(true)); // use blockage or not
  Config::SetDefault ("ns3::MmWave3gppChannel::PortraitMode", BooleanValue(true)); // use blockage model with UT in portrait mode
  Config::SetDefault ("ns3::MmWave3gppChannel::NumNonselfBlocking", IntegerValue(4)); // number of non-self blocking obstacles

  // default 28e9
  Config::SetDefault ("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(frequencyInGHz)); // check MmWavePhyMacCommon for other PHY layer parameters
  Config::SetDefault ("ns3::MmWavePhyMacCommon::MacSchedulerType", TypeIdValue (TypeId::LookupByName("ns3::MmWaveMacSchedulerTdmaRR")));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue(1500));
  //Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue(10));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (9000000));
  Config::SetDefault ("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MicroSeconds(1.0)));
  //Config::SetDefault ("ns3::MmWaveHelper::RlcAmEnabled", BooleanValue(1)); // by default is disabled
  //Config::SetDefault ("ns3::LteRlcAm::PollRetransmitTimer", TimeValue(MilliSeconds(1.0)));
  //set mobile device and base station antenna heights in meters, according to the chosen scenario
  double hBS; //base station antenna height in meters;
  double hUT; //user antenna height in meters;
  if(scenario.compare("RMa")==0)
    {
      hBS = 35;
      hUT = 1.5;
    }
  else if(scenario.compare("UMa")==0)
    {
      hBS = 25;
      hUT = 1.5;
    }
  else if (scenario.compare("UMi-StreetCanyon")==0)
    {
      hBS = 10;
      hUT = 1.5;
    }
  else if (scenario.compare("InH-OfficeMixed")==0 || scenario.compare("InH-OfficeOpen")==0 || scenario.compare("InH-ShoppingMall")==0)
    {
      //The fast fading model does not support 'InH-ShoppingMall' since it is listed in table 7.5-6
      hBS = 3;
      hUT = 1;
    }
  else
    {
      return 1;
    }

  // setup the mmWave simulation
  Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper> (); 
  if(enableBuildings)
    {
      mmWaveHelper->SetAttribute ("PathlossModel", StringValue ("ns3::MmWave3gppBuildingsPropagationLossModel"));
    }
  else
    {
      mmWaveHelper->SetAttribute ("PathlossModel", StringValue ("ns3::MmWave3gppPropagationLossModel"));
    }


  mmWaveHelper->SetAttribute ("ChannelModel", StringValue ("ns3::MmWave3gppChannel"));
  Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  mmWaveHelper->SetEpcHelper (epcHelper);
  mmWaveHelper->Initialize();

  // create base stations and mobile terminals
  NodeContainer enbNodes;
  NodeContainer ueNodes;
  enbNodes.Create (2);
  ueNodes.Create (2);

  // deploy buildings if the option is enabled
  if(enableBuildings)
    {
      Ptr < Building > building1;
      building1 = Create<Building> ();
      building1->SetBoundaries (Box (50.0,60.0, // xMin, xMax
                                     0.0, 10, // yMin, yMax
                                     0.0, 20)); // zMin, zMax
      Ptr < Building > building2;
      building2 = Create<Building> ();
      building2->SetBoundaries (Box (50.0,60.0,
                                     10.0, 30,
                                     0.0, 9));
      Ptr < Building > building3;
      building3 = Create<Building> ();
      building3->SetBoundaries (Box (20.0,40.0,
                                     30.0, 40,
                                     0.0, 50));

      Ptr < Building > building4;
      building4 = Create<Building> ();
      building4->SetBoundaries (Box (30.0,70.0,
                                     60.0, 80,
                                     0.0, 20));
    }

  // position the base stations
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, hBS));
  enbPositionAlloc->Add (Vector (0.0, 80.0, hBS));
  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator(enbPositionAlloc);
  enbmobility.Install (enbNodes);

  // position the mobile terminals and enable the mobility
  MobilityHelper uemobility;
  uemobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  uemobility.Install (ueNodes);
  if(enableBuildings)
    {
      BuildingsHelper::Install (enbNodes);
      BuildingsHelper::Install (ueNodes);
    }
  bool mobility = true;
  if(mobility)
    {
      ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (90, 15, hUT)); // (x, y, z) in m
      ueNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, speed, 0)); // move UE1 along the y axis

      ueNodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (30, 50.0, hUT)); // (x, y, z) in m
      ueNodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (-speed, 0, 0)); // move UE2 along the x axis
    }
  else
    {
      ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (90, 15, hUT));
      ueNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, 0, 0));

      ueNodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (30, 50.0, hUT));
      ueNodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, 0, 0));
    }

  // install mmWave net devices
  NetDeviceContainer enbNetDev = mmWaveHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueNetDev = mmWaveHelper->InstallUeDevice (ueNodes);

  // create the internet and install the IP stack on the UEs
  // get SGW/PGW and create a single RemoteHost 
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // connect a remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));
  // assign IP address to UEs, and install UDP downlink applications
  uint16_t dlPort = 1234;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      UdpServerHelper dlPacketSinkHelper (dlPort);
      serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));

      UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
      dlClient.SetAttribute ("Interval", TimeValue (MicroSeconds(1)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue(0xFFFFFFFF));
      clientApps.Add (dlClient.Install (remoteHost));
    }
  // start server and client apps
  serverApps.Start(Seconds(0.4));
  clientApps.Start(Seconds(0.5));
  serverApps.Stop(Seconds(simTime));
  clientApps.Stop(Seconds(simTime-0.2));

  // attach UEs to the closest eNB
  mmWaveHelper->AttachToClosestEnb (ueNetDev, enbNetDev);

  // print the position of the buildings
  if(enableBuildings)
    {
      BuildingsHelper::MakeMobilityModelConsistent ();
      TraceBuildingLoc();
    }

  // enable the traces provided by the mmWave module
  // mmWaveHelper->EnableTraces();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


