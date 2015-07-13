/* Topology
   --------

   +-+      +-+      +-+      +-+
   |A|<-----|a|      |b|<-----|B|  UDP data flow: a->A, B->b
   +-+      +-+      +-+      +-+
    |<------>|<------>|<------>|
       {A,a}    {a,b}    {b,B}     Conflicting pairs


   +-----------+--------------+-------------------+-------------+
   | Node Name |  Node Type   |    MAC Address    | IP Address  |
   +-----------+--------------+-------------------+-------------+
   |     A     |      AP      | 00:00:00:00:00:01 | 192.168.1.1 |
   +-----------+--------------+-------------------+-------------+
   |     a     | Station Node | 00:00:00:00:00:02 | 192.168.1.2 |
   +-----------+--------------+-------------------+-------------+
   |     B     |      AP      | 00:00:00:00:00:03 | 192.168.2.1 |
   +-----------+--------------+-------------------+-------------+
   |     b     | Station Node | 00:00:00:00:00:04 | 192.168.2.2 |
   +-----------+--------------+-------------------+-------------+

*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h" 
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    
    //RTS/CTS activation
    UintegerValue ctsThreshold = 0;
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThreshold);
    
    //Create access point A and node a
    NodeContainer node_A;
    node_A.Create(1);
    NodeContainer node_a;
    node_a.Create(1);
    
    //Create access point B and node b
    NodeContainer node_B;
    node_B.Create(1);
    NodeContainer node_b;
    node_b.Create(1);
    
    // Nodes do not change their positions
    node_A.Get(0)->AggregateObject(CreateObject<ConstantPositionMobilityModel> ());
    node_a.Get(0)->AggregateObject(CreateObject<ConstantPositionMobilityModel> ());
    node_B.Get(0)->AggregateObject(CreateObject<ConstantPositionMobilityModel> ());
    node_b.Get(0)->AggregateObject(CreateObject<ConstantPositionMobilityModel> ());
    
    //The propagation loss is fixed for each pair of nodes 
    //and does not depend on their actual positions.
    Ptr<MatrixPropagationLossModel> propagationLoss = CreateObject<MatrixPropagationLossModel> ();
    propagationLoss->SetDefaultLoss(200); //default loss: 200 dB
    
    //node_a and accessPoint_A are within the transmission range of each other
    propagationLoss->SetLoss(node_a.Get(0)->GetObject<MobilityModel>(), node_A.Get(0)->GetObject<MobilityModel>(), 0);
    //node_b and accessPoint_B are within the transmission range of each other
    propagationLoss->SetLoss(node_b.Get(0)->GetObject<MobilityModel>(), node_B.Get(0)->GetObject<MobilityModel>(), 0);
    //node_a and node_b are within the transmission range of each other
    propagationLoss->SetLoss(node_a.Get(0)->GetObject<MobilityModel>(), node_b.Get(0)->GetObject<MobilityModel>(), 0);
    
    //Create Channel and Phy
    Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
    wifiChannel->SetPropagationLossModel(propagationLoss);
    wifiChannel->SetPropagationDelayModel(CreateObject <ConstantSpeedPropagationDelayModel> ());
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel);
    
    //Create WifiHelper and MACHelper
    WifiHelper wifiHelper = WifiHelper::Default();
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);//Setting WiFi Standard to 802.11b
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue("DsssRate11Mbps"),
            "ControlMode", StringValue("DsssRate11Mbps"));//Setting Data rate and Control rate both to 11Mbps
    NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
    
    //Create SSID
    Ssid ssid_self = Ssid("ssid_self");
    Ssid ssid_neighbor = Ssid("ssid_neighbor");
    
    //Create NetDevices for Self
    wifiMacHelper.SetType("ns3::ApWifiMac","Ssid", SsidValue(ssid_self));
    NetDeviceContainer device_A = wifiHelper.Install(wifiPhy, wifiMacHelper, node_A);
    wifiMacHelper.SetType("ns3::StaWifiMac","Ssid", SsidValue(ssid_self),"ActiveProbing", BooleanValue(false));
    NetDeviceContainer device_a = wifiHelper.Install(wifiPhy, wifiMacHelper, node_a);

    //Create NetDevices for Neighbor
    wifiMacHelper.SetType("ns3::ApWifiMac","Ssid", SsidValue(ssid_neighbor));
    NetDeviceContainer device_B = wifiHelper.Install(wifiPhy, wifiMacHelper, node_B);
    wifiMacHelper.SetType("ns3::StaWifiMac","Ssid", SsidValue(ssid_neighbor),"ActiveProbing", BooleanValue(false));
    NetDeviceContainer device_b = wifiHelper.Install(wifiPhy, wifiMacHelper, node_b);
    
    //Setting up Internet stack in the nodes
    InternetStackHelper stack;
    stack.Install(node_A);
    stack.Install(node_a);
    stack.Install(node_B);
    stack.Install(node_b);
    
    //Create IPv4 Address Helper
    Ipv4AddressHelper ipv4AddressHelper;

    //Assign IP Addresses to Self
    ipv4AddressHelper.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaceContainer_A = ipv4AddressHelper.Assign(device_A);
    Ipv4InterfaceContainer interfaceContainer_a = ipv4AddressHelper.Assign(device_a);

    //Assign IP Addresses to Neighbor
    ipv4AddressHelper.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaceContainer_B = ipv4AddressHelper.Assign(device_B);
    Ipv4InterfaceContainer interfaceContainer_b = ipv4AddressHelper.Assign(device_b);
    
    //flow:  B->b (b: UDP Server, B: UDP Client)
    UdpServerHelper udpServer_Neighbor(55555);//UDP Server listens on port 55555
    ApplicationContainer udpAppl_Neighbor = udpServer_Neighbor.Install(node_b.Get(0));
    udpAppl_Neighbor.Start(Seconds(0.1));//UDP Server starts at 0.1sec simulation time
    udpAppl_Neighbor.Stop(Seconds(200.0));
    ApplicationContainer application_Neighbor;
    OnOffHelper onOffHelper_Neighbor("ns3::UdpSocketFactory", InetSocketAddress(interfaceContainer_b.GetAddress(0), 55555));//UDP Client is bound to UDP Server
    onOffHelper_Neighbor.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper_Neighbor.SetAttribute("DataRate", StringValue("11Mbps"));
    onOffHelper_Neighbor.SetAttribute("StartTime", TimeValue(Seconds(0.2)));//UDP Client starts after UDP server has been started
    application_Neighbor.Add(onOffHelper_Neighbor.Install(node_B.Get(0)));
    
    //flow:  a->A (A: UDP Server, a: UDP Client)
    UdpServerHelper udpServer_Self(55555);//UDP Server listens on port 55555
    ApplicationContainer udpAppl_Self = udpServer_Self.Install(node_A.Get(0));
    udpAppl_Self.Start(Seconds(0.1));//UDP Server starts at 0.1sec simulation time
    udpAppl_Self.Stop(Seconds(200.0));
    ApplicationContainer application_Self;
    OnOffHelper onOffHelper_Self("ns3::UdpSocketFactory", InetSocketAddress(interfaceContainer_A.GetAddress(0), 55555));//UDP Client is bound to UDP Server
    onOffHelper_Self.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper_Self.SetAttribute("DataRate", StringValue("11Mbps"));
    onOffHelper_Self.SetAttribute("StartTime", TimeValue(Seconds(0.2)));//UDP Client starts after UDP server has been started
    application_Self.Add(onOffHelper_Self.Install(node_a.Get(0)));
    
    //Packet capture settings
    wifiPhy.EnablePcap("1c_node_A", node_A.Get(0)->GetId(), 0);
    wifiPhy.EnablePcap("1c_node_a", node_a.Get(0)->GetId(), 0);
    wifiPhy.EnablePcap("1c_node_B", node_B.Get(0)->GetId(), 0);
    wifiPhy.EnablePcap("1c_node_b", node_b.Get(0)->GetId(), 0);

    //Simulator settings
    Simulator::Stop(Seconds(200.0));
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}
