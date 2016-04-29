#include "graph_manager.h"

pthread_mutex_t GraphManager::graph_manager_mutex;
uint32_t GraphManager::nextControllerPort = FIRTS_OF_CONTROLLER_PORT;

void GraphManager::mutexInit()
{
	pthread_mutex_init(&graph_manager_mutex, NULL);
}

GraphManager::GraphManager(int core_mask,set<string> physical_ports,string un_address,bool orchestrator_in_band,string un_interface,string ipsec_certificate) :
	un_address(un_address), orchestrator_in_band(orchestrator_in_band), un_interface(un_interface), ipsec_certificate(ipsec_certificate), switchManager()
{
	//TODO: this code can be simplified. Why don't providing the set<string> to the switch manager?
	set<CheckPhysicalPortsIn> phyPortsRequired;
	for(set<string>::iterator pp = physical_ports.begin(); pp != physical_ports.end(); pp++)
	{
		CheckPhysicalPortsIn cppi(*pp);
		phyPortsRequired.insert(cppi);
	}

	set<string> phyPorts;//maps the name into the side
	for(set<CheckPhysicalPortsIn>::iterator pp = phyPortsRequired.begin(); pp != phyPortsRequired.end(); pp++)
		phyPorts.insert(pp->getPortName());

	//Create the openflow controller for the LSI-0

	pthread_mutex_lock(&graph_manager_mutex);
	uint32_t controllerPort = nextControllerPort;
	nextControllerPort++;
	pthread_mutex_unlock(&graph_manager_mutex);

	ostringstream strControllerPort;
	strControllerPort << controllerPort;

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Checking the available physical interfaces...");
	try
	{
		//Check the available physical ports
		switchManager.checkPhysicalInterfaces(phyPortsRequired);
	} catch (...)
	{
		throw GraphManagerException();
	}

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%d physical interfaces under the control of the un-orchestrator:",phyPorts.size());
	for(set<string>::iterator p = phyPorts.begin(); p != phyPorts.end(); p++)
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t%s",(*p).c_str());

	//Create the openflow controller for the lsi-0
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Creating the openflow controller for LSI-0...");

	rofl::openflow::cofhello_elem_versionbitmap versionbitmap;
	switch(OFP_VERSION)
	{
		case OFP_10:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.0");
			versionbitmap.add_ofp_version(rofl::openflow10::OFP_VERSION);
			break;
		case OFP_12:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.2");
			versionbitmap.add_ofp_version(rofl::openflow12::OFP_VERSION);
			break;
		case OFP_13:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.3");
			versionbitmap.add_ofp_version(rofl::openflow13::OFP_VERSION);
			break;
	}

	Controller *controller = new Controller(versionbitmap,graph,strControllerPort.str());
	controller->start();

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Creating the LSI-0...");

	//The following structures are empty. No network function, virtual link gre-tunnel endpoint is attached.
	list<highlevel::VNFs> dummy_network_functions;
	map<string, map<unsigned int, PortType> > dummy_nfs_ports_type;
	list<highlevel::EndPointGre> dummy_gre_endpoints;
	vector<VLink> dummy_virtual_links;

	LSI *lsi = new LSI(string(OF_CONTROLLER_ADDRESS), strControllerPort.str(), phyPorts, dummy_network_functions,
	dummy_gre_endpoints,dummy_virtual_links,dummy_nfs_ports_type);

	try
	{
		//Create a new LSI, which is the LSI-0 of the node
		map<string, vector<string> > gre_endpoints;
		assert(lsi->getEndpointsPorts().size() == 0);
		map<string,nf_t>  nf_types;
		map<string,list<nf_port_info> > netFunctionsPortsInfo;

		CreateLsiIn cli(string(OF_CONTROLLER_ADDRESS),strControllerPort.str(),lsi->getPhysicalPortsName(),nf_types,netFunctionsPortsInfo,gre_endpoints,lsi->getVirtualLinksRemoteLSI(), this->un_address, this->ipsec_certificate);

		CreateLsiOut *clo = switchManager.createLsi(cli);

		lsi->setDpid(clo->getDpid());
		map<string,unsigned int> physicalPorts = clo->getPhysicalPorts();
		//TODO check that the physical ports returned are the same provided to the switch manager
		for(map<string,unsigned int>::iterator it = physicalPorts.begin(); it != physicalPorts.end(); it++)
		{
			if(!lsi->setPhysicalPortID(it->first,it->second))
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "An unknown physical interface \"%s\" has been attached to the lsi-0",it->first.c_str());
				delete(clo);
				throw GraphManagerException();
			}
		}

		map<string,map<string, unsigned int> > nfsports = clo->getNetworkFunctionsPorts();
		if(!nfsports.empty())
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Non required NFs ports have been attached to the lsi-0");
			delete(clo);
			throw GraphManagerException();
		}

		map<string, unsigned int> epsports = clo->getEndpointsPorts();
		if(!epsports.empty())
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Non required endpoints ports have been attached to the lsi-0");
			delete(clo);
			throw GraphManagerException();
		}

		list<pair<unsigned int, unsigned int> > vl = clo->getVirtualLinks();
		if(!vl.empty())
		{
			logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "Non required connections have been created between the lsi-0 and other(?) lsis");
			delete(clo);
			throw GraphManagerException();
		}

		delete(clo);
	} catch (SwitchManagerException e)
	{
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		throw GraphManagerException();
	}

	dpid0 = lsi->getDpid();
	map<string,unsigned int> lsi_ports = lsi->getPhysicalPorts();

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "LSI ID: %d",dpid0);
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Physical ports:",lsi_ports.size());
	for(map<string,unsigned int>::iterator p = lsi_ports.begin(); p != lsi_ports.end(); p++)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s -> %d",(p->first).c_str(),p->second);

	graphInfoLSI0.setLSI(lsi);
	graphInfoLSI0.setController(controller);

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "LSI-0 and its controller are created");

	ComputeController::setCoreMask(core_mask);

	//if control is in band install the default rules on LSI-0 otherwise skip this code
	if(orchestrator_in_band && !un_interface.empty() && !un_address.empty())
		handleInBandController(lsi,controller);
}

void GraphManager::handleInBandController(LSI *lsi, Controller *controller)
{
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Handling in band controller");

	unsigned int i = 0;

	//remove first " character
	un_interface.erase(0,1);
	//remove last " character
	un_interface.erase(un_interface.size()-1,1);

	//Install the default rules on LSI-0
	map<string,unsigned int> lsi_ports = lsi->getPhysicalPorts();
	lowlevel::Match lsi0Match, lsi0Match0, lsi0Match1, lsi0Match2;
	if(lsi_ports.count((char *)un_interface.c_str()) == 0)
	{
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Control interface does not exist in a list of available plysical ports.");
		throw GraphManagerException();
	}

	map<string,unsigned int>::iterator translation = lsi_ports.find((char *)un_interface.c_str());

	/* It is necessary to intercept incoming arp requests with IP source equal to un_interface?

	lsi0Match.setArpSpa((char *)un_address.c_str());
	lsi0Match.setEthType(2054 & 0xFFFF);
	lsi0Match.setInputPort(translation->second);


	//Create the rule and add it to the graph
	//The rule ID is created as follows DEFAULT-GRAPH_ID
	newRuleID << DEFAULT_GRAPH << "_" << i;
	lowlevel::Rule lsi0Rule(lsi0Match,lsi0Action,newRuleID.str(),HIGH_PRIORITY);
	graphLSI0lowLevel.addRule(lsi0Rule);

	i++;
	*/

	lsi0Match0.setArpTpa((char *)un_address.c_str());
	lsi0Match0.setEthType(2054 & 0xFFFF);
	lsi0Match0.setInputPort(translation->second);

	lsi0Match1.setIpv4Dst((char *)un_address.c_str());
	lsi0Match1.setEthType(2048 & 0xFFFF);
	lsi0Match1.setInputPort(translation->second);

	lowlevel::Action lsi0Action(true);

	//Create the rule and add it to the graph
	//The rule ID is created as follows DEFAULT-GRAPH_ID
	stringstream newRuleID;
	newRuleID << DEFAULT_GRAPH << "_" << i;
	lowlevel::Rule lsi0Rule0(lsi0Match0,lsi0Action,newRuleID.str(),HIGH_PRIORITY);
	graphLSI0lowLevel.addRule(lsi0Rule0);

	i++;

	//Create the rule and add it to the graph
	//The rule ID is created as follows DEFAULT-GRAPH_ID
	newRuleID.str("");
	newRuleID << DEFAULT_GRAPH << "_" << i;
	lowlevel::Rule lsi0Rule1(lsi0Match1,lsi0Action,newRuleID.str(),HIGH_PRIORITY);
	graphLSI0lowLevel.addRule(lsi0Rule1);

	i++;

	lowlevel::Match lsi0Match3(true);

	lowlevel::Action lsi0Action1(translation->second);

	//Create the rule and add it to the graph
	//The rule ID is created as follows DEFAULT-GRAPH_ID
	newRuleID.str("");
	newRuleID << DEFAULT_GRAPH << "_" << i;
	lowlevel::Rule lsi0Rule3(lsi0Match3,lsi0Action1,newRuleID.str(),HIGH_PRIORITY);
	graphLSI0lowLevel.addRule(lsi0Rule3);

	graphLSI0lowLevel.print();

	//Insert new rules into the LSI-0
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the LSI-0");
	controller->installNewRules(graphLSI0lowLevel.getRules());

	printInfo(graphLSI0lowLevel,graphInfoLSI0.getLSI());
}

GraphManager::~GraphManager()
{
	//Deleting tenants LSIs
	for(map<string,GraphInfo>::iterator lsi = tenantLSIs.begin(); lsi != tenantLSIs.end();)
	{
		map<string,GraphInfo>::iterator tmp = lsi;
		lsi++;
		try
		{
			deleteGraph(tmp->first);
		}catch(...)
		{
			assert(0);
			/*nothing to do, since the node orchestrator is terminating*/
		}
	}

	//Deleting LSI-0
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Deleting the graph for the LSI-0...");
	LSI *lsi0 = graphInfoLSI0.getLSI();

	try
	{
		switchManager.destroyLsi(lsi0->getDpid());
	} catch (SwitchManagerException e)
	{
		logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		//we don't throw any exception here, since the graph manager is terminating
	}

	Controller *controller = graphInfoLSI0.getController();
	delete(controller);
	controller = NULL;
}

bool GraphManager::graphExists(string graphID)
{
	if(tenantLSIs.count(graphID) == 0)
		return false;

	return true;
}

bool GraphManager::flowExists(string graphID, string flowID)
{
	assert(tenantLSIs.count(graphID) != 0);

	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	highlevel::Graph *graph = graphInfo.getGraph();

	if(!graph->ruleExists(flowID))
		return false;

	return true;
}

Object GraphManager::toJSON(string graphID)
{
	if(tenantLSIs.count(graphID) == 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The graph \"%s\" does not exist",graphID.c_str());
		assert(0);
		throw GraphManagerException();
	}
	highlevel::Graph *graph = (tenantLSIs.find(graphID))->second.getGraph();
	assert(graph != NULL);

	Object flow_graph;

	try
	{
		flow_graph[FORWARDING_GRAPH] = graph->toJSON();
	}catch(...)
	{
		assert(0);
		throw GraphManagerException();
	}

	return flow_graph;
}

bool GraphManager::deleteGraph(string graphID)
{
	if(tenantLSIs.count(graphID) == 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The graph \"%s\" does not exist",graphID.c_str());
		return false;
	}

	/**
	*	@outline:
	*
	*		0) remove the rules from the LSI0
	*		1) stop the NFs
	*		2) delete the LSI, the virtual links and the
	*			ports related to NFs
	*		3) handle the internal LSI associated with the internal endpoints that are part of the graph
	*/

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Deleting graph '%s'...",graphID.c_str());

	LSI *tenantLSI = (tenantLSIs.find(graphID))->second.getLSI();
	highlevel::Graph *highLevelGraph = (tenantLSIs.find(graphID))->second.getGraph();

	/**
	*		0) remove the rules from the LSI-0
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "0) Remove the rules from the LSI-0");

	lowlevel::Graph graphLSI0 = GraphTranslator::lowerGraphToLSI0(highLevelGraph,tenantLSI,graphInfoLSI0.getLSI(),un_interface,internalLSIsConnections,orchestrator_in_band,false);
	graphLSI0lowLevel.removeRules(graphLSI0.getRules());

	//Remove rules from the LSI-0
	graphInfoLSI0.getController()->removeRules(graphLSI0.getRules());

	/**
	*		1) stop the NFs
	*/
	ComputeController *computeController = (tenantLSIs.find(graphID))->second.getComputeController();
#ifdef RUN_NFS
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "1) Stop the NFs");
	computeController->stopAll();
