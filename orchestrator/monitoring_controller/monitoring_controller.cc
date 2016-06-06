#include "monitoring_controller.h"

MonitoringController::MonitoringController()
{
}

void MonitoringController::startMonitoring(string measure_string, list< pair<string, string> > portsMapping, list< pair<string, string> > vnfsMapping, list<map<unsigned int, string> > vnfsPortsMapping)
{
	logger(ORCH_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "Starting a monitoring session (through the monitoring plugin)");
	logger(ORCH_WARNING, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "The monitoring is supported only in 'veth' ports towards VNFs");
	
	assert(vnfsMapping.size() == vnfsPortsMapping.size());

	//Prepare the json to publish on the Double Decker bus

	//Handle the physical ports
	Array sap_array;
	for(list< pair<string, string> >::iterator port = portsMapping.begin(); port != portsMapping.end();port++)
	{
		Object sap;
		sap["name"] = port->first;
		sap["interface"] = port->second;

		sap_array.push_back(sap);
	}

	//Handle the VNFs
	list<map<unsigned int, string> >::iterator vnfPortMapping = vnfsPortsMapping.begin();
	list<pair<string, string> >::iterator vnfMapping = vnfsMapping.begin();
	Array vnfs_array;
	for(; vnfPortMapping != vnfsPortsMapping.end(); vnfPortMapping++, vnfMapping++)
	{
		Array ports;

		map<unsigned int, string> aMapping = *vnfPortMapping;
		for(map<unsigned int, string>::iterator mm = aMapping.begin(); mm != aMapping.end(); mm++)
		{
			Object port;
			
			stringstream port_name;
			port_name << mm->second << ".lxc";
			
			port["id"] = mm->first;
			port["name"] = port_name.str();
		
			ports.push_back(port);
		}

		Object vnf;
		vnf["id"] = vnfMapping->first;
		vnf["name"] = vnfMapping->second;
		vnf["ports"] = ports;

		vnfs_array.push_back(vnf);
	}

	Object nffg;
	nffg["measure"] = measure_string.c_str();
	nffg["VNFs"] = vnfs_array;
	nffg["sap"] = sap_array;
	
	Object params;
	params["nffg"] = nffg;
	
	Object json;
	json["params"] = params;
	json["jsonrpc"] = "2.0";
	json["method"] = "startNFFG";

	stringstream message;
	write_formatted(json,message);

	logger(ORCH_DEBUG_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "Sending the following message to the monitoring plugin:");
	logger(ORCH_DEBUG_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "%s",message.str().c_str());

	DoubleDeckerClient::publish(UNIFY_MPP,message.str().c_str());
}

void MonitoringController::stopMonitoring()
{
	logger(ORCH_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "Stopping a monitoring session (through the monitoring plugin)");

	Object json;
	json["params"] = "";
	json["jsonrpc"] = "2.0";
	json["method"] = "stopNFFG";

	stringstream message;
	write_formatted(json,message);

	logger(ORCH_DEBUG_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "Sending the following message to the plugin monitor:");
	logger(ORCH_DEBUG_INFO, MONITORING_CONTROLLER_MODULE_NAME, __FILE__, __LINE__, "%s",message.str().c_str());

	DoubleDeckerClient::publish(UNIFY_MPP,message.str().c_str());
}
