#ifndef EP4_ROUTERLOGIC_H_
#define EP4_ROUTERLOGIC_H_

#include <string>
#include <sstream>

namespace ep4 {

class RouterLogic {
	public:
		virtual void handle_msg (unsigned reveiver_id, unsigned sender_id,
                             const std::string& msg_name,
                             std::stringstream& args) {}
    virtual void make_group (unsigned source_id, unsigned group_id) {}
    virtual void join_group (unsigned router_id, unsigned group_id) {}
    virtual void leave_group (unsigned router_id, unsigned group_id) {}
		virtual ~RouterLogic() {}
};

} // namespace ep4

#endif
