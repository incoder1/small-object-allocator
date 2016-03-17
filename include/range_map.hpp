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

	range(const K& min,const K& max):
		min_(min),
		max_(max)
	{
		comparator_type cmp;
		assert( cmp( min_ , max_ ) );
	}

	range(BOOST_COPY_ASSIGN_REF(range) rhs):
		min_(rhs.min_),
		max_(rhs.max_)
	{}

	inline range& operator=(BOOST_COPY_ASSIGN_REF(range) rhs)
	{
		range tmp(rhs);
		tmp.swap(*this);
		return *this;
	}

	range(BOOST_RV_REF(K) min,BOOST_RV_REF(K) max) BOOST_NOEXCEPT:
		min_( BOOST_MOVE_BASE(K,min) ),
		max_( BOOST_MOVE_BASE(K,max) )
	{}

	range(BOOST_RV_REF(range) rhs):
		min_( BOOST_MOVE_BASE(range,rhs.min_) ),
		max_( BOOST_MOVE_BASE(range, rhs.max_) )
	{}

	inline range& operator=(BOOST_RV_REF(range) rhs)
	{
		min_ = BOOST_MOVE_BASE(K, rhs.min_);
		max_ = BOOST_MOVE_BASE(K, rhs.max_);
		return *this;
	}

	~range()
	{}

	inline const K& min() const {
		return min_;
	}

	inline const K& max() const {
		return max_;
	}

	inline void swap(range& rhs)
	{
		std::swap( min_, rhs.min_ );
		std::swap( max_, rhs.max_ );
	}

	inline int8_t compare_to_key(const K& key) const
	{
		comparator_type cmp;
		int8_t result = cmp( key, min_) ? -1 : 0;
		if(!result) result = cmp( max_, key) ? 1 : 0;
		return result;
	}

	inline int8_t compare_to(const range& rhs) const {
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

	movable_pair(const first_type& __f, const second_type& __s):
		first(__f),
		second(__s)
	{}

	movable_pair(BOOST_RV_REF(first_type) __f, BOOST_RV_REF(second_type) __s):
       first( BOOST_MOVE_BASE(first_type,__f) ),
       second( BOOST_MOVE_BASE(second_type,__s) )
	{}

	movable_pair(const first_type& __f, BOOST_RV_REF(second_type) __s):
       first(__f),
       second( BOOST_MOVE_BASE(second_type,__s) )
	{}

	movable_pair(BOOST_RV_REF(first_type) __f, const second_type& __s):
       first( BOOST_MOVE_BASE(first_type,__f) ),
       second(__s)
	{}

	movable_pair( BOOST_RV_REF(movable_pair) rhs) BOOST_NOEXCEPT:
	   first( BOOST_MOVE_BASE(first_type,rhs.first) ),
       second( BOOST_MOVE_BASE(second_type,rhs.second) )
	{}

	movable_pair& operator=( BOOST_RV_REF(movable_pair) rhs) BOOST_NOEXCEPT
	{
		first = BOOST_MOVE_BASE(first_type,first);
		second = BOOST_MOVE_BASE(second_type,first);
		return *this;
	}

	movable_pair(const movable_pair& rhs):
		first(rhs.first),
		second(rhs.second)
	{}

	movable_pair& operator=(const movable_pair& rhs)
	{
		movable_pair tmp(rhs);
		swap(tmp);
		return *this;
	}

	void swap(movable_pair& rhs)
	{
		std::swap(first, rhs.first);
		std::swap(second. rhs.second);
	}

	~movable_pair()
	{}

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

	explicit avl_tree_node(BOOST_FWD_REF(value_type) v):
		top_(NULL),
		left_(NULL),
		right_(NULL),
		height_(0),
		value_( BOOST_MOVE_BASE(value_type, v) )
	{}

	~avl_tree_node()
	{}

	const value_type* value() const
	{
		return &value_;
	}

	inline self_type* top() const {
		return top_;
	}

	inline self_type* left() const
	{
		return left_;
	}

	inline void set_left(self_type* const new_left)
	{
		left_ = new_left;
		if(new_left) new_left->top_ = const_cast<self_type*>(this);
	}

	inline self_type* right() const
	{
		return right_;
	}

	inline void set_right(self_type* const new_right)
	{
		right_ = new_right;
		if(new_right) new_right->top_ = const_cast<self_type*>(this);
	}

	inline void make_root() {
		top_ = NULL;
	}

	static self_type* balance(self_type * const p)
	{
		p->fix_height();
		int8_t factor = b_factor(p);
		if ( factor == 2 ) {
			factor = b_factor( p->right() );
			if ( factor < 0) {
				p->set_right( rotate_right( p->right() ) );
			}
			return rotate_left(p);
		}
		if ( factor == -2 ) {
			factor = b_factor( p->left() );
			if ( factor > 0) {
				p->set_left( rotate_left( p->left() ) );
			}
			return rotate_right(p);
		}
		return p;
	}

	static self_type*  inc_node(self_type* const from) {
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

	static inline int8_t height(self_type* const node)
	{
		return (NULL != node) ? node->height_ : 0;
	}

	static inline self_type* rotate_right(self_type* const node)
	{
		self_type* result = node->right_;
		node->set_right(result->left_);
		result->set_left(node);
		node->fix_height();
		result->fix_height();
		return result;
	}

	static inline self_type* rotate_left(self_type* const node)
	{
		avl_tree_node* result = node->left_;
		node->set_left(result->right_);
		result->set_right(node);
		node->fix_height();
		result->fix_height();
		return result;
	}

	static inline int8_t b_factor(self_type* const node)
	{
		return (NULL != node) ? node->factor() : 0;
	}

	inline void fix_height()
	{
		int8_t hl = height(left_);
		int8_t hr = height(right_);
		height_ =  ( (hl > hr ? hl : hr) + 1);
	}

	inline int8_t factor() const
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

	explicit avl_tree_iterator(link_type* const node):
		node_(node)
	{}

	reference operator*() const  {
		return *(node_->value());
	}

	pointer operator->() const {
		return const_cast<value_type*>( node_->value() );
	}

	self_type& operator++() {
		if(NULL != node_) {
			node_ = link_type::inc_node(node_);
		}
		return *this;
	}

	bool operator != (const self_type& rhs) const {
		return node_ != rhs.node_;
	}

	bool operator == (const self_type& rhs) const {
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
	inline node_type* operator()(const _iterator_type& it) {
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
	basic_range_map():
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

	~basic_range_map()
	{
		if(NULL == root_) return;
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
		if(inserted_into) {
			do_rebalance_from(inserted_into);
			return true;
		}
		return false;
	}

	_node_t* do_insert_node(BOOST_FWD_REF(value_type) v)
	{
		_node_t *it = root_;
		bool inserted = false;
		while(!inserted) {
			int8_t cmp = v.first.compare_to( it->value()->first );
			if(0 == cmp) {
				break; // interception
			} else if( cmp < 0) {
				// left insert
				inserted = do_insert_left(it, boost::forward<value_type>(v) );
				if(!inserted) {
					it = it->left();
				}
			} else {
				// right insert
				inserted = do_insert_right(it, boost::forward<value_type>(v) );
				if(!inserted) {
					it = it->right();
				}
			}
		}
		return it;
	}

	inline bool do_insert_left(_node_t * node, BOOST_FWD_REF(value_type) v) {
        bool result = NULL == node->left();
        if(result) {
			node->set_left( create_node( boost::forward<value_type>(v) ) );
        }
        return result;
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
			}
			if(compare < 0) {
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
		class A = std::allocator< movable_pair< const range<K,C>, V > >
		>
class range_map:public basic_range_map<K,V,C,A>
{
private:
	typedef basic_range_map<K,V,C,A> base_type;
public:
	range_map()
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
		class A = std::allocator< movable_pair< const range<K,C>, V > >
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

	synchronized_range_map():
		base_type(),
		rwb_()
	{}
	bool insert(BOOST_FWD_REF(value_type) v)
	{
		sys::write_lock lock(rwb_);
		return base_type::insert( boost::forward<value_type>(v) );
	}
	bool insert(BOOST_FWD_REF(key_type) min, BOOST_FWD_REF(key_type) max, BOOST_FWD_REF(mapped_type) val)
	{
		range_type range( boost::forward<key_type>(min), boost::forward<key_type>(max) );
		return insert( value_type( BOOST_MOVE_BASE(range_type,range), boost::forward<mapped_type>(val) ) );
	}
	void erase(const iterator& position)
	{
		sys::write_lock lock(rwb_);
		base_type::erase(position);
	}
	const iterator find(const key_type& key)
	{
		sys::read_lock lock(rwb_);
		return base_type::find(key);
	}
	const iterator begin() {
		sys::read_lock lock(rwb_);
		return base_type::begin();
	}
	bool empty() {
		sys::read_lock lock(rwb_);
		return base_type::empty();
	}
private:
	sys::read_write_barrier rwb_;
};

} // { namespace smallobject

#endif // __SMALL_OBJECT_RANGE_MAP_HPP_INCLUDED__