#else
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "1) Flag RUN_NFS disabled. No NF to be stopped");
#endif

	/**
	*		2) delete the LSI, the internal LSIs, the virtual links, the
	*			ports related to NFs and the ports related to GRE tunnel
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "2) Delete the LSI, the vlinks, and the ports used by NFs");

	try
	{
		//delete the LSI
		switchManager.destroyLsi(tenantLSI->getDpid());
	} catch (SwitchManagerException e)
	{
		logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		throw GraphManagerException();
	}

	tenantLSIs.erase(tenantLSIs.find(highLevelGraph->getID()));

	delete(tenantLSI);
	delete(computeController);
	
	tenantLSI = NULL;
	computeController = NULL;

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Tenant LSI (ID: %s) and its controller have been destroyed!",graphID.c_str());
	printInfo(graphLSI0lowLevel,graphInfoLSI0.getLSI());

	/**
	* 	3) If no other graph is still connected to an internal graph, such an internal graph can be deleted.
	*	Otherwise, only the flow and the virtual link related to the current graph are deleted.
	*/
	list<highlevel::EndPointInternal> internalEndpoints = highLevelGraph->getEndPointsInternal();
	for(list<highlevel::EndPointInternal>::iterator iep = internalEndpoints.begin(); iep != internalEndpoints.end(); iep++)
	{
		string internal_group(iep->getGroup());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3) Considering the internal graph associated with the internal group '%s'",internal_group.c_str());

		LSI *internalLSI = (internalLSIs.find(internal_group))->second.getLSI();
		//The number of virtual links of the internal LSI is equal to the number of graph attached to the internal endpoint represented by such an LSI
		assert((internalLSI->getVirtualLinks()).size() == timesUsedEndPointsInternal[internal_group]);

		if(timesUsedEndPointsInternal[internal_group] == 1)
		{
			//The internal graph representing this internal endpoint can be removed. In fact to other graph is connected with it :)
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal graph associated with the internal group '%s' must be deleted",internal_group.c_str());

			try
			{
				//delete the LSI
				switchManager.destroyLsi(internalLSI->getDpid());
			} catch (SwitchManagerException e)
			{
				logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}

			internalLSIs.erase(internalLSIs.find(internal_group));
			internalLSIsConnections.erase(internalLSIsConnections.find(internal_group));
			assert(internalLSIsConnections.count(internal_group) == 0);

			delete(internalLSI);
			internalLSI = NULL;
			
			internalLSIsCreated[internal_group] = false;
		}
		else
		{
			//In this case, only the parts related to the current graph (i.e., a flow and a virtual link) must be deleted.
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "A virtual link and a rule must be deleted from the internal graph associated with the internal group '%s'",internal_group.c_str());

			//Remove the flow from the internal LSI
			GraphInfo internalGraphInfo = (internalLSIs.find(internal_group))->second;
			Controller *internalController = internalGraphInfo.getController();
			assert(internalController != NULL);

			stringstream ruleID;
			ruleID << "INTERNAL-GRAPH" << "-" << internal_group << "_" << highLevelGraph->getID(); //This guarantees that the ID of the rule is unique
			internalController->removeRuleFromID(ruleID.str());

			//Remove the virtual link connecting the internal LSI with the LSI-0
			
			assert(internalLSIsConnections.count(internal_group) != 0);
			map <string, unsigned int> internalLSIsConnectionsOfGroup = internalLSIsConnections[internal_group];
			assert(internalLSIsConnectionsOfGroup.count(graphID) != 0);
			unsigned int vlink_port_on_lsi_0 = internalLSIsConnectionsOfGroup[graphID];

			//We have to identify the virtual link to be removed
			vector<VLink> vlinks = internalLSI->getVirtualLinks();
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal LSI has currently the following virtual links");
			vector<VLink>::iterator vl = vlinks.begin();
			for(; vl != vlinks.end(); vl++)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t(ID: %x) %x:%d -> %x:%d",vl->getID(),internalLSI->getDpid(),vl->getLocalID(),vl->getRemoteDpid(),vl->getRemoteID());
				if(vl->getRemoteID() == vlink_port_on_lsi_0)
				{
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\tTHIS VLINK MUST BE REMOVED");
					DestroyVirtualLinkIn dvli(internalLSI->getDpid(), vl->getLocalID(), vl->getRemoteDpid(), vl->getRemoteID());
					switchManager.destroyVirtualLink(dvli);
					internalLSI->removeVlink(vl->getID());
					break;
				}
			}
			assert(vl != vlinks.end());
			if(vl == vlinks.end())
				return false;

			internalLSIsConnectionsOfGroup.erase(internalLSIsConnectionsOfGroup.find(graphID));
			internalLSIsConnections[internal_group] = internalLSIsConnectionsOfGroup;
			//The following two instruction check that everything has been removed correctly
			internalLSIsConnectionsOfGroup = internalLSIsConnections[internal_group];
			assert(internalLSIsConnectionsOfGroup.count(graphID) == 0);
		}

		timesUsedEndPointsInternal[internal_group]--;
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal graph associated with the internal group '%s' is still used by %d other graphs",internal_group.c_str(),timesUsedEndPointsInternal[internal_group]);
	}//end iteration on the internal endpoints

	delete(highLevelGraph);
	highLevelGraph = NULL;

	return true;
}

bool GraphManager::deleteFlow(string graphID, string flowID)
{
	if(tenantLSIs.count(graphID) == 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The graph \"%s\" does not exist",graphID.c_str());
		assert(0);
		return false;
	}

	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	highlevel::Graph *graph = graphInfo.getGraph();

#if 0
	//Check if the rule to be removed exists
	if(!graph->ruleExists(flowID))
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The flow \"%s\" does not exist in graph \"%s\"",flowID.c_str(),graphID.c_str());
		assert(0);
		return false;
	}

	//if the graph has only this flow, remove the entire graph
	if(graph->getNumberOfRules() == 1)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The graph \"%s\" has only one flow. Then the entire graph will be removed",graphID.c_str());
		return deleteGraph(graphID);
	}
#endif

#if 0
	string endpointInvolved = graph->getEndpointInvolved(flowID);
	bool definedHere = false;
	if(endpointInvolved != "")
		definedHere = graph->isDefinedHere(endpointInvolved);*/
#endif

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Removing the flow from the LSI-0 graph");
	Controller *lsi0Controller = graphInfoLSI0.getController();
	stringstream lsi0FlowID;
	lsi0FlowID << graph->getID() << "_" << flowID;
	lsi0Controller->removeRuleFromID(lsi0FlowID.str());
	graphLSI0lowLevel.removeRuleFromID(lsi0FlowID.str());

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Removing the flow from the tenant-LSI graph");
	Controller *tenantController = graphInfo.getController();
	tenantController->removeRuleFromID(flowID);

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Removing the flow from the high level graph");
	RuleRemovedInfo rri = graph->removeRuleFromID(flowID);

	ComputeController *nfs_manager = graphInfo.getComputeController();
	LSI *lsi = graphInfo.getLSI();

	removeUselessPorts_NFs_Endpoints_VirtualLinks(rri,nfs_manager,graph,lsi);

	/*if(endpointInvolved != "")
	{
		if(definedHere)
		{
			availableEndPoints.erase(endpointInvolved);
			if(endPointsDefinedInActions.count(endpointInvolved) != 0)
				endPointsDefinedInActions.erase(endpointInvolved);
			if(endPointsDefinedInMatches.count(endpointInvolved) != 0)
				endPointsDefinedInMatches.erase(endpointInvolved);
		}
		else
		{
			availableEndPoints[endpointInvolved]--;
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "internal end point \"%s\" still used %d times",endpointInvolved.c_str(), availableEndPoints[endpointInvolved]);
		}
	}*/

	printInfo(graphLSI0lowLevel,graphInfoLSI0.getLSI());

	return true;
}

bool GraphManager::checkGraphValidity(highlevel::Graph *graph, ComputeController *computeController)
{
	list<highlevel::EndPointInterface> phyPorts = graph->getEndPointsInterface();
	list<highlevel::EndPointInternal> endPointsInternal = graph->getEndPointsInternal();
	list<highlevel::EndPointGre> endPointsGre = graph->getEndPointsGre();
	list<highlevel::EndPointVlan> endPointsVlan = graph->getEndPointsVlan();

	string graphID = graph->getID();

	/**
	*	Check if the required interface endpoints (i.e., physical ports) are under the control of the un-orchestrator
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The command requires %d new 'interface' endpoints",phyPorts.size());
	LSI *lsi0 = graphInfoLSI0.getLSI();
	map<string,unsigned int> physicalPorts = lsi0->getPhysicalPorts();
	for(list<highlevel::EndPointInterface>::iterator p = phyPorts.begin(); p != phyPorts.end(); p++)
	{
		string interfaceName = p->getInterface();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "* %s",interfaceName.c_str());
		if((physicalPorts.count(interfaceName)) == 0)
		{
			logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "Physical port \"%s\" does not exist",interfaceName.c_str());
			return false;
		}
	}

	/**
	*	No check is required for an internal endpoint
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The command requires %d 'internal' endpoints",endPointsInternal.size());
	for(list<highlevel::EndPointInternal>::iterator i = endPointsInternal.begin(); i != endPointsInternal.end(); i++)
	{
		string group = i->getGroup();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "* group: %s",group.c_str());
	}

	/**
	*	No check is required for a GRE endpoint
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The command requires %d 'gre-tunnel' endpoints",endPointsGre.size());

	/**
	*	No check is required for a VLAN endpoint
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The command requires %d 'vlan' endpoints",endPointsVlan.size());

	/**
	*	Check if the required network functions are available
	*/
	list<highlevel::VNFs> network_functions = graph->getVNFs();
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The command requires to retrieve %d new NFs",network_functions.size());
	//The description must be actually retrieved only for new VNFs, and not for VNFs whose number of ports is changed
	for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
	{
		//FIXME: not sure that this check is necessary
		if(computeController->getNFSelectedImplementation(nf->getName()))
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t* NF \"%s\" is already part of the graph; it is not retrieved again",nf->getName().c_str());
			continue;
		}
		nf_manager_ret_t retVal = computeController->retrieveDescription(nf->getName());
		if(retVal == NFManager_NO_NF)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "NF \"%s\" cannot be retrieved",nf->getName().c_str());
			return false;
		}
		else if(retVal == NFManager_SERVER_ERROR)
		{
			throw GraphManagerException();
		}
	}

	/**
	*	The graph is valid!
	*/
	return true;
}

void *startNF(void *arguments)
{
    to_thread_t *args = (to_thread_t *)arguments;
    assert(args->computeController != NULL);

    if(!args->computeController->startNF(args->nf_name, args->namesOfPortsOnTheSwitch, args->portsConfiguration
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
	    , args->controlConfiguration, args->environmentVariables
#endif
	))
		return (void*) 0;
	else
		return (void*) 1;
}


