#ifndef __BOOST_SMALLOBJECT_FORWARD_LIST__
#define __BOOST_SMALLOBJECT_FORWARD_LIST__

#include <iterator>

#include <boost/atomic.hpp>
#include <boost/move/move.hpp>
// for node memory allocation
#include <boost/pool/pool.hpp>
#include <boost/thread/locks.hpp>
#include "critical_section.hpp"

namespace boost {

namespace lockfree {

namespace detail {

using namespace boost::atomics;

template<typename E>
class forward_list_node {
// not movable and not copyable
BOOST_MOVABLE_BUT_NOT_COPYABLE(forward_list_node)
typedef forward_list_node<E> _self;
public:
	typedef E value_type;
	explicit forward_list_node() BOOST_NOEXCEPT_OR_NOTHROW:
		element_(NULL),
		next_(NULL)
	{}
	forward_list_node(BOOST_RV_REF(E) e):
		element_(new E() ),
		next_(NULL)
	{
		*element_ = BOOST_MOVE_BASE(E,e);
	}
	forward_list_node(const E& e):
		element_(new E()),
		next_(NULL)
	{
		*element = e;
	}
	~forward_list_node() BOOST_NOEXCEPT_OR_NOTHROW
	{
		delete element_;
	}
	inline _self* next() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return next_.load(boost::memory_order_relaxed);
	}
	void set_next(_self* next) BOOST_NOEXCEPT_OR_NOTHROW {
		next_.store(next, boost::memory_order_release);
	}
	bool exchange_next(_self* next) BOOST_NOEXCEPT_OR_NOTHROW {
		_self* old = next_.load(boost::memory_order_relaxed);
		 return next_.compare_exchange_weak(old, next, boost::memory_order_release, boost::memory_order_relaxed);
	}
	inline E& element() const BOOST_NOEXCEPT_OR_NOTHROW {
		return *element_;
	}
private:
	mutable E* element_;
	atomic< forward_list_node<E>* > next_;
};

template<class node_type>
class forward_list_iterator {
private:
	typedef forward_list_iterator<node_type> _self;
public:
	typedef typename node_type::value_type   value_type;
	typedef const value_type&				   reference;
	typedef ptrdiff_t				     difference_type;
	typedef std::forward_iterator_tag  iterator_category;

	forward_list_iterator(node_type* node) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(node)
	{}

	forward_list_iterator() BOOST_NOEXCEPT_OR_NOTHROW:
		node_(NULL)
	{}

	~forward_list_iterator() BOOST_NOEXCEPT_OR_NOTHROW
	{}

	forward_list_iterator(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(rhs.node_)
	{}

	_self& operator=(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = rhs.node_;
		return *this;
	}

	inline reference operator*() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_->element();
	}

	_self& operator++() BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = node_->next();
		return *this;
	}

	_self operator++(int) BOOST_NOEXCEPT_OR_NOTHROW {
		_self tmp( *this );
		node_ = node_->next();
		return tmp;
	}

	bool operator==(const _self& rhs) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_ == rhs.node_;
	}
	bool operator!=(const _self& rhs) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_ != rhs.node_;
	}
	_self next() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node ? _self( node_->next() ) : _self(NULL);
	}

	inline node_type* node() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_cast<const node_type*>(node_);
	}

private:
	node_type* node_;
};

template<class node_type>
class forward_list_const_iterator {
public:
	typedef forward_list_const_iterator<node_type> _self;
	typedef forward_list_iterator<node_type>  iterator;
	typedef typename node_type::value_type   value_type;
	typedef const value_type&                reference;
	typedef ptrdiff_t                        difference_type;
	typedef std::forward_iterator_tag       iterator_category;

	explicit forward_list_const_iterator(const node_type* n)  BOOST_NOEXCEPT_OR_NOTHROW:
		node_( const_cast<node_type*>(n) )
	{}

	forward_list_const_iterator() BOOST_NOEXCEPT_OR_NOTHROW:
		node_(NULL)
	{}

	forward_list_const_iterator(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(rhs.node_)
	{}

	forward_list_const_iterator(const iterator& it) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(it.node())
	{}

	_self& operator=(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW
	{
		node_ = rhs.node_;
		return *this;
	}

	_self& operator=(const iterator& it) BOOST_NOEXCEPT_OR_NOTHROW
	{
		node_ = it.node();
		return *this;
	}

	reference operator*() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_->element();
	}

	_self& operator++() BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = node_->next();
		return *this;
	}

	_self operator++(int) BOOST_NOEXCEPT_OR_NOTHROW {
		_self tmp(*this);
		node_ = node_->next();
		return tmp;
	}

	bool operator==(const _self& rhs) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_ == rhs.node_;
	}

	bool operator!=(const _self& rhs) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_ != rhs.node_;
	}

	_self next() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return  node_ ? _self( node_->next() ): _self(NULL);
	}

	inline node_type* node() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return node_;
	}

private:
	node_type* node_;
};

/**
*  @brief  Forward list iterator equality comparison.
*/
template<class N>
inline bool operator==(
        const forward_list_iterator<N>& lsh,
        const forward_list_const_iterator<N>& rhs)
{
	return lsh.node() == rhs.node();
}

