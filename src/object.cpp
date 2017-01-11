#include "object.hpp"

namespace smallobject {

// object

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

void* object::operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc)
{
	if(bytes > detail::object_allocator::MAX_SIZE)
		return ::operator new(bytes);
	for(;;) {
		void* result = detail::object_allocator::instance()->malloc(bytes);
		if(NULL != result)
			return result;
		std::new_handler new_handler = std::get_new_handler();
		if(NULL == new_handler)
#ifdef BOOST_NO_EXCEPTIONS
			std::terminate();
#else
			throw std::bad_alloc();
#endif
		new_handler();
	}
	return NULL;
}

void object::operator delete(void* const ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW {
	if(bytes <= detail::object_allocator::MAX_SIZE) {
		detail::object_allocator::instance()->free(ptr, bytes);
	} else {
		::operator delete(ptr);
	}
}

} // namespace smallobject

#if (defined(SO_DLL) && defined(BUILD_DLL)) && (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
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
