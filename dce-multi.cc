#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"

#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>

#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "misc-tools.h"

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("DceCoba");

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |                |    |                |
//   +----------------+    +----------------+
//   |    10.9.1.1    |    |    10.9.2.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                5 Mbps, 2 ms
//
// 2 nodes : iperf client en iperf server ....
//
// Note : Tested with iperf 2.0.5, you need to modify iperf source in order to
//        allow DCE to have a chance to end an endless loop in iperf as follow:
//        in source named Thread.c at line 412 in method named thread_rest
//        you must add a sleep (1); to break the infinite loop....
// ===========================================================================

// function to get the last value in tcp_mem for tcp_mem_max
string SplitLastValue(const std::string &str) {
    //std::cout << "Splitting: " << str << '\n';
    unsigned found = str.find_last_of(" ");
    ostringstream temp;
    temp << str.substr(found + 1);
    return temp.str();
}

// remove comma if user make input with comma
string RemoveComma(std::string &str) {
    //std::cout << "test : " << str << '\n';
    int i = 0;
    std::string str2 = str;
    for (i = 0; i < 3; i++) {

        std::size_t found = str.find(',');
        if (found != std::string::npos) {
            str2 = str.replace(str.find(','), 1, " ");
        } else {
            //std::cout<<"no comma found.."<<std::endl;
        }
    }
    return str2;
}

double FindPk(double k) {
//read pk.txt and store it in 50x1 matrix
    ifstream in;
    in.open("pk.txt");

    if (!in) {
        cout << "Cannot open input file.\n";
        return 1;
    }

    char str[255];
    double pkval[50];
    char pkchar[15];
    string buff;
    int pkcount = 0;
    while (std::getline(in, buff)) {
        for (int a = 0; a != 15; a++) {
            pkchar[a] = buff[a + 6];
        }

        pkval[pkcount] = atof(pkchar);

        pkcount = pkcount + 1;
    }
    in.close();

    int y = static_cast<int>(k * 10);
    double pk = pkval[(y - 1)];
    cout << "y " << y << endl;
    return pk;
}

void GenerateHtmlFile(int fileSize) {
    std::ofstream myfile;
    myfile.open("files-1/index.html");

    myfile << "<http>\n <body>\n";
    std::vector<char> empty(1024, 0);

    for (int i = 0; i < 1024 * fileSize; i++) {
        if (!myfile.write(&empty[0], empty.size())) {
            std::cerr << "problem writing to file" << std::endl;
        }
    }

    myfile << "Dummy html file\n";
    myfile << "</body>\n</http>\n";
    myfile.close();
}

static void RunIP(Ptr <Node> node, Time at, string str) {
    DceApplicationHelper process;
    ApplicationContainer apps;
    process.SetBinary("ip");
    process.SetStackSize(1 << 16);
    process.ResetArguments();
    process.ParseArguments(str.c_str());
    apps = process.Install(node);
    apps.Start(at);
}

