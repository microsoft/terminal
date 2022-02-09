/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_TREAP_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_TREAP_ALGORITHMS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>

#include <cstddef>

#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/intrusive/bstree_algorithms.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

namespace detail
{

template<class ValueTraits, class NodePtrPrioCompare, class ExtraChecker>
struct treap_node_extra_checker
      : public ExtraChecker
{
   typedef ExtraChecker                            base_checker_t;
   typedef ValueTraits                             value_traits;
   typedef typename value_traits::node_traits      node_traits;
   typedef typename node_traits::const_node_ptr const_node_ptr;

   typedef typename base_checker_t::return_type    return_type;

   treap_node_extra_checker(const NodePtrPrioCompare& prio_comp, ExtraChecker extra_checker)
      : base_checker_t(extra_checker), prio_comp_(prio_comp)
   {}

   void operator () (const const_node_ptr& p,
                     const return_type& check_return_left, const return_type& check_return_right,
                     return_type& check_return)
   {
      if (node_traits::get_left(p))
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!prio_comp_(node_traits::get_left(p), p));
      if (node_traits::get_right(p))
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!prio_comp_(node_traits::get_right(p), p));
      base_checker_t::operator()(p, check_return_left, check_return_right, check_return);
   }

   const NodePtrPrioCompare prio_comp_;
};

} // namespace detail

#endif   //#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