/**
*  @brief  Forward list iterator equality comparison.
*/
template<class N>
inline bool operator==(
        const forward_list_const_iterator<N>& lsh,
        const forward_list_iterator<N>& rhs)
{
	return lsh.node() == rhs.node();
}

} // namespace detail

template<typename E>
class forward_list {
	BOOST_MOVABLE_BUT_NOT_COPYABLE(forward_list)
private:
	typedef detail::forward_list_node<E> node_type;
	typedef boost::pool<boost::default_user_allocator_new_delete> node_allocator;
public:
	typedef detail::forward_list_const_iterator<node_type> const_iterator;
	typedef detail::forward_list_iterator<node_type> iterator;

	forward_list():
		head_(NULL),
		node_allocator_( sizeof(node_type) )
	{
		void *ptr = node_allocator_.malloc();
		head_ = new (ptr) node_type();
	}

	~forward_list() BOOST_NOEXCEPT_OR_NOTHROW
	{
		node_type *it = const_cast<node_type*>(head_);
		node_type* tmp;
		while(NULL != it)
		{
			tmp = it->next();
			it->~node_type();
			node_allocator_.free(it);
			it = tmp;
		}
	}

	inline bool empty() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return head_->next() == NULL;
	}

	iterator insert_after(const_iterator position, BOOST_RV_REF(E) e)
	{
		node_type* new_next = create_node(boost::forward<E>(e));
		node_type *pos = position.node();
		node_type *old_next;
		do {
			old_next = pos->next();
			new_next->set_next(old_next);
		} while( !pos->exchange_next(new_next) );
		return iterator( new_next );
	}

	inline iterator insert_after(const_iterator position, const E& e)
	{
		insert_after(position, BOOST_MOVE_BASE(E,e) );
	}

	inline iterator insert_after(const_iterator position, std::size_t n, const E& val)
	{
		for(std::size_t i=0; i < n; i++) {
			insert_after( position, val);
			++position;
		}
		return iterator( position.node() );
	}

	template <class InputIterator>
	iterator insert_after(const_iterator position, InputIterator first, InputIterator last )
	{
		InputIterator src = last;
		while(src != last) {
			insert_after(position, *src );
			++src;
			++position;
		}
		return iterator( position.node() );
	}

	iterator pop_after(const_iterator position) {
		node_type *new_next = NULL;
		node_type *pos = position.node();
		node_type *to_remove = pos->next();
		if(NULL != to_remove) {
			do {
				new_next = to_remove->next();
			} while(! pos->exchange_next(new_next) );
			release_node(to_remove);
		}
        return iterator(new_next);
	}

	E& front() BOOST_NOEXCEPT_OR_NOTHROW {
		return head_->next()->element();
	}

	const E& front() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return head_->next()->element();
	}

	inline void push_front(BOOST_RV_REF(E) e)
	{
		node_type* new_next = create_node(boost::forward<E>(e));
		node_type *pos = head_->next();
		node_type *mhead = const_cast<node_type*>(head_);
		node_type *old_next;
		do {
			old_next = head_->next();
			new_next->set_next(old_next);
		} while( !mhead->exchange_next(new_next) );
	}

	inline void push_front(const E& e)
	{
		push_front( BOOST_MOVE_BASE(E, e) );
	}

	void pop_front(E& e) {
		node_type *frnt = head_->next();
		if( NULL != frnt ) {
			e = BOOST_MOVE_BASE(E, frnt->element() );
		}
		pop_after( before_begin() );
	}

	iterator before_begin() BOOST_NOEXCEPT_OR_NOTHROW {
		return iterator( const_cast<node_type*>(head_) );
	}

	iterator begin() BOOST_NOEXCEPT_OR_NOTHROW {
		return iterator( head_->next() );
	}

	iterator end() BOOST_NOEXCEPT_OR_NOTHROW {
		return iterator( NULL );
	}

	const_iterator cbegin() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_iterator( head_->next() );
	}

	const_iterator cbefore_begin() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_iterator(head_);
	}

	const_iterator cend() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_iterator(NULL);
	}

	inline std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		const_iterator it = cbegin();
		const_iterator end = cend();
		std::size_t result = 0;
		while(it != end) {
			++it;
			++result;
		}
		return result;
	}

private:
	inline node_type *create_node(BOOST_RV_REF(E) e) {
		boost::unique_lock<boost::smallobject::sys::critical_section> lock(mmtx_);
		return new ( node_allocator_.malloc() ) node_type(boost::forward<E>(e));
	}
	inline node_type *create_node(const E& e) {
		boost::unique_lock<boost::smallobject::sys::critical_section> lock(mmtx_);
		return new ( node_allocator_.malloc() ) node_type(e);
	}
	inline void release_node(node_type* const nd) {
		nd->~node_type();
		boost::unique_lock<boost::smallobject::sys::critical_section> lock(mmtx_);
		node_allocator_.free( nd );
	}
private:
	const node_type* head_;
	boost::smallobject::sys::critical_section mmtx_;
	node_allocator node_allocator_;
};

} // namespace lockfree

} // namespace io

#endif // __BOOST_SMALLOBJECT_FORWARD_LIST__
