/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

//
// This is an illustration of how one could use virtualization techniques to
// allow running applications on virtual machines talking over simulated
// networks.
//
// The actual steps required to configure the virtual machines can be rather
// involved, so we don't go into that here.  Please have a look at one of
// our HOWTOs on the nsnam wiki for more details about how to get the
// system confgured.  For an example, have a look at "HOWTO Use Linux
// Containers to set up virtual networks" which uses this code as an
// example.
//
// The configuration you are after is explained in great detail in the
// HOWTO, but looks like the following:
//
//  +----------+                           +----------+
//  | virtual  |                           | virtual  |
//  |  Linux   |                           |  Linux   |
//  |   Host   |                           |   Host   |
//  |          |                           |          |
//  |   eth0   |                           |   eth0   |
//  +----------+                           +----------+
//       |                                      |
//  +----------+                           +----------+
//  |  Linux   |                           |  Linux   |
//  |  Bridge  |                           |  Bridge  |
//  +----------+                           +----------+
//       |                                      |
//  +------------+                       +-------------+
//  | "tap-left" |                       | "tap-right" |
//  +------------+                       +-------------+
//       |           n0            n1           |
//       |       +--------+    +--------+       |
//       +-------|  tap   |    |  tap   |-------+
//               | bridge |    | bridge |
//               +--------+    +--------+
//               |  CSMA  |    |  CSMA  |
//               +--------+    +--------+
//                   |             |
//                   |             |
//                   |             |
//                   ===============
//                      CSMA LAN
//
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
// #include "ns3/csma-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/tap-bridge-module.h"

#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mpi-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TapCsmaVirtualMachineExample");

