#include <iostream>
#include <object.hpp>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include <boost/lockfree/queue.hpp>

#include <mutex>

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
	try {
		for(int i=0; i < 1000000; i++) {
			s_MyWidget wd(new MyWidget());
			wd->foo();
			s_MyButton btn(new MyButton());
			btn->foo();
		}
	} catch(std::exception& exc) {
        std::cerr<<exc.what()<<std::endl;
	}
}


int main(int argc, const char** argv)
{
	test_routine();
	boost::thread_group pool;
	for(int i=0; i < 4; i++) {
		pool.create_thread( boost::bind( test_routine ) );
	}
	pool.join_all();
	return 0;
}
