#include "object.hpp"

namespace smallobject {

// object

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

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__) && defined(BUILD_DLL)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE; // successful
}
#endif // Windows DLL