bool GraphManager::newGraph(highlevel::Graph *graph)
{
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Creating a new graph '%s'...",graph->getID().c_str());

	assert(tenantLSIs.count(graph->getID()) == 0);

	/**
	*	@outline:
	*
	*		0) check the validity of the graph
	*		1) create the Openflow controller for the tenant LSI
	*		2) select an implementation for each NF of the graph
	*		3) create the LSI, with the proper ports
	*		4) start the network functions
	*		5) create the OpenFlow controller for the internal LSIs (if it does not exist yet)
	*		6) create the internal LSI (if it does not exist yet), with the proper vlinks and download the rules in internal-LSI
	*		7) download the rules in LSI-0, tenant-LSI
	*/

	/**
	*	@Considerations:
	*		- the number of ports of a VNF described in the graph are immediately created (also those ports not used in any flow)
	*		- the GRE-tunnel endpoints described in the graph are immediately created (also those not used in any flow)
	*		- the internal endpoints described in the graph are immediately created (also those not used in any flow)
	*/

	/**
	*	@Endpoint internal limitation: 
	*		- connection from physical port to internal endpoint is not supported
	*		- connection from internal endpoint to physical port is not supported
	*/

	/**
	*	0) Check the validity of the graph
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "0) Checking the validity of the graph");
	ComputeController *computeController = new ComputeController();

	if(!checkGraphValidity(graph,computeController))
	{
		//This is an error in the request
		delete(computeController);
		computeController = NULL;
		return false;
	}

	/**
	*	1) Create the Openflow controller for the tenant LSI
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "1) Create the Openflow controller for the tenant LSI");

	pthread_mutex_lock(&graph_manager_mutex);
	uint32_t controllerPort = nextControllerPort;
	nextControllerPort++;
	pthread_mutex_unlock(&graph_manager_mutex);

	ostringstream strControllerPort;
	strControllerPort << controllerPort;

	rofl::openflow::cofhello_elem_versionbitmap versionbitmap;
	switch(OFP_VERSION)
	{
		case OFP_10:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.0");
			versionbitmap.add_ofp_version(rofl::openflow10::OFP_VERSION);
			break;
		case OFP_12:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.2");
			versionbitmap.add_ofp_version(rofl::openflow12::OFP_VERSION);
			break;
		case OFP_13:
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.3");
			versionbitmap.add_ofp_version(rofl::openflow13::OFP_VERSION);
			break;
	}

	lowlevel::Graph graphTmp ;
	Controller *controller = new Controller(versionbitmap,graphTmp,strControllerPort.str());
	controller->start();

	/**
	*	2) Select an implementation for each network function of the graph
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "2) Select an implementation for each NF of the graph");
	if(!computeController->selectImplementation())
	{
		//This is an internal error
		delete(computeController);
		delete(controller);
		computeController = NULL;
		controller = NULL;
		throw GraphManagerException();
	}

	/**
	*	3) Create the LSI
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3) Create the LSI");


	list<highlevel::VNFs> network_functions = graph->getVNFs();
	list<highlevel::EndPointInternal> endpointsInternal = graph->getEndPointsInternal();
	list<highlevel::EndPointGre> endpointsGre = graph->getEndPointsGre();

	vector<set<string> > vlVector = identifyVirtualLinksRequired(graph);
	set<string> vlNFs = vlVector[0];
	set<string> vlPhyPorts = vlVector[1];
	set<string> vlEndPointsGre = vlVector[2];
	set<string> vlEndPointsInternal = vlVector[3];

	/**
	*	A virtual link can be used in two direction, hence it can be shared between a NF port and a physical port.
	*	In principle a virtual link could also be shared between a NF port and an endpoint but, for simplicity, we
	*	use separated virtual links in case of endpoint.
	*/
	unsigned int toUp = vlNFs.size() + vlEndPointsGre.size();
	unsigned int toDown = vlPhyPorts.size() + vlEndPointsInternal.size();
	unsigned int numberOfVLrequired = (toUp > toDown)? toUp : toDown;

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "%d virtual links are required to connect the new LSI with LSI-0",numberOfVLrequired);

	vector<VLink> virtual_links;
	for(unsigned int i = 0; i < numberOfVLrequired; i++)
		virtual_links.push_back(VLink(dpid0));

	//The tenant-LSI is not connected to physical ports, but just the LSI-0
	//through virtual links, to network functions through virtual ports, and to gre tunnels through proper ports
	set<string> dummyPhyPorts;

	//Check the types of the VNFs ports
	map<string, nf_t>  nf_types;
	map<string, map<unsigned int, PortType> > nfs_ports_type;
	for(list<highlevel::VNFs>::iterator nf_it = network_functions.begin(); nf_it != network_functions.end(); nf_it++) 
	{
		const string& nf_name = nf_it->getName();
		list<unsigned int> nf_ports = nf_it->getPortsId();

		nf_types[nf_name] = computeController->getNFType(nf_name);

		//Gather VNF ports types
		const Description* descr = computeController->getNFSelectedImplementation(nf_name);
		map<unsigned int, PortType> nf_ports_type = descr->getPortTypes();  // Port types as specified by the retrieved and selected NF implementation

		if (nf_ports.size() > nf_ports_type.size())
		{
			//TODO: when we select the implementation, we should take into account the number of ports supported by the VNF!
			logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "Number of ports from (%d) graph is greater then the number of ports from NF description (%d) for \"%s\"",nf_ports.size(),nf_ports_type.size(), nf_name.c_str());
			return false;
		}

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "NF \"%s\" selected implementation (type %d) defines type for %d ports", nf_name.c_str(), nf_types[nf_name], nf_ports_type.size());
		// Fill in incomplete port type specifications (unless we make it mandatory input from name-resolver)
		for (list<unsigned int>::iterator p_it = nf_ports.begin(); p_it != nf_ports.end(); p_it++) {
			map<unsigned int, PortType>::iterator pt_it = nf_ports_type.find(*p_it);
			if (pt_it == nf_ports_type.end()) {
				logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "\tNF Port \"%s\":%d has no type defined in NF description", nf_name.c_str(), (*p_it));
				logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "\tThe ports ID used in the graph must correspond to those specified in the name resolver...");
				//This is an error of the client, which specified a wrong NF-FG (wrong ports towards a VNF)
				delete(computeController);
				delete(controller);
				computeController = NULL;
				controller = NULL;
				return false;
			}
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tNF Port \"%s\":%d is of type '%s'", nf_name.c_str(), (*p_it), portTypeToString(pt_it->second).c_str());
		}
		nfs_ports_type[nf_name] = nf_ports_type;
	}

	//Prepare the structure representing the new tenant-LSI
	LSI *lsi = new LSI(string(OF_CONTROLLER_ADDRESS), strControllerPort.str(), dummyPhyPorts, network_functions,
		endpointsGre,virtual_links,nfs_ports_type);

	CreateLsiOut *clo = NULL;
	try
	{
		//Create a new tenant-LSI
		map<string, vector<string> > description_gre_endpoints;
		list<highlevel::EndPointGre> endpoints_gre = lsi->getEndpointsPorts();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "%d Gre endpoints must be created",endpoints_gre.size());
		if(endpoints_gre.size() != 0)
		{
			vector<string> v_ep(5);
			string iface;
			for(list<highlevel::EndPointGre>::iterator e = endpoints_gre.begin(); e != endpoints_gre.end(); e++)
			{
				iface.assign(e->getId());
				v_ep[0].assign(e->getGreKey());
				v_ep[1].assign(e->getLocalIp());
				v_ep[2].assign(e->getRemoteIp());
				v_ep[3].assign(un_interface);
				if(e->isSafe())
					v_ep[4].assign("true");
				else
					v_ep[4].assign("false");
				description_gre_endpoints[iface] = v_ep;
				
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tkey: %s - local IP: %s - remote IP: %s - iface: %s",(e->getGreKey()).c_str(),(e->getLocalIp()).c_str(),(e->getRemoteIp()).c_str(),iface.c_str());
			}
		}

		assert(description_gre_endpoints.size() == endpoints_gre.size());

		map<string,list<string> > netFunctionsPortsName;
		for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++) 
		{
			netFunctionsPortsName[nf->getName()] = lsi->getNetworkFunctionsPortNames(nf->getName());
		}
		CreateLsiIn cli(string(OF_CONTROLLER_ADDRESS),strControllerPort.str(), lsi->getPhysicalPortsName(), nf_types, lsi->getNetworkFunctionsPortsInfo(), description_gre_endpoints, lsi->getVirtualLinksRemoteLSI(), string(OF_CONTROLLER_ADDRESS), this->ipsec_certificate);

		clo = switchManager.createLsi(cli);

		lsi->setDpid(clo->getDpid());

		map<string,unsigned int> physicalPorts = clo->getPhysicalPorts();
		if(physicalPorts.size() > 1)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Non required physical ports have been attached to the tenant-lsi");
			delete(clo);
			throw GraphManagerException();
		}

		map<string,map<string, unsigned int> > nfsports = clo->getNetworkFunctionsPorts();
		//TODO: check if the number of vnfs and ports is the same required
		for(map<string,map<string, unsigned int> >::iterator nfp = nfsports.begin(); nfp != nfsports.end(); nfp++)
		{
			if(!lsi->setNfSwitchPortsID(nfp->first,nfp->second))
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required network function port related to the network function \"%s\" has been attached to the tenant-lsi",nfp->first.c_str());
				delete(clo);
				throw GraphManagerException();
			}
		}

		map<string,unsigned int > epsports = clo->getEndpointsPorts();
		for(map<string,unsigned int >::iterator ep = epsports.begin(); ep != epsports.end(); ep++)
		{
			if(!lsi->setEndpointPortID(ep->first,ep->second))
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required gre end point port \"%s\" has been attached to the tenant-lsi",ep->first.c_str());
				delete(clo);
				throw GraphManagerException();
			}
		}

		map<string,list<string> > networkFunctionsPortsNameOnSwitch = clo->getNetworkFunctionsPortsNameOnSwitch();

		for(map<string,list<string> >::iterator nfpnos = networkFunctionsPortsNameOnSwitch.begin(); nfpnos != networkFunctionsPortsNameOnSwitch.end(); nfpnos++)
			lsi->setNetworkFunctionsPortsNameOnSwitch(nfpnos->first, nfpnos->second);

		list<pair<unsigned int, unsigned int> > vl = clo->getVirtualLinks();
		//TODO: check if the number of vlinks is the same required
		unsigned int currentTranslation = 0;
		for(list<pair<unsigned int, unsigned int> >::iterator it = vl.begin(); it != vl.end(); it++)
		{
			lsi->setVLinkIDs(currentTranslation,it->first,it->second);
			currentTranslation++;
		}
		delete(clo);

	} catch (SwitchManagerException e)
	{
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		switchManager.destroyLsi(lsi->getDpid());
		if(clo != NULL)
			delete(clo);
		delete(graph);
		delete(lsi);
		delete(computeController);
		delete(controller);
		graph = NULL;
		lsi = NULL;
		computeController = NULL;
		controller = NULL;
		throw GraphManagerException();
	}

	uint64_t dpid = lsi->getDpid();

	map<string,unsigned int> lsi_ports = lsi->getPhysicalPorts();
	set<string> nfs = lsi->getNetworkFunctionsName();
	list<highlevel::EndPointGre > eps = lsi->getEndpointsPorts();
	vector<VLink> vls = lsi->getVirtualLinks();

	//The following code just prints information on the VNFs that are part of the graph
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "LSI ID: %d",dpid);
	for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\tNF %s:",(nf->getName()).c_str());
		list<highlevel::vnf_port_t> nf_ports = nf->getPorts();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\tPorts (%d):",nf_ports.size());
		
		for(list<highlevel::vnf_port_t>::iterator n = nf_ports.begin(); n != nf_ports.end(); n++)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\t* %s - %s",(n->name).c_str(),(n->id).c_str());
			port_network_config config = n->configuration;

			if(!(config.mac_address).empty())
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\t\tMac address -> %s",(config.mac_address).c_str());
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
			if(!(config.ip_address).empty())
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\t\tIP address -> %s",(config.ip_address).c_str());
#endif
		}

#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
		list<string> nf_environment_variables = nf->getEnvironmentVariables();
		if(nf_environment_variables.size() != 0)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\tEnvironment variables (%d):",nf_environment_variables.size());
			for(list<string>::iterator ev = nf_environment_variables.begin(); ev != nf_environment_variables.end(); ev++)
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\t* %s",ev->c_str());
		}
		
		list<port_mapping_t> control_ports = nf->getControlPorts();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\tControl interfaces (%d):",control_ports.size());
		for(list<port_mapping_t >::iterator n = control_ports.begin(); n != control_ports.end(); n++)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\tHost TCP port -> %s",(n->host_port).c_str());
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\tVNF TCP port -> %s",(n->guest_port).c_str());
		}
#endif
	}

	//The following code just prints information on gre-tunnel endpoints
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Gre end points (%d):",eps.size());
	for(list<highlevel::EndPointGre>::iterator it = eps.begin(); it != eps.end(); it++){
		int id = 0;
		sscanf(it->getId().c_str(), "%d", &id);
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\tID %d:", id);
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\tKey: %s", it->getGreKey().c_str());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\tLocal ip: %s", it->getLocalIp().c_str());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t\t\tRemote_ip: %s", it->getRemoteIp().c_str());
	}

	//The following code prints information on the virtual link just created and that are necessary to interconnect the tenant LSI to the LSI-0
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual links (%u): ",vls.size());
	for(vector<VLink>::iterator v = vls.begin(); v != vls.end(); v++)
		//localID is the identifier of the virtual link on the tenant LSI
		//remoteID is the identifier of the virtual link on the LSI-0
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t(ID: %x) %x:%d -> %x:%d",v->getID(),dpid,v->getLocalID(),v->getRemoteDpid(),v->getRemoteID());

	//vlToUp is used to associate a virtual link on an action to VNF or gre-tunnels
	//vlToDown is used to associate a virtual link on an action to phisical ports or internal endpoints
	if(vls.size() != 0)
	{
		//Virtual links are required to implement the graph

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "NF port is virtual link ID (up):");
		map<string, uint64_t> nfs_vlinks;
		vector<VLink>::iterator vlToUp = vls.begin();
		//Associate a virtual link to the network functions ports that needed it
		for(set<string>::iterator nf = vlNFs.begin(); nf != vlNFs.end(); nf++, vlToUp++)
		{
			nfs_vlinks[*nf] = vlToUp->getID();
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t%s -> %x",(*nf).c_str(),vlToUp->getID());
		}
		lsi->setNFsVLinks(nfs_vlinks);
		
		//associate the vlinks to the physical ports
		//Note that the same virtual link can be used both for a VNF port and for a physical interface
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Physical port is virtual link ID (down):");
		map<string, uint64_t> ports_vlinks;
		vector<VLink>::iterator vlToDown = vls.begin();
		for(set<string>::iterator p = vlPhyPorts.begin(); p != vlPhyPorts.end(); p++, vlToDown++)
		{
			ports_vlinks[*p] = vlToDown->getID();
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t%s -> %x",(*p).c_str(),vlToDown->getID());
		}
		lsi->setPortsVLinks(ports_vlinks);

		//associate the vlinks to the gre end points
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Gre endpoint is virtual link ID (up):");
		map<string, uint64_t> endpoints_vlinks;

		for(set<string>::iterator ep = vlEndPointsGre.begin(); ep != vlEndPointsGre.end(); ep++, vlToUp++)
		{
			//TODO: rename to add the word "gre" :)
			endpoints_vlinks[*ep] = vlToUp->getID();
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t%s -> %x",(*ep).c_str(),vlToUp->getID());
		}			

		lsi->setEndPointsGreVLinks(endpoints_vlinks);

		//associate the vlinks to the internal end points
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Internal end point is virtual link ID (down):");
		map<string, uint64_t> endpoints_internal_vlinks;
	
		for(set<string>::iterator ep = vlEndPointsInternal.begin(); ep != vlEndPointsInternal.end(); ep++, vlToDown++)
		{
			endpoints_internal_vlinks[*ep] = vlToDown->getID();
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t%s -> %x",(*ep).c_str(),vlToDown->getID());
		}
		lsi->setEndPointsVLinks(endpoints_internal_vlinks);		
	}

	/**
	*	4) Start the network functions
	*/
#ifdef RUN_NFS
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "4) start the network functions");

	computeController->setLsiID(dpid);
	#ifndef STARTVNF_SINGLE_THREAD
	pthread_t some_thread[network_functions.size()];
	#endif
	to_thread_t thr[network_functions.size()];
	int i = 0;

	for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
	{
		thr[i].nf_name = nf->getName();
		thr[i].computeController = computeController;
		thr[i].namesOfPortsOnTheSwitch = lsi->getNetworkFunctionsPortsNameOnSwitchMap(nf->getName());
		thr[i].portsConfiguration = nf->getPortsID_configuration();
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
		thr[i].controlConfiguration = nf->getControlPorts();
		thr[i].environmentVariables = nf->getEnvironmentVariables();
#endif

#ifdef STARTVNF_SINGLE_THREAD
		startNF((void *) &thr[i]);
#else
		if (pthread_create(&some_thread[i], NULL, &startNF, (void *)&thr[i]) != 0)
		{
			assert(0);
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "An error occurred while creating a new thread");
			throw GraphManagerException();
		}
#endif
		i++;
	}

	bool ok = true;
#ifndef STARTVNF_SINGLE_THREAD
	for(int j = 0; j < i;j++)
	{
		void *returnValue;
		pthread_join(some_thread[j], &returnValue);
		int *c = (int*)returnValue;

		if(c == 0)
			ok = false;
	}
#endif

	if(!ok)
	{
		for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
			computeController->stopNF(nf->getName());

		switchManager.destroyLsi(lsi->getDpid());

		delete(graph);
		delete(lsi);
		delete(computeController);
		delete(controller);

		graph = NULL;;
		lsi = NULL;
		computeController = NULL;
		controller = NULL;

		throw GraphManagerException();
	}

