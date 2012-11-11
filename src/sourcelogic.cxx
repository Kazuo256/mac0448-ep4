
#include "sourcelogic.h"
#include "router.h"

using std::string;
using std::stringstream;

namespace ep4 {

void SourceLogic::handle_msg (unsigned reveiver_id, unsigned sender_id,
                             const std::string& msg_name,
                             std::stringstream& args) {}


void SourceLogic::make_group (ep4::Router& source, unsigned group_id) {
	stringstream msg;
	source.add_new_group(group_id, source.id());
	msg << "ADD_GROUP " << group_id << " " << source.id();
	source.broadcast(msg.str());
}

void SourceLogic::join_group (Router& router, unsigned group_id) {}
void SourceLogic::leave_group (Router& router, unsigned group_id) {}

} // namespace ep4s