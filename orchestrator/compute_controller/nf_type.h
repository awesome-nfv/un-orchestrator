#ifndef NF_TYPE_H_
#define NF_TYPE_H_ 1

#include <string>
#include <assert.h>

using namespace std;

typedef enum{DPDK,DOCKER,KVM}nf_t;

class NFType
{
public:
	static string toString(nf_t type)
	{
		if(type == DPDK)
			return string("dpdk");
		else if(type == DOCKER)
			return string("docker");
		else if(type == KVM)
			return string("kvm");

		assert(0);
		return "";
	}
	
	static unsigned int toID(nf_t type)
	{
		if(type == DPDK)
			return 2;
		else if(type == DOCKER)
			return 1;
		else if(type == KVM)
			return 0;

		assert(0);
		return 0;
	}

	static bool isValid(string type)
	{
		if(type == "dpdk" || type == "docker" || type == "kvm")
			return true;
	
		assert(0);
		
		return false;
	}
};

#endif //NF_TYPE_H_
