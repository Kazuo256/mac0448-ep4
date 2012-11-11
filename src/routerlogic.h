#ifndef EP4_ROUTERLOGIC_H_
#define EP4_ROUTERLOGIC_H_

#include <string>
#include <sstream>

namespace ep4 {

class Router;

class RouterLogic {
	public:
		virtual void handle_msg (unsigned reveiver_id, unsigned sender_id,
                             const std::string& msg_name,
                             std::stringstream& args) {}
    virtual void make_group (Router& source, unsigned group_id) {}
    virtual void join_group (Router& router, unsigned group_id) {}
    virtual void leave_group (Router& router, unsigned group_id) {}
		virtual ~RouterLogic() {}
};

} // namespace ep4

#endif
