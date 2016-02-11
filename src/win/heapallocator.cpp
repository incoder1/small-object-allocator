#include "heapallocator.hpp"

namespace boost {

namespace smallobject {

namespace sys {

namespace detail {

smallobject::spin_lock heap_allocator::_mtx;
boost::atomic<heap_allocator*> heap_allocator::_instance(NULL);

} // namespace detail

} // namespace sys

}  // namespace smallobject

} // namespace boost
