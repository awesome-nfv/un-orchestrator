#ifndef HIGH_LEVEL_GRAPH_H_
#define HIGH_LEVEL_GRAPH_H_ 1

#pragma once

#include <list>
#include <set>
#include <string>
#include <iostream>
#include <sstream>

#include "high_level_graph_endpoint_interface.h"
#include "high_level_graph_endpoint_interface_out.h"
#include "high_level_graph_endpoint_gre.h"
#include "high_level_graph_vnf.h"
#include "high_level_rule.h"
#include "high_level_output_action_nf.h"
#include "high_level_output_action_port.h"
#include "../../graph_manager/rule_removed_info.h"
#include "../../../utils/logger.h"
#include "../../../utils/constants.h"

#include <json_spirit/json_spirit.h>
#include <json_spirit/value.h>
#include <json_spirit/writer.h>

using namespace json_spirit;
using namespace std;

namespace highlevel
{

/**
*	@brief: describes the graph required from the extern
*/
class Graph
{

private:	
	/**
	*	@brief: for each NF attached to the graph specifies a list of ports. For
	*		instance, if in the graph there is NF:1 and NF:2,
	*		an element of the map is <NF, <1,2> >
	*/
	map<string, list<unsigned int> > networkFunctions;
	
	/**
	*	@brief: for each end point attached to the graph specifies a list of params
	*/
	map<string, list<string> > endpoints;

	/**
	*	@brief: physical ports to be attached to the graph
	*/
	set<string> ports;
	
	/**
	*	@brief: List of rules describing the graph
	*/
	list<Rule> rules;
	
	/**
	*	@brief: List of endPointsInterface describing the graph
	*/
	list<EndPointInterface> endPointsInterface;
	
	/**
	*	@brief: List of endPointsInterfaceOut describing the graph
	*/
	list<EndPointInterfaceOut> endPointsInterfaceOut;
	
	/**
	*	@brief: List of endPointsGRE describing the graph
	*/
	list<EndPointGre> endPointsGre;
	
	/**
	*	@brief: List of VNFs describing the graph
	*/
	list<VNFs> vnfs;
	
	/**
	*	@brief: Identifier of the graph
	*/
	string ID;
	
	/**
	*	@brief: id of the graph
	*/
	string id;
	
	/**
	*	@brief: Name of the graph
	*/
	string name;
	
public:	

	/**
	*	@brief: construction
	*
	*	@param: ID	identifier of the graph
	*/
	Graph(string ID);

	/**
	*	@brief: Add a physical port to the graph
	*
	*	@param: port	Name of the physical port to be added
	*/
	bool addPort(string port);
	
	/**
	*	@brief: Return the physical ports of the graph
	*/
	set<string> getPorts();
	
	/**
	*	@brief: Add a NF to the graph
	*
	*	@param:	nf	Name of the network function to be added
	*/
	bool addNetworkFunction(string nf);
	
	/**
	*	@brief: Update a NF by adding a port
	*	//FIXME: is this useful?
	*
	*	@param: nf		Name of the network function to be updated
	*	@param: port	Identifier of the port of the network function
	*/
	bool updateNetworkFunction(string nf, unsigned port);
	
	/**
	*	@brief: Return the NFs of the graph and the ports they require
	*/
	map<string, list<unsigned int> > getNetworkFunctions();

	/**
	*	@brief: Add an end point to the graph, to be used to connect the graph itself with
	*		other graphs.
	*
	*	@param: ep			Identifier of the graph defining the end point
	*	@param: p			List of three elements (local ip, remote ip and key)
	*
	*	@return: true if the endpoint is defined in the current graph; false otherwise
	*/
	bool addEndPoint(string ep, list<string> p);
	
	/**
	*	@brief: Return the end points of the graph, i.e. the ports to be used to connect
	*		multiple graphs together.
	*/
	map<string, list<string> > getEndPoints();
	
	/**
	*	@brief: given a graph endpoint (in the form graphID:epID), return true if it is defined
	*		in this graph
	*/
	bool isDefinedHere(string endpoint);

	/**
	*	@brief: Return the ID of the graph
	*/
	string getID();
	
	/**
	*	@brief: Set the id of the graph
	*/
	void setId(string id);
	