#else
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3) Flag RUN_NFS disabled. NFs will not start");
#endif

	/**
	*	5) Create the Openflow controller for each internal LSI
	*/
	handleControllerForInternalEndpoint(graph);
	
	/**
	*	6) Handle the Internal controller graph
	*/
	try
	{
		handleGraphForInternalEndpoint(graph);
	}
	catch(GraphManagerException e)
	{
		for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
			computeController->stopNF(nf->getName());

		switchManager.destroyLsi(lsi->getDpid());

		delete(graph);
		delete(lsi);
		delete(computeController);
		delete(controller);

		graph = NULL;;
		lsi = NULL;
		computeController = NULL;
		controller = NULL;
		throw GraphManagerException();
	}

	/**
	*	7) Create the rules and download them in LSI-0, tenant-LSI
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "7) Create the rules and download them in LSI-0, tenant-LSI");
	try
	{
		//creates the rules for LSI-0 and for the tenant-LSI
		lowlevel::Graph graphLSI0 = GraphTranslator::lowerGraphToLSI0(graph, lsi, graphInfoLSI0.getLSI(), un_interface, internalLSIsConnections, orchestrator_in_band);
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "New graph for LSI-0:");
		graphLSI0.print();

		graphLSI0lowLevel.addRules(graphLSI0.getRules());
		list<lowlevel::Rule> llrules = graphLSI0.getRules();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Graph for LSI-0 (%d new rules added):",llrules.size());
		graphLSI0.print();

		lowlevel::Graph graphTenant =  GraphTranslator::lowerGraphToTenantLSI(graph,lsi,graphInfoLSI0.getLSI());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Graph for tenant LSI:");
		graphTenant.print();

		controller->installNewRules(graphTenant.getRules());

		GraphInfo graphInfoTenantLSI;
		graphInfoTenantLSI.setGraph(graph);
		graphInfoTenantLSI.setComputeController(computeController);
		graphInfoTenantLSI.setLSI(lsi);
		graphInfoTenantLSI.setController(controller);

		//Save the graph information
		tenantLSIs[graph->getID()] = graphInfoTenantLSI;

		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Tenant LSI (ID: %s) and its controller are created",graph->getID().c_str());

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the LSI-0");
		(graphInfoLSI0.getController())->installNewRules(graphLSI0.getRules());

	} catch (SwitchManagerException e)
	{
#ifdef RUN_NFS
		for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++) 
			computeController->stopNF(nf->getName());
#endif

		switchManager.destroyLsi(lsi->getDpid());

		if(tenantLSIs.count(graph->getID()) != 0)
			tenantLSIs.erase(tenantLSIs.find(graph->getID()));

		delete(graph);
		delete(lsi);
		delete(computeController);
		delete(controller);

		graph = NULL;
		lsi = NULL;
		computeController = NULL;
		controller = NULL;
		
		//TODO: also delete things related to the internal endpoint

		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		throw GraphManagerException();
	}
	return true;
}

void GraphManager::handleControllerForInternalEndpoint(highlevel::Graph *graph)
{
	list<highlevel::EndPointInternal> internalEPs = graph->getEndPointsInternal();

	for(list<highlevel::EndPointInternal>::iterator iep = internalEPs.begin(); iep != internalEPs.end(); iep++)
	{
		string internal_group(iep->getGroup());
		
		//In case the internal LSI representing the current internal endpoints has not been yet created, let's create its controller
		if(!internalLSIsCreated[internal_group])
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "4) Create the Openflow controller for the internal endpoint '%s'",internal_group.c_str());

			pthread_mutex_lock(&graph_manager_mutex);
			uint32_t controllerPort = nextControllerPort;
			nextControllerPort++;
			pthread_mutex_unlock(&graph_manager_mutex);
			
			ostringstream strControllerPort;
			strControllerPort << controllerPort;

			rofl::openflow::cofhello_elem_versionbitmap versionbitmap;
			switch(OFP_VERSION)
			{
				case OFP_10:
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.0");
					versionbitmap.add_ofp_version(rofl::openflow10::OFP_VERSION);
					break;
				case OFP_12:
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.2");
					versionbitmap.add_ofp_version(rofl::openflow12::OFP_VERSION);
					break;
				case OFP_13:
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\tUsing Openflow 1.3");
					versionbitmap.add_ofp_version(rofl::openflow13::OFP_VERSION);
					break;
			}

			//create a new OF controller associated with the internal LSI
			lowlevel::Graph graphTmp;
			Controller *controller = new Controller(versionbitmap,graphTmp,strControllerPort.str());
			controller->start();

			//store the information related to the OpenFlow controller associated with the internal LSI
			GraphInfo graphInfoInternalLSI;
			graphInfoInternalLSI.setController(controller);
			internalLSIs[internal_group] = graphInfoInternalLSI;
		}
	}
}

void GraphManager::handleGraphForInternalEndpoint(highlevel::Graph *graph)
{
	list<highlevel::EndPointInternal> internalEPs = graph->getEndPointsInternal();
	
	//Iterate on the internal endpoints of the graph
	for(list<highlevel::EndPointInternal>::iterator iep = internalEPs.begin(); iep != internalEPs.end(); iep++)
	{
		string internal_group(iep->getGroup());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "5) handling the internal LSI representing the internal-group: \"%s\"", internal_group.c_str());
		
		GraphInfo graphInfoInternalLSI = internalLSIs[internal_group];
		Controller *controller = graphInfoInternalLSI.getController();

		//create a new internal LSI if does not exists yet
		//this LSI is created with a virtual link connected to the LSI-0. The port on the LSI-0 corresponding to the virtual link will be used in the creation of the rules
		//to implement the tenant-LSI
		if(!internalLSIsCreated[internal_group])
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Create the internal LSI related to internal-group: \"%s\"", internal_group.c_str());

			string controllerPort = controller->getControllerPort();

			vector<VLink> virtual_links;
			virtual_links.push_back(VLink(dpid0));

			//The internal-LSI is neither connected to physical ports, nor to VNFs. It is in fact just 
			//connected to the LSI-0 through virtual links
			set<string> dummyPhyPorts;
			list<highlevel::VNFs> dummyNetworkFunctions;
			list<highlevel::EndPointGre> dummyEndpointsGre;
			map<string, vector<string> > endpoints;//[ivanofrancesco] unused? It's gre endpoints FIXME
			map<string, map<unsigned int, PortType> > dummyNfsPortsType;

			//Prepare the structure representing the new internal-LSI
			LSI *lsi = new LSI(string(OF_CONTROLLER_ADDRESS), controllerPort, dummyPhyPorts, dummyNetworkFunctions,
				dummyEndpointsGre,virtual_links,dummyNfsPortsType);

			CreateLsiOut *clo = NULL;
			try
			{
				map<string, nf_t> dummyNfsPortsTypeForCli;
				//Create a new internal-LSI
				CreateLsiIn cli(string(OF_CONTROLLER_ADDRESS),controllerPort, lsi->getPhysicalPortsName(), dummyNfsPortsTypeForCli, lsi->getNetworkFunctionsPortsInfo(), endpoints, lsi->getVirtualLinksRemoteLSI(), string(OF_CONTROLLER_ADDRESS), this->ipsec_certificate);
				
				clo = switchManager.createLsi(cli);

				lsi->setDpid(clo->getDpid());

				map<string,unsigned int> physicalPorts = clo->getPhysicalPorts();
				if(physicalPorts.size() > 0)
				{
					logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Non required physical ports have been attached to the internal-lsi");
					delete(lsi);
					delete(controller);
					lsi = NULL;
					controller = NULL;
					delete(clo);
					throw GraphManagerException();
				}

				map<string,map<string, unsigned int> > nfsports = clo->getNetworkFunctionsPorts();
				if(nfsports.size() > 0)
				{
					logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required network function has been attached to the internal-lsi");
					delete(lsi);
					delete(controller);
					lsi = NULL;
					controller = NULL;
					delete(clo);
					throw GraphManagerException();
				}

				map<string,unsigned int > epsports = clo->getEndpointsPorts();//FIXME this is gre endpoint
				if(epsports.size() > 0)
				{
					logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required gre-tunnel endpoint has been attached to the internal-lsi");
					delete(lsi);
					delete(controller);
					lsi = NULL;
					controller = NULL;
					delete(clo);
					throw GraphManagerException();
				}

				list<pair<unsigned int, unsigned int> > vl = clo->getVirtualLinks();
				//TODO: check if the number of vlinks is the same required
				unsigned int currentTranslation = 0;
				for(list<pair<unsigned int, unsigned int> >::iterator it = vl.begin(); it != vl.end(); it++)
				{
					lsi->setVLinkIDs(currentTranslation,it->first,it->second);
					currentTranslation++;
				}
			
				//the internal LSI now created
				internalLSIsCreated[internal_group] = true;
				timesUsedEndPointsInternal[internal_group] = 1;

				delete(clo);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				switchManager.destroyLsi(lsi->getDpid());
				if(clo != NULL)
					delete(clo);
				delete(lsi);
				delete(controller);
				lsi = NULL;
				controller = NULL;
				throw GraphManagerException();
			}
			
			/**
			*	At this point the internal LSI is created
			*/
			
			uint64_t dpid = lsi->getDpid();
			vector<VLink> vls = lsi->getVirtualLinks();

			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "LSI ID: %d",dpid);

			assert(vls.size() == 1);

			//associate the vlinks to the internal end points
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Internal end point '%s' is virtual link ID:",internal_group.c_str());
			map<string, uint64_t> endpoints_internal_vlinks;
			
			for(vector<VLink>::iterator v = vls.begin(); v != vls.end(); v++)
			{
				//localID is the identifier of the virtual link on the internal LSI
				//remoteID is the identifier of the virtual link on the LSI-0
				endpoints_internal_vlinks[internal_group] = v->getID();
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t\t(ID: %x) %x:%d -> %x:%d",v->getID(),dpid,v->getLocalID(),v->getRemoteDpid(),v->getRemoteID());
				
				//Save the port of the LSI-0 that will be used to match and send packets through the internal endpoint
				map<string, unsigned int> internalLSIsConnectionOfAGroup = internalLSIsConnections[internal_group];
				internalLSIsConnectionOfAGroup[graph->getID()] = v->getRemoteID();
				internalLSIsConnections[internal_group] = internalLSIsConnectionOfAGroup;
			}
			
			lsi->setEndPointsVLinks(endpoints_internal_vlinks);

			graphInfoInternalLSI.setLSI(lsi);
			internalLSIs[internal_group] = graphInfoInternalLSI;
			
			/**
			*	[ivanofrancesco] maybe it must be moved after the 'else'
			*/
			/**
			*	Insert a rule on the internal LSI, which matches on the virtual link and that has NORMAL as action
			*/
			lowlevel::Graph internalLSIlowLevel;
			vector<VLink>::iterator vl = vls.begin();
			
			lowlevel::Match match;
			unsigned int vlink_local_id = vl->getLocalID();
			match.setInputPort(vlink_local_id);
			lowlevel::Action action(false, true);
			
			stringstream newRuleID;
			newRuleID << "INTERNAL-GRAPH" << "-" << internal_group << "_" << graph->getID(); //This guarantees that the ID of the rule is unique
			lowlevel::Rule lsiRule(match,action,newRuleID.str(),HIGH_PRIORITY);
			internalLSIlowLevel.addRule(lsiRule);
			
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Graph for internal LSI related to internal group \"%s\":", internal_group.c_str());
			internalLSIlowLevel.print();

			//Insert new rules into the internal-LSI
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the internal-LSI");
			controller->installNewRules(internalLSIlowLevel.getRules());
	
		}
		else
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal LSI related to internal-group \"%s\" already exists and must be updated!", internal_group.c_str());
			
			LSI *lsi =  graphInfoInternalLSI.getLSI(); 
			
			VLink newLink(graphInfoLSI0.getLSI()->getDpid());
			int vlinkPosition = lsi->addVlink(newLink);
			
			AddVirtualLinkIn avli(lsi->getDpid(), graphInfoLSI0.getLSI()->getDpid());
			AddVirtualLinkOut *avlo = switchManager.addVirtualLink(avli);
			
			assert(avlo != NULL);
			lsi->setVLinkIDs(vlinkPosition,avlo->getIdA(),avlo->getIdB());
			
			uint64_t vlinkID = newLink.getID();
			VLink vlink = lsi->getVirtualLink(vlinkID); //FIXME: vlink is the same of newLink
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link: (ID: %x) %x:%d -> %x:%d",vlink.getID(),lsi->getDpid(),vlink.getLocalID(),vlink.getRemoteDpid(),vlink.getRemoteID());
			
			//Save the port of the LSI-0 that will be used to match and send packets through the internal endpoint
			map<string, unsigned int> internalLSIsConnectionOfAGroup = internalLSIsConnections[internal_group];
			internalLSIsConnectionOfAGroup[graph->getID()] = vlink.getRemoteID();
			internalLSIsConnections[internal_group] = internalLSIsConnectionOfAGroup;
			
			graphInfoInternalLSI.setLSI(lsi);//FIXME: is it needed to store again the LSI, since it is a pointer?
			internalLSIs[internal_group] = graphInfoInternalLSI;
			
			timesUsedEndPointsInternal[internal_group]++;

			delete avlo;
			
			/**
			*	Insert a rule on the internal LSI, which matches on the virtual link and that has NORMAL as action
			*/
			lowlevel::Graph internalLSIlowLevel;
			
			lowlevel::Match match;
			unsigned int vlink_local_id = vlink.getLocalID();
			match.setInputPort(vlink_local_id);
			lowlevel::Action action(false, true);
			
			stringstream newRuleID;
			newRuleID << "INTERNAL-GRAPH" << "-" << internal_group << "_" << graph->getID(); //This guarantees that the ID of the rule is unique
			lowlevel::Rule lsiRule(match,action,newRuleID.str(),HIGH_PRIORITY);
			internalLSIlowLevel.addRule(lsiRule);
			
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Graph for internal LSI related to internal group \"%s\":", internal_group.c_str());
			internalLSIlowLevel.print();

			//Insert new rules into the internal-LSI
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the internal-LSI");
			controller->installNewRules(internalLSIlowLevel.getRules());

		}
	}//end iteration on the internal endpoints of the graph
}

