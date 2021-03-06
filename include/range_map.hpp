#ifndef __SMALL_OBJECT_RANGE_MAP_HPP_INCLUDED__
#define __SMALL_OBJECT_RANGE_MAP_HPP_INCLUDED__

#include <boost/config.hpp>

#if __cplusplus <= 201103L
#include <boost/cstdint.hpp>
#endif // __cplusplus

#ifndef NULL
#	define NULL reinterpret_cast<void*>(0)
#endif // NULL

#include <boost/move/move.hpp>

#include <functional>
#include <iterator>

#include "sys_allocator.hpp"
#include "rw_barrier.hpp"

namespace smallobject {

/// \brief the range of keys from min to max values
/// \param K key type
/// \param C key comparator, std::less<K> is default
template<typename K, class C = std::less<const K> >
class range {
BOOST_COPYABLE_AND_MOVABLE(range)
public:

	typedef C comparator_type;



#ifndef NDEBUG
	range(const K& min,const K& max) BOOST_NOEXCEPT_OR_NOTHROW:
		min_(min),
		max_(max)
	{
		comparator_type cmp;
		assert( cmp( min_ , max_ ) );
	}

	range(BOOST_RV_REF(K) min,BOOST_RV_REF(K) max) BOOST_NOEXCEPT:
		min_( boost::forward<K>(min) ),
		max_( boost::forward<K>(max) )
	{
		comparator_type cmp;
		assert( cmp( min_ , max_ ) );
	}
#else
	BOOST_CONSTEXPR range(const K& min,const K& max) BOOST_NOEXCEPT_OR_NOTHROW:
		min_(min),
		max_(max)
	{}
	BOOST_CONSTEXPR range(BOOST_RV_REF(K) min,BOOST_RV_REF(K) max) BOOST_NOEXCEPT:
		min_( boost::forward<K>(min) ),
		max_( boost::forward<K>(max) )
	{}
#endif // NDEBUG


	BOOST_CONSTEXPR range(BOOST_COPY_ASSIGN_REF(range) rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		min_(rhs.min_),
		max_(rhs.max_)
	{}

	inline range& operator=(BOOST_COPY_ASSIGN_REF(range) rhs) BOOST_NOEXCEPT_OR_NOTHROW
	{
		range( (rhs) ).swap( *this );
		return *this;
	}

	BOOST_CONSTEXPR range(BOOST_RV_REF(range) rhs) BOOST_NOEXCEPT:
		min_( BOOST_MOVE_BASE(K,rhs.min_) ),
		max_( BOOST_MOVE_BASE(K,rhs.max_) )
	{}

	inline range& operator=(BOOST_RV_REF(range) rhs)
	{
		range( boost::forward<range>(rhs) ).swap( *this );
		return *this;
	}

	void swap(range& other) BOOST_NOEXCEPT_OR_NOTHROW {
		std::swap(min_,other.min_);
		std::swap(max_,other.max_);
	}


	BOOST_FORCEINLINE const K& min() const {
		return min_;
	}

	BOOST_FORCEINLINE const K& max() const {
		return max_;
	}

	BOOST_FORCEINLINE int8_t compare_to_key(const K& key) const
	{
		comparator_type cmp;
		int8_t result = cmp( key, min_) ? -1 : 0;
		if(!result) result = cmp( max_, key) ? 1 : 0;
		return result;
	}

	BOOST_FORCEINLINE int8_t compare_to(const range& rhs) const {
		comparator_type cmp;
		int8_t result = ( cmp(rhs.min_,min_) && cmp(rhs.max_,max_) ) ? 1 : 0;
		if(0 == result) {
			result = ( cmp(max_,rhs.max_) && cmp(min_,rhs.min_) ) ? -1 : 0;
		}
		return result;
	}
private:
	K min_;
	K max_;
};

template<typename F, typename S>
struct movable_pair
{
BOOST_COPYABLE_AND_MOVABLE(movable_pair)
public:

