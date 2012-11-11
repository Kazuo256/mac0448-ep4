#ifndef EP4_SOURCELOGIC_H_
#define EP4_SOURCELOGIC_H_

#include "routerlogic.h"

namespace ep4 {

class SourceLogic : public RouterLogic {
	public:
		void handle_msg (unsigned reveiver_id, unsigned sender_id,
                             const std::string& msg_name,
                             std::stringstream& args);
		void make_group (Router& source, unsigned group_id);
		void join_group (Router& router, unsigned group_id);
		void leave_group (Router& router, unsigned group_id);
};
} // namespace ep4

#endif