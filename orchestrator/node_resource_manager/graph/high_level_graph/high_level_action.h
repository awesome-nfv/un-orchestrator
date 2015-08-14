#ifndef HIGH_LEVEL_ACTION_H_
#define HIGH_LEVEL_ACTION_H_ 1

#pragma once

#include "../../../utils/constants.h"
#include "../generic_action.h"

#include <string>
#include <iostream>
#include <list>

#include <json_spirit/json_spirit.h>
#include <json_spirit/value.h>
#include <json_spirit/writer.h>

using namespace json_spirit;
using namespace std;

namespace highlevel
{

enum action_t {INVALID,ACTION_ON_PORT,ACTION_ON_NETWORK_FUNCTION,ACTION_ON_ENDPOINT};

class Action
{

private:
	action_t type;
	list<GenericAction*> genericActions;

public:
	action_t getType();
	virtual string getInfo() = 0;
	
	virtual void print() = 0;
	virtual Object toJSON() = 0;
	virtual string toString() = 0;

	void addGenericAction(GenericAction *ga);

	virtual ~Action();
	
protected:
	Action(action_t type);
};

}

#endif //HIGH_LEVEL_ACTION_H_
