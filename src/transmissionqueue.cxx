
#include "transmissionqueue.h"

#include <algorithm>
#include <tr1/functional>

namespace ep4 {

using std::push_heap;
using std::pop_heap;
using std::for_each;
using std::tr1::bind;
using namespace std::tr1::placeholders;

void TransmissionQueue::insert (const Packet& packet, double delay) {
  Transmission transmission = { packet, delay };
  queue_.push_back(transmission);
  push_heap(queue_.begin(), queue_.end());
}

const Packet TransmissionQueue::next () {
  Transmission trans = queue_.front();
  pop_heap(queue_.begin(), queue_.end());
  queue_.pop_back();
  for_each(
    queue_.begin(),
    queue_.end(),
    bind(&Transmission::update, _1, trans.delay)
  );
  return trans.packet;
}

} // namespace ep4

