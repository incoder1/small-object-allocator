#include <iostream>
#include <object.hpp>

#include <boost/noncopyable.hpp>
#include <thread>
#include <vector>
#include <chrono>

struct rect {
	int l,t,r,b;
};

class MyObject:public boost::noncopyable {
protected:
	MyObject() BOOST_NOEXCEPT_OR_NOTHROW
	{}
public:
	virtual ~MyObject() BOOST_NOEXCEPT_OR_NOTHROW = 0;
private:
	friend BOOST_FORCEINLINE void intrusive_ptr_add_ref(MyObject* obj);
	friend BOOST_FORCEINLINE void intrusive_ptr_release(MyObject* const obj);
	boost::atomics::atomic_size_t refCount_;
};

void intrusive_ptr_add_ref(MyObject* obj)
{
	obj->refCount_.fetch_add(1, boost::memory_order_relaxed);
}

void  intrusive_ptr_release(MyObject* const obj)
{
	if (obj->refCount_.fetch_sub(1, boost::memory_order_release) == 1) {
		boost::atomics::atomic_thread_fence(boost::memory_order_acquire);
		delete obj;
	}
}

MyObject::~MyObject() BOOST_NOEXCEPT_OR_NOTHROW
{
}

class MyWidget:public virtual MyObject {
public:
	MyWidget():
		MyObject()
	{}
	virtual ~MyWidget() BOOST_NOEXCEPT_OR_NOTHROW {
	}
private:
	rect r_;
};

class MyPanel:public MyWidget
{
public:
	MyPanel() BOOST_NOEXCEPT_OR_NOTHROW:
		MyWidget()
	{}
private:
	MyPanel *parent_;
};

class MyButton:public MyWidget {
public:
	MyButton() BOOST_NOEXCEPT_OR_NOTHROW:
		MyWidget()
	{}
private:
	enum _state {ENABLED, DISABLED};
	_state state_;
};

BOOST_DECLARE_OBJECT_PTR_T(MyWidget);
BOOST_DECLARE_OBJECT_PTR_T(MyButton);
BOOST_DECLARE_OBJECT_PTR_T(MyPanel);

class Widget:public virtual boost::object {
public:
  Widget() BOOST_NOEXCEPT_OR_NOTHROW:
 	boost::object()
   {}
   virtual ~Widget() BOOST_NOEXCEPT_OR_NOTHROW
   {}
private:
   rect r_;
};

class Panel:public Widget {
public:
	Panel() BOOST_NOEXCEPT_OR_NOTHROW:
		Widget()
	{}
private:
	Panel *parent_;
};

class Button:public Widget {
public:
	Button():
		Widget()
	{}
private:
	enum _state {ENABLED, DISABLED};
	_state state_;
};

BOOST_DECLARE_OBJECT_PTR_T(Widget);
BOOST_DECLARE_OBJECT_PTR_T(Panel);
BOOST_DECLARE_OBJECT_PTR_T(Button);

static const int THREADS = std::thread::hardware_concurrency();
static const int TEST_COUNT = 185042; // 24mb / 136b

void test_small_routine()
{
	for(int i=0; i < TEST_COUNT; i++) {
		s_Widget wd0(new Widget());
		s_Button btn(new Button());
		s_Panel pnl(new Panel());
		s_Widget wd1(new Widget());
	}
}

void test_clib_routine()
{
	for(int i=0; i < TEST_COUNT; i++) {
	    s_MyWidget wd0(new MyWidget());
		s_MyButton btn(new MyButton());
		s_MyPanel pnl(new MyPanel());
		s_MyWidget wd1(new MyWidget());
	}
}

typedef void (*routine_f)();

void multi_thread_test(const char *msg, routine_f r) {
	auto start = std::chrono::steady_clock::now();
	std::vector<std::thread> workers;
	for(int i=0; i < THREADS; i++) {
		workers.push_back( std::thread( std::bind( r ) ) );
	}
	for (std::thread &t: workers) {
      if (t.joinable()) {
        t.join();
      }
    }
    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    std::cout <<  msg << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;
}

void single_thread_test(const char *msg, routine_f r) {
	auto start = std::chrono::steady_clock::now();
    r();
    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    std::cout <<  msg << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;
}


int main(int argc, const char** argv)
{
	std::cout<<"Widget: " << sizeof(Widget) <<" MyWidget: " << sizeof(MyWidget) <<" bytes" << std::endl;
	std::cout<<"Button: " << sizeof(Button) <<" MyButton: " << sizeof(MyButton) <<" bytes" << std::endl;
	std::cout<<"Panel: " << sizeof(Panel) <<" MyPanel: " << sizeof(MyPanel) <<" bytes" << std::endl << std::endl;

	// macke OS cache bofere testing
	// alloc and free 5 mb
	void *dummy = std::malloc(5242880);
	::std::free(dummy);
	for(int i=0; i < 3; i++) {
		single_thread_test("SS Libc: ", test_clib_routine);
		single_thread_test("SS sobj: ", test_small_routine);
	}
	std::cout<<std::endl;
	dummy = std::malloc(5242880);
	::std::free(dummy);
	for(int i=0; i < 3; i++) {
		multi_thread_test("MT Libc: ",test_clib_routine);
		multi_thread_test("MT sobj: ",test_small_routine);
	}
	return 0;
}
