#include "vlan_action.h"

VlanAction::VlanAction(vlan_action_t type, string vlan_endpoint, uint16_t label):
	GenericAction(), type(type), vlan_endpoint(vlan_endpoint), label(label)
{

}

VlanAction::~VlanAction()
{

}

void VlanAction::print()
{
	stringstream ss;
	if(type == ACTION_VLAN_PUSH || type == ACTION_ENDPOINT_VLAN)
		ss << "push_vlan " << label;
	else if(type == ACTION_VLAN_POP)
		ss << "pop_vlan";
	ss << endl;

	cout << "\t\t\tvlan: " << ss.str();
}

void VlanAction::toJSON(Object &action)
{
	Object vlanAction;
	string vlan_op;

	if(type == ACTION_ENDPOINT_VLAN)
		action[OUTPUT] = vlan_endpoint.c_str();
	else if(type == ACTION_VLAN_PUSH)
		vlan_op = "push_vlan";
	else
		vlan_op = "pop_vlan";
	
	if(type != ACTION_ENDPOINT_VLAN)
	{
		stringstream s_label;
		s_label << label;
		action[vlan_op] = s_label.str();
	}
}

void VlanAction::fillFlowmodMessage(rofl::openflow::cofflowmod &message, unsigned int *position)
{
	switch(OFP_VERSION)
	{
		case OFP_10:
			assert(0 && "TODO");
			//TODO
			exit(0);
			break;
		case OFP_12:
			if(type == ACTION_VLAN_PUSH || type == ACTION_ENDPOINT_VLAN)
			{
				message.set_instructions().set_inst_apply_actions().set_actions().add_action_push_vlan(rofl::cindex(*position)).set_eth_type(rofl::fvlanframe::VLAN_CTAG_ETHER);
				(*position)++;
				message.set_instructions().set_inst_apply_actions().set_actions().add_action_set_field(rofl::cindex(*position)).set_oxm(rofl::openflow::coxmatch_ofb_vlan_vid(label | rofl::openflow::OFPVID_PRESENT));
				(*position)++;
			}
			else
			{
				message.set_instructions().set_inst_apply_actions().set_actions().add_action_pop_vlan(rofl::cindex(*position));
				(*position)++;
			}
			break;
		case OFP_13:
			assert(0 && "TODO");
			//TODO
			exit(0);
			break;
	}
}

string VlanAction::prettyPrint()
{
	stringstream ss;
	ss << " # vlan: " << ((type == ACTION_VLAN_PUSH)? "push_vlan " : "pop_vlan");
	if(type == ACTION_VLAN_PUSH)
		ss << " " << label;
	return ss.str();
}