	typedef F first_type;
	typedef S second_type;

	BOOST_CONSTEXPR movable_pair(BOOST_COPY_ASSIGN_REF(first_type) __f, BOOST_COPY_ASSIGN_REF(second_type) __s) BOOST_NOEXCEPT_OR_NOTHROW:
		first(__f),
		second(__s)
	{}

	BOOST_CONSTEXPR movable_pair(BOOST_RV_REF(first_type) __f, BOOST_RV_REF(second_type) __s) BOOST_NOEXCEPT:
       first( boost::forward<first_type>(__f) ),
       second( boost::forward<second_type>(__s) )
	{}

	BOOST_CONSTEXPR movable_pair( BOOST_COPY_ASSIGN_REF(first_type) __f, BOOST_RV_REF(second_type) __s) BOOST_NOEXCEPT_OR_NOTHROW:
       first(__f),
       second( boost::forward<second_type>(__s) )
	{}

	BOOST_CONSTEXPR movable_pair(BOOST_RV_REF(first_type) __f, BOOST_COPY_ASSIGN_REF(second_type) __s) BOOST_NOEXCEPT_OR_NOTHROW:
       first( boost::forward<first_type>(__f) ),
       second(__s)
	{}

	BOOST_CONSTEXPR movable_pair( BOOST_RV_REF(movable_pair) rhs) BOOST_NOEXCEPT_OR_NOTHROW:
	   first( BOOST_MOVE_BASE(first_type,rhs.first) ),
       second( BOOST_MOVE_BASE(second_type,rhs.second) )
	{}

	movable_pair& operator=( BOOST_RV_REF(movable_pair) rhs) BOOST_NOEXCEPT_OR_NOTHROW
	{
		movable_pair( boost::forward<movable_pair>(rhs) ).swap( *this );
		return *this;
	}

	BOOST_CONSTEXPR movable_pair(BOOST_COPY_ASSIGN_REF(movable_pair) rhs) BOOST_NOEXCEPT_OR_NOTHROW:
		first(rhs.first),
		second(rhs.second)
	{}

	movable_pair& operator=(BOOST_COPY_ASSIGN_REF(movable_pair) rhs) BOOST_NOEXCEPT_OR_NOTHROW
	{
		movable_pair( rhs ).swap( *this );
		return *this;
	}

	inline void swap(movable_pair& rhs) BOOST_NOEXCEPT_OR_NOTHROW
	{
		std::swap(first, rhs.first);
		std::swap(second, rhs.second);
	}

	first_type first;
	second_type second;
};

namespace detail {

template<typename R, typename V>
class avl_tree_node {
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      avl_tree_node( const avl_tree_node& ) = delete;
      avl_tree_node& operator=( const avl_tree_node& ) = delete;
#else
      avl_tree_node( const avl_tree_node& );
      avl_tree_node& operator=( const avl_tree_node& );
#endif // no deleted functions
public:
	typedef avl_tree_node<R,V> self_type;
	typedef movable_pair<const R, V> value_type;
	typedef value_type* base_ptr;

	explicit BOOST_CONSTEXPR avl_tree_node(BOOST_FWD_REF(value_type) v):
		top_(NULL),
		left_(NULL),
		right_(NULL),
		height_(0),
		value_( BOOST_MOVE_BASE(value_type, v) )
	{}

	avl_tree_node(BOOST_FWD_REF(avl_tree_node) other) BOOST_NOEXCEPT:
		top_(other.top_),
		left_(other.left_),
		right_(other.right_),
		height_(other.height_),
		value_( BOOST_MOVE_BASE(value_type, other.value_) )
	{
		other.top_ = NULL;
		other.left_ = NULL;
		other.right_ = NULL;
		other.height_ = 0;
	}

	avl_tree_node& operator=(BOOST_FWD_REF(avl_tree_node) rhs) BOOST_NOEXCEPT
	{
		avl_tree_node( boost::forward<avl_tree_node>(rhs) ).swap( *this );
		return *this;
	}