bool GraphManager::updateGraph(string graphID, highlevel::Graph *newGraph)
{
	/**
	*	The update of the graph is split in three parts:
	*	-	create the new VNFs, VNF ports, gre tunnels and virtual links
	*	-	deleted the proper VNFs, VNF ports, gre tunnels, rules from the LSI-0 and tenant LSI, as required by the update
	*	-	insert in the LSI-0 and tenant LSI the proper rules, as required by the update
	*/

	highlevel::Graph *diff_to_add = NULL;
	try
	{
		diff_to_add = updateGraph_add(graphID,newGraph);
	}
	catch(GraphManagerException e)
	{
		delete(diff_to_add);
		diff_to_add = NULL;
		return false;
	}

	if(!updateGraph_remove(graphID,newGraph))
	{
		delete(diff_to_add);
		diff_to_add = NULL;
		return false;
	}

	return updateGraph_add_rules(graphID,diff_to_add);
}

bool GraphManager::updateGraph_add_rules(string graphID, highlevel::Graph *diff)
{
	//Retrieve the information already stored for the graph
	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	LSI *lsi = graphInfo.getLSI();
	Controller *tenantController = graphInfo.getController();

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Create the new rules and download them in LSI-0 and tenant-LSI");

	try
	{
		//creates the new rules for LSI-0 and for the tenant-LSI

		lowlevel::Graph graphLSI0 = GraphTranslator::lowerGraphToLSI0(diff,lsi,graphInfoLSI0.getLSI(),un_interface,internalLSIsConnections,orchestrator_in_band);

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "New piece of graph for LSI-0:");
		graphLSI0.print();
		graphLSI0lowLevel.addRules(graphLSI0.getRules());

		lowlevel::Graph graphTenant =  GraphTranslator::lowerGraphToTenantLSI(diff,lsi,graphInfoLSI0.getLSI());
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "New piece of graph for tenant LSI:");
		graphTenant.print();

		//Insert new rules into the LSI-0
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the LSI-0");
		(graphInfoLSI0.getController())->installNewRules(graphLSI0.getRules());

		//Insert new rules into the tenant-LSI
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Adding the new rules to the tenant-LSI");
		tenantController->installNewRules(graphTenant.getRules());

		printInfo(graphLSI0lowLevel,graphInfoLSI0.getLSI());

	} catch (SwitchManagerException e)
	{
		//TODO: no idea on what I have to do at this point
		assert(0);
		delete(diff);
		diff = NULL;
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		return false;
	}

	delete(diff);
	diff = NULL;

	return true;
}

highlevel::Graph *GraphManager::updateGraph_add(string graphID, highlevel::Graph *newGraph)
{
	/**
	*	Limitations:
	*	- only new ports (and the related configuration) can be added to VNFs
	*		It is instead not possible to add new:
	*		- environment variables
	*		- control connections
	*	- internal endpoints not supported
	**/

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Updating the graph '%s' with new 'pieces'...",graphID.c_str());

	assert(tenantLSIs.count(graphID) != 0);

	//Retrieve the information already stored for the graph (i.e., retrieve the as it is
	//currently implemented, without the update)
	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	ComputeController *computeController = graphInfo.getComputeController();
	highlevel::Graph *graph = graphInfo.getGraph();
	LSI *lsi = graphInfo.getLSI();

	uint64_t dpid = lsi->getDpid();

	/**
	*	Outline:
	*
	*	0) calculate the diff with respect to the graph already deployed (and check its validity)
	*	1) update the high level graph
	*	2) select an implementation for the new NFs
	*	3) update the lsi (in case of new interface/NFs/gre/vlan endpoints are required)
	*	4) start the new NFs
	*/

	/**
	*	The three following variables will be used in the following and that contain
	*	an high level graph:
	*		* graph -> the original graph to be updated
	*		* newGraph -> graph containing the update
	*		* diff -> graph that will contain the parts in newGraph that are not part of graph
	*/

	/**
	*	0) Calculate the diff ad check the validity of the update
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "0) Calculate the new pieces of the graph");
	highlevel::Graph *diff = graph->calculateDiff(newGraph, graphID);

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The diff graph is:");
	diff->print();

	if(!checkGraphValidity(diff,computeController))
	{
		//This is an error in the request
		delete(diff);
		diff = NULL;
		throw GraphManagerException();
	}
	//The required graph update is valid

	/**
	*	1) Update the high level graph
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "1) Update the high level graph");

	graph->addGraphToGraph(diff);
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The final graph is:");
	graph->print();

	/**
	*	2) Select an implementation for the new NFs
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "2) Select an implementation for the new NFs");
	if(!computeController->selectImplementation())
	{
		//This is an internal error
		delete(computeController);
		computeController = NULL;
		throw GraphManagerException();
	}

	/**
	*	3) Update the lsi (in case of new interface/VNFs/gre/vlan endpoints are required)
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3) update the lsi (in case of new ports/VNFs/gre/vlan endpoints are required)");

	list<highlevel::VNFs> network_functions = diff->getVNFs();

	/**
	*	3-a) condider the virtual links
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3-a) considering the new virtual links");

	//Since the NFs cannot specify new ports, new virtual links can be required only by the new NFs and the physical ports

	vector<set<string> > vlVector = identifyVirtualLinksRequired(diff,lsi);
	set<string> vlNFs = vlVector[0];
	set<string> vlPhyPorts = vlVector[1];
	set<string> vlEndPointsGre = vlVector[2];
	set<string> vlEndPointsInternal = vlVector[3];

	//TODO: check if a virtual link is already available and can be used (because it is currently used only in one direction)
	//FIXME: I think the part that calculates the number of vlink required should be rewritten. It should be based on the code of new graph, which is very optimized
	unsigned int numberOfVLrequiredBeforeEndPoints = (vlNFs.size() > vlPhyPorts.size())? vlNFs.size() : vlPhyPorts.size(); //The same virtual link can be exploited both towards VNFs and towards physical ports
	unsigned int numberOfVLrequired = numberOfVLrequiredBeforeEndPoints + vlEndPointsInternal.size()  + vlEndPointsGre.size(); //TODO: optimize this; in fact, the same virtual link could be exploited both towards GRE endpoints and towards internal endpoints

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "%d virtual links are required to connect the new part of the LSI with LSI-0",numberOfVLrequired);

	set<string>::iterator nf = vlNFs.begin();
	set<string>::iterator p = vlPhyPorts.begin();
	for(; nf != vlNFs.end() || p != vlPhyPorts.end() ;)
	{
		//FIXME: here I am referring to a vlink through its position. It would be really better to use its ID
		AddVirtualLinkOut *avlo = NULL;
		try
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, " Adding vlink required to a interface endpoint (i.e., physical port) and/or to a VNF port");
			VLink newLink(dpid0);
			int vlinkPosition = lsi->addVlink(newLink);

			AddVirtualLinkIn avli(dpid,dpid0);
			avlo = switchManager.addVirtualLink(avli);

			assert(avlo != NULL);
			lsi->setVLinkIDs(vlinkPosition,avlo->getIdA(),avlo->getIdB());

			delete(avlo);

			uint64_t vlinkID = newLink.getID();

			VLink vlink = lsi->getVirtualLink(vlinkID); //FIXME: vlink is the same of newLink
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link: (ID: %x) %x:%d -> %x:%d",vlink.getID(),dpid,vlink.getLocalID(),vlink.getRemoteDpid(),vlink.getRemoteID());
			assert(vlinkID == vlink.getID());

			if(nf != vlNFs.end())
			{
				lsi->addNFvlink(*nf,vlinkID);
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "NF '%s' uses the vlink '%x'",(*nf).c_str(),vlink.getID());
				nf++;
			}
			if(p != vlPhyPorts.end())
			{
				lsi->addPortvlink(*p,vlinkID);
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Physical port '%s' uses the vlink '%x'",(*p).c_str(),vlink.getID());
				p++;
			}
		}catch(SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			if(avlo != NULL)
				delete(avlo);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
	}

	for(set<string>::iterator ep = vlEndPointsGre.begin(); ep != vlEndPointsGre.end(); ep++)
	{
		//FIXME: here I am referring to a vlink through its position. It would be really better to use its ID
		AddVirtualLinkOut *avlo = NULL;
		try
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, " Adding vlink required to a gre-tunnel endpoint");
		
			VLink newLink(dpid0);
			int vlinkPosition = lsi->addVlink(newLink);

			AddVirtualLinkIn avli(dpid,dpid0);
			avlo = switchManager.addVirtualLink(avli);

			lsi->setVLinkIDs(vlinkPosition,avlo->getIdA(),avlo->getIdB());

			delete(avlo);

			uint64_t vlinkID = newLink.getID();

			VLink vlink = lsi->getVirtualLink(vlinkID);
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link: (ID: %x) %x:%d -> %x:%d",vlink.getID(),dpid,vlink.getLocalID(),vlink.getRemoteDpid(),vlink.getRemoteID());
			assert(vlinkID == vlink.getID());

			lsi->addEndpointGrevlink(*ep,vlinkID);
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Gre endpoint '%s' uses the vlink '%x'",(*ep).c_str(),vlink.getID());
		}catch(SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			if(avlo != NULL)
				delete(avlo);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
	}

	for(set<string>::iterator ep = vlEndPointsInternal.begin(); ep != vlEndPointsInternal.end(); ep++)
	{
		//FIXME: here I am referring to a vlink through its position. It would be really better to use its ID
		AddVirtualLinkOut *avlo = NULL;
		try
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, " Adding vlink required to an internal endpoint");

			VLink newLink(dpid0);
			int vlinkPosition = lsi->addVlink(newLink);

			AddVirtualLinkIn avli(dpid,dpid0);
			avlo = switchManager.addVirtualLink(avli);

			lsi->setVLinkIDs(vlinkPosition,avlo->getIdA(),avlo->getIdB());

			delete(avlo);

			uint64_t vlinkID = newLink.getID();

			VLink vlink = lsi->getVirtualLink(vlinkID);
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link: (ID: %x) %x:%d -> %x:%d",vlink.getID(),dpid,vlink.getLocalID(),vlink.getRemoteDpid(),vlink.getRemoteID());

			lsi->addEndpointInternalvlink(*ep,vlinkID);
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Internal endpoint '%s' uses the vlink '%x'",(*ep).c_str(),vlink.getID());

		}catch(SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			if(avlo != NULL)
				delete(avlo);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
	}

	/**
	*	3-b) condider the virtual network functions
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3-b) considering the new virtual network functions (%d)",network_functions.size());

	//Itarate on all the new network functions
	//TODO: when the hotplug will be introduced, here we will also iterate on the network functions to be updated
	for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
	{
		AddNFportsOut *anpo = NULL;
		try
		{
			//TODO: for the hotplug, modify lsi->addNF as suggested into the function itself.
			list<unsigned int> nf_ports = nf->getPortsId(); // nf_it->second;
			lsi->addNF(nf->getName()/*first*/, /*nf->second*/ nf_ports, computeController->getNFSelectedImplementation(nf->getName()/*first*/)->getPortTypes());

			map<string, list<struct nf_port_info> >pi_map = lsi->getNetworkFunctionsPortsInfo();//for each network function, retrieve a list of "port name, port type"
			map<string, list<struct nf_port_info> >::iterator pi_it = pi_map.find(nf->getName()/*first*/); //select the info related to the network function currently considered
			//TODO: when the hotplug will be introduced, pi_it->second will also contain the old ports of the VNF. Then a further skimming will be required
			assert(pi_it != pi_map.end());
			AddNFportsIn anpi(dpid, nf->getName()/*first*/, computeController->getNFType(nf->getName()/*first*/), pi_it->second); //prepare the input for the switch manager

			//We add, with a single call, all the ports of a single network function
			anpo = switchManager.addNFPorts(anpi);

			//anpo->getPorts() returns the map "ports name, identifier within the lsi"
			if(!lsi->setNfSwitchPortsID(anpo->getNFname(), anpo->getPorts()))
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required network function port related to the network function \"%s\" has been attached to the tenant-lsi",nf->getName().c_str()/*first.c_str()*/);
				lsi->removeNF(nf->getName()/*first*/);
				delete(anpo);
				throw GraphManagerException();
			}

			//FIXME: useful? Probably no!
			//TODO: not sure that this works in case of hotplug. In fact anpo->getPortsNameOnSwitch() returns a list of ports name on the switch, but it does not 
			//map such names with ports names calculated before (or with the port id). 
			//I think that it should be done something similar to anpo->getPorts(), which maps the identifier on the switch to the port name.
			lsi->setNetworkFunctionsPortsNameOnSwitch(anpo->getNFname(),anpo->getPortsNameOnSwitch());

			delete(anpo);
		}catch(SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			lsi->removeNF(nf->getName()/*first*/);
			if(anpo != NULL)
				delete(anpo);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
	}

	/**
	*	3-c) condider the gre-tunnel endpoints
	*/
	list<highlevel::EndPointGre> tmp_gre_endpoints = diff->getEndPointsGre();
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3-c) considering the new gre-tunnel endpoints (%d)",tmp_gre_endpoints.size());

	for(list<highlevel::EndPointGre>::iterator ep = tmp_gre_endpoints.begin(); ep != tmp_gre_endpoints.end(); ep++)
	{
#ifdef VSWITCH_IMPLEMENTATION_OVSDB
		//fill the vector related to the endpoint params [gre key, local-ip, remote-ip, interface, isSafe]
		vector<string> ep_param(5);
		ep_param[0] = ep->getLocalIp();
		ep_param[1] = ep->getGreKey();
		ep_param[2] = ep->getRemoteIp();
		ep_param[3] = un_interface;
		if(ep->isSafe())
			ep_param[4] = "true";
		else
			ep_param[4] = "false";

		AddEndpointOut *aepo = NULL;
		try
		{
			lsi->addEndpoint(*ep);
			AddEndpointIn aepi(dpid,ep->getId(),ep_param);

			aepo = switchManager.addEndpoint(aepi);

			if(!lsi->setEndpointPortID(aepo->getEPname(), aepo->getEPid()))
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "A non-required gre endpoint \"%s\" has been attached to the tenant-lsi",ep->getId().c_str());
				lsi->removeEndpoint(ep->getId());
				delete(aepo);
				throw GraphManagerException();
			}

			delete(aepo);
		}catch(SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			lsi->removeEndpoint(ep->getId());
			if(aepo != NULL)
				delete(aepo);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
#else
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "GRE tunnel unavailable with the selected virtual switch");
		throw GraphManagerException();
