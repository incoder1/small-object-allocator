#include "object.hpp"

namespace smallobject {

// object
object::object() BOOST_NOEXCEPT_OR_NOTHROW:
	ref_count_(0)
{}

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

void* object::operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc)
{
	void* result = NULL;
	if(bytes <= detail::object_allocator::MAX_SIZE) {
		result = detail::object_allocator::instance()->malloc(bytes);
		if( ! result ) {
			boost::throw_exception( std::bad_alloc() );
		}
	} else {
		result = ::operator new(bytes);
	}
	return result;
}

void object::operator delete(void* const ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW {
	if(bytes <= detail::object_allocator::MAX_SIZE) {
		detail::object_allocator::instance()->free(ptr, bytes);
	} else {
		::operator delete(ptr);
	}
}

} // namespace smallobject