	inline void swap(avl_tree_node& other) BOOST_NOEXCEPT_OR_NOTHROW {
		std::swap(top_, other.top_ );
		std::swap(left_, other.left_);
		std::swap(right_, other.right_);
		std::swap(height_, other.height_);
	 	value_.swap( other.value_ );
	}

	BOOST_FORCEINLINE const value_type* value() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return &value_;
	}

	BOOST_FORCEINLINE self_type* top() const BOOST_NOEXCEPT_OR_NOTHROW {
		return top_;
	}

	BOOST_FORCEINLINE self_type* left() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return left_;
	}

	BOOST_FORCEINLINE void set_left(self_type* const new_left) BOOST_NOEXCEPT_OR_NOTHROW
	{
		left_ = new_left;
		if(new_left)
			new_left->top_ = const_cast<self_type*>(this);
	}

	BOOST_FORCEINLINE self_type* right() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return right_;
	}

	inline void set_right(self_type* const new_right) BOOST_NOEXCEPT_OR_NOTHROW
	{
		right_ = new_right;
		if(new_right)
			new_right->top_ = const_cast<self_type*>(this);
	}

	BOOST_FORCEINLINE void make_root() BOOST_NOEXCEPT_OR_NOTHROW {
		top_ = NULL;
	}

	static self_type* balance(self_type * const p) BOOST_NOEXCEPT_OR_NOTHROW
	{
		p->fix_height();
		switch( b_factor(p) ) {
		case 2: {
					if ( b_factor( p->right() ) < 0 ) {
						p->set_right( rotate_right( p->right() ) );
					}
					return rotate_left(p);
				}
		case -2: {
					if ( b_factor( p->left() ) > 0 ) {
						p->set_left( rotate_left( p->left() ) );
					}
					return rotate_right(p);
				 }
		}
		return p;
	}

	static self_type*  inc_node(self_type* const from) BOOST_NOEXCEPT_OR_NOTHROW {
		self_type *result = from;
		if (from->right_ != NULL) {
			result = result->right_;
			while (NULL != result->left_ ) {
				result = result->left_;
			}
		} else {
			result = from->top_;
			if(NULL != result) {
				if(result->right_ == from) {
					if(NULL == result->top_) {
						result = NULL;
					} else if(result == result->top_->left_ ) {
						result = result->top_;
					} else {
						result = NULL;
					}
				}
			}
		}
		return result;
	}

private:

	static inline int8_t height(self_type* const node) BOOST_NOEXCEPT_OR_NOTHROW
	{
		return (NULL != node) ? node->height_ : 0;
	}

	static inline self_type* rotate_right(self_type* const node) BOOST_NOEXCEPT_OR_NOTHROW
	{
		self_type* result = node->right_;
		node->set_right(result->left_);
		result->set_left(node);
		node->fix_height();
		result->fix_height();
		return result;
	}

	static inline self_type* rotate_left(self_type* const node) BOOST_NOEXCEPT_OR_NOTHROW
	{
		avl_tree_node* result = node->left_;
		node->set_left(result->right_);
		result->set_right(node);
		node->fix_height();
		result->fix_height();
		return result;
	}

	static inline int8_t b_factor(self_type* const node) BOOST_NOEXCEPT_OR_NOTHROW
	{
		return (NULL != node) ? node->factor() : 0;
	}

	inline void fix_height() BOOST_NOEXCEPT_OR_NOTHROW
	{
		int8_t hl = height(left_);
		int8_t hr = height(right_);
		height_ =  ( (hl > hr ? hl : hr) + 1);
	}

	inline int8_t factor() const BOOST_NOEXCEPT_OR_NOTHROW
	{
		return height(left_) - height(right_);
	}

private:
	self_type* top_;
	self_type* left_;
	self_type* right_;
	int8_t height_;
	const value_type value_;
};