#endif
	}

	/**
	*	3-d) condider the internal endpoints
	*/
	list<highlevel::EndPointInternal> tmp_internal_endpoints = diff->getEndPointsInternal();
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3-c) considering the new internal endpoints (%d)",tmp_internal_endpoints.size());

	for(list<highlevel::EndPointInternal>::iterator ep = tmp_internal_endpoints.begin(); ep != tmp_internal_endpoints.end(); ep++)
	{
		//TODO: implement the creation of new internal endpoints
		assert(0 && "The creation of new internal endpoint in the update is not supported yet!");
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The creation of new internal endpoint in the update is not supported yet!");
	}

	/**
	*	4) Start the new NFs
	*/
	//TODO: in case of hotplug, the function is already running then a different function on the compute controller should be called.
#ifdef RUN_NFS
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "4) start the new NFs");

	computeController->setLsiID(dpid);

	for(list<highlevel::VNFs>::iterator nf = network_functions.begin(); nf != network_functions.end(); nf++)
	{
		map<unsigned int, string> nfPortIdToNameOnSwitch = lsi->getNetworkFunctionsPortsNameOnSwitchMap(nf->getName()/*first*/); //Returns the map <port ID, port name on switch>
		//TODO: the following information should be retrieved through the highlevel graph
		map<unsigned int, port_network_config_t > nfs_ports_configuration = nf->getPortsID_configuration();
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
		list<port_mapping_t > nfs_control_configuration = nf->getControlPorts();
		list<string> environment_variables_tmp = nf->getEnvironmentVariables();
#endif
		//TODO: for the hotplug, we may extend the computeController with a call that says if a VNF is already running or not.
		//If not, startNF should then be called; if yes, o new function must be called
		if(!computeController->startNF(nf->getName()/*first*/, nfPortIdToNameOnSwitch, nfs_ports_configuration
#ifdef ENABLE_UNIFY_PORTS_CONFIGURATION
			, nfs_control_configuration, environment_variables_tmp
#endif
		))
		{
			//TODO: no idea on what I have to do at this point
			assert(0);
			delete(diff);
			diff = NULL;
			throw GraphManagerException();
		}
	}
#else
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "4) Flag RUN_NFS disabled. New NFs will not start");
#endif

	//The new endpoints, VNFs, VNF ports and virtual links have been added to the graph!

	return diff;
}

bool GraphManager::updateGraph_remove(string graphID, highlevel::Graph *newGraph)
{
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Updating the graph '%s' by removing 'pieces'...",graphID.c_str());

	assert(tenantLSIs.count(graphID) != 0);

	//Retrieve the information already stored for the graph (i.e., retrieve the as it is
	//currently implemented, without the update)
	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	highlevel::Graph *graph = graphInfo.getGraph();
	LSI *lsi = graphInfo.getLSI();

	/**
	*	Outline:
	*
	*	0) calculate the diff with respect to the graph already deployed (and check its validity)
	*	1) update the high level graph
	*	2) remove the rules from LSI-0 and tenant-LSI
	*	3) remove the virtual links that are no longer used
	*	4) delete the gre-tunnel endpoints from the LSI, if required by the update
	*/
	
	/**
	*	Limitations:
	*	- only ports (and the related configuration) can be removed from VNFs
	*		It is instead not possible to remove:
	*		- environment variables
	*		- control connections
	*	- internal endpoints not supported
	**/


	/**
	*	The three following variables will be used in the following and that contain
	*	an high level graph:
	*		* graph -> the original graph to be updated
	*		* newGraph -> graph containing the update
	*		* diff -> graph that will contain the parts in "graph" and that are not in "newGraph",
	*				and then that must be removed from "newGraph"
	*/

	/**
	*	0) Calculate the diff
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "0) Calculate the pieces to be removed from the graph");
	highlevel::Graph *diff = newGraph->calculateDiff(graph, graphID);

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The diff graph is:");
	diff->print();

	/**
	*	1) Update the high level graph
	*/
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "1) Update the high level graph");

	list<RuleRemovedInfo> removeRuleInfo = graph->removeGraphFromGraph(diff);
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The final graph is:");
	graph->print();
	
	/**
	*	The following things must be done
	*	- 2) delete the flowrules
	*	- 3) delete the virtual links no longer used. We support virtual link related to each type of endpoint
	*	- 4) delete the gre-tunnel ports
	*	- 5) delete the interface endpoints (i.e., physical ports) 
	*	- 6) delete the VNFs
	*/

	/**
	*	2) Remove the rules from LSI-0 and tenant-LSI
	*/

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "2) Removing the flow rules");
	Controller *lsi0Controller = graphInfoLSI0.getController();

	list<highlevel::Rule> rulesToBeRemoved = diff->getRules();
	for(list<highlevel::Rule>::iterator rule = rulesToBeRemoved.begin(); rule != rulesToBeRemoved.end(); rule++)
	{
		string ruleID = rule->getRuleID();
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Removing the flow rule with id '%s'",ruleID.c_str());

		// Considering the LSI-0
		stringstream lsi0RuleID;
		lsi0RuleID << graph->getID() << "_" << ruleID;
		lsi0Controller->removeRuleFromID(lsi0RuleID.str());
		graphLSI0lowLevel.removeRuleFromID(lsi0RuleID.str());

		// Considering the tenant-LSI
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Removing the flow from the tenant-LSI graph");
		Controller *tenantController = graphInfo.getController();
		tenantController->removeRuleFromID(ruleID);
	}

	printInfo(graphLSI0lowLevel,graphInfoLSI0.getLSI());

	/**
	*	3)	Remove the virtual links that are no longer used.
	*		The fact that such endpoints (of whatever type) are removed or not from the graph is not important; in fact, the vlink are created/destroyed depending on the rules.
	*/
	
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "3) Removing the virtual links");

	assert(removeRuleInfo.size() == rulesToBeRemoved.size());
	for(list<RuleRemovedInfo>::iterator tbr = removeRuleInfo.begin(); tbr != removeRuleInfo.end(); tbr++)
		removeUselessVlinks(*tbr,graph,lsi);

	/**
	*	4) Delete the gre-tunnel endpoints, as required by the diff
	*/
	
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "4) Removing the gre-tunnel endpoints");

	list<highlevel::EndPointGre> greEndpoints = diff->getEndPointsGre();
	for(list<highlevel::EndPointGre>::iterator gep = greEndpoints.begin(); gep != greEndpoints.end(); gep++)
	{
#ifdef VSWITCH_IMPLEMENTATION_OVSDB
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The gre endpoint '%s' is no longer part of the graph",(gep->getId()).c_str());

		try
		{
			DestroyEndpointIn depi(lsi->getDpid(),gep->getId());
			switchManager.destroyEndpoint(depi);
			lsi->removeEndpoint((gep->getId()));
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}
#else
		logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "GRE tunnel not supported with the selected virtual switch");
#endif
	}

	//TODO: remove VNFs, phy ports- add an error in case internal endpoints have to be removed

	return true;
}

void GraphManager::removeUselessVlinks(RuleRemovedInfo rri, highlevel::Graph *graph, LSI *lsi)
{

	map<string, uint64_t> nfs_vlinks = lsi->getNFsVlinks();
	map<string, uint64_t> ports_vlinks = lsi->getPortsVlinks();
	map<string, uint64_t> endpoints_internal_vlinks = lsi->getEndPointsVlinks();
	map<string, uint64_t> endpoints_gre_vlinks = lsi->getEndPointsGreVlinks();

	list<highlevel::Rule> rules = graph->getRules();

	/**
	*	Consider the network function ports
	*/

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Checking if some virtual links can be removed...");
	if(rri.isNFport)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the NF port '%s' must be removed (if this vlink exists)",rri.nf_port.c_str());

	if(rri.isNFport && nfs_vlinks.count(rri.nf_port) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The NF port '%s' is associated with a vlink",rri.nf_port.c_str());

		/**
		*	In case NF:port does not appear in other actions, and it is not use for any physical port or internal endpoint, then the vlink must be removed
		*/
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			highlevel::Match m = again->getMatch();
			
			if(a->getType() == highlevel::ACTION_ON_NETWORK_FUNCTION && (m.matchOnEndPointInternal() || m.matchOnPort() ))
			{
				//The network function port can still be used in an action, but the match should be on something not requiring the vlink: another VNF port or a gre tunnel

				stringstream nf_port;
				nf_port << ((highlevel::ActionNetworkFunction*)a)->getInfo() << "_" << ((highlevel::ActionNetworkFunction*)a)->getPort();
				string nf_port_string = nf_port.str();

				if(nf_port_string == rri.nf_port)
				{
					//The action is on the same VNF port of the removed one, hence the vlink must not be removed
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The vlink cannot be removed, since there are other actions expressed on the NF port '%s'",rri.nf_port.c_str());
					return;
				}
			}
		}//end of again iterator on the rules of the graph

		//We just know that the vlink is no longer used for a NF port. However, it might be used in the opposite
		//direction: for a physical port or for an internal endpoint

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for NF port: %s",rri.nf_port.c_str());
		uint64_t tobeRemovedID = nfs_vlinks.find(rri.nf_port)->second;

		//The virtual link is no longer associated with the network function port
		lsi->removeNFvlink(rri.nf_port);

		for(map<string, uint64_t>::iterator pvl = ports_vlinks.begin(); pvl != ports_vlinks.end(); pvl++)
		{
			if(pvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the interface: %s",pvl->first.c_str());
				return;
			}
		}

		for(map<string, uint64_t>::iterator ivl = endpoints_internal_vlinks.begin(); ivl != endpoints_internal_vlinks.end(); ivl++)
		{
			if(ivl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the internal endpoint: %s",ivl->first.c_str());
				return;
			}
		}

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

		try
		{
			VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
			DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
			switchManager.destroyVirtualLink(dvli);
			lsi->removeVlink(tobeRemovedID);
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}
		return;
	}//end if(rri.isNFport && nfs_vlinks.count(rri.nf_port) != 0)

	/**
	*	Consider the gre-tunnel endpoints
	*/

	if(rri.isEndpointGre)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the gre endpoint '%s' must be removed (if this vlink exists)",rri.endpointGre.c_str());

	if(rri.isEndpointGre && endpoints_gre_vlinks.count(rri.endpointGre) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The gre endpoint '%s' is associated with a vlink",rri.endpointGre.c_str());

		/**
		*	In case the gre endpoint does not appear in other actions, and it is not use for any physical port or internal endpoint, then the vlink must be removed
		*/
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			highlevel::Match m = again->getMatch();

			if(a->getType() == highlevel::ACTION_ON_ENDPOINT_GRE && (m.matchOnEndPointInternal() || m.matchOnPort()))
			{
				if(((highlevel::ActionEndPointGre*)a)->toString() == rri.endpointGre)
				{
					//The action is on the same gre endpoint of the removed one, hence the vlink must not be removed
					return;
				}
			}

		}//end of again iterator on the rules of the graph

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for the gre endpoint: %s",rri.endpointGre.c_str());

		uint64_t tobeRemovedID = endpoints_gre_vlinks.find(rri.endpointGre)->second;
		lsi->removeEndPointGrevlink(rri.endpointGre);

		for(map<string, uint64_t>::iterator pvl = ports_vlinks.begin(); pvl != ports_vlinks.end(); pvl++)
		{
			if(pvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the port: %s",pvl->first.c_str());
				return;
			}
		}

		for(map<string, uint64_t>::iterator ivl = endpoints_internal_vlinks.begin(); ivl != endpoints_internal_vlinks.end(); ivl++)
		{
			if(ivl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the internal endpoint: %s",ivl->first.c_str());
				return;
			}
		}

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

		try
		{
			VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
			DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
			switchManager.destroyVirtualLink(dvli);
			lsi->removeVlink(tobeRemovedID);
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}
		return;
	}//end if(rri.isEndpointGre && endpoints_gre_vlinks.count(rri.endpointGre) != 0)

	/**
	*	Consider the interface endpoints (i.e., physical ports)
	*/

	if(rri.isPort)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check of the vlink associated with the port '%s' must be removed (if this vlink exists)",rri.port.c_str());

	if(rri.isPort && ports_vlinks.count(rri.port) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The port '%s' is associated with a vlink",rri.port.c_str());
		/**
		*	In case port does not appear in other actions, and it is not use for any gre-tunnel or VNF port, then the vlink must be removed
		*/
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			highlevel::Match m = again->getMatch();

			if(a->getType() == highlevel::ACTION_ON_PORT && (m.matchOnEndPointGre() || m.matchOnNF()))
			{
				if(((highlevel::ActionPort*)a)->getInfo() == rri.port)
				{
					//The action are the same, hence no vlink must be removed
					return;
				}
			}

		}//end of again iterator on the rules of the graph

		//We just know that the vlink is no longer used for a port. However, it might used in the opposite
		//direction, for a NF port or for a gre-tunnel endpoint
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for port: %s",rri.port.c_str());
		uint64_t tobeRemovedID = ports_vlinks.find(rri.port)->second;

		lsi->removePortvlink(rri.port);

		for(map<string, uint64_t>::iterator nfvl = nfs_vlinks.begin(); nfvl != nfs_vlinks.end(); nfvl++)
		{
			if(nfvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the NF port: %s",nfvl->first.c_str());
				return;
			}
		}

		for(map<string, uint64_t>::iterator gvl = endpoints_gre_vlinks.begin(); gvl != endpoints_gre_vlinks.end(); gvl++)
		{
			if(gvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the gre-tunnel endpoint: %s",gvl->first.c_str());
				return;
			}
		}

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");
		try
		{
			VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
			DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
			switchManager.destroyVirtualLink(dvli);
			lsi->removeVlink(tobeRemovedID);
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}
	}

	/**
	*	Consider the internal endpoints
	*/

	if(rri.isEndpointInternal)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the internal endpoint '%s' must be removed (if this vlink exists)",rri.endpointInternal.c_str());

	if(rri.isEndpointInternal && endpoints_internal_vlinks.count(rri.endpointInternal) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal endpoint '%s' is associated with a vlink",rri.endpointInternal.c_str());

		/**
		*	In case the internal endpoint does not appear in other actions, and it is not use for any gre-tunnel or VNF port, then the vlink must be removed
		*/
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			highlevel::Match m = again->getMatch();

			if(a->getType() == highlevel::ACTION_ON_ENDPOINT_INTERNAL  && (m.matchOnEndPointGre() || m.matchOnNF()))
			{
				if(((highlevel::ActionEndPointInternal*)a)->toString() == rri.endpointInternal)
				{
					//The action is on the same endpoint of the removed one, hence the vlink must not be removed
					return;
				}
			}

		}//end of again iterator on the rules of the graph

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for the internal endpoint: %s",rri.endpointInternal.c_str());

		uint64_t tobeRemovedID = endpoints_internal_vlinks.find(rri.endpointInternal)->second;
		lsi->removeEndPointvlink(rri.endpointInternal);

		for(map<string, uint64_t>::iterator nfvl = nfs_vlinks.begin(); nfvl != nfs_vlinks.end(); nfvl++)
		{
			if(nfvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the NF port: %s",nfvl->first.c_str());
				return;
			}
		}

		for(map<string, uint64_t>::iterator gvl = endpoints_gre_vlinks.begin(); gvl != endpoints_gre_vlinks.end(); gvl++)
		{
			if(gvl->second == tobeRemovedID)
			{
				logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the gre-tunnel endpoint: %s",gvl->first.c_str());
				return;
			}
		}

		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

		try
		{
			VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
			DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
			switchManager.destroyVirtualLink(dvli);
			lsi->removeVlink(tobeRemovedID);
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}

	}


