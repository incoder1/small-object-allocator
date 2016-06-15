#ifndef __BOOST_SMALLOBJECT_LOCKFREELIST__
#define __BOOST_SMALLOBJECT_LOCKFREELIST__

#include <iterator>

#include <boost/atomic.hpp>
#include <boost/move/move.hpp>
#include <boost/thread/thread_only.hpp>

#ifndef _SOBJ_SPINCOUNT
#	define	_SOBJ_SPINCOUNT 4000
#endif

namespace smallobject {

namespace detail {

template<typename E>
class list_node {
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      list_node( const list_node& ) = delete;
      list_node& operator=( const list_node& ) = delete;
#else
      list_node( const list_node& );
      list_node& operator=( const list_node& );
#endif // no deleted functions
private:
typedef list_node<E> _self;
public:
	typedef E value_type;

	list_node(BOOST_FWD_REF(E) e):
		element_( BOOST_MOVE_BASE(E,e) ),
		prev_(NULL),
		next_(NULL)
	{}

	~list_node()
	{}

	inline _self* next() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return next_.load(boost::memory_order_seq_cst);
	}

	inline bool exchange_next(_self* next) BOOST_NOEXCEPT_OR_NOTHROW {
		return next_.exchange(next, boost::memory_order_acq_rel);
	}

	inline _self* prev() const BOOST_NOEXCEPT_OR_NOTHROW {
		return prev_.load(boost::memory_order_seq_cst);
	}

	inline bool exchange_prev(_self* prev) BOOST_NOEXCEPT_OR_NOTHROW {
		return prev_.exchange(prev, boost::memory_order_acq_rel);
	}

	inline const E* element() const BOOST_NOEXCEPT_OR_NOTHROW {
		return &element_;
	}
private:
	const E element_;
	boost::atomic< _self* > prev_;
	boost::atomic< _self* > next_;
};

template<class node_type>
class list_iterator {
private:
	typedef list_iterator<node_type> _self;
public:
	typedef typename node_type::value_type value_type;
	typedef const value_type& reference;
	typedef ptrdiff_t difference_type;
	typedef std::forward_iterator_tag iterator_category;

	list_iterator(node_type* const node) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(node)
	{}

	list_iterator() BOOST_NOEXCEPT_OR_NOTHROW:
		node_(NULL)
	{}

	~list_iterator() BOOST_NOEXCEPT_OR_NOTHROW
	{}

	list_iterator(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(rhs.node_)
	{}

	_self& operator=(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = rhs.node_;
		return *this;
	}

	inline reference operator*() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return *( node_->element() );
	}

	_self& operator++() BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = (NULL != node_) ? node_->next() : NULL;
		return *this;
	}

	_self operator++(int) BOOST_NOEXCEPT_OR_NOTHROW {
		_self tmp( *this );
		node_ = (NULL != node_) ? node_->next() : NULL;
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

	inline const node_type* node() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_cast<const node_type*>(node_);
	}

private:
	node_type* node_;
};

template<class node_type>
class list_const_iterator {
public:
	typedef list_const_iterator<node_type> _self;
	typedef list_iterator<node_type>  iterator;
	typedef typename node_type::value_type   value_type;
	typedef const value_type&                reference;
	typedef ptrdiff_t                        difference_type;
	typedef std::forward_iterator_tag iterator_category;

	explicit list_const_iterator(const node_type* n)  BOOST_NOEXCEPT_OR_NOTHROW:
		node_( const_cast<node_type*>(n) )
	{}

	list_const_iterator() BOOST_NOEXCEPT_OR_NOTHROW:
		node_(NULL)
	{}

	list_const_iterator(const _self& rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(rhs.node_)
	{}

	list_const_iterator(const iterator& it) BOOST_NOEXCEPT_OR_NOTHROW:
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
		return *(node_->element());
	}

	_self& operator++() BOOST_NOEXCEPT_OR_NOTHROW {
		node_ = (NULL != node_) ? node_->next() : NULL;
		return *this;
	}

	_self operator++(int) BOOST_NOEXCEPT_OR_NOTHROW {
		_self tmp(*this);
		node_ = (NULL != node_) ? node_->next() : NULL;
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
		return  NULL != node_ ? _self( node_->next() ): _self(NULL);
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
        const list_iterator<N>& lsh,
        const list_const_iterator<N>& rhs)
{
	return lsh.node() == rhs.node();
}

/**
*  @brief  Forward list iterator equality comparison.
*/
template<class N>
inline bool operator==(
        const list_const_iterator<N>& lsh,
        const list_iterator<N>& rhs)
{
	return lsh.node() == rhs.node();
}

} // namespace detail

