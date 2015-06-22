/* Topology
   --------

                +-----+
                | AP  |
               /+-----+\
              /    |    \
             /     |     \
            /      |      \
           /       |       \
   +-----+/     +-----+     \+-----+
   |STA-1|      |STA-2| .... |STA-n|
   +-----+      +-----+      +-----+
 
   UDP data flow: STA-1->AP, STA-2->AP, ..., STA-n->AP

   +-----------+--------------+-----------------------+-----------------+
   | Node Name |  Node Type   |      MAC Address      |   IP Address    |
   +-----------+--------------+-----------------------+-----------------+
   |     AP    | Access Point |   00:00:00:00:00:01   |   192.168.1.1   |
   +-----------+--------------+-----------------------+-----------------+
   |   STA-1   | Station Node |   00:00:00:00:00:02   |   192.168.1.2   |
   +-----------+--------------+-----------------------+-----------------+
   |   STA-2   | Station Node |   00:00:00:00:00:03   |   192.168.1.3   |
   +-----------+--------------+-----------------------+-----------------+
                                    :
                                    :
   +-----------+--------------+-----------------------+-----------------+
   |   STA-n   | Station Node | 00:00:00:00:00:0<n+1> | 192.168.1.<n+1> |
   +-----------+--------------+-----------------------+-----------------+

 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h" 
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"

#include <iostream>
#include <string.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Problem3a");

int main(int argc, char *argv[]) {

    bool verbose = true;
    uint32_t nWifi = 1;//No. of station nodes (simulation performed for the values 1-10)
    uint32_t counter = 0; //Used to count station nodes later in the program

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("nWifi", "Number of Stations", nWifi);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_FUNCTION);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_FUNCTION);
    }

    //RTS/CTS activation
    UintegerValue ctsThreshold = 0;
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThreshold);

    //Create Access Point and Nodes
    NodeContainer wifiApNode;
    wifiApNode.Create(1); //Access Point
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi); //Nodes

    //Create Wifi Channel and Phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    //Create WifiHelper and MACHelper
    WifiHelper wifiHelper = WifiHelper::Default();
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b); //Setting WiFi Standard to 802.11b
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue("DsssRate11Mbps"),
            "ControlMode", StringValue("DsssRate11Mbps")); //Setting Data rate and Control rate both to 11Mbps
    NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();

    //Create SSID
    Ssid ssid = Ssid("ssid_3a");

    //Create NetDevices for Access Point and Nodes
    NetDeviceContainer apDevices;
    wifiMacHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifiHelper.Install(phy, wifiMacHelper, wifiApNode);
    NetDeviceContainer staDevices;
    wifiMacHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifiHelper.Install(phy, wifiMacHelper, wifiStaNodes);

    //Create MobilityHelper
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); //Positions of AP and Nodes are fixed
    mobility.Install(wifiApNode); //Add Access Point to this Mobility Model
    mobility.Install(wifiStaNodes); //Add all Nodes to this Mobility Model


    //Setting up Internet stack in the Access Point and Nodes
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    //Create IPv4 Address Helper
    Ipv4AddressHelper ipv4AddressHelper;

    //Assign IP Addresses to Access Point and Nodes
    ipv4AddressHelper.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaceContainer_ap = ipv4AddressHelper.Assign(apDevices);
    Ipv4InterfaceContainer interfaceContainer_sta = ipv4AddressHelper.Assign(staDevices);

    //UDP flows: Individual Nodes -> Access Point
    //Access Point: UDP Server, Individual Nodes: UDP Clients
    UdpServerHelper udpServer(55555); //UDP Server listens on port 55555
    ApplicationContainer udpAppl = udpServer.Install(wifiApNode.Get(0));
    udpAppl.Start(Seconds(0.1)); //UDP Server starts at 0.1sec simulation time
    udpAppl.Stop(Seconds(500.0));
    ApplicationContainer application;
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaceContainer_ap.GetAddress(0), 55555)); //UDP Client is bound to UDP Server
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("DataRate", StringValue("11Mbps"));
    onOffHelper.SetAttribute("StartTime", TimeValue(Seconds(0.2))); //UDP Clients start after UDP server has been started
    for (counter = 0; counter < nWifi; counter++) {//Create UDP Client on each node
        application.Add(onOffHelper.Install(wifiStaNodes.Get(counter)));
    }

    //Simulator stop time
    Simulator::Stop(Seconds(500.0));

    static char dir[200];
    static char accessPointDir[200];
    static char stationDir[200];

    sprintf(dir, "PacketCapture/Problem3a/%d", nWifi);
    sprintf(accessPointDir, "%s/AccessPoint", dir);
    sprintf(stationDir, "%s/Stations", dir);
    ;

    //Packet capture settings
    phy.EnablePcap(accessPointDir, apDevices, true);
    phy.EnablePcap(stationDir, staDevices, true);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
