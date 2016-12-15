#ifndef HARDWARE_CONTROLLER_H
#define HARDWARE_CONTROLLER_H 1
#include "hw_manager.h"

using namespace std;

class HardwareController
{
private:
	list<HWManager *> listOfHardware;
public:
	HardwareController();
	~HardwareController();
	bool checkHWAvailability();
};

#endif