#if 0
	//Remove NFs, if they no longer appear in the graph
	for(list<string>::iterator nf = rri.nfs.begin(); nf != rri.nfs.end(); nf++)
	{
#if 0 
		//IVANO: this check has been moved in highlevel graph, in the function that removes pieces from the graph
		if(!graph->stillExistNF(*nf))
#endif
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The NF '%s' is no longer part of the graph",(*nf).c_str());

			//Stop the NF
#ifdef RUN_NFS
			computeController->stopNF(*nf);
#else
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Flag RUN_NFS disabled. No NF to be stopped");
#endif

			set<string> nf_ports;
			map<unsigned int, string>lsi_nf_ports = lsi->getNetworkFunctionsPortsNameOnSwitchMap(*nf);
			for (map<unsigned int,string>::iterator lsi_nfp_it = lsi_nf_ports.begin(); lsi_nfp_it != lsi_nf_ports.end(); ++lsi_nfp_it) {
				nf_ports.insert(lsi_nfp_it->second);
			}

			try
			{
				DestroyNFportsIn dnpi(lsi->getDpid(), *nf, nf_ports);
				switchManager.destroyNFPorts(dnpi);
				lsi->removeNF(*nf);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

#if 0 
	//IVANO: moved in highlevelgraph, in the function that removes pieces from the graph
	//Remove physical ports, if they no longer appear in the graph
	for(list<string>::iterator p = rri.ports.begin(); p != rri.ports.end(); p++)
	{
		if(!graph->stillExistPort(*p))
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The port '%s' is no longer part of the graph",(*p).c_str());
	}
#endif

	//Remove the internal endpoint, if it no longer appear in the graph
	if((rri.endpointInternal != "") && (!graph->stillExistEndpoint(rri.endpointInternal)))
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal endpoint '%s' is no longer part of the graph",rri.endpointInternal.c_str());

#endif
}

vector<set<string> > GraphManager::identifyVirtualLinksRequired(highlevel::Graph *graph)
{
	set<string> NFs;
	set<string> endPointsGre;
	set<string> phyPorts;
	set<string> endPointsInternal;

	//The number of virtual link depends on the rules, and not on the VNF ports / endpoints of the graph
	list<highlevel::Rule> rules = graph->getRules();

	for(list<highlevel::Rule>::iterator rule = rules.begin(); rule != rules.end(); rule++)
	{
		highlevel::Action *action = rule->getAction();
		highlevel::Match match = rule->getMatch();
		if(action->getType() == highlevel::ACTION_ON_NETWORK_FUNCTION)
		{
			//we are considering rules as
			//	- match: internal-25 - action: output to VNF firewall port 1
			//	- match: interface eth0 - action: output to VNF firewall port 1
			//Each different VNF port that is in an action matching an internal endpoint or an interface endpoint requires a different virtual link
			if(match.matchOnPort() || match.matchOnEndPointInternal())
			{
				highlevel::ActionNetworkFunction *action_nf = (highlevel::ActionNetworkFunction*)action;
				stringstream ss;
				ss << action->getInfo() << "_" << action_nf->getPort();
				NFs.insert(ss.str()); //the set avoids duplications
			}
			
			// gre -> VNF does not require any virtual link
			// VNF -> VNF does not require any virtual link
		}
		else if(action->getType() == highlevel::ACTION_ON_ENDPOINT_GRE)
		{
			//we are considering rules as
			//	- match: internal-25 - action: output to gre tunnel with key 1
			//	- match: interface eth0 - action: output to gre tunnel with key 1
			//Each different gre tunnel (i.e., tunnel with a different key) that is in an action matching 
			//an internal endpoint or an interface endpoint requires a different virtual link

			if(match.matchOnPort() || match.matchOnEndPointInternal())
			{
				highlevel::ActionEndPointGre *action_ep = (highlevel::ActionEndPointGre*)action;
				endPointsGre.insert(action_ep->toString());
			}
			
			// gre -> gre does not require any virtual link
			// VNF -> gre does not require any virtual link
			// VNF -> VNF does not require any virtual link
		}
		else if(action->getType() == highlevel::ACTION_ON_PORT)
		{
			//we are considering rules as
			//	- match: gre tunnel with key 1 - action: output to interface eth0
			//	- match: VNF firewall port 1 - action: output to interface eth0
			//Each different interface requires a different virtual link
			if(match.matchOnNF() || match.matchOnEndPointGre())
				phyPorts.insert(action->getInfo());
				
			// interface -> interface does not require any virtual link
			// internal endpoint -> interface does not require any virtual link
		}
		else if(action->getType() == highlevel::ACTION_ON_ENDPOINT_INTERNAL)
		{
			if(!match.matchOnPort() && !match.matchOnEndPointInternal())
			{
				highlevel::ActionEndPointInternal *action_ep = (highlevel::ActionEndPointInternal*)action;
				endPointsInternal.insert(action_ep->toString());
			}
			
			// internal endpoint -> internal endpoint does not require any virtual link
			// interface -> internal endpoint does not require any virtual link
		}
	}

	if(NFs.size() != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Network functions input ports requiring a virtual link:");
		for(set<string>::iterator nf = NFs.begin(); nf != NFs.end(); nf++)
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*nf).c_str());
	}
	if(phyPorts.size() != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Physical ports requiring a virtual link:");
		for(set<string>::iterator p = phyPorts.begin(); p != phyPorts.end(); p++)
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*p).c_str());
	}
	if(endPointsGre.size() != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Gre endpoints requiring a virtual link:");
		for(set<string>::iterator e = endPointsGre.begin(); e != endPointsGre.end(); e++)
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*e).c_str());
	}
	if(endPointsInternal.size() != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Internal endpoints requiring a virtual link:");
		for(set<string>::iterator e = endPointsInternal.begin(); e != endPointsInternal.end(); e++)
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*e).c_str());
	}
	
	vector<set<string> > retval;
	vector<set<string> >::iterator rv;

	rv = retval.end();
	retval.insert(rv,NFs);
	rv = retval.end();
	retval.insert(rv,phyPorts);
	rv = retval.end();
	retval.insert(rv,endPointsGre);
	rv = retval.end();
	retval.insert(rv,endPointsInternal);

	return retval;
}

vector<set<string> > GraphManager::identifyVirtualLinksRequired(highlevel::Graph *newPiece, LSI *lsi)
{
	set<string> NFs;
	set<string> phyPorts;
	set<string> endPointsGre;
	set<string> endPointsInternal;
	
	list<highlevel::Rule> rules = newPiece->getRules();
	for(list<highlevel::Rule>::iterator rule = rules.begin(); rule != rules.end(); rule++)
	{
		highlevel::Action *action = rule->getAction();
		highlevel::Match match = rule->getMatch();
		if(action->getType() == highlevel::ACTION_ON_NETWORK_FUNCTION)
		{
			if(match.matchOnPort() || match.matchOnEndPointInternal())
			{
				highlevel::ActionNetworkFunction *action_nf = (highlevel::ActionNetworkFunction*)action;

				//Check if a vlink is required for this network function port
				map<string, uint64_t> nfs_vlinks = lsi->getNFsVlinks();
				stringstream ss;
				ss << action->getInfo() << "_" << action_nf->getPort();
				if(nfs_vlinks.count(ss.str()) == 0)
					NFs.insert(ss.str());
			}
		}
		else if(action->getType() == highlevel::ACTION_ON_ENDPOINT_GRE)
		{
			if(match.matchOnPort() || match.matchOnEndPointInternal())
			{
				//check if a vlink is required for this gre-tunnel endpoint
				map<string, uint64_t> endpoints_vlinks = lsi->getEndPointsGreVlinks();
				highlevel::ActionEndPointGre *action_ep = (highlevel::ActionEndPointGre*)action;
				if(endpoints_vlinks.count(action_ep->toString()) == 0)
					endPointsGre.insert(action_ep->toString());
			}
		}
		else if(action->getType() == highlevel::ACTION_ON_PORT)
		{
			if(match.matchOnNF() || match.matchOnEndPointGre())
			{
				//check if a vlink is required for this physical port (i.e., interface endpoint)
				map<string, uint64_t> ports_vlinks = lsi->getPortsVlinks();
				highlevel::ActionPort *action_port = (highlevel::ActionPort*)action;
				if(ports_vlinks.count(action_port->getInfo()) == 0)
					phyPorts.insert(action_port->getInfo());
			}
		}
		else if(action->getType() == highlevel::ACTION_ON_ENDPOINT_INTERNAL)
		{
			if(!match.matchOnPort() && !match.matchOnEndPointInternal())
			{
				//check if a vlink is required for this internal endpoint
				map<string, uint64_t> endpoints_vlinks = lsi->getEndPointsVlinks();
				highlevel::ActionEndPointInternal *action_ep = (highlevel::ActionEndPointInternal*)action;
				if(endpoints_vlinks.count(action_ep->toString()) == 0)
					endPointsInternal.insert(action_ep->toString());
			}
		}
	}

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Network functions input ports requiring a virtual link:");
	for(set<string>::iterator nf = NFs.begin(); nf != NFs.end(); nf++)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*nf).c_str());
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Physical ports requiring a virtual link:");
	for(set<string>::iterator p = phyPorts.begin(); p != phyPorts.end(); p++)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*p).c_str());
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Gre endpoints requiring a virtual link:");
	for(set<string>::iterator e = endPointsGre.begin(); e != endPointsGre.end(); e++)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*e).c_str());
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Internal endpoints requiring a virtual link:");
	for(set<string>::iterator e = endPointsInternal.begin(); e != endPointsInternal.end(); e++)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "\t%s",(*e).c_str());

	//prepare the return value
	vector<set<string> > retval;
	vector<set<string> >::iterator rv;
	rv = retval.end();
	retval.insert(rv,NFs);
	rv = retval.end();
	retval.insert(rv,phyPorts);
	rv = retval.end();
	retval.insert(rv,endPointsGre);
	rv = retval.end();
	retval.insert(rv,endPointsInternal);

	return retval;
}