template<class _node_type >
class avl_tree_iterator
{
private:
	typedef avl_tree_iterator<_node_type> self_type;
	typedef _node_type link_type;
	typedef typename link_type::base_ptr base_ptr;

	template <class _iterator_type>
	friend struct get_it_avl_node;

public:
	typedef typename link_type::value_type value_type;
	typedef value_type& reference;
	typedef value_type* pointer;
	typedef std::forward_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;

	explicit avl_tree_iterator(link_type* const node) BOOST_NOEXCEPT_OR_NOTHROW:
		node_(node)
	{}

	inline reference operator*() const BOOST_NOEXCEPT_OR_NOTHROW {
		return *(node_->value());
	}

	inline pointer operator->() const BOOST_NOEXCEPT_OR_NOTHROW {
		return const_cast<value_type*>( node_->value() );
	}

	self_type& operator++() {
		if(NULL != node_) {
			node_ = link_type::inc_node(node_);
		}
		return *this;
	}

	inline bool operator != (const self_type& rhs) const {
		return node_ != rhs.node_;
	}

	inline bool operator == (const self_type& rhs) const {
		return node_ == rhs.node_;
	}

private:
	link_type * node_;
};

template <class _iterator_type>
struct get_it_avl_node
{
	typedef typename _iterator_type::link_type node_type;
	BOOST_CONSTEXPR get_it_avl_node()
	{}
	BOOST_FORCEINLINE node_type* operator()(const _iterator_type& it) {
		return it.node_;
	}
};

} // namespace detail

template<typename K, typename V, class C, class A>
class basic_range_map {
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      basic_range_map( const basic_range_map& ) = delete;
      basic_range_map& operator=( const basic_range_map& ) = delete;
#else
      basic_range_map( const basic_range_map& );
      basic_range_map& operator=( const basic_range_map& );
#endif // no deleted functions
public:
	typedef K key_type;
	typedef C key_comparator_type;
	typedef range<key_type, key_comparator_type> range_type;
	typedef V mapped_type;
	typedef A allocator_type;
private:
	typedef typename detail::avl_tree_node<range_type, mapped_type> _node_t;
	typedef typename allocator_type::template rebind<_node_t>::other _node_allocator;
public:
	typedef typename _node_t::value_type value_type;
	typedef detail::avl_tree_iterator<_node_t> iterator;
protected:
	BOOST_CONSTEXPR basic_range_map() BOOST_NOEXCEPT_OR_NOTHROW:
		root_(NULL)
	{}
public:

	inline bool insert(BOOST_FWD_REF(value_type) v)
	{
		return insert_node( boost::forward<value_type>(v) );
	}

	inline bool insert(BOOST_COPY_ASSIGN_REF(value_type) v)
	{
		return insert( BOOST_MOVE_BASE(value_type,v) );
	}

	inline bool insert(BOOST_FWD_REF(key_type) min, BOOST_FWD_REF(key_type) max, BOOST_FWD_REF(mapped_type) val)
	{
		range_type range( boost::forward<key_type>(min), boost::forward<key_type>(max) );
		return insert( value_type( BOOST_MOVE_BASE(range_type,range), boost::forward<mapped_type>(val) ) );
	}

	inline const iterator begin() {
		_node_t *most_left = root_;
		if(NULL != most_left) {
			while( most_left->left() ) {
				most_left = most_left->left();
			}
		}
		return iterator(most_left);
	}

	inline const iterator end() {
		return iterator(NULL);
	}

	inline const iterator find(const key_type& key)
	{
		return iterator( find_node(key) );
	}

	inline bool empty()
	{
		return NULL == root_;
	}

	inline void erase(const iterator& position)
	{
		_node_t * const to_remove = detail::get_it_avl_node<iterator>()(position);
		if(NULL != to_remove) {
			do_erase_node(to_remove);
		}
	}

