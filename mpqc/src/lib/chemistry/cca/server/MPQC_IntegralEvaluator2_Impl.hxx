// 
// File:          MPQC_IntegralEvaluator2_Impl.hxx
// Symbol:        MPQC.IntegralEvaluator2-v0.2
// Symbol Type:   class
// Description:   Server-side implementation for MPQC.IntegralEvaluator2
// 
// WARNING: Automatically generated; only changes within splicers preserved
// 
// 

#ifndef included_MPQC_IntegralEvaluator2_Impl_hxx
#define included_MPQC_IntegralEvaluator2_Impl_hxx

#ifndef included_sidl_cxx_hxx
#include "sidl_cxx.hxx"
#endif
#ifndef included_MPQC_IntegralEvaluator2_IOR_h
#include "MPQC_IntegralEvaluator2_IOR.h"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_CompositeIntegralDescrInterface_hxx
#include "Chemistry_QC_GaussianBasis_CompositeIntegralDescrInterface.hxx"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_IntegralDescrInterface_hxx
#include "Chemistry_QC_GaussianBasis_IntegralDescrInterface.hxx"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_IntegralEvaluator2Interface_hxx
#include "Chemistry_QC_GaussianBasis_IntegralEvaluator2Interface.hxx"
#endif
#ifndef included_Chemistry_QC_GaussianBasis_MolecularInterface_hxx
#include "Chemistry_QC_GaussianBasis_MolecularInterface.hxx"
#endif
#ifndef included_MPQC_IntegralEvaluator2_hxx
#include "MPQC_IntegralEvaluator2.hxx"
#endif
#ifndef included_sidl_BaseClass_hxx
#include "sidl_BaseClass.hxx"
#endif
#ifndef included_sidl_BaseInterface_hxx
#include "sidl_BaseInterface.hxx"
#endif
#ifndef included_sidl_ClassInfo_hxx
#include "sidl_ClassInfo.hxx"
#endif


// DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator2._includes)
#include "integral_evaluator.h"
#include "reorder_engine.h"
using namespace sc;
using namespace Chemistry::QC::GaussianBasis;
using namespace MpqcCca;
// DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator2._includes)

namespace MPQC { 

  /**
   * Symbol "MPQC.IntegralEvaluator2" (version 0.2)
   */
  class IntegralEvaluator2_impl : public virtual ::MPQC::IntegralEvaluator2 
  // DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator2._inherits)

  /** IntegralEvaluator2_impl implements a class interface for
      supplying 2-center molecular integrals.

      This is an implementation of a SIDL interface.
      The stub code is generated by the Babel tool.  Do not make
      modifications outside of splicer blocks, as these will be lost.
      This is a server implementation for a Babel class, the Babel
      client code is provided by the cca-chem-generic package.
   */

  // Put additional inheritance here...
  // DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator2._inherits)
  {

  // All data marked protected will be accessable by 
  // descendant Impl classes
  protected:

    bool _wrapped;

    // DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator2._implementation)

    IntegralEvaluator< OneBodyInt, onebody_computer > eval_; 
    IntegralEvaluator< OneBodyDerivInt, onebody_deriv_computer > deriv_eval_;
    Ref<GaussianBasisSet> bs1_, bs2_, bs3_, bs4_;
    bool reorder_;
    ReorderEngine reorder_engine_;
    onebody_computer computer_;
    onebody_deriv_computer deriv_computer_;
    BufferSize buffer_size_;

    // DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator2._implementation)

  public:
    // default constructor, used for data wrapping(required)
    IntegralEvaluator2_impl();
    // sidl constructor (required)
    // Note: alternate Skel constructor doesn't call addref()
    // (fixes bug #275)
    IntegralEvaluator2_impl( struct MPQC_IntegralEvaluator2__object * s ) : 
      StubBase(s,true), _wrapped(false) { _ctor(); }

    // user defined construction
    void _ctor();

    // virtual destructor (required)
    virtual ~IntegralEvaluator2_impl() { _dtor(); }

    // user defined destruction
    void _dtor();

    // true if this object was created by a user newing the impl
    inline bool _isWrapped() {return _wrapped;}

    // static class initializer
    static void _load();

  public:

    /**
     * user defined non-static method.
     */
    void
    add_evaluator_impl (
      /* in */void* eval,
      /* in */::Chemistry::QC::GaussianBasis::IntegralDescrInterface desc
    )
    ;

    /**
     * user defined non-static method.
     */
    void
    set_basis_impl (
      /* in */::Chemistry::QC::GaussianBasis::MolecularInterface bs1,
      /* in */::Chemistry::QC::GaussianBasis::MolecularInterface bs2
    )
    ;

    /**
     * user defined non-static method.
     */
    void
    init_reorder_impl() ;

    /**
     *  Deprecated -- do not use !!! 
     */
    void*
    get_buffer_impl (
      /* in */::Chemistry::QC::GaussianBasis::IntegralDescrInterface desc
    )
    ;


    /**
     *  Get sidl array buffer for given type.
     * @param desc Integral descriptor.
     * @return Sidl array. 
     */
    ::sidl::array<double>
    get_array_impl (
      /* in */::Chemistry::QC::GaussianBasis::IntegralDescrInterface desc
    )
    ;

    /**
     * user defined non-static method.
     */
    ::Chemistry::QC::GaussianBasis::CompositeIntegralDescrInterface
    get_descriptor_impl() ;

    /**
     *  This should be called when the object is no longer needed.
     * No other members may be called after finalize. 
     */
    int32_t
    finalize_impl() ;

    /**
     *  Compute a shell doublet of integrals.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2.
     * @param deriv_level Derivative level. 
     */
    void
    compute_impl (
      /* in */int64_t shellnum1,
      /* in */int64_t shellnum2
    )
    ;


    /**
     *  Deprecated -- do not use !!! 
     */
    ::sidl::array<double>
    compute_array_impl (
      /* in */const ::std::string& type,
      /* in */int32_t deriv_lvl,
      /* in */int64_t shellnum1,
      /* in */int64_t shellnum2
    )
    ;


    /**
     *  Compute integral bounds.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2. 
     */
    double
    compute_bounds_impl (
      /* in */int64_t shellnum1,
      /* in */int64_t shellnum2
    )
    ;


    /**
     *  Compute array of integral bounds.
     * @param shellnum1 Gaussian shell number 1.
     * @param shellnum2 Gaussian shell number 2. 
     */
    ::sidl::array<double>
    compute_bounds_array_impl (
      /* in */int64_t shellnum1,
      /* in */int64_t shellnum2
    )
    ;

  };  // end class IntegralEvaluator2_impl

} // end namespace MPQC

// DO-NOT-DELETE splicer.begin(MPQC.IntegralEvaluator2._misc)
// Put miscellaneous things here...
// DO-NOT-DELETE splicer.end(MPQC.IntegralEvaluator2._misc)

#endif
