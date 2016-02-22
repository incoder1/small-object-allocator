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
	MyWidget *child_;
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
	Widget *child_;
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

static const int THREADS = std::thread::hardware_concurrency() * 2;
static const int OBJECTS_COUNT = 56980; // ~10mb for 64 bit
static const int TESTS_COUNT = 64;

void so_routine()
{
	// normal flow
	for(int i=0; i < OBJECTS_COUNT; i++) {
		s_Widget wd0(new Widget());
		s_Button btn(new Button());
		s_Panel pnl(new Panel());
		s_Widget wd1(new Widget());
	}
}

void libc_routine()
{
	for(int i=0; i < OBJECTS_COUNT; i++) {
	    s_MyWidget wd0(new MyWidget());
		s_MyButton btn(new MyButton());
		s_MyPanel pnl(new MyPanel());
		s_MyWidget wd1(new MyWidget());
	}
}

typedef void (*routine_f)();
typedef double (*benchmark_f)(routine_f);

double multi_threads_benchmark(routine_f routine) {
	auto start = std::chrono::steady_clock::now();
	std::vector<std::thread> workers;
	for(int i=0; i < THREADS; i++) {
		workers.push_back( std::thread( std::bind( routine ) ) );
	}
	for (std::thread &t: workers) {
      if (t.joinable()) {
        t.join();
      }
    }
    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    return std::chrono::duration <double, std::milli>(diff).count();
}

double single_thread_benchmark(routine_f routine) {
	auto start = std::chrono::steady_clock::now();
    routine();
    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    return std::chrono::duration <double, std::milli> (diff).count();
}

double run_benchmarks(benchmark_f benchmark, routine_f routine) {
	double result = 0.0;
	for(int i=0; i < TESTS_COUNT; i++) {
		result += benchmark(routine);
	}
	return result;
}

void print_benchmarks_result(const char* type,double libc_total, double so_total) {
	std::cout<<"    Benchmarks results for "<< type << " threading" << std::endl;;
	std::cout << "\tLibC average time: "<<  libc_total / TESTS_COUNT << " ms" << std::endl;
	std::cout << "\tSmallobject average time: "<< so_total / TESTS_COUNT << " ms" << std::endl;
	if(so_total < libc_total) {
		double percent = 100.0 - ( (so_total/TESTS_COUNT) * 100.0) / (libc_total/TESTS_COUNT);
		std::cout<<"    Small object "<<percent<<"% more effective"<<std::endl;
	} else {
		double percent = 100.0 - ( (libc_total/TESTS_COUNT) * 100.0) / (so_total/TESTS_COUNT);
		std::cout<<"   LibC "<<percent<<"% more effective"<<std::endl;
	}
}

int main(int argc, const char** argv)
{
	std::cout<<"Banchmarks testing objects:"<<std::endl;
	std::cout<<"Small object  std new/delete"<<std::endl;
	std::cout<<"Widget: " << sizeof(Widget) <<"    MyWidget: " << sizeof(MyWidget) <<" bytes" << std::endl;
	std::cout<<"Button: " << sizeof(Button) <<"    MyButton: " << sizeof(MyButton) <<" bytes" << std::endl;
	std::cout<<"Panel:  " <<  sizeof(Panel) <<"    MyPanel : " << sizeof(MyPanel) <<" bytes" << std::endl << std::endl;

	// macke OS cache bofere testing
	// alloc and free 5 mb
	void *dummy = std::malloc(5242880);
	::std::free(dummy);

	// Single thread benchmark
	std::cout<<"Single threading benchmark"<<std::endl;
	double libc_total = run_benchmarks(single_thread_benchmark, libc_routine);
	double so_total = run_benchmarks(single_thread_benchmark, so_routine);
	print_benchmarks_result("single", libc_total, so_total);

	std::cout<<std::endl<<"Multi threading with " << THREADS << " threads benchmark"<<std::endl;
	libc_total = run_benchmarks(multi_threads_benchmark, libc_routine);
	so_total = run_benchmarks(multi_threads_benchmark, so_routine);
	print_benchmarks_result("multi", libc_total, so_total);

	return 0;
}