	inline void clear() {
		if(NULL == root_) return;
		do_destroy_tree(root_);
		destroy_node(root_);
		root_ = NULL;
	}

	~basic_range_map() BOOST_NOEXCEPT_OR_NOTHROW
	{
		if(NULL == root_)
			return;
		do_destroy_tree(root_);
		destroy_node(root_);
	}
private:

	inline _node_t* create_node(BOOST_FWD_REF(value_type) v) {
		return new ( static_cast<void*>( allocator_.allocate(1) ) ) _node_t( boost::forward<value_type>(v) );
	}

	inline void destroy_node(_node_t* const node) {
		allocator_.destroy(node);
		allocator_.deallocate(node, 1);
	}

	inline bool insert_node(BOOST_FWD_REF(value_type) v)
	{
		if(!root_) {
			root_ = create_node( boost::forward<value_type>(v) );
			return true;
		}
		// do insert node
		_node_t* inserted_into = do_insert_node( boost::forward<value_type>(v) );
		if(NULL != inserted_into) {
			do_rebalance_from(inserted_into);
			return true;
		}
		return false;
	}

	_node_t* do_insert_node(BOOST_FWD_REF(value_type) v)
	{
		_node_t *it = root_;
		for(;;) {
			switch( v.first.compare_to( it->value()->first) )
			{
			case 0:
				return NULL;
			case 1:
				// insert right
				if( ! do_insert_right(it, boost::forward<value_type>(v) ) ) {
					it = it->right();
					continue;
				}
				return it;
			default:
				if( ! do_insert_left(it, boost::forward<value_type>(v) ) ) {
					it = it->left();
					continue;
				}
				return it;
			}
		}
		return it;
	}

	inline bool do_insert_left(_node_t * node, BOOST_FWD_REF(value_type) v) {
        if( NULL == node->left() ) {
			node->set_left( create_node( boost::forward<value_type>(v) ) );
			return true;
		}
        return false;
	}

	inline bool do_insert_right(_node_t * node, BOOST_FWD_REF(value_type) v) {
        bool result = NULL == node->right();
        if(result) {
			node->set_right( create_node( boost::forward<value_type>(v) ) );
        }
        return result;
	}

	void do_rebalance_from(_node_t* const from) BOOST_NOEXCEPT_OR_NOTHROW {
		_node_t *it = from;
		_node_t *top = it->top();
		while(top) {
			if(it == top->left() ) {
				top->set_left( _node_t::balance(it) );
			} else {
				top->set_right( _node_t::balance(it) );
			}
			it = top;
			top = it->top();
		}
		root_ = _node_t::balance(it);
		root_->make_root();
	}

	void do_destroy_tree(_node_t * const from) {
		_node_t *it = from;
		while(NULL != it) {
			while(NULL != it->right() ) {
				it = it->right();
			}
			while(NULL != it->left()) {
				it = it->left();
			}
			if(it->right()) {
				it = it->right();
				continue;
			}
			_node_t* tmp = it;
			if( NULL == it->top() ) {
				it = it->left();
				continue;
			} else {
				it = it->top();
			}
			if( tmp == it->left() ) {
				it->set_left(NULL);
			} else {
				it->set_right(NULL);
			}
			destroy_node(tmp);
		}
	}

	inline _node_t* find_node(const K& key) const
	{
		_node_t *it = root_;
		bool found = false;
		while(!found && NULL != it) {
			int8_t compare = it->value()->first.compare_to_key(key);
			if(compare == 0) {
				found = true;
				break;
			} else if(compare < 0) {
				it = it->left();
			} else {
				it = it->right();
			}
		}
		return found ? it: NULL;
	}