int
main (int argc, char *argv[])
{
  bool AnimationOn = false;
  bool TracingPcap = false;
  bool TracingAscii = true;
  bool nullmsg = true;
  // bool FlowMonitorOn = true;
  int NumClientNodes = 5;
  int NumServerNodes = 1;
  double TotalTime = 600.0;
  std::string tracefilePath = "/var/log/dockemu";

  CommandLine cmd;
  cmd.AddValue ("NumClientNodes", "Number of client nodes/devices", NumClientNodes);
  cmd.AddValue ("NumServerNodes", "Number of server nodes/devices", NumServerNodes);
  cmd.AddValue ("TotalTime", "Total simulation time", TotalTime);
  cmd.AddValue ("AnimationOn", "Enable animation", AnimationOn);
  cmd.AddValue ("TracingPcap", "Enable PCAP tracing", TracingPcap);
  cmd.AddValue ("TracingAscii", "Enable ASCII tracing", TracingAscii);
  cmd.AddValue ("TracefilePath", "Path to store tracing and PCAP files", tracefilePath);
  cmd.AddValue ("nullmsg", "Enable the use of null-message synchronization", nullmsg);

  cmd.Parse (argc,argv);

    if(nullmsg)
      {
        GlobalValue::Bind ("SimulatorImplementationType",
                           StringValue ("ns3::NullMessageSimulatorImpl"));
      }
    else
      {
        GlobalValue::Bind ("SimulatorImplementationType",
                           StringValue ("ns3::DistributedSimulatorImpl"));
      }

  // Enable parallel simulator with the command line arguments
  MpiInterface::Enable (&argc, &argv);

  //
  // We are interacting with the outside, real, world.  This means we have to
  // interact in real-time and therefore means we have to use the real-time
  // simulator and take the time to calculate checksums.
  //
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  // Create Server node
  NodeContainer server_nodes;
  server_nodes.Create(2, 0);
  PointToPointHelper p2p_server;
  p2p_server.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_server.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer server_devices;
  server_devices = p2p_server.Install(server_nodes);

  InternetStackHelper stack;
  stack.Install(server_nodes);
  //
  // Create two ghost nodes.  The first will represent the virtual machine host
  // on the left side of the network; and the second will represent the VM on
  // the right side.
  //
  NS_LOG_UNCOND ("Creating nodes");
  std::vector<NodeContainer> clientnodes(NumClientNodes);
  std::vector<NetDeviceContainer> clientdevices(NumClientNodes);
  std::vector<PointToPointHelper> p2p_clients(NumClientNodes);

  for (int i = 0; i < NumClientNodes; ++i)
  {
    int rank = i % (MpiInterface::GetSize() - 1) + 1;
    clientnodes[i].Create(2, rank);
    p2p_clients[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p_clients[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    clientdevices[i] = p2p_clients[i].Install(clientnodes[i]);
    stack.Install(clientnodes[i]);
  }

  // Use a CsmaHelper to get a CSMA channel created, and the needed net
  // devices installed on both of the nodes.  The data rate and delay for the
  // channel can be set through the command-line parser.  For example,
  //
  // ./waf --run "tap=csma-virtual-machine --ns3::CsmaChannel::DataRate=10000000"
  //
  // CsmaHelper csma;
  // NetDeviceContainer devices = csma.Install (nodes);

  //
  // Use the TapBridgeHelper to connect to the pre-configured tap devices for
  // the left side.  We go with "UseBridge" mode since the CSMA devices support
  // promiscuous mode and can therefore make it appear that the bridge is
  // extended into ns-3.  The install method essentially bridges the specified
  // tap to the specified CSMA device.
  //
  NS_LOG_UNCOND ("Creating tap bridges");
  TapBridgeHelper tapBridge;
  tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));

  for (int i = 0; i < NumClientNodes; i++)
    {
        std::stringstream tapName;
        tapName << "tap-c-" << i;
        NS_LOG_UNCOND ("Tap bridge = " + tapName.str ());

        tapBridge.SetAttribute ("DeviceName", StringValue (tapName.str ()));
        tapBridge.Install (clientnodes[i].Get (0), clientdevices[i].Get (0));

    }

    std::stringstream tapName;
    tapName << "tap-s-0";
    NS_LOG_UNCOND ("Tap bridge = " + tapName.str ());

    tapBridge.SetAttribute ("DeviceName", StringValue (tapName.str ()));
    tapBridge.Install (server_nodes.Get (0), server_devices.Get (0));


  if (TracingPcap)
    {
        for (int i = 0; i < NumClientNodes; i++)
          {
              p2p_clients[i].EnablePcap(tracefilePath + "/ns3_tap_client_" + std::to_string(i), clientdevices[i].Get(1), false, true);
          }
        p2p_server.EnablePcap(tracefilePath + "/ns3_tap_server", server_devices.Get(1), false, true);
    }

  if (TracingAscii)
    {
        for (int i = 0; i < NumClientNodes; i++)
          {
              p2p_clients[i].EnableAscii(tracefilePath + "client_" + std::to_string(i), clientdevices[i].Get(1));
          }
        p2p_server.EnableAscii(tracefilePath + "server", server_devices.Get(1));
    }

  // FlowMonitorHelper flowHelper;
  // Ptr<FlowMonitor> flowMonitor = flowHelper.Install(nodes);
  // FlowMonitor->SerializeToXmlFile(tracefilePath + "/flowmonitor.xml", false, true);
  // if( AnimationOn )
  // {
  //   NS_LOG_UNCOND ("Activating Animation");
  //   AnimationInterface anim ("animation.xml"); // Mandatory
  //   for (uint32_t i = 0; i < nodes.GetN (); ++i)
  //     {
  //       std::stringstream ssi;
  //       ssi << i;
  //       anim.UpdateNodeDescription (nodes.Get (i), "Node" + ssi.str()); // Optional
  //       anim.UpdateNodeColor (nodes.Get (i), 255, 0, 0); // Optional
  //     }

  //   anim.EnablePacketMetadata (); // Optional
  //   // anim.EnableIpv4RouteTracking ("routingtable-wireless.xml", Seconds (0), Seconds (5), Seconds (0.25)); //Optional
  //   anim.EnableWifiMacCounters (Seconds (0), Seconds (TotalTime)); //Optional
  //   anim.EnableWifiPhyCounters (Seconds (0), Seconds (TotalTime)); //Optional
  // }

  //
  // Run the simulation for TotalTime seconds to give the user time to play around
  //
  NS_LOG_UNCOND ("Running simulation in csma mode");
  Simulator::Stop (Seconds (TotalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}
