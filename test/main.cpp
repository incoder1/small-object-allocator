#include <iostream>
#include <object.hpp>

#include <boost/noncopyable.hpp>
#include <thread>
#include <vector>

struct rect {
	int l,t,r,b;
};

class MyObject:public boost::noncopyable {
protected:
	MyObject() BOOST_NOEXCEPT_OR_NOTHROW
	{}
public:
	virtual ~MyObject() BOOST_NOEXCEPT_OR_NOTHROW;
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


MyObject::~MyObject() BOOST_NOEXCEPT_OR_NOTHROW {
}

class MyWidget:public virtual MyObject {
public:
	MyWidget():
		MyObject()
	{
	}
	virtual void foo()
	{
	}
	virtual ~MyWidget() BOOST_NOEXCEPT_OR_NOTHROW {
	}
};

class MyButton:public MyWidget {
public:
	MyButton():
		MyWidget()
	{}
	virtual void foo()
	{
	}
	virtual ~MyButton() BOOST_NOEXCEPT_OR_NOTHROW {
	}
private:
	rect r_;
};


BOOST_DECLARE_OBJECT_PTR_T(MyWidget);
BOOST_DECLARE_OBJECT_PTR_T(MyButton);

class Widget:public virtual boost::object {
public:
Widget() BOOST_NOEXCEPT_OR_NOTHROW:
	boost::object()
	{}
	virtual void foo()
	{
	}
	virtual ~Widget() BOOST_NOEXCEPT_OR_NOTHROW {
	}
};

class Button:public Widget {
public:
	Button():
		Widget()
	{}
	virtual void foo()
	{
	}
	virtual ~Button() BOOST_NOEXCEPT_OR_NOTHROW {
	}
private:
	rect r_;
};

BOOST_DECLARE_OBJECT_PTR_T(Widget);
BOOST_DECLARE_OBJECT_PTR_T(Button);


void test_routine()
{
	for(int i=0; i < 250000; i++) {
		s_Widget wd(new Widget());
		wd->foo();
		s_Button btn(new Button());
		btn->foo();
	}
}


int main(int argc, const char** argv)
{
	std::vector<std::thread> workers;
	for(int i=0; i < 32; i++) {
		workers.push_back( std::thread( std::bind( test_routine ) ) );
	}
	for (std::thread &t: workers) {
      if (t.joinable()) {
        t.join();
      }
    }
	return 0;
}
