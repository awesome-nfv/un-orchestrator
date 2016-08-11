#ifndef NODE_ORCHESTRATOR_ACTION_NETWORK_FUNCTION_H
#define NODE_ORCHESTRATOR_ACTION_NETWORK_FUNCTION_H

#include "../../../utils/logger.h"
#include "output_action.h"
#include <iostream>
#include <sstream>


class TempActionNetworkFunction: public OutputAction {
private:
    /**
    *	@brief: the id of the NF (e.g., 0001)
    */
    string nfId;

    /**
    *	@brief: the name of the endpoint port (e.g., vnf:0001:inout:0)
    */
    string endpointPortName;

    /**
    *	@brief: the port of the NF (e.g., 1)
    */
    unsigned int port;

public:

    TempActionNetworkFunction(string nfId, string endpointPortName, unsigned int port = 1);
    string getInfo();
    unsigned int getPort();
    string toString();

    bool operator==(const TempActionNetworkFunction &other) const;

    Object toJSON();
};


#endif //NODE_ORCHESTRATOR_ACTION_NETWORK_FUNCTION_H
