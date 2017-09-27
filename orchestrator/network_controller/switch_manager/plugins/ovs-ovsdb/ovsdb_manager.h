#ifndef OVSDBManager_H_
#define OVSDBManager_H_ 1

#include "../../../../utils/logger.h"
#include "../../../../utils/constants.h"

#include "../../switch_manager.h"

#include "commands.h"

//TODO: remove this class. It is in fact quite useless, as it only contains wrappers to functions
//in commands.h

using namespace std;

class LSI;

class OVSDBManager : public SwitchManager
{
private:

public:
	OVSDBManager();

	~OVSDBManager();

	CreateLsiOut *createLsi(CreateLsiIn cli);

	AddNFportsOut *addNFPorts(AddNFportsIn anpi);

	AddEndpointOut *addGreEndpoint(AddEndpointIn aepi);

	AddEndpointHoststackOut *addHoststackEndpoint(AddEndpointHoststackIn aepi);

	AddVirtualLinkOut *addVirtualLink(AddVirtualLinkIn avli);

	AddLinkToL3PortOut *addLinkToL3Port(AddLinkToL3PortIn al3i);

	void destroyLsi(uint64_t dpid);

	void destroyNFPorts(DestroyNFportsIn dnpi);

    void destroyEndpoint(DestroyEndpointIn depi);

	void destroyHoststackEndpoint(DestroyHoststackEndpointIn dhepi);

	void destroyVirtualLink(DestroyVirtualLinkIn dvli);

	void checkPhysicalInterfaces(set<CheckPhysicalPortsIn> cppi);

};

class OVSDBManagerException: public SwitchManagerException
{
public:
	virtual const char* what() const throw()
	{
		return "OVSDBManagerException";
	}
};

#endif //OVSDBManager_H_