	inline void do_erase_node(_node_t* const node) {
		_node_t* top = node->top();

		_node_t *rebalance = top;
		_node_t *left = node->left();
		_node_t *right = node->right();

		if(NULL == left && NULL == right) {
			if(NULL != top) {
				if(node == top->left()) {
					top->set_left(NULL);
				} else {
					top->set_right(NULL);
				}
			} else {
				root_ = NULL;
				return;
			}
		}

		if(NULL != left && NULL == right ) {
			if(NULL != top) {
				if(node == top->left()) {
					top->set_left( left );
				} else {
					top->set_right( left );
				}
			} else {
				root_ = left;
				root_->make_root();
				rebalance = root_;
			}
		}

		if(NULL != right && NULL == left) {
			if(NULL != top) {
				if(node == top->left()) {
					top->set_left( right );
				} else {
					top->set_right( right );
				}
			} else {
				root_ = right;
				root_->make_root();
				rebalance = root_;
			}
		}

		if(NULL != right && NULL != left) {
			rebalance = NULL != left->right() ? left->right() : left;
			while(NULL != rebalance->right() ) {
				rebalance = rebalance->right();
			}
			rebalance->set_right(right);
			if(NULL != top) {
				if(node == top->left()) {
					top->set_left(left);
				} else {
					top->set_right(left);
				}
			} else {
				root_ = left;
				root_->make_root();
				rebalance = root_;
			}
		}

        destroy_node(node);

        do_rebalance_from(rebalance);
	}

private:
	_node_t* root_;
	_node_allocator allocator_;
};

/// \brief Associative sorted container where key can be in range of values
/// Based on AVL tree, optimized for search and insertion when erase operation is time expensive.
/// \param K key type
/// \param V value type
/// \param C key comparator, std::less<K> is default
/// \param A allocator type, std::allocator is default
template<
		typename K, typename V,
		class C = std::less<K>,
		class A = sys::allocator< movable_pair< const range<K,C>, V > >
		>
class range_map:public basic_range_map<K,V,C,A>
{
private:
	typedef basic_range_map<K,V,C,A> base_type;
public:
	BOOST_CONSTEXPR range_map()
	{}
};

/// \brief Slim reader/writer thread safe synchronized version of range map
/// \param K key type
/// \param V value type
/// \param C key comparator, std::less<K> is default
/// \param A allocator type, std::allocator is default
template<
		typename K, typename V,
		class C = std::less<K>,
		class A = sys::allocator< movable_pair< const range<K,C>, V > >
		>
class synchronized_range_map:public basic_range_map<K,V,C,A>
{
private:
	typedef basic_range_map<K,V,C,A> base_type;
public:
	typedef typename base_type::key_type key_type;
	typedef typename base_type::range_type range_type;
	typedef typename base_type::mapped_type mapped_type;
	typedef typename base_type::value_type value_type;
	typedef typename base_type::iterator iterator;

	BOOST_CONSTEXPR synchronized_range_map():
		base_type(),
		rwb_()
	{}
	inline bool insert(BOOST_FWD_REF(value_type) v)
	{
		sys::write_lock lock(rwb_);
		return base_type::insert( boost::forward<value_type>(v) );
	}
	inline bool insert(BOOST_FWD_REF(key_type) min, BOOST_FWD_REF(key_type) max, BOOST_FWD_REF(mapped_type) val)
	{
		range_type range( boost::forward<key_type>(min), boost::forward<key_type>(max) );
		return insert( value_type( BOOST_MOVE_BASE(range_type,range), boost::forward<mapped_type>(val) ) );
	}
	inline void erase(const iterator& position)
	{
		sys::write_lock lock(rwb_);
		base_type::erase(position);
	}
	inline const iterator find(const key_type& key)
	{
		sys::read_lock lock(rwb_);
		return base_type::find(key);
	}
	inline const iterator begin()
	{
		sys::read_lock lock(rwb_);
		return base_type::begin();
	}
	inline bool empty()
	{
		sys::read_lock lock(rwb_);
		return base_type::empty();
	}
private:
	sys::read_write_barrier rwb_;
};

} // { namespace smallobject

#endif // __SMALL_OBJECT_RANGE_MAP_HPP_INCLUDED__