	/**
	*	@brief: Return the id of the graph
	*/
	string getId();
	
	/**
	*	@brief: Set the name of the graph
	*/
	void setName(string name);
	
	/**
	*	@brief: Return the name of the graph
	*/
	string getName();
	
	/**
	*	@brief: Add a new endpointInterface to the graph
	*/
	bool addEndPointInterface(EndPointInterface endpoint);
	
	/**
	*	@brief: Return the endpointsInterface of the graph
	*/
	list<EndPointInterface> getEndPointsInterface();
	
	/**
	*	@brief: Add a new endpointInterfaceOut to the graph
	*/
	bool addEndPointInterfaceOut(EndPointInterfaceOut endpoint);
	
	/**
	*	@brief: Return the endpointsInterfaceOut of the graph
	*/
	list<EndPointInterfaceOut> getEndPointsInterfaceOut();
	
	/**
	*	@brief: Add a new endpointGre to the graph
	*/
	bool addEndPointGre(EndPointGre endpoint);
	
	/**
	*	@brief: Return the endpointsGre of the graph
	*/
	list<EndPointGre> getEndPointsGre();
	
	/**
	*	@brief: Add a new vnf to the graph
	*/
	bool addVNF(VNFs vnf);
	
	/**
	*	@brief: Return the vnfs of the graph
	*/
	list<VNFs> getVNFs();
	
	/**
	*	@brief: Return the rules of the graph
	*/
	list<Rule> getRules();
	
	/**
	*	@brief: Return the rule with a specific ID
	*
	*	@param: ID	Identifier of the rule to be returned
	*/
	Rule getRuleFromID(string ID);

	/**
	*	@brief: Add a new rule to the graph. THe rule is not added if it has an ID already
	*		existing into the graph
	*
	*	@param:	rule	Rule to be added
	*/
	bool addRule(Rule rule);
	
	/**
	*	@brief: Check if the graph contains a specific flowID
	*
	*	@param:	ID	Identifier of a graph
	*/
	bool ruleExists(string ID);
	
	/**
	*	@brief: Remove a rule starting from its ID, and return
	*		information on the removed rule
	*
	*	@param:	ID	Identifier of the graph to be removed
	*/
	RuleRemovedInfo removeRuleFromID(string ID);
	
	/**
	*	@brief: Checks if a NF still exist in the graph. If it is no longer used
	*		in any rule, but it is still part of the "networkFunctions" map,
	*		it is removed from this map as well.
	*
	*	@param:	nf	Name of a network function
	*/
	bool stillExistNF(string nf);
	
	/**
	*	@brief: Checks if a physical port still exist in the graph. If it is no
	*		longer used in any rule, but it is part of the "ports" set, it
	*		is removed from this set as well.
	*
	*	@param:	port	Name of a physical port
	*/
	bool stillExistPort(string port);
	
	/**
	*	@brief: Checks if an endpoint port still exist in the graph. If it is no
	*		longer used in any rule, but it is part of the "endpoints" set, it
	*		is removed from this set as well.
	*
	*	@param: endpoint	Name of an endpoint
	*/
	bool stillExistEndpoint(string endpoint);
	
	/**
	*	@brief: check if a specific flow uses an endpoint (it does not matter if the endpoint is defined in the
	*		current graph or not); in this case, return that endpoint, otherwise return "".
	*
	*	@param: flowID	Identifier of the flow that could contain an endpoint
	*/
	string getEndpointInvolved(string flowID);
	
	/**
	*	@brief: check if an endpoint is used in some action of the graph
	*
	*	@param: endpoint	Idenfier of the endpoint to be checked
	*/
	bool endpointIsUsedInAction(string endpoint);
	
	/**
	*	@brief: check if an endpoint is used in some match of the graph
	*
	*	@param: endpoint	Idenfier of the endpoint to be checked
	*/
	bool endpointIsUsedInMatch(string endpoint);
	
	/**
	*	@brief: Return the number of flows in the graph
	*/
	int getNumberOfRules();
	
	/**
	*	@brief: Create a JSON representation of the graph
	*/
	Object toJSON();
	
	void print();
};

}

#endif //HIGH_LEVEL_GRAPH_H_