int main(int argc, char *argv[]) {
    string stack = "linux";

    double errRateDown = 0.001;
    double errRateUp = 0.001;
    string tcp_cc = "reno";
    string tcp_mem_user = "4096 8192 8388608";
    string tcp_mem_user_wmem = "4096 8192 8388608";
    string tcp_mem_user_rmem = "4096 8192 8388608";

    string tcp_mem_server = "4096 8192 8388608";
    string tcp_mem_server_wmem = "4096 8192 8388608";
    string tcp_mem_server_rmem = "4096 8192 8388608";

    string udp_bw = "1";
    
    double k_up = 0;
	double pdv_up = 0;
	double avg_delay_up=1;
	
	double k_dw = 0;
	double pdv_dw = 0;
	double avg_delay_dw=1;

    int ErrorModelDown = 1;
    int ErrorModelUp = 1;
    int monitor = 1;
    int mode = 0;

    double k = 1;
    double pdv = 0;
    double avg_delay = 1;
    int htmlSize = 2; // in mega bytes
    char TypeOfConnection = 'p'; // iperf tcp connection
    string dataRateUp = "10Mbps";
    string dataRateDown = "10Mbps";
    bool downloadMode = true;

    
    CommandLine cmd;
    cmd.AddValue("stack",
                 "Name of IP stack: ns3/linux/freebsd.",
                 stack);
    cmd.AddValue("toc",
                 "Link type: p for iperf-tcp, u for iperf-udp and w for wget-thttpd, default to iperf-tcp",
                 TypeOfConnection);
    cmd.AddValue("op", "True for download mode. False for upload moded. HTTP will always be download mode.", downloadMode);
    cmd.AddValue("tcp_cc",
                 "TCP congestion control algorithm. Default is reno. Other options: bic, cubic, highspeed, htcp, hybla, illinois, lp, probe, scalable, vegas, veno, westwood, yeah",
                 tcp_cc);
    cmd.AddValue("htmlSize", "Size of html to be downloaded by wget (Mbytes).", htmlSize);
    cmd.AddValue("dataRateUp", "Data rate of devices (Mbps).", dataRateUp);
    cmd.AddValue("dataRateDown", "Data rate of devices (Mbps).", dataRateDown);
    cmd.AddValue("udp_bw", "BandWidth. Default 1m.", udp_bw);
    cmd.AddValue("errRateDown", "Error rate for downloaded.", errRateDown);
    cmd.AddValue("errRateUp", "Error rate for upload.", errRateUp);
    cmd.AddValue("ErrorModelDown",
                 "Choose error model you want to use. options: 1 -rate error model-default, 2 - burst error model",
                 ErrorModelDown);
    cmd.AddValue("ErrorModelUp",
                 "Choose error model you want to use. options: 1 -rate error model-default, 2 - burst error model",
                 ErrorModelUp);
    cmd.AddValue("avg_delay", "Average delay.", avg_delay);
    cmd.AddValue("pdv", "theta for normal random distribution in this channel", pdv);
    cmd.AddValue("chan_k", "Normal random distribution k in this channel", k);
    
    cmd.AddValue ("avg_delay_up", "average delay upstream.", avg_delay_up);
    cmd.AddValue ("delay_pdv_up", "theta for normal random distribution in server-BS conection upstream", pdv_up);
    cmd.AddValue ("chan_k_up", " Normal random distribution k in server-BS conection upstream", k_up);
    
    cmd.AddValue ("avg_delay_dw", "average delay downstream.", avg_delay_dw);
    cmd.AddValue ("delay_pdv_dw", "theta for normal random distribution in server-BS conection downstream", pdv_dw);
    cmd.AddValue ("chan_k_dw", " Normal random distribution k in server-BS conection downstream", k_dw);
    cmd.Parse(argc, argv);

    // BEGIN Calculating theta and delay
    double delay;
    double theta = 0;
    double pk = FindPk(k);

    theta = pdv / pk;
    delay = avg_delay - k * theta;
    
    if (delay < 0) {
        cout << "Impossible delay. Abort simulation" << std::endl;
        cout << "Downstream" << std::endl;
        cout << "Calculated theta" << theta << std::endl;
        cout << "Calculated node processing time" << delay << std::endl;
        return 0;
    }
    
    double delay_up;
	double theta_up;
	double pk_up = FindPk(k_up);

	theta_up = pdv_up/pk_up;
	delay_up = avg_delay_up-k_up*theta_up;
	cout << "k_up: " << k_up << " | pk_up: " << pk_up << " | theta_up: " << theta_up << " | delay_up: " << delay_up << std::endl;

	if (delay_up < 0) {
		std::cout << "IMPOSIBLE DELAY ABORT SIMULATION" << std::endl;
		std::cout << "UPSTREAM" << std::endl;
		std::cout << "calculated theta " << theta_up << std::endl;
		std::cout << "calculater node processing time " << delay_up << std::endl;
		return 0;
	}


	double delay_dw;
	double theta_dw;
	double pk_dw = FindPk(k_dw);

	theta_dw = pdv_dw/pk_dw;
	delay_dw = avg_delay_dw-k_dw*theta_dw;
	cout << "k_dw: " << k_dw << " | pk_dw: " << pk_dw << " | theta_dw: " << theta_dw << " | delay_dw: " << delay_dw << std::endl;

	if (delay_dw < 0) {
		std::cout << "IMPOSIBLE DELAY ABORT SIMULATION" << std::endl;
		std::cout << "DOWNSTREAM" << std::endl;
		std::cout << "calculated theta " << theta_dw << std::endl;
		std::cout << "calculater node processing time " << delay_dw << std::endl;
		return 0;
	}
	// END Calculating theta and delay

    // Determining Mode
    if (downloadMode) {
        std::cout << "Download mode is used " << std::endl;
        mode = 0;
    }
    if (!downloadMode) {
        std::cout << "Upload mode is used " << std::endl;
        mode = 1;
    }

    // Building topology
    NodeContainer mobile, BS, router, core;
    mobile.Create(1);
    BS.Create(2);
    router.Create(2);
    core.Create(1);

    NodeContainer mobileRouter = NodeContainer(mobile.Get(0), router.Get(0));
    NodeContainer routerBSDown = NodeContainer(router.Get(0), BS.Get(0));
    NodeContainer routerBSUp = NodeContainer(router.Get(0), BS.Get(1));
    NodeContainer BSRouterDown = NodeContainer(BS.Get(0), router.Get(1));
    NodeContainer BSRouterUp = NodeContainer(BS.Get(1), router.Get(1));
    NodeContainer routerCore = NodeContainer(router.Get(1), core.Get(0));


    if (TypeOfConnection == 'w') {
        cout << "Generating html file with size =" << htmlSize << "Mbytes" << endl;
        mkdir("files-1", 0744);
        GenerateHtmlFile(htmlSize);
        //SimuTime=100;
        // if (htmlSize*1000 > atoi(tcp_mem_user_max_wmem.c_str())){
        //     double tmp2=atof(tcp_mem_user_max_wmem.c_str())/(htmlSize*1000);
        //     SimuTime = (htmlSize*10)/(tmp2)*1.5*(htmlSize/tmp2);
        // }

    }

    // p2p2 #1
    // Beginning of channel for mobile router to BSDown
    PointToPointHelper p2pMobRouterBSDown;
    p2pMobRouterBSDown.SetDeviceAttribute("DataRate", StringValue(dataRateDown));
    p2pMobRouterBSDown.SetChannelAttribute("Delay", StringValue("0ms"));
    NetDeviceContainer chanRouterBSDown = p2pMobRouterBSDown.Install(routerBSDown);
    // Ending of channel for mobile router to BSDown

    // p2p #2
    // Beginning of channel for mobile router to BSUp
    PointToPointHelper p2pMobRouterBSUp;
    p2pMobRouterBSUp.SetDeviceAttribute("DataRate", StringValue(dataRateUp));
    p2pMobRouterBSUp.SetChannelAttribute("Delay", StringValue("0ms"));
    NetDeviceContainer chanRouterBSUp = p2pMobRouterBSUp.Install(routerBSUp);
    // Ending of channel for mobile router to BSUp

    // p2p #3
    // Beginning of channel for core router to BSDown
	std::ostringstream delay_oss;
	delay_oss.str ("");
	delay_oss << delay_dw <<"ms";
	//std::cout << "delay ="<<delay_oss.str () <<std::endl;

    PointToPointHelper p2pCoreRouterBSDown;
    p2pCoreRouterBSDown.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pCoreRouterBSDown.SetChannelAttribute("Delay", StringValue(delay_oss.str ().c_str ()));
    p2pCoreRouterBSDown.SetChannelAttribute("Jitter", UintegerValue(1));
    p2pCoreRouterBSDown.SetChannelAttribute("k", DoubleValue(k));
    p2pCoreRouterBSDown.SetChannelAttribute("transparent", UintegerValue(0));
    p2pCoreRouterBSDown.SetChannelAttribute("theta", DoubleValue(theta));
    NetDeviceContainer chanBSRouterDown = p2pCoreRouterBSDown.Install(BSRouterDown);
    // Ending of channel for core router to BSDown

	// p2p #4
    // Beginning of channel for core router to BSUp  
	delay_oss.str ("");
	delay_oss << delay_dw <<"ms";
	//std::cout << "delay ="<<delay_oss.str () <<std::endl;
    
    PointToPointHelper p2pCoreRouterBSUp;
    p2pCoreRouterBSUp.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pCoreRouterBSUp.SetChannelAttribute("Delay", StringValue(delay_oss.str ().c_str ()));
    p2pCoreRouterBSUp.SetChannelAttribute("Jitter", UintegerValue(1));
    p2pCoreRouterBSUp.SetChannelAttribute("k", DoubleValue(k));
    p2pCoreRouterBSUp.SetChannelAttribute("transparent", UintegerValue(0));
    p2pCoreRouterBSUp.SetChannelAttribute("theta", DoubleValue(theta));
    NetDeviceContainer chanBSRouterUp = p2pCoreRouterBSUp.Install(BSRouterUp);
    // Ending of channel for core router to BSUp

    // p2p #5
    // Beginning of channel for mobile router to mobile
    PointToPointHelper p2pMobRouter;
    p2pMobRouter.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pMobRouter.SetChannelAttribute("Delay", StringValue("0ms"));
    p2pMobRouter.SetChannelAttribute("transparent", UintegerValue(1));
    p2pMobRouter.SetChannelAttribute("coreRouter", UintegerValue(0));
    p2pMobRouter.SetChannelAttribute("monitor", UintegerValue(monitor));
    p2pMobRouter.SetChannelAttribute("mode", UintegerValue(mode));
    NetDeviceContainer chanMobileRouter = p2pMobRouter.Install(mobileRouter);
    // Ending of channel for mobile router to mobile

    // p2p #6
    // Beginning of channel for core router to core
    PointToPointHelper p2pCoreRouter;
    p2pCoreRouter.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pCoreRouter.SetChannelAttribute("Delay", StringValue("0ms"));
    p2pMobRouter.SetChannelAttribute("transparent", UintegerValue(1));
    p2pMobRouter.SetChannelAttribute("coreRouter", UintegerValue(1));
    p2pMobRouter.SetChannelAttribute("monitor", UintegerValue(monitor));
    p2pMobRouter.SetChannelAttribute("mode", UintegerValue(mode));
    NetDeviceContainer chanRouterCore = p2pCoreRouter.Install(routerCore);
    // Ending of channel for core router to core

	// BEGIN error model creation
    Ptr <RateErrorModel> emDown = CreateObjectWithAttributes<RateErrorModel>(
            "RanVar", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"),
            "ErrorRate", DoubleValue(errRateDown),
            "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET)
    );
    std::cout << "Building rate error model for downstream..." << std::endl;

    if (ErrorModelDown == 1) {
        std::cout << "Rate Error Model is selected for downstream" << std::endl;
        Ptr <RateErrorModel> emDown = CreateObjectWithAttributes<RateErrorModel>(
                "RanVar", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"),
                "ErrorRate", DoubleValue(errRateDown),
                "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET)
        );
        std::cout << "Building rate error model is completed for downstream" << std::endl;
    }
    else if (ErrorModelDown == 2) {
        std::cout << "Burst Error Model is selected for downstream" << std::endl;
        Ptr <BurstErrorModel> emDown = CreateObjectWithAttributes<BurstErrorModel>(
                "BurstSize", StringValue("ns3::UniformRandomVariable[Min=1,Max=4]"),
                "BurstStart", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"),
                "ErrorRate", DoubleValue(errRateDown)
        );
        std::cout << "Building burst error model is completed for downstream" << std::endl;
    }
    else {
        //this will not change the error model
        std::cout << "Unknown download error model for downstream. Restore to default: rate error model" << std::endl;
    }

    Ptr <RateErrorModel> emUp = CreateObjectWithAttributes<RateErrorModel>(
            "RanVar", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"),
            "ErrorRate", DoubleValue(errRateUp),
            "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET)
    );
    std::cout << "Building rate error model for upstream..." << std::endl;

    if (ErrorModelUp == 1) {
        std::cout << "Rate Error Model is selected for upstream" << std::endl;
        Ptr <RateErrorModel> emUp = CreateObjectWithAttributes<RateErrorModel>(
                "RanVar", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"),
                "ErrorRate", DoubleValue(errRateUp),
                "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET)
        );
        std::cout << "Building rate error model is completed for upstream" << std::endl;
    }
    else if (ErrorModelUp == 2) {
        std::cout << "Burst Error Model is selected for upstream" << std::endl;
        Ptr <BurstErrorModel> emUp = CreateObjectWithAttributes<BurstErrorModel>(
                "BurstSize", StringValue("ns3::UniformRandomVariable[Min=1,Max=4]"),
                "BurstStart", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"),
                "ErrorRate", DoubleValue(errRateUp)
        );
        std::cout << "Building burst error model is completed for upstream" << std::endl;
    }
    else {
        //this will not change the error model
        std::cout << "Unknown upload error model for upstream. Restore to default: rate error model" << std::endl;
    }
    // END error model creation

    DceManagerHelper dceManager, dceManagerNS3;
    dceManagerNS3.SetTaskManagerAttribute("FiberManagerType", StringValue("UcontextFiberManager"));

	if (stack == "linux") {
#ifdef KERNEL_STACK
        // DCE Manager Installation
        dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
        dceManager.Install (mobile);
        dceManager.Install (router);
        dceManager.Install (BS);
        dceManager.Install (core);

        // Stack Installation
        LinuxStackHelper stack;
        stack.Install (mobile);
        stack.Install (router);
        stack.Install (BS);
        stack.Install (core);

        // remove comma from input, if any
        tcp_mem_user = RemoveComma(tcp_mem_user);
        tcp_mem_user_wmem = RemoveComma(tcp_mem_user_wmem);
        tcp_mem_user_rmem = RemoveComma(tcp_mem_user_rmem);
        tcp_mem_server = RemoveComma(tcp_mem_server);
        tcp_mem_server_wmem = RemoveComma(tcp_mem_server_wmem);
        tcp_mem_server_rmem = RemoveComma(tcp_mem_server_rmem);

        // after comma removal
        string tcp_mem_user_max_wmem = SplitLastValue(tcp_mem_server_wmem);
        string tcp_mem_user_max_rmem = SplitLastValue(tcp_mem_server_rmem);
        string tcp_mem_server_max_wmem = SplitLastValue(tcp_mem_server_wmem);
        string tcp_mem_server_max_rmem = SplitLastValue(tcp_mem_server_rmem);

		// Set the value of send buffer size - net.ipv4.tcp_wmem
		// Set the vaue of receive buffer size - net.ipv4.tcp_rmem
        stack.SysctlSet (mobile.Get(0), ".net.ipv4.tcp_mem", tcp_mem_user);
        stack.SysctlSet (mobile.Get(0), ".net.ipv4.tcp_wmem", tcp_mem_server_wmem);
        stack.SysctlSet (mobile.Get(0), ".net.ipv4.tcp_rmem", tcp_mem_user_rmem);
        stack.SysctlSet (mobile.Get(0), ".net.core.wmem_max", tcp_mem_user_max_wmem);
        stack.SysctlSet (mobile.Get(0), ".net.core.rmem_max", tcp_mem_user_max_rmem);
        stack.SysctlSet (mobile.Get(0), ".net.core.netdev_max_backlog", "250000");
            
        stack.SysctlSet (core.Get(0), ".net.ipv4.tcp_mem", tcp_mem_server);
        stack.SysctlSet (core.Get(0), ".net.ipv4.tcp_wmem", tcp_mem_server_wmem);
        stack.SysctlSet (core.Get(0), ".net.ipv4.tcp_rmem", tcp_mem_server_rmem);
        stack.SysctlSet (core.Get(0), ".net.core.wmem_max", tcp_mem_server_max_wmem);
        stack.SysctlSet (core.Get(0), ".net.core.rmem_max", tcp_mem_server_max_rmem);
        stack.SysctlSet (core.Get(0), ".net.core.netdev_max_backlog", "250000");

        stack.SysctlSet (mobile, ".net.ipv4.tcp_congestion_control", tcp_cc);
        stack.SysctlSet (BS, ".net.ipv4.tcp_congestion_control", tcp_cc);
        stack.SysctlSet (core, ".net.ipv4.tcp_congestion_control", tcp_cc);
        stack.SysctlSet (router, ".net.ipv4.tcp_congestion_control", tcp_cc);
#else
        NS_LOG_ERROR("Linux kernel stack for DCE is not available. build with dce-linux module.");
        // silently exit
        return 0;
#endif
    }

    // Assigning IP Addresses to devices
    Ipv4AddressHelper address;
    // for router mobile and BS net devices
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer IPRouterBSDown = address.Assign(chanRouterBSDown);

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer IPRouterBSUp = address.Assign(chanRouterBSUp);

    // for router core and BS devices
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer IPBSRouterDown = address.Assign(chanBSRouterDown);

    address.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer IPBSRouterUp = address.Assign(chanBSRouterUp);

    // for router to (mobile & core)
    address.SetBase("10.9.1.0", "255.255.255.0");
    Ipv4InterfaceContainer IPMobileRouter = address.Assign(chanMobileRouter);

    address.SetBase("10.9.2.0", "255.255.255.0");
    Ipv4InterfaceContainer IPRouterCore = address.Assign(chanRouterCore);

    // Beginning of setup ip routes

    // routing configuration if use ns3 stack
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
#ifdef KERNEL_STACK
    //routing configuration if use linux stack
    if(stack == "linux"){
        ostringstream cmd_oss;

        // mobile
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.2.2" << "/255.255.255.255" << " via " << "10.9.1.2";
        LinuxStackHelper::RunIp(mobile.Get(0), Seconds(0.1), cmd_oss.str().c_str());

        // core
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.1.1" << "/255.255.255.255" << " via " << "10.9.2.1";
        LinuxStackHelper::RunIp(core.Get(0), Seconds(0.1), cmd_oss.str().c_str());

        // mobile router
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.2.2" << "/255.255.255.255" << " via " << "10.2.1.2";
        LinuxStackHelper::RunIp(router.Get(0), Seconds(0.1), cmd_oss.str().c_str());

        // core router
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.1.1" << "/255.255.255.255" << " via " << "10.1.2.1";
        LinuxStackHelper::RunIp(router.Get(1), Seconds(0.1), cmd_oss.str().c_str());

        // BS downstream
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.1.1" << "/255.255.255.255" << " via " << "10.1.1.1";
        LinuxStackHelper::RunIp(BS.Get(0), Seconds(0.1), cmd_oss.str().c_str());

        // BS upstream
        cmd_oss.str("");
        cmd_oss << "route add " << "10.9.2.2" << "/255.255.255.255" << " via " << "10.2.2.2";
        LinuxStackHelper::RunIp(BS.Get(1), Seconds(0.1), cmd_oss.str().c_str());

        LinuxStackHelper::PopulateRoutingTables ();
    }
#endif
    // Ending of setup ip routes 

    DceApplicationHelper dce;
    dce.SetStackSize(1 << 20);

 
    switch (TypeOfConnection) {
        case 'p': {
            if(downloadMode){
                chanRouterBSDown.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emDown));
                chanBSRouterUp.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emUp));

                // Launch iperf client on node 5 (core)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-c");
                dce.AddArgument("10.9.1.1");
                dce.AddArgument("-i");
                dce.AddArgument("1");
                dce.AddArgument("--time");
                dce.AddArgument("10");
                ApplicationContainer ClientApps0 = dce.Install(core.Get(0));
                ClientApps0.Start(Seconds(0.7));
                ClientApps0.Stop(Seconds(20));

                // dump traffics from several channels
                p2pMobRouter.EnablePcap("TCP_download", chanMobileRouter);
                p2pMobRouter.EnablePcap("TCP_download", chanRouterCore);


                // Launch iperf server on node 0 (mobile device)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-s");
                dce.AddArgument("-P");
                dce.AddArgument("1");
                ApplicationContainer SerApps0 = dce.Install(mobile.Get(0));
                SerApps0.Start(Seconds(0.6));

            } else {
                chanRouterBSDown.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emDown));
                chanBSRouterUp.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emUp));

                // Launch iperf client on node 0 (mobile device)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-c");
                dce.AddArgument("10.9.2.2");
                dce.AddArgument("-i");
                dce.AddArgument("1");
                dce.AddArgument("--time");
                dce.AddArgument("10");
                ApplicationContainer ClientApps0 = dce.Install(mobile.Get(0));
                ClientApps0.Start(Seconds(0.7));
                ClientApps0.Stop(Seconds(20));

                // dump traffics from several channels
                p2pMobRouter.EnablePcap("TCP_upload", chanMobileRouter);
                p2pMobRouter.EnablePcap("TCP_upload", chanRouterCore);

                // Launch iperf server on node 5 (core)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-s");
                dce.AddArgument("-P");
                dce.AddArgument("1");
                ApplicationContainer SerApps0 = dce.Install(core.Get(0));
                SerApps0.Start(Seconds(0.6));

            }
            
        }
            break;

        case 'u': {
            if(downloadMode){
                chanRouterBSDown.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emDown));
                chanBSRouterUp.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emUp));

                // Launch iperf client on node 5 (core)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-c");
                dce.AddArgument("10.9.1.1");
                dce.AddArgument("-i");
                dce.AddArgument("1");
                dce.AddArgument("--time");
                dce.AddArgument("10");
                dce.AddArgument("-u");
                dce.AddArgument("-b");
                dce.AddArgument(udp_bw + "m");
                ApplicationContainer apps = dce.Install(core.Get(0));
                apps.Start(Seconds(0.7));
                apps.Stop(Seconds(20));

                // dump traffics from several channels
                p2pMobRouter.EnablePcap("UDP_download", chanMobileRouter);
                p2pMobRouter.EnablePcap("UDP_download", chanRouterCore);

                // Launch iperf server on node 0 (mobile)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-s");
                dce.AddArgument("-P");
                dce.AddArgument("1");
                dce.AddArgument("-u");
                ApplicationContainer ServerApps0 = dce.Install(mobile.Get(0));
                ServerApps0.Start(Seconds(0.6));

            } else {
                chanRouterBSDown.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emDown));
                chanBSRouterUp.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emUp));

                // Launch iperf client on node 0 (mobile)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-c");
                dce.AddArgument("10.9.2.2");
                dce.AddArgument("-i");
                dce.AddArgument("1");
                dce.AddArgument("--time");
                dce.AddArgument("10");
                dce.AddArgument("-u");
                dce.AddArgument("-b");
                dce.AddArgument(udp_bw + "m");
                ApplicationContainer apps = dce.Install(mobile.Get(0));
                apps.Start(Seconds(0.7));
                apps.Stop(Seconds(20));

                // dump traffics from several channels
                p2pMobRouter.EnablePcap("UDP_download", chanMobileRouter);
                p2pMobRouter.EnablePcap("UDP_download", chanRouterCore);

                // Launch iperf server on node 0 (mobile)
                dce.SetBinary("iperf");
                dce.ResetArguments();
                dce.ResetEnvironment();
                dce.AddArgument("-s");
                dce.AddArgument("-P");
                dce.AddArgument("1");
                dce.AddArgument("-u");
                ApplicationContainer ServerApps0 = dce.Install(core.Get(0));
                ServerApps0.Start(Seconds(0.6));
            }
            
        }
            break;


        case 'w': {
            chanRouterBSDown.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emDown));
            chanBSRouterUp.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emUp));

            dce.SetBinary("thttpd");
            dce.ResetArguments();
            dce.ResetEnvironment();
            dce.SetUid(1);
            dce.SetEuid(1);
            // dce.AddArgument("-r");
            // dce.AddArgument("-d");
            // dce.AddArgument("/var/www/htdocs/");
            ApplicationContainer serHttp = dce.Install(core.Get(0));
            serHttp.Start(Seconds(1));

            dce.SetBinary("wget");
            dce.ResetArguments();
            dce.ResetEnvironment();
            dce.AddArgument("http://10.9.2.2/index.html");
            dce.AddArgument("-o");
            dce.AddArgument("wgetlog");
            //dce.AddArgument("-r");

            ApplicationContainer clientHttp = dce.Install(mobile.Get(0));
            clientHttp.Start(Seconds(2));
        }
            break;

        default:
            break;
    }
    
    p2pCoreRouter.EnablePcapAll("dce-multi", false);
    
	AnimationInterface::SetConstantPosition(mobile.Get(0), 1.0, 2.0);
	AnimationInterface::SetConstantPosition(router.Get(0), 3.0, 2.0);
	AnimationInterface::SetConstantPosition(BS.Get(0), 5.0, 1.0);
	AnimationInterface::SetConstantPosition(BS.Get(1), 5.0, 3.0);
	AnimationInterface::SetConstantPosition(router.Get(1), 7.0, 2.0);
	AnimationInterface::SetConstantPosition(core.Get(0), 9.0, 2.0);
    AnimationInterface animation("de-multi.xml");

    Simulator::Stop(Seconds(40.0));
    std::cout << "Running simulation" << std::endl;
    Simulator::Run();
    std::cout << "Simulation is completed" << std::endl;
    Simulator::Destroy();

    return 0;
}
