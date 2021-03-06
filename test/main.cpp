#include <iostream>
#include <object.hpp>

#include <boost/noncopyable.hpp>
#include <thread>
#include <vector>
#include <chrono>

//#include <jemalloc.h>

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
	void throw_exception(std::exception const & e)
	{
		std::cerr<< e.what();
		std::exit(-1);
	}
} // namespace boost
#endif // BOOST_NO_EXCEPTIONS

struct rect {
	uint8_t l,t,r,b;
};

class MyObject:public boost::noncopyable {
protected:
	BOOST_CONSTEXPR MyObject() BOOST_NOEXCEPT_OR_NOTHROW:
		refCount_(0)
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
	{
		r_.l = r_.b + r_.t;
	}
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
	MyWidget **childs_;
};

class MyButton:public MyWidget {
public:
	MyButton() BOOST_NOEXCEPT_OR_NOTHROW:
		MyWidget()
	{}
private:
	rect border_;
	uint64_t color;
};

DECLARE_OBJECT_PTR_T(MyWidget);
DECLARE_OBJECT_PTR_T(MyButton);
DECLARE_OBJECT_PTR_T(MyPanel);

class Widget:public virtual smallobject::object {
public:
   Widget() BOOST_NOEXCEPT_OR_NOTHROW:
		smallobject::object()
   {
		r_.l = r_.b + r_.t;
   }
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
	Widget **childs_;
};

class Button:public Widget {
public:
	Button():
		Widget()
	{}
private:
	rect border_;
	uint64_t color;
};

DECLARE_OBJECT_PTR_T(Widget);
DECLARE_OBJECT_PTR_T(Panel);
DECLARE_OBJECT_PTR_T(Button);

static const size_t THREADS =  std::thread::hardware_concurrency();
static constexpr size_t TOTAL_TEST_SIZE = sizeof(Widget)+sizeof(Panel)+sizeof(Button)+sizeof(Widget);
static const size_t OBJECTS_COUNT = ((1 << 20) * 32)  / TOTAL_TEST_SIZE;
static const size_t TESTS_COUNT = 16;

template<class W, class B, class P>
void BOOST_NOINLINE initialize(W* w, B *b, P *p, W* w1) {
	w = new W();
	b = new B();
	p = new P();
	w1 = new W();
}


// avoid compiller to eleminate call to malloc
template<class W, class B, class P>
void BOOST_NOINLINE release(W* w, B *b, P *p, W* w1) {
	delete b;
	delete w;
	delete p;
	delete w1;
}

#if defined( __GNUC__) && ! defined( __clang__)
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif // __GNUC__

void BOOST_NOINLINE so_routine()
{
	Widget* w[2] = {nullptr,nullptr};
	Button* b = nullptr;
	Panel*  p = nullptr;
	for(size_t i=0; i < OBJECTS_COUNT; i++) {
		initialize(w[0],b,p,w[1]);
		release(w[0],b,p,w[1]);
	}
}

void BOOST_NOINLINE libc_routine()
{
	MyWidget *w[2] = {nullptr, nullptr};
	MyButton* b = nullptr;
	MyPanel*  p = nullptr;
	// normal flow
	for(size_t i=0; i < OBJECTS_COUNT; i++) {
		initialize(w[0],b,p,w[1]);
		release(w[0],b,p,w[1]);
	}
}

#if defined( __GNUC__) && ! defined( __clang__)
#pragma GCC pop_options
#endif // __GNUC__

typedef void (*routine_f)();
typedef double (*benchmark_f)(routine_f);

double multi_threads_benchmark(routine_f routine) {
	auto start = std::chrono::steady_clock::now();
	std::vector<std::thread> workers(THREADS);
	for(size_t i=0; i < THREADS; i++) {
		workers.push_back( std::thread( std::bind( routine ) ) );
	}
	for (std::thread &t: workers) {
      if (t.joinable()) {
        t.join();
      }
    }
    auto diff = std::chrono::steady_clock::now() - start;
    return std::chrono::duration <double, std::milli>(diff).count();
}

double single_thread_benchmark(routine_f routine) {
	auto start = std::chrono::steady_clock::now();
    routine();
    auto diff = std::chrono::steady_clock::now() - start;
    return std::chrono::duration <double, std::milli> (diff).count();
}

double run_benchmarks(benchmark_f benchmark, routine_f routine) {
	double result = 0.0;
	for(size_t i=0; i < TESTS_COUNT; i++) {
		result += benchmark(routine);
	}
	return result;
}

void print_benchmarks_result(const char* type,double libc_total, double so_total) {
	std::cout<<"    Benchmarks results for "<< type << " threading" << std::endl;
	double av_libc = libc_total / TESTS_COUNT;
	double av_so = so_total / TESTS_COUNT;
	std::cout << "\tLibC average time: "<<  libc_total / TESTS_COUNT << " ms" << std::endl;
	std::cout << "\tSmallobject average time: "<< so_total / TESTS_COUNT << " ms" << std::endl;
	if(av_so < av_libc) {
		double percent =  100 -  (av_so * 100  / av_libc);
		std::cout<<"    Small object "<<percent<<"% more effective"<<std::endl;
	} else {
		double percent = 100 - (av_libc * 100 / av_so);
		std::cout<<"   LibC "<<percent<<"% more effective"<<std::endl;
	}
}

static constexpr std::size_t CACHE_BLOCK = std::size_t(1 << 30);

void BOOST_NOINLINE mem_set_noignore_cache(void * block) {
	std::memset(block, 0x9A, CACHE_BLOCK);
}

void heating_up() {
	std::cout<< "Heating up OS pages" << std::endl;
	void * block = std::malloc( CACHE_BLOCK );
	mem_set_noignore_cache( block );
	std::free(block);
}


int main(int argc, const char** argv)
{
	// mem cache
	std::cout<<"Banchmarks testing objects:"<<std::endl;
	std::cout<<"Small object  std new/delete"<<std::endl;
	std::cout<<"Widget: " << sizeof(Widget) <<"    MyWidget: " << sizeof(MyWidget) <<" bytes" << std::endl;
	std::cout<<"Button: " << sizeof(Button) <<"    MyButton: " << sizeof(MyButton) <<" bytes" << std::endl;
	std::cout<<"Panel:  " <<  sizeof(Panel) <<"    MyPanel : " << sizeof(MyPanel) <<" bytes" << std::endl << std::endl;

	std::cout<< "Objects count: " << OBJECTS_COUNT << std::endl;

	// Single thread benchmark
	std::cout<<"Single threading benchmark"<<std::endl;

	heating_up();
	std::cout<<"Running LibC benchmark"<<std::endl;
	double libc_total = run_benchmarks(single_thread_benchmark, libc_routine);

	heating_up();
	std::cout<<"Running SO benchmark"<<std::endl;
	double so_total = run_benchmarks(single_thread_benchmark, so_routine);

	print_benchmarks_result("single", libc_total, so_total);

	std::cout<<std::endl<<"Multi threading with " << THREADS << " threads benchmark"<<std::endl;


	heating_up();
	std::cout<<"Running LibC benchmark"<<std::endl;
	libc_total = run_benchmarks(multi_threads_benchmark, libc_routine);

	heating_up();
	std::cout<<"Running SO benchmark"<<std::endl;
	so_total = run_benchmarks(multi_threads_benchmark, so_routine);

	print_benchmarks_result("multi", libc_total, so_total);
	return 0;
}
