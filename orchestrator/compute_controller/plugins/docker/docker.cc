#include "docker.h"

bool Docker::isSupported(Description&)
{
	int retVal;

	retVal = system(CHECK_DOCKER);
	retVal = retVal >> 8;

	logger(ORCH_DEBUG, DOCKER_MODULE_NAME, __FILE__, __LINE__, "Script returned: %d\n",retVal);

	if(retVal > 0)
	{
		logger(ORCH_DEBUG_INFO, DOCKER_MODULE_NAME, __FILE__, __LINE__, "Docker deamon is running.");
		return true;
	}

	logger(ORCH_DEBUG_INFO, DOCKER_MODULE_NAME, __FILE__, __LINE__, "Docker deamon is not running (at least, it is not running with the LXC implementation).");
	return false;
}

bool Docker::startNF(StartNFIn sni)
{
	uint64_t lsiID = sni.getLsiID();
	string nf_name = sni.getNfName();
	
	string uri_image = description->getURI();
	
	map<unsigned int, string> namesOfPortsOnTheSwitch = sni.getNamesOfPortsOnTheSwitch();
	unsigned int n_ports = namesOfPortsOnTheSwitch.size();
	
	//The second element of a map is a structure that have two elements, the mac address and the ip address
	//In case of empty string, such an information is not present
	map<unsigned int, port_network_config_t > portsConfiguration = sni.getPortsConfiguration();
	for(map<unsigned int, port_network_config_t >::iterator configuration = portsConfiguration.begin(); configuration != portsConfiguration.end(); configuration++)
	{
		logger(ORCH_DEBUG, DOCKER_MODULE_NAME, __FILE__, __LINE__, "\t MAC address: %s",(configuration->second.mac_address).c_str());
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION		
		logger(ORCH_DEBUG, DOCKER_MODULE_NAME, __FILE__, __LINE__, "\t IP address: %s",(configuration->second.ip_address).c_str());
#endif
	}
	
	stringstream command;
	command << PULL_AND_RUN_DOCKER_NF << " " << lsiID << " " << nf_name << " " << uri_image << " " << n_ports;
	
	assert(portsConfiguration.size() == namesOfPortsOnTheSwitch.size());
	map<unsigned int, port_network_config_t >::iterator configuration = portsConfiguration.begin();
	for(map<unsigned int, string>::iterator pn = namesOfPortsOnTheSwitch.begin(); pn != namesOfPortsOnTheSwitch.end(); pn++)
	{
		command << " "  << pn->second;
		command << " ";
		if(configuration->second.mac_address != "")
			command <<  configuration->second.mac_address;
		else
			command << 0;
			
		command << " ";
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION	
		if(configuration->second.ip_address != "")
			command <<  configuration->second.ip_address;
		else
#endif
			command << 0;
			
		configuration++;
	}
		
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION		
	list<port_mapping_t >  control_ports = sni.getControlPorts();
	command << " " << control_ports.size();
	if(control_ports.size() != 0)
	{
		logger(ORCH_DEBUG, DOCKER_MODULE_NAME, __FILE__, __LINE__, "VNF '%s' requires %d control connections",nf_name.c_str(), control_ports.size());
		for(list<port_mapping_t >::iterator control = control_ports.begin(); control != control_ports.end(); control++)
		{
			logger(ORCH_DEBUG, DOCKER_MODULE_NAME, __FILE__, __LINE__, "\t host TCP port: %s - VNF TCP port: %s",(control->host_port).c_str(), (control->guest_port).c_str());
			command << " " << control->host_port << " " << control->guest_port;
		}
	}
#else
	command << " 0";
#endif

	logger(ORCH_DEBUG_INFO, DOCKER_MODULE_NAME, __FILE__, __LINE__, "Executing command \"%s\"",command.str().c_str());

	int retVal = system(command.str().c_str());
	retVal = retVal >> 8;
	
	if(retVal == 0)
		return false;
		
	return true;
}

bool Docker::stopNF(StopNFIn sni)
{
	uint64_t lsiID = sni.getLsiID();
	string nf_name = sni.getNfName();

	stringstream command;
	command << STOP_DOCKER_NF << " " << lsiID << " " << nf_name;
	
	logger(ORCH_DEBUG_INFO, DOCKER_MODULE_NAME, __FILE__, __LINE__, "Executing command \"%s\"",command.str().c_str());
	int retVal = system(command.str().c_str());
	retVal = retVal >> 8;

	if(retVal == 0)
		return false;

	return true;
}

#if 0
unsigned int Docker::convertNetmask(string netmask)
{
	unsigned int slash = 0;
	unsigned int mask;
	
	int first, second, third, fourth;
	sscanf(netmask.c_str(),"%d.%d.%d.%d",&first,&second,&third,&fourth);
	mask = (first << 24) + (second << 16) + (third << 8) + fourth;
	
	for(int i = 0; i < 32; i++)
	{
		if((mask & 0x1) == 1)
			slash++;
		mask = mask >> 1;
	}
	
	return slash;
}
#endif