//! treap_algorithms provides basic algorithms to manipulate
//! nodes forming a treap.
//!
//! (1) the header node is maintained with links not only to the root
//! but also to the leftmost node of the tree, to enable constant time
//! begin(), and to the rightmost node of the tree, to enable linear time
//! performance when used with the generic set algorithms (set_union,
//! etc.);
//!
//! (2) when a node being deleted has two children its successor node is
//! relinked into its place, rather than copied, so that the only
//! pointers invalidated are those referring to the deleted node.
//!
//! treap_algorithms is configured with a NodeTraits class, which encapsulates the
//! information about the node to be manipulated. NodeTraits must support the
//! following interface:
//!
//! <b>Typedefs</b>:
//!
//! <tt>node</tt>: The type of the node that forms the treap
//!
//! <tt>node_ptr</tt>: A pointer to a node
//!
//! <tt>const_node_ptr</tt>: A pointer to a const node
//!
//! <b>Static functions</b>:
//!
//! <tt>static node_ptr get_parent(const_node_ptr n);</tt>
//!
//! <tt>static void set_parent(node_ptr n, node_ptr parent);</tt>
//!
//! <tt>static node_ptr get_left(const_node_ptr n);</tt>
//!
//! <tt>static void set_left(node_ptr n, node_ptr left);</tt>
//!
//! <tt>static node_ptr get_right(const_node_ptr n);</tt>
//!
//! <tt>static void set_right(node_ptr n, node_ptr right);</tt>
template<class NodeTraits>
class treap_algorithms
   #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   : public bstree_algorithms<NodeTraits>
   #endif
{
   public:
   typedef NodeTraits                           node_traits;
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr       node_ptr;
   typedef typename NodeTraits::const_node_ptr const_node_ptr;

   /// @cond
   private:

   typedef bstree_algorithms<NodeTraits>  bstree_algo;

   class rerotate_on_destroy
   {
      rerotate_on_destroy& operator=(const rerotate_on_destroy&);

      public:
      rerotate_on_destroy(node_ptr header, node_ptr p, std::size_t &n)
         :  header_(header), p_(p), n_(n), remove_it_(true)
      {}

      ~rerotate_on_destroy()
      {
         if(remove_it_){
            rotate_up_n(header_, p_, n_);
         }
      }

      void release()
      {  remove_it_ = false;  }

      const node_ptr header_;
      const node_ptr p_;
      std::size_t &n_;
      bool remove_it_;
   };

   static void rotate_up_n(const node_ptr header, const node_ptr p, std::size_t n)
   {
      node_ptr p_parent(NodeTraits::get_parent(p));
      node_ptr p_grandparent(NodeTraits::get_parent(p_parent));
      while(n--){
         if(p == NodeTraits::get_left(p_parent)){  //p is left child
            bstree_algo::rotate_right(p_parent, p, p_grandparent, header);
         }
         else{ //p is right child
            bstree_algo::rotate_left(p_parent, p, p_grandparent, header);
         }
         p_parent      = p_grandparent;
         p_grandparent = NodeTraits::get_parent(p_parent);
      }
   }

   /// @endcond

   public:
   //! This type is the information that will be
   //! filled by insert_unique_check
   struct insert_commit_data
      /// @cond
      :  public bstree_algo::insert_commit_data
      /// @endcond
   {
      /// @cond
      std::size_t rotations;
      /// @endcond
   };

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::get_header(const const_node_ptr&)
   static node_ptr get_header(const_node_ptr n);

   //! @copydoc ::boost::intrusive::bstree_algorithms::begin_node
   static node_ptr begin_node(const_node_ptr header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::end_node
   static node_ptr end_node(const_node_ptr header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_tree
   static void swap_tree(node_ptr header1, node_ptr header2);

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_nodes(node_ptr,node_ptr)
   static void swap_nodes(node_ptr node1, node_ptr node2);

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_nodes(node_ptr,node_ptr,node_ptr,node_ptr)
   static void swap_nodes(node_ptr node1, node_ptr header1, node_ptr node2, node_ptr header2);

   //! @copydoc ::boost::intrusive::bstree_algorithms::replace_node(node_ptr,node_ptr)
   static void replace_node(node_ptr node_to_be_replaced, node_ptr new_node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::replace_node(node_ptr,node_ptr,node_ptr)
   static void replace_node(node_ptr node_to_be_replaced, node_ptr header, node_ptr new_node);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::unlink(node_ptr)
   template<class NodePtrPriorityCompare>
   static void unlink(node_ptr node, NodePtrPriorityCompare pcomp)
   {
      node_ptr x = NodeTraits::get_parent(node);
      if(x){
         while(!bstree_algo::is_header(x))
            x = NodeTraits::get_parent(x);
         erase(x, node, pcomp);
      }
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::bstree_algorithms::unlink_leftmost_without_rebalance
   static node_ptr unlink_leftmost_without_rebalance(node_ptr header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::unique(const const_node_ptr&)
   static bool unique(const_node_ptr node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::size(const const_node_ptr&)
   static std::size_t size(const_node_ptr header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::next_node(const node_ptr&)
   static node_ptr next_node(node_ptr node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::prev_node(const node_ptr&)
   static node_ptr prev_node(node_ptr node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::init(node_ptr)
   static void init(node_ptr node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::init_header(node_ptr)
   static void init_header(node_ptr header);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::erase(node_ptr,node_ptr)
   template<class NodePtrPriorityCompare>
   static node_ptr erase(node_ptr header, node_ptr z, NodePtrPriorityCompare pcomp)
   {
      rebalance_for_erasure(header, z, pcomp);
      bstree_algo::erase(header, z);
      return z;
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::bstree_algorithms::clone(const const_node_ptr&,node_ptr,Cloner,Disposer)
   template <class Cloner, class Disposer>
   static void clone
      (const_node_ptr source_header, node_ptr target_header, Cloner cloner, Disposer disposer);

   //! @copydoc ::boost::intrusive::bstree_algorithms::clear_and_dispose(const node_ptr&,Disposer)
   template<class Disposer>
   static void clear_and_dispose(node_ptr header, Disposer disposer);

   //! @copydoc ::boost::intrusive::bstree_algorithms::lower_bound(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr lower_bound
      (const_node_ptr header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::upper_bound(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr upper_bound
      (const_node_ptr header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::find(const const_node_ptr&, const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr find
      (const_node_ptr header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::equal_range(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> equal_range
      (const_node_ptr header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::bounded_range(const const_node_ptr&,const KeyType&,const KeyType&,KeyNodePtrCompare,bool,bool)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> bounded_range
      (const_node_ptr header, const KeyType &lower_key, const KeyType &upper_key, KeyNodePtrCompare comp
      , bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::bstree_algorithms::count(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static std::size_t count(const_node_ptr header, const KeyType &key, KeyNodePtrCompare comp);

   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! <b>Requires</b>: "h" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before the upper bound
   //!   according to "comp" and rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Average complexity for insert element is at
   //!   most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throw or "pcomp" throw.
   template<class NodePtrCompare, class NodePtrPriorityCompare>
   static node_ptr insert_equal_upper_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::insert_equal_upper_bound_check(h, new_node, comp, commit_data);
      rebalance_check_and_commit(h, new_node, pcomp, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "h" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before the upper bound
   //!   according to "comp" and rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Average complexity for insert element is at
   //!   most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class NodePtrCompare, class NodePtrPriorityCompare>
   static node_ptr insert_equal_lower_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::insert_equal_lower_bound_check(h, new_node, comp, commit_data);
      rebalance_check_and_commit(h, new_node, pcomp, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs. "hint" is node from
   //!   the "header"'s tree.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree, using "hint" as a hint to
   //!   where it will be inserted. If "hint" is the upper_bound
   //!   the insertion takes constant time (two comparisons in the worst case).
   //!   Rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Logarithmic in general, but it is amortized
   //!   constant time if new_node is inserted immediately before "hint".
   //!
   //! <b>Throws</b>: If "comp" throw or "pcomp" throw.
   template<class NodePtrCompare, class NodePtrPriorityCompare>
   static node_ptr insert_equal
      (node_ptr h, node_ptr hint, node_ptr new_node, NodePtrCompare comp, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::insert_equal_check(h, hint, new_node, comp, commit_data);
      rebalance_check_and_commit(h, new_node, pcomp, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "pos" must be a valid node of the tree (including header end) node.
   //!   "pos" must be a node pointing to the successor to "new_node"
   //!   once inserted according to the order of already inserted nodes. This function does not
   //!   check "pos" and this precondition must be guaranteed by the caller.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before "pos"
   //!   and rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: If "pcomp" throws, strong guarantee.
   //!
   //! <b>Note</b>: If "pos" is not the successor of the newly inserted "new_node"
   //! tree invariants might be broken.
   template<class NodePtrPriorityCompare>
   static node_ptr insert_before
      (node_ptr header, node_ptr pos, node_ptr new_node, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::insert_before_check(header, pos, commit_data);
      rebalance_check_and_commit(header, new_node, pcomp, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "new_node" must be, according to the used ordering no less than the
   //!   greatest inserted key.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts x into the tree in the last position
   //!   and rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: If "pcomp" throws, strong guarantee.
   //!
   //! <b>Note</b>: If "new_node" is less than the greatest inserted key
   //! tree invariants are broken. This function is slightly faster than
   //! using "insert_before".
   template<class NodePtrPriorityCompare>
   static void push_back(node_ptr header, node_ptr new_node, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::push_back_check(header, commit_data);
      rebalance_check_and_commit(header, new_node, pcomp, commit_data);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "new_node" must be, according to the used ordering, no greater than the
   //!   lowest inserted key.
   //!   NodePtrPriorityCompare is a priority function object that induces a strict weak
   //!   ordering compatible with the one used to create the
   //!   the tree. NodePtrPriorityCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts x into the tree in the first position
   //!   and rotates the tree according to "pcomp".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: If "pcomp" throws, strong guarantee.
   //!
   //! <b>Note</b>: If "new_node" is greater than the lowest inserted key
   //! tree invariants are broken. This function is slightly faster than
   //! using "insert_before".
   template<class NodePtrPriorityCompare>
   static void push_front(node_ptr header, node_ptr new_node, NodePtrPriorityCompare pcomp)
   {
      insert_commit_data commit_data;
      bstree_algo::push_front_check(header, commit_data);
      rebalance_check_and_commit(header, new_node, pcomp, commit_data);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares KeyType with a node_ptr.
   //!
   //! <b>Effects</b>: Checks if there is an equivalent node to "key" in the
   //!   tree according to "comp" and obtains the needed information to realize
   //!   a constant-time node insertion if there is no equivalent node.
   //!
   //! <b>Returns</b>: If there is an equivalent value
   //!   returns a pair containing a node_ptr to the already present node
   //!   and false. If there is not equivalent key can be inserted returns true
   //!   in the returned pair's boolean and fills "commit_data" that is meant to
   //!   be used with the "insert_commit" function to achieve a constant-time
   //!   insertion function.
   //!
   //! <b>Complexity</b>: Average complexity is at most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   //!
   //! <b>Notes</b>: This function is used to improve performance when constructing
   //!   a node is expensive and the user does not want to have two equivalent nodes
   //!   in the tree: if there is an equivalent value
   //!   the constructed object must be discarded. Many times, the part of the
   //!   node that is used to impose the order is much cheaper to construct
   //!   than the node and this function offers the possibility to use that part
   //!   to check if the insertion will be successful.
   //!
   //!   If the check is successful, the user can construct the node and use
   //!   "insert_commit" to insert the node in constant-time. This gives a total
   //!   logarithmic complexity to the insertion: check(O(log(N)) + commit(O(1)).
   //!
   //!   "commit_data" remains valid for a subsequent "insert_unique_commit" only
   //!   if no more objects are inserted or erased from the set.
   template<class KeyType, class KeyNodePtrCompare, class PrioType, class PrioNodePtrPrioCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      ( const_node_ptr header
      , const KeyType &key, KeyNodePtrCompare comp
      , const PrioType &prio, PrioNodePtrPrioCompare pcomp
      , insert_commit_data &commit_data)
   {
      std::pair<node_ptr, bool> ret =
         bstree_algo::insert_unique_check(header, key, comp, commit_data);
      if(ret.second)
         rebalance_after_insertion_check(header, commit_data.node, prio, pcomp, commit_data.rotations);
      return ret;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares KeyType with a node_ptr.
   //!   "hint" is node from the "header"'s tree.
   //!
   //! <b>Effects</b>: Checks if there is an equivalent node to "key" in the
   //!   tree according to "comp" using "hint" as a hint to where it should be
   //!   inserted and obtains the needed information to realize
   //!   a constant-time node insertion if there is no equivalent node.
   //!   If "hint" is the upper_bound the function has constant time
   //!   complexity (two comparisons in the worst case).
   //!
   //! <b>Returns</b>: If there is an equivalent value
   //!   returns a pair containing a node_ptr to the already present node
   //!   and false. If there is not equivalent key can be inserted returns true
   //!   in the returned pair's boolean and fills "commit_data" that is meant to
   //!   be used with the "insert_commit" function to achieve a constant-time
   //!   insertion function.
   //!
   //! <b>Complexity</b>: Average complexity is at most logarithmic, but it is
   //!   amortized constant time if new_node should be inserted immediately before "hint".
   //!
   //! <b>Throws</b>: If "comp" throws.
   //!
   //! <b>Notes</b>: This function is used to improve performance when constructing
   //!   a node is expensive and the user does not want to have two equivalent nodes
   //!   in the tree: if there is an equivalent value
   //!   the constructed object must be discarded. Many times, the part of the
   //!   node that is used to impose the order is much cheaper to construct
   //!   than the node and this function offers the possibility to use that part
   //!   to check if the insertion will be successful.
   //!
   //!   If the check is successful, the user can construct the node and use
   //!   "insert_commit" to insert the node in constant-time. This gives a total
   //!   logarithmic complexity to the insertion: check(O(log(N)) + commit(O(1)).
   //!
   //!   "commit_data" remains valid for a subsequent "insert_unique_commit" only
   //!   if no more objects are inserted or erased from the set.
   template<class KeyType, class KeyNodePtrCompare, class PrioType, class PrioNodePtrPrioCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      ( const_node_ptr header, node_ptr hint
      , const KeyType &key, KeyNodePtrCompare comp
      , const PrioType &prio, PrioNodePtrPrioCompare pcomp
      , insert_commit_data &commit_data)
   {
      std::pair<node_ptr, bool> ret =
         bstree_algo::insert_unique_check(header, hint, key, comp, commit_data);
      if(ret.second)
         rebalance_after_insertion_check(header, commit_data.node, prio, pcomp, commit_data.rotations);
      return ret;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "commit_data" must have been obtained from a previous call to
   //!   "insert_unique_check". No objects should have been inserted or erased
   //!   from the set between the "insert_unique_check" that filled "commit_data"
   //!   and the call to "insert_commit".
   //!
   //!
   //! <b>Effects</b>: Inserts new_node in the set using the information obtained
   //!   from the "commit_data" that a previous "insert_check" filled.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Notes</b>: This function has only sense if a "insert_unique_check" has been
   //!   previously executed to fill "commit_data". No value should be inserted or
   //!   erased between the "insert_check" and "insert_commit" calls.
   static void insert_unique_commit
      (node_ptr header, node_ptr new_node, const insert_commit_data &commit_data)
   {
      bstree_algo::insert_unique_commit(header, new_node, commit_data);
      rotate_up_n(header, new_node, commit_data.rotations);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::transfer_unique
   template<class NodePtrCompare, class PrioNodePtrPrioCompare>
   static bool transfer_unique
      (node_ptr header1, NodePtrCompare comp, PrioNodePtrPrioCompare pcomp, node_ptr header2, node_ptr z)
   {
      insert_commit_data commit_data;
      bool const transferable = insert_unique_check(header1, z, comp, z, pcomp, commit_data).second;
      if(transferable){
         erase(header2, z, pcomp);
         insert_unique_commit(header1, z, commit_data);         
      }
      return transferable;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::transfer_equal
   template<class NodePtrCompare, class PrioNodePtrPrioCompare>
   static void transfer_equal
      (node_ptr header1, NodePtrCompare comp, PrioNodePtrPrioCompare pcomp, node_ptr header2, node_ptr z)
   {
      insert_commit_data commit_data;
      bstree_algo::insert_equal_upper_bound_check(header1, z, comp, commit_data);
      rebalance_after_insertion_check(header1, commit_data.node, z, pcomp, commit_data.rotations);
      rebalance_for_erasure(header2, z, pcomp);
      bstree_algo::erase(header2, z);
      bstree_algo::insert_unique_commit(header1, z, commit_data);
      rotate_up_n(header1, z, commit_data.rotations);
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::is_header
   static bool is_header(const_node_ptr p);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   /// @cond
   private:

   template<class NodePtrPriorityCompare>
   static void rebalance_for_erasure(node_ptr header, node_ptr z, NodePtrPriorityCompare pcomp)
   {
      std::size_t n = 0;
      rerotate_on_destroy rb(header, z, n);

      node_ptr z_left  = NodeTraits::get_left(z);
      node_ptr z_right = NodeTraits::get_right(z);
      while(z_left || z_right){
         const node_ptr z_parent(NodeTraits::get_parent(z));
         if(!z_right || (z_left && pcomp(z_left, z_right))){
            bstree_algo::rotate_right(z, z_left, z_parent, header);
         }
         else{
            bstree_algo::rotate_left(z, z_right, z_parent, header);
         }
         ++n;
         z_left  = NodeTraits::get_left(z);
         z_right = NodeTraits::get_right(z);
      }
      rb.release();
   }

   template<class NodePtrPriorityCompare>
   static void rebalance_check_and_commit
      (node_ptr h, node_ptr new_node, NodePtrPriorityCompare pcomp, insert_commit_data &commit_data)
   {
      rebalance_after_insertion_check(h, commit_data.node, new_node, pcomp, commit_data.rotations);
      //No-throw
      bstree_algo::insert_unique_commit(h, new_node, commit_data);
      rotate_up_n(h, new_node, commit_data.rotations);
   }

   template<class Key, class KeyNodePriorityCompare>
   static void rebalance_after_insertion_check
      (const_node_ptr header, const_node_ptr up, const Key &k
      , KeyNodePriorityCompare pcomp, std::size_t &num_rotations)
   {
      const_node_ptr upnode(up);
      //First check rotations since pcomp can throw
      num_rotations = 0;
      std::size_t n = 0;
      while(upnode != header && pcomp(k, upnode)){
         ++n;
         upnode = NodeTraits::get_parent(upnode);
      }
      num_rotations = n;
   }

   template<class NodePtrPriorityCompare>
   static bool check_invariant(const_node_ptr header, NodePtrPriorityCompare pcomp)
   {
      node_ptr beg = begin_node(header);
      node_ptr end = end_node(header);

      while(beg != end){
         node_ptr p = NodeTraits::get_parent(beg);
         if(p != header){
            if(pcomp(beg, p))
               return false;
         }
         beg = next_node(beg);
      }
      return true;
   }

   /// @endcond
};

/// @cond

template<class NodeTraits>
struct get_algo<TreapAlgorithms, NodeTraits>
{
   typedef treap_algorithms<NodeTraits> type;
};

template <class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct get_node_checker<TreapAlgorithms, ValueTraits, NodePtrCompare, ExtraChecker>
{
   typedef detail::bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_TREAP_ALGORITHMS_HPP