/// \brief Double linked non blocking thread safe forward list
/// with erase in the midle support
template<typename E, class A = sys::allocator<E> >
class list {
private:
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      list( const list& ) = delete;
      list& operator=( const list& ) = delete;
#	ifdef BOOST_HAS_RVALUE_REFS
      list(list&& ) = delete;
      list& operator=(list&& ) = delete;
#	endif // BOOST_HAS_RVALUE_REFS
#else
      list( const list& );
      list& operator=( const list& );
#endif // no deleted functions
	typedef detail::list_node<E> node_type;
	typedef typename A::template rebind<node_type>::other node_allocator;
public:
	typedef A allocator_type;
	typedef detail::list_const_iterator<node_type> const_iterator;
	typedef detail::list_iterator<node_type> iterator;

	BOOST_CONSTEXPR list():
		head_(NULL),
		node_allocator_()
	{}

	~list() BOOST_NOEXCEPT_OR_NOTHROW
	{
		do_destroy_list();
	}

	/// Returns whether current list is empty
	/// \return \b true if list is empty otherwise false
	inline bool empty() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return NULL == head_.load(boost::memory_order_seq_cst);
	}

	/// Erase element by it constant iterator
	/// \param position constant iterator on element
	inline void erase(const const_iterator& position) {
		if(end() == position) return;
		do_erase_node( position.node() );
	}

	/// Erace element by it iterator
	/// \param position iterator on element
	inline void erase(const iterator& position) {
		if(end() == position) return;
		do_erase_node( position.node() );
	}

	/// Push element in the head of the list
	/// \param element copy-assign reference on element
	void push_front(BOOST_COPY_ASSIGN_REF(E) element) {
		 push_front( BOOST_MOVE_BASE(E,element) );
	}

	/// Push element in the head of the list
	/// \param element move reference on element
	void push_front(BOOST_FWD_REF(E) element) {
		node_type *new_node = create_node( boost::forward<E>(element) );
		node_type *old_head = NULL;
		std::size_t spin_count = 0;
		for(;;){
			old_head = head_.load(boost::memory_order_consume);
			if(NULL != old_head) {
				old_head->exchange_prev(new_node);
			}
			new_node->exchange_next(old_head);
			if(head_.compare_exchange_weak(old_head, new_node, boost::memory_order_release, boost::memory_order_relaxed) )
			{
				break;
			}
			do_await(spin_count);
		};
	}

	/// Returns iterator on the head of this list
	/// \return iterator on this list head element
	/// \throw never throws
	iterator begin() BOOST_NOEXCEPT_OR_NOTHROW {
		return iterator( head_.load(boost::memory_order_seq_cst) );
	}

	/// Returns iterator on the tail of this list (after last element)
	/// \return iterator on this list tail
	/// \throw never throws
	iterator end() BOOST_NOEXCEPT_OR_NOTHROW {
		return iterator( NULL );
	}

	/// Returns constant iterator on the head of this list
	/// \return constant iterator on this list head element
	/// \throw never throws
	const_iterator cbegin() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_iterator( head_.load(boost::memory_order_seq_cst) );
	}

	/// Returns constant iterator on the tail of this list (after last element)
	/// \return constant iterator on this list tail
	/// \throw never throws
	const_iterator cend() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return const_iterator(NULL);
	}

	/// Returns this list size in elements count
	/// \return this list elements count
	/// \throw never throws
	inline std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return std::distance( cbegin(), cend() );
	}

private:
	void do_insert_node(node_type* prev, node_type * const new_node)
	{
		new_node->exchange_prev(prev);
		std::size_t spin_count = 0;
		while( !prev->exchange_next(new_node) ) {
			do_await(spin_count);
		}
	}
	void do_erase_node(node_type * node) {
		node_type *old_prev = node->prev();
		if(NULL != old_prev) {
			// exchange prev first
			std::size_t spin_count = 0;
			while( !node->exchange_prev(old_prev) ) {
				do_await(spin_count);
				old_prev = node->prev();
			}
			// link next with new prev
			do_insert_node(old_prev, node->next() );
		}
		destroy_node(node);
	}

	// a spinlock await
	inline void do_await(std::size_t& spin_count) {
		if(++spin_count > _SOBJ_SPINCOUNT) {
			boost::this_thread::yield();
			spin_count = 0;
		}
	}

	void do_destroy_list() {
		node_type *it = head_.load(boost::memory_order_acquire);
		head_.store(NULL, boost::memory_order_release);

		node_type *tmp;
		while(NULL != it)
		{
			tmp = it;
			it = it->next();
			destroy_node(tmp);
		}
	}

	inline node_type *create_node(BOOST_FWD_REF(E) e) {
		node_type* result = node_allocator_.allocate(1);
		result = new ( static_cast<void*>(result) ) node_type( boost::forward<E>(e) );
		return result;
	}

	inline void destroy_node(node_type* const nd) {
		node_allocator_.destroy(nd);
		node_allocator_.deallocate(nd, 1);
	}
private:
	boost::atomic<node_type*> head_;
	node_allocator node_allocator_;
};

} // namespace smallobject

#endif // __BOOST_SMALLOBJECT_FORWARD_LIST__
