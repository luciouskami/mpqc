//
// conc_cache.h
//
// Copyright (C) 2013 David Hollman
//
// Author: David Hollman
// Maintainer: DSH
// Created: Nov 19, 2013
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#include <cassert>
#include <utility>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/pop_back.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/size.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <boost/thread/future.hpp>

#include <world/worldhashmap.h>
#include <world/worldfut.h>


#ifndef _util_container_conc_cache_h
#define _util_container_conc_cache_h

namespace mpl = boost::mpl;

#define ALLOW_DYNAMIC_MUTEX_CREATION 0

#define USE_BOOST_THREAD 1
#if USE_BOOST_THREAD
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
using boost::mutex;
using boost::shared_mutex;
using boost::upgrade_mutex;
using boost::shared_lock;
using boost::unique_lock;
using boost::function;
using boost::lock;
using boost::defer_lock;
using boost::upgrade_lock;
using boost::upgrade_to_unique_lock;
#else
#include <mutex>
#include <shared_mutex>
using std::mutex;
// This requires C++14 support
using std::shared_lock;
using std::unique_lock;
using std::shared_mutex;
using std::function;
using std::lock;
using std::defer_lock;
using std::upgrade_lock;
using std::upgrade_to_unique_lock;
#endif


namespace boost_ext{
  // for_each that loops over several boost::tuples together

  // base case
  template<typename FuncT, int I = 0, typename... tuple_types>
  inline typename boost::enable_if_c<
      I == boost::tuples::length<
          typename mpl::at<
              typename mpl::vector<tuple_types...>::type, mpl::int_<0>
          >::type
      >::value,
      void
  >::type
  for_each(FuncT, tuple_types&...)
  { }

  template<typename FuncT, int I = 0, typename... tuple_types>
  inline typename boost::enable_if_c<
      I <= boost::tuples::length<
          typename mpl::at<
              typename mpl::vector<tuple_types...>::type, mpl::int_<0>
          >::type
      >::value - 1,
      void
  >::type
  for_each(FuncT f, tuple_types&... tups)
  {
    f(boost::get<I>(tups)...);
    for_each<FuncT, I + 1, tuple_types...>(f, tups...);
  }
} // end namespace boost_ext


namespace sc{
  namespace detail{
    struct hash_accumulate {
        madness::hashT& key_;
        hash_accumulate(madness::hashT& key) : key_(key) {}
        template <typename T>
        inline typename boost::enable_if<boost::is_convertible<T, int>, void>::type
        operator()(const T& item){
          madness::hash_combine(key_, (int)item);
        }
        template <typename T>
        inline typename boost::enable_if<mpl::not_<boost::is_convertible<T, int>>, void>::type
        operator()(const T& item){
          madness::hash_combine(key_, item);
        }
    };
  }
}

namespace boost {
  namespace tuples{
    template<typename... key_types>
    inline madness::hashT
    hash_value(boost::tuple<key_types...> keys) {
      using madness::hashT;
      madness::hashT key = 0;
      sc::detail::hash_accumulate hacc(key);
      boost_ext::for_each(hacc, keys);
      return key;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////

namespace sc {

namespace mpl_ext{
// mpl_ext::apply "splats" an mpl::vector (or any mpl back extensible sequence) into C++11 variadic
//   template arguments.
// credit: https://github.com/scientific-coder/Computer-Languages/blob/master/interpreting/apply.hxx
template<template<typename...> class T, bool empty, typename C, typename... Types> struct apply_helper {
  typedef typename boost::mpl::pop_back<C>::type rest;
  typedef typename apply_helper<T, boost::mpl::empty<rest>::value, rest, typename boost::mpl::back<C>::type, Types...>::type type;
};
template<template<typename...> class T,typename C, typename... Types> struct apply_helper<T, true, C, Types...> {
  typedef T<Types...> type;
};

template<template<typename...> class T,typename C> struct apply : apply_helper<T, boost::mpl::empty<C>::value, C> {};

template <typename T, int N>
struct repeated_vector{
  typedef typename mpl::copy<
      mpl::range_c<int, 0, N>,
      mpl::inserter<
        mpl::vector<>,
        mpl::push_back<mpl::_1, T>
      >
  >::type type;
};

} // end namespace mpl_ext

////////////////////////////////////////////////////////////////////////////////


// TODO This has a serious bug.  STL maps do not support simultaneous writes, even of different objects.
//        Thus, this class needs to lock the whole value map when writing, killing efficiency.
//        A workaround would be to add a boolean template parameter that only allows adding or changing
//        of keys in a setup phase, similarly to how mutex_map_mutexes_ functions now.  A more
//        perminant solution is going to be using Intel's Thread Building Blocks (TBB) library,
//        with the current implementation as a fallback when the compilation doesn't have TBB
//        available.

/**
 * A cache of objects that can be safely accessed concurrently by threads that share memory.
 */
template <
    typename val_type,
    typename... key_types
>
class ConcurrentCache {

  public:
    //----------------------------------------//
    // types for keys
    template<typename... Types> using tuple_type = boost::tuple<Types...>;
    typedef tuple_type<key_types...> key_tuple;
    //----------------------------------------//
    // types for map of values
    typedef val_type value_type;
    typedef boost::shared_future<value_type> future_value;
    typedef madness::ConcurrentHashMap<key_tuple, future_value, boost::hash<key_tuple>> future_map;
    typedef typename future_map::accessor future_map_accessor;
    typedef typename future_map::const_accessor future_map_const_accessor;


    value_type get(
        key_types... keys,
        const function<value_type()>& compute_fxn
    )
    {
      key_tuple k(boost::forward<key_types>(keys)...);
      future_map_accessor a;
      /* Madness futures version:
           if(cached_values_.insert(a,
                 std::make_pair(k, future_value::default_initializer())
           )){
             a->second = future_value();
             a->second.set(compute_fxn());
           }
      */
      // Boost version
      if(cached_values_.insert(a, std::make_pair(k, future_value()))){
        boost::promise<value_type> p;
        p.set_value(compute_fxn());
        a->second = p.get_future().share();
      }
      return a->second.get();

    }

  protected:

    // The actual map from key tuples to values
    future_map cached_values_;

};

} // end namespace sc
#endif /* _util_container_conc_cache_h */