void GraphManager::removeUselessPorts_NFs_Endpoints_VirtualLinks(RuleRemovedInfo rri, ComputeController *computeController,highlevel::Graph *graph, LSI * lsi)
{
	/*
	*	Check if ports, NFs, end point and virtual links used by the rule removed are still useful.
	*	Note that the rule has already been removed from the high level graph
	*/
	map<string, uint64_t> nfs_vlinks = lsi->getNFsVlinks();
	map<string, uint64_t> ports_vlinks = lsi->getPortsVlinks();
#ifndef UNIFY_NFFG
	map<string, uint64_t> endpoints_vlinks = lsi->getEndPointsVlinks();
#endif
	map<string, uint64_t> endpoints_gre_vlinks = lsi->getEndPointsGreVlinks();

	list<highlevel::Rule> rules = graph->getRules();

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Checking if ports, network functions, endpoints and virtual links can be removed...");
	if(rri.isNFport)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the NF port '%s' must be removed (if this vlink exists)",rri.nf_port.c_str());

	if(rri.isNFport && nfs_vlinks.count(rri.nf_port) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The NF port '%s' is associated with a vlink",rri.nf_port.c_str());

		/**
		*	In case NF:port does not appear in other actions, and it is not use for any physical port, then the vlink must be removed
		*/

		bool equal = false;
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			if(a->getType() == highlevel::ACTION_ON_NETWORK_FUNCTION)
			{

				stringstream nf_port;
				nf_port << ((highlevel::ActionNetworkFunction*)a)->getInfo() << "_" << ((highlevel::ActionNetworkFunction*)a)->getPort();
				string nf_port_string = nf_port.str();

				if(nf_port_string == rri.nf_port)
				{
					//The action is on the same NF:port of the removed one, hence
					//the vlink must not be removed
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The vlink cannot be removed, since there are other actions expressed on the NF port '%s'",rri.nf_port.c_str());
					equal = true;
					break;
				}
			}

		}//end of again iterator on the rules of the graph
		if(!equal)
		{
			//We just know that the vlink is no longer used for a NF. However, it might be used in the opposite
			//direction, for a port

			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for NF port: %s",rri.nf_port.c_str());
			uint64_t tobeRemovedID = nfs_vlinks.find(rri.nf_port)->second;

			//The virtual link is no longer associated with the network function port
			lsi->removeNFvlink(rri.nf_port);

			for(map<string, uint64_t>::iterator pvl = ports_vlinks.begin(); pvl != ports_vlinks.end(); pvl++)
			{
				if(pvl->second == tobeRemovedID)
				{
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the port: %s",pvl->first.c_str());
					goto next;
				}
			}

			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

			try
			{
				VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
				DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
				switchManager.destroyVirtualLink(dvli);
				lsi->removeVlink(tobeRemovedID);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

	if(rri.isEndpointGre)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the gre endpoint '%s' must be removed (if this vlink exists)",rri.endpointGre.c_str());

	if(rri.isEndpointGre && endpoints_gre_vlinks.count(rri.endpointGre) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The gre endpoint '%s' is associated with a vlink",rri.endpointGre.c_str());

		/**
		*	In case the gre endpoint does not appear in other actions, the vlink must be removed
		*/
		bool equal = false;
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{

			highlevel::Action *a = again->getAction();
			if(a->getType() == highlevel::ACTION_ON_ENDPOINT_GRE)
			{
				if(((highlevel::ActionEndPointGre*)a)->toString() == rri.endpointGre)
				{
					//The action is on the same gre endpoint of the removed one, hence
					//the vlink must not be removed
					equal = true;
					break;
				}
			}

		}//end of again iterator on the rules of the graph
		if(!equal)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for the gre endpoint: %s",rri.endpointGre.c_str());

			uint64_t tobeRemovedID = endpoints_gre_vlinks.find(rri.endpointGre)->second;
			lsi->removeEndPointGrevlink(rri.endpointGre);

			for(map<string, uint64_t>::iterator pvl = ports_vlinks.begin(); pvl != ports_vlinks.end(); pvl++)
			{
				if(pvl->second == tobeRemovedID)
				{
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the port: %s",pvl->first.c_str());
					goto next;
				}
			}

			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

			try
			{
				VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
				DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
				switchManager.destroyVirtualLink(dvli);
				lsi->removeVlink(tobeRemovedID);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

next:

	if(rri.isPort)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check of the vlink associated with the port '%s' must be removed (if this vlink exists)",rri.port.c_str());

	if(rri.isPort && ports_vlinks.count(rri.port) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The port '%s' is associated with a vlink",rri.port.c_str());
		/**
		*	In case port does not appear in other actions, the vlink must be removed
		*/
		bool equal = false;
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{
			highlevel::Action *a = again->getAction();
			if(a->getType() == highlevel::ACTION_ON_PORT)
			{
				if(((highlevel::ActionPort*)a)->getInfo() == rri.port)
				{
					//The action are the same, hence no vlink must be removed
					equal = true;
					break;
				}
			}

		}//end of again iterator on the rules of the graph
		if(!equal)
		{
			//We just know that the vlink is no longer used for a port. However, it might used in the opposite
			//direction, for a NF port
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for port: %s",rri.port.c_str());
			uint64_t tobeRemovedID = ports_vlinks.find(rri.port)->second;

			lsi->removePortvlink(rri.port);

			for(map<string, uint64_t>::iterator nfvl = nfs_vlinks.begin(); nfvl != nfs_vlinks.end(); nfvl++)
			{
				if(nfvl->second == tobeRemovedID)
				{
					logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link cannot be removed because it is still used by the NF port: %s",nfvl->first.c_str());
					goto next2;
				}
			}

			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");
			try
			{
				VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
				DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
				switchManager.destroyVirtualLink(dvli);
				lsi->removeVlink(tobeRemovedID);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

next2:

	if(rri.isEndpointInternal)
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Check if the vlink associated with the internal endpoint '%s' must be removed (if this vlink exists)",rri.endpointInternal.c_str());

	if(rri.isEndpointInternal && endpoints_vlinks.count(rri.endpointInternal) != 0)
	{
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal endpoint '%s' is associated with a vlink",rri.endpointInternal.c_str());

		/**
		*	In case the internal endpoint does not appear in other actions, the vlink must be removed
		*/
		bool equal = false;
		for(list<highlevel::Rule>::iterator again = rules.begin(); again != rules.end(); again++)
		{

			highlevel::Action *a = again->getAction();
			if(a->getType() == highlevel::ACTION_ON_ENDPOINT_INTERNAL)
			{
				if(((highlevel::ActionEndPointInternal*)a)->toString() == rri.endpointInternal)
				{
					//The action is on the same endpoint of the removed one, hence
					//the vlink must not be removed
					equal = true;
					break;
				}
			}

		}//end of again iterator on the rules of the graph
		if(!equal)
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Virtual link no longer required for the internal endpoint: %s",rri.endpointInternal.c_str());

			uint64_t tobeRemovedID = endpoints_vlinks.find(rri.endpointInternal)->second;
			lsi->removeEndPointvlink(rri.endpointInternal);
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The virtual link must be removed");

			try
			{
				VLink toBeRemoved = lsi->getVirtualLink(tobeRemovedID);
				DestroyVirtualLinkIn dvli(lsi->getDpid(), toBeRemoved.getLocalID(), toBeRemoved.getRemoteDpid(), toBeRemoved.getRemoteID());
				switchManager.destroyVirtualLink(dvli);
				lsi->removeVlink(tobeRemovedID);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

	//Remove NFs, if they no longer appear in the graph
	for(list<string>::iterator nf = rri.nfs.begin(); nf != rri.nfs.end(); nf++)
	{
#if 0 
		//IVANO: this check has been moved in highlevel graph, in the function that removes pieces from the graph
		if(!graph->stillExistNF(*nf))
#endif
		{
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The NF '%s' is no longer part of the graph",(*nf).c_str());

			//Stop the NF
#ifdef RUN_NFS
			computeController->stopNF(*nf);
#else
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Flag RUN_NFS disabled. No NF to be stopped");
#endif

			set<string> nf_ports;
			map<unsigned int, string>lsi_nf_ports = lsi->getNetworkFunctionsPortsNameOnSwitchMap(*nf);
			for (map<unsigned int,string>::iterator lsi_nfp_it = lsi_nf_ports.begin(); lsi_nfp_it != lsi_nf_ports.end(); ++lsi_nfp_it) {
				nf_ports.insert(lsi_nfp_it->second);
			}

			try
			{
				DestroyNFportsIn dnpi(lsi->getDpid(), *nf, nf_ports);
				switchManager.destroyNFPorts(dnpi);
				lsi->removeNF(*nf);
			} catch (SwitchManagerException e)
			{
				logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
				throw GraphManagerException();
			}
		}
	}

#if 0 
	//IVANO: moved in highlevelgraph, in the function that removes pieces from the graph
	//Remove physical ports, if they no longer appear in the graph
	for(list<string>::iterator p = rri.ports.begin(); p != rri.ports.end(); p++)
	{
		if(!graph->stillExistPort(*p))
			logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The port '%s' is no longer part of the graph",(*p).c_str());
	}
#endif

	//Remove the internal endpoint, if it no longer appear in the graph
	if((rri.endpointInternal != "") && (!graph->stillExistEndpoint(rri.endpointInternal)))
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The internal endpoint '%s' is no longer part of the graph",rri.endpointInternal.c_str());

	//Remove the gre endpoint, if it no longer appear in the graph
	if((rri.endpointGre != "") && (!graph->stillExistEndpointGre(rri.endpointGre)))
	{
#ifdef VSWITCH_IMPLEMENTATION_OVSDB
		logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "The gre endpoint '%s' is no longer part of the graph",rri.endpointGre.c_str());

		try
		{
			DestroyEndpointIn depi(lsi->getDpid(),rri.endpointGre);
			switchManager.destroyEndpoint(depi);
			lsi->removeEndpoint(rri.endpointGre);
		} catch (SwitchManagerException e)
		{
			logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
			throw GraphManagerException();
		}
#else
		logger(ORCH_WARNING, MODULE_NAME, __FILE__, __LINE__, "GRE tunnel unavailable");
#endif
	}
}

#if 0
bool GraphManager::stopNetworkFunction(string graphID, string nf_name)
{
	//TODO: the NF must not have flows associated!!

	GraphInfo graphInfo = (tenantLSIs.find(graphID))->second;
	highlevel::Graph *graph = graphInfo.getGraph();
	ComputeController *computeController = graphInfo.getComputeController();
	LSI *lsi = graphInfo.getLSI();

	if(graph->stillExistNF(nf_name))
	{
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "The graph still contains flows associated with the NF: \"%s\"",nf_name.c_str());
		return false;
	}

#ifdef RUN_NFS
	computeController->stopNF(nf_name);
#else
	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Flag RUN_NFS disabled. No NF to be stopped");
#endif
	try
	{
	//	list<struct nf_port_info> tmpListPorts = lsi->getNetworkFunctionsPortsInfo(nf_name);
		map<string,unsigned int>tmpListPorts= lsi->getNetworkFunctionsPorts(nf_name);

		set<string> nf_ports;
		for (map<string,unsigned int>::iterator lsi_nfp_it = tmpListPorts.begin(); lsi_nfp_it != tmpListPorts.end(); ++lsi_nfp_it)
				nf_ports.insert(lsi_nfp_it->first);


//		set<struct nf_port_info> portsToBeRemoved(tmpListPorts.begin(),tmpListPorts.end());

		DestroyNFportsIn dnpi(lsi->getDpid(),nf_name,/*portsToBeRemoved*/nf_ports);
		switchManager.destroyNFPorts(dnpi);
		lsi->removeNF(nf_name);
	} catch (SwitchManagerException e)
	{
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "%s",e.what());
		return false;
	}

	return true;
}
#endif

#if 0
string GraphManager::findEndPointTowardsGRE(highlevel::Graph *graph, string ep)
{
	list<highlevel::Rule> rules = graph->getRules();
	for(list<highlevel::Rule>::iterator rule = rules.begin(); rule != rules.end(); rule++)
	{
		highlevel::Action *action = rule->getAction();
		highlevel::Match match = rule->getMatch();
		if(action->getType() == highlevel::ACTION_ON_ENDPOINT_GRE && match.matchOnEndPointInternal())
		{
			highlevel::ActionEndPointGre *action_ep = (highlevel::ActionEndPointGre*)action;
			stringstream ss;
			ss << action_ep->getPort();

			if(ep == ss.str())
			{
				stringstream ssm;
				ssm << match.getEndPoint();
				return ssm.str();
			}
		}
	}

	assert(0);

	return ""; //just for the compiler
}
#endif

string GraphManager::findEndPointTowardsNF(highlevel::Graph *graph, string nf)
{
	list<highlevel::Rule> rules = graph->getRules();
	for(list<highlevel::Rule>::iterator rule = rules.begin(); rule != rules.end(); rule++)
	{
		highlevel::Action *action = rule->getAction();
		highlevel::Match match = rule->getMatch();
		if(action->getType() == highlevel::ACTION_ON_NETWORK_FUNCTION && match.matchOnEndPointInternal())
		{
			highlevel::ActionNetworkFunction *action_nf = (highlevel::ActionNetworkFunction*)action;
			stringstream ss;
			ss << action->getInfo() << "_" << action_nf->getPort();

			if(nf == ss.str())
			{
				stringstream ssm;
				ssm << match.getEndPointInternal();
				return ssm.str();
			}
		}
	}

	assert(0);

	return ""; //just for the compiler
}

void GraphManager::printInfo(lowlevel::Graph graphLSI0, LSI *lsi0)
{
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Graphs deployed:");
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tID: 'LSI-0'");
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tRules: ");

	map<string,LSI *> lsis;
	for(map<string,GraphInfo>::iterator graphs = tenantLSIs.begin(); graphs != tenantLSIs.end(); graphs++)
		lsis[graphs->first] = graphs->second.getLSI();

	graphLSI0.prettyPrint(lsi0,lsis);
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");

	printInfo(false);
}

void GraphManager::printInfo(bool completed)
{
	if(completed)
	{
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "Graphs deployed:");
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
	}

	map<string,GraphInfo>::iterator it;
	for(it = tenantLSIs.begin(); it != tenantLSIs.end(); it++)
	{
		int id;
		sscanf(it->first.c_str(),"%d",&id);

		if(id == 2)
		{
			coloredLogger(ANSI_COLOR_BLUE,ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tGraph ID: '%s'",it->first.c_str());
			coloredLogger(ANSI_COLOR_BLUE,ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tVNF installed:");
		}
		else if(id == 3)
		{
			coloredLogger(ANSI_COLOR_RED,ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tID: '%s'",it->first.c_str());
			coloredLogger(ANSI_COLOR_RED,ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tVNF installed:");
		}
		else
		{
			logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tID: '%s'",it->first.c_str());
			logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "\tVNF installed:");
		}

		ComputeController *computeController = it->second.getComputeController();
		computeController->printInfo(id);
		logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
	}

	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__, "");
}

