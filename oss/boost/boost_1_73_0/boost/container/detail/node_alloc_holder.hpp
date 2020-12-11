//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_NODE_ALLOC_HPP_
#define BOOST_CONTAINER_DETAIL_NODE_ALLOC_HPP_

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

// container
#include <boost/container/allocator_traits.hpp>
// container/detail
#include <boost/container/detail/addressof.hpp>
#include <boost/container/detail/alloc_helpers.hpp>
#include <boost/container/detail/allocator_version_traits.hpp>
#include <boost/container/detail/construct_in_place.hpp>
#include <boost/container/detail/destroyers.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/placement_new.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/version_type.hpp>
// intrusive
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/options.hpp>
// move
#include <boost/move/utility_core.hpp>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
// other
#include <boost/core/no_exceptions_support.hpp>


namespace boost {
namespace container {
namespace dtl {

BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(key_compare)
BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(key_equal)
BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(hasher)
BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(predicate_type)

template<class Allocator, class ICont>
struct node_alloc_holder
   : public allocator_traits<Allocator>::template
            portable_rebind_alloc<typename ICont::value_type>::type   //NodeAlloc
{
   //If the intrusive container is an associative container, obtain the predicate, which will
   //be of type node_compare<>. If not an associative container val_compare will be a "nat" type.
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      ( boost::container::dtl::
      , ICont, key_compare, dtl::nat)                 intrusive_val_compare;
   //In that case obtain the value predicate from the node predicate via predicate_type
   //if intrusive_val_compare is node_compare<>, nat otherwise
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      ( boost::container::dtl::
      , intrusive_val_compare
      , predicate_type, dtl::nat)                    val_compare;

   //If the intrusive container is a hash container, obtain the predicate, which will
   //be of type node_compare<>. If not an associative container val_equal will be a "nat" type.
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      (boost::container::dtl::
         , ICont, key_equal, dtl::nat2)              intrusive_val_equal;
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
   (boost::container::dtl::
      , ICont, hasher, dtl::nat3)                     intrusive_val_hasher;
   //In that case obtain the value predicate from the node predicate via predicate_type
   //if intrusive_val_compare is node_compare<>, nat otherwise
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
   (boost::container::dtl::
      , intrusive_val_equal
      , predicate_type, dtl::nat2)                    val_equal;
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
   (boost::container::dtl::
      , intrusive_val_hasher
      , predicate_type, dtl::nat3)                    val_hasher;

   typedef allocator_traits<Allocator>                            allocator_traits_type;
   typedef typename allocator_traits_type::value_type             val_type;
   typedef ICont                                                  intrusive_container;
   typedef typename ICont::value_type                             Node;
   typedef typename allocator_traits_type::template
      portable_rebind_alloc<Node>::type                           NodeAlloc;
   typedef allocator_traits<NodeAlloc>                            node_allocator_traits_type;
   typedef dtl::allocator_version_traits<NodeAlloc>  node_allocator_version_traits_type;
   typedef Allocator                                              ValAlloc;
   typedef typename node_allocator_traits_type::pointer           NodePtr;
   typedef dtl::scoped_deallocator<NodeAlloc>        Deallocator;
   typedef typename node_allocator_traits_type::size_type         size_type;
   typedef typename node_allocator_traits_type::difference_type   difference_type;
   typedef dtl::integral_constant<unsigned,
      boost::container::dtl::
         version<NodeAlloc>::value>                               alloc_version;
   typedef typename ICont::iterator                               icont_iterator;
   typedef typename ICont::const_iterator                         icont_citerator;
   typedef allocator_destroyer<NodeAlloc>                         Destroyer;
   typedef allocator_traits<NodeAlloc>                            NodeAllocTraits;
   typedef allocator_version_traits<NodeAlloc>                    AllocVersionTraits;

   private:
   BOOST_COPYABLE_AND_MOVABLE(node_alloc_holder)

   public:

   //Constructors for sequence containers
   node_alloc_holder()
   {}

   explicit node_alloc_holder(const ValAlloc &a)
      : NodeAlloc(a)
   {}

   //Constructors for associative containers
   node_alloc_holder(const val_compare &c, const ValAlloc &a)
      : NodeAlloc(a), m_icont(typename ICont::key_compare(c))
   {}

   node_alloc_holder(const val_hasher &hf, const val_equal &eql, const ValAlloc &a)
      : NodeAlloc(a)
      , m_icont(typename ICont::bucket_traits()
         , typename ICont::hasher(hf)
         , typename ICont::key_equal(eql))
   {}

   node_alloc_holder(const val_hasher &hf, const ValAlloc &a)
      : NodeAlloc(a)
      , m_icont(typename ICont::bucket_traits()
         , typename ICont::hasher(hf)
         , typename ICont::key_equal())
   {}

   node_alloc_holder(const val_hasher &hf)
      : m_icont(typename ICont::bucket_traits()
         , typename ICont::hasher(hf)
         , typename ICont::key_equal())
   {}

   explicit node_alloc_holder(const node_alloc_holder &x)
      : NodeAlloc(NodeAllocTraits::select_on_container_copy_construction(x.node_alloc()))
   {}

   node_alloc_holder(const node_alloc_holder &x, const val_compare &c)
      : NodeAlloc(NodeAllocTraits::select_on_container_copy_construction(x.node_alloc()))
      , m_icont(typename ICont::key_compare(c))
   {}

   node_alloc_holder(const node_alloc_holder &x, const val_hasher &hf, const val_equal &eql)
      : NodeAlloc(NodeAllocTraits::select_on_container_copy_construction(x.node_alloc()))
      , m_icont( typename ICont::bucket_traits()
               , typename ICont::hasher(hf)
               , typename ICont::key_equal(eql))
   {}

   node_alloc_holder(const val_hasher &hf, const val_equal &eql)
      : m_icont(typename ICont::bucket_traits()
         , typename ICont::hasher(hf)
         , typename ICont::key_equal(eql))
   {}

   explicit node_alloc_holder(BOOST_RV_REF(node_alloc_holder) x)
      : NodeAlloc(boost::move(x.node_alloc()))
   {  this->icont().swap(x.icont());  }

   explicit node_alloc_holder(const val_compare &c)
      : m_icont(typename ICont::key_compare(c))
   {}

   //helpers for move assignments
   explicit node_alloc_holder(BOOST_RV_REF(node_alloc_holder) x, const val_compare &c)
      : NodeAlloc(boost::move(x.node_alloc())), m_icont(typename ICont::key_compare(c))
   {  this->icont().swap(x.icont());  }

   explicit node_alloc_holder(BOOST_RV_REF(node_alloc_holder) x, const val_hasher &hf, const val_equal &eql)
      : NodeAlloc(boost::move(x.node_alloc()))
      , m_icont( typename ICont::bucket_traits()
               , typename ICont::hasher(hf)
               , typename ICont::key_equal(eql))
   {  this->icont().swap(x.icont());   }

   void copy_assign_alloc(const node_alloc_holder &x)
   {
      dtl::bool_<allocator_traits_type::propagate_on_container_copy_assignment::value> flag;
      dtl::assign_alloc( static_cast<NodeAlloc &>(*this)
                       , static_cast<const NodeAlloc &>(x), flag);
   }

   void move_assign_alloc( node_alloc_holder &x)
   {
      dtl::bool_<allocator_traits_type::propagate_on_container_move_assignment::value> flag;
      dtl::move_alloc( static_cast<NodeAlloc &>(*this)
                     , static_cast<NodeAlloc &>(x), flag);
   }

   ~node_alloc_holder()
   {  this->clear(alloc_version()); }

   size_type max_size() const
   {  return allocator_traits_type::max_size(this->node_alloc());  }

   NodePtr allocate_one()
   {  return AllocVersionTraits::allocate_one(this->node_alloc());   }

   void deallocate_one(const NodePtr &p)
   {  AllocVersionTraits::deallocate_one(this->node_alloc(), p);  }

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template<class ...Args>
   NodePtr create_node(Args &&...args)
   {
      NodePtr p = this->allocate_one();
      BOOST_TRY{
         ::new(boost::movelib::iterator_to_raw_pointer(p), boost_container_new_t()) Node;
         allocator_traits<NodeAlloc>::construct
            (this->node_alloc()
            , p->get_real_data_ptr(), boost::forward<Args>(args)...);
      }
      BOOST_CATCH(...) {
         p->destroy_header();
         this->node_alloc().deallocate(p, 1);
         BOOST_RETHROW
      }
      BOOST_CATCH_END
      return (p);
   }

   #else //defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   #define BOOST_CONTAINER_NODE_ALLOC_HOLDER_CONSTRUCT_IMPL(N) \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   NodePtr create_node(BOOST_MOVE_UREF##N)\
   {\
      NodePtr p = this->allocate_one();\
      BOOST_TRY{\
         ::new(boost::movelib::iterator_to_raw_pointer(p), boost_container_new_t()) Node;\
         allocator_traits<NodeAlloc>::construct\
            ( this->node_alloc()\
            , p->get_real_data_ptr()\
             BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      }\
      BOOST_CATCH(...) {\
         p->destroy_header();\
         this->node_alloc().deallocate(p, 1);\
         BOOST_RETHROW\
      }\
      BOOST_CATCH_END\
      return (p);\
   }\
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_NODE_ALLOC_HOLDER_CONSTRUCT_IMPL)
   #undef BOOST_CONTAINER_NODE_ALLOC_HOLDER_CONSTRUCT_IMPL

   #endif   // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template<class It>
   NodePtr create_node_from_it(const It &it)
   {
      NodePtr p = this->allocate_one();
      BOOST_TRY{
         ::new(boost::movelib::iterator_to_raw_pointer(p), boost_container_new_t()) Node;
         ::boost::container::construct_in_place(this->node_alloc(), p->get_real_data_ptr(), it);
      }
      BOOST_CATCH(...) {
         p->destroy_header();
         this->node_alloc().deallocate(p, 1);
         BOOST_RETHROW
      }
      BOOST_CATCH_END
      return (p);
   }

   template<class KeyConvertible>
   NodePtr create_node_from_key(BOOST_FWD_REF(KeyConvertible) key)
   {
      NodePtr p = this->allocate_one();
      BOOST_TRY{
         ::new(boost::movelib::iterator_to_raw_pointer(p), boost_container_new_t()) Node;
         NodeAlloc &na = this->node_alloc();
         node_allocator_traits_type::construct
            (na, dtl::addressof(p->get_real_data().first), boost::forward<KeyConvertible>(key));
         BOOST_TRY{
            node_allocator_traits_type::construct(na, dtl::addressof(p->get_real_data().second));
         }
         BOOST_CATCH(...){
            node_allocator_traits_type::destroy(na, dtl::addressof(p->get_real_data().first));
            BOOST_RETHROW;
         }
         BOOST_CATCH_END
      }
      BOOST_CATCH(...) {
         p->destroy_header();
         this->node_alloc().deallocate(p, 1);
         BOOST_RETHROW
      }
      BOOST_CATCH_END
      return (p);
   }

   void destroy_node(const NodePtr &nodep)
   {
      allocator_traits<NodeAlloc>::destroy(this->node_alloc(), boost::movelib::to_raw_pointer(nodep));
      this->deallocate_one(nodep);
   }

   void swap(node_alloc_holder &x)
   {
      this->icont().swap(x.icont());
      dtl::bool_<allocator_traits_type::propagate_on_container_swap::value> flag;
      dtl::swap_alloc(this->node_alloc(), x.node_alloc(), flag);
   }

   template<class FwdIterator, class Inserter>
   void allocate_many_and_construct
      (FwdIterator beg, difference_type n, Inserter inserter)
   {
      if(n){
         typedef typename node_allocator_version_traits_type::multiallocation_chain multiallocation_chain_t;

         //Try to allocate memory in a single block
         typedef typename multiallocation_chain_t::iterator multialloc_iterator_t;
         multiallocation_chain_t chain;
         NodeAlloc &nalloc = this->node_alloc();
         node_allocator_version_traits_type::allocate_individual(nalloc, n, chain);
         multialloc_iterator_t itbeg  = chain.begin();
         multialloc_iterator_t itlast = chain.last();
         chain.clear();

         Node *p = 0;
            BOOST_TRY{
            Deallocator node_deallocator(NodePtr(), nalloc);
            dtl::scoped_destructor<NodeAlloc> sdestructor(nalloc, 0);
            while(n){
               --n;
               p = boost::movelib::iterator_to_raw_pointer(itbeg);
               ++itbeg; //Increment iterator before overwriting pointed memory
               //This does not throw
               p = ::new(p, boost_container_new_t()) Node;
               node_deallocator.set(p);
               //This can throw
               boost::container::construct_in_place(nalloc, p->get_real_data_ptr(), beg);
               sdestructor.set(p);
               ++beg;
               //This can throw in some containers (predicate might throw).
               //(sdestructor will destruct the node and node_deallocator will deallocate it in case of exception)
               inserter(*p);
               sdestructor.set(0);
            }
            sdestructor.release();
            node_deallocator.release();
         }
         BOOST_CATCH(...){
            p->destroy_header();
            chain.incorporate_after(chain.last(), &*itbeg, &*itlast, n);
            node_allocator_version_traits_type::deallocate_individual(this->node_alloc(), chain);
            BOOST_RETHROW
         }
         BOOST_CATCH_END
      }
   }

   void clear(version_1)
   {  this->icont().clear_and_dispose(Destroyer(this->node_alloc()));   }

   void clear(version_2)
   {
      typename NodeAlloc::multiallocation_chain chain;
      allocator_destroyer_and_chain_builder<NodeAlloc> builder(this->node_alloc(), chain);
      this->icont().clear_and_dispose(builder);
      //BOOST_STATIC_ASSERT((::boost::has_move_emulation_enabled<typename NodeAlloc::multiallocation_chain>::value == true));
      if(!chain.empty())
         this->node_alloc().deallocate_individual(chain);
   }

   icont_iterator erase_range(const icont_iterator &first, const icont_iterator &last, version_1)
   {  return this->icont().erase_and_dispose(first, last, Destroyer(this->node_alloc())); }

   icont_iterator erase_range(const icont_iterator &first, const icont_iterator &last, version_2)
   {
      typedef typename NodeAlloc::multiallocation_chain multiallocation_chain;
      NodeAlloc & nalloc = this->node_alloc();
      multiallocation_chain chain;
      allocator_destroyer_and_chain_builder<NodeAlloc> chain_builder(nalloc, chain);
      icont_iterator ret_it = this->icont().erase_and_dispose(first, last, chain_builder);
      nalloc.deallocate_individual(chain);
      return ret_it;
   }

   template<class Key, class Comparator>
   size_type erase_key(const Key& k, const Comparator &comp, version_1)
   {  return this->icont().erase_and_dispose(k, comp, Destroyer(this->node_alloc())); }

   template<class Key, class Comparator>
   size_type erase_key(const Key& k, const Comparator &comp, version_2)
   {
      allocator_multialloc_chain_node_deallocator<NodeAlloc> chain_holder(this->node_alloc());
      return this->icont().erase_and_dispose(k, comp, chain_holder.get_chain_builder());
   }

   protected:
   struct cloner
   {
      explicit cloner(node_alloc_holder &holder)
         :  m_holder(holder)
      {}

      NodePtr operator()(const Node &other) const
      {  return m_holder.create_node(other.get_real_data());  }

      node_alloc_holder &m_holder;
   };

   struct move_cloner
   {
      move_cloner(node_alloc_holder &holder)
         :  m_holder(holder)
      {}

      NodePtr operator()(Node &other)
      {  //Use get_real_data() instead of get_real_data to allow moving const key in [multi]map
         return m_holder.create_node(::boost::move(other.get_real_data()));
      }

      node_alloc_holder &m_holder;
   };

   ICont &non_const_icont() const
   {  return const_cast<ICont&>(this->m_icont);   }

   NodeAlloc &node_alloc()
   {  return static_cast<NodeAlloc &>(*this);   }

   const NodeAlloc &node_alloc() const
   {  return static_cast<const NodeAlloc &>(*this);   }

   public:
   ICont &icont()
   {  return this->m_icont;   }

   const ICont &icont() const
   {  return this->m_icont;   }

   private:
   //The intrusive container
   ICont m_icont;
};

}  //namespace dtl {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_DETAIL_NODE_ALLOC_HPP_
