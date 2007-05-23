//
// tbintcca.h
//
// Copyright (C) 2004, Sandia National Laboratories
//
// Author: Joe Kenny <jpkenny@sandia.gov>
// Maintainer: Joe Kenny
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

#ifdef __GNUG__
#pragma interface
#endif

#ifndef _chemistry_cca_int_tbintcca_h
#define _chemistry_cca_int_tbintcca_h

#include <vector>
#include <map>
#include <sidl_cxx.hxx>
#include <chemistry/qc/basis/tbint.h>
#include <Chemistry_QC_GaussianBasis_IntegralEvaluatorFactoryInterface.hxx>
#include <Chemistry_QC_GaussianBasis_DerivCentersInterface.hxx>
#include <MPQC_GaussianBasisMolecular.hxx>

namespace sc {

  //////////////////////////////////////////////////////////////////////////

/** This implements two body integrals through the CCA interface. */

  class TwoBodyIntCCA : public TwoBodyInt {
  
  private:

    Integral* integral_;
    Ref<GaussianBasisSet> bs1_, bs2_, bs3_, bs4_;
    Chemistry::QC::GaussianBasis::IntegralEvaluatorFactoryInterface eval_factory_;
    Chemistry::QC::GaussianBasis::CompositeIntegralDescrInterface cdesc_;
    std::vector< Chemistry::QC::GaussianBasis::IntegralDescrInterface > descriptors_;
    std::vector< std::string > types_;
    MPQC::GaussianBasisMolecular cca_bs1_, cca_bs2_, cca_bs3_, cca_bs4_;
    Chemistry::QC::GaussianBasis::IntegralEvaluator4Interface eval_;
    int int_bound_min_;
    int ndesc_;
    double tol_;
    double loginv_;
    std::map< std::string, int > dtype_to_tbtype_;
    double** tbtype_to_buf_;
    sidl::array<double> sidl_buffer_;
    std::vector< int > segments_;

  public:
    TwoBodyIntCCA( Integral* integral,
		   const Ref<GaussianBasisSet>&b1, 
		   const Ref<GaussianBasisSet>&b2,
		   const Ref<GaussianBasisSet>&b3, 
		   const Ref<GaussianBasisSet>&b4,	   
		   Chemistry::QC::GaussianBasis::IntegralEvaluatorFactoryInterface,
		   Chemistry::QC::GaussianBasis::CompositeIntegralDescrInterface );
    ~TwoBodyIntCCA();

    void compute_shell(int,int,int,int);
    const double* buffer(tbint_type te_type) const;
    const double* buffer_not_const();
    int log2_shell_bound(int,int,int,int);
    int redundant() const { return redundant_; }
    void set_redundant(int i) { redundant_ = i; }
    unsigned int num_tbint_types() const;
    void remove_redundant(int,int,int,int);
};


  ////////////////////////////////////////////////////////////////////////////

  /** This implements two body derivative integrals through the 
      CCA interface. */
  class TwoBodyDerivIntCCA : public TwoBodyDerivInt {

  private:
    Integral* integral_;
    Ref<GaussianBasisSet> bs1_, bs2_, bs3_, bs4_;
    Chemistry::QC::GaussianBasis::IntegralEvaluatorFactoryInterface eval_factory_;
    Chemistry::QC::GaussianBasis::CompositeIntegralDescrInterface cdesc_;
    std::vector< Chemistry::QC::GaussianBasis::IntegralDescrInterface > descriptors_;
    std::vector< std::string > types_;
    MPQC::GaussianBasisMolecular cca_bs1_, cca_bs2_, cca_bs3_, cca_bs4_;
    double* buff_;
    Chemistry::QC::GaussianBasis::IntegralEvaluator4Interface eval_;
    bool redundant_;
    double tol_;
    double loginv_;
    int int_bound_min_;
    int max_deriv_lvl_;
    int ndesc_;
    std::vector<Chemistry::QC::GaussianBasis::DerivCentersInterface> cca_dcs_;
    sidl::array<double> sidl_buffer_;
    std::string type_;
    std::vector< int > segments_;
    bool fast_deriv_;
    std::vector<sc::DerivCenters> sc_dcs_;
    std::map< std::string, int > dtype_to_tbtype_;
    double** tbtype_to_buf_;

  public:
    TwoBodyDerivIntCCA( Integral* integral,
			const Ref<GaussianBasisSet>&b1, 
			const Ref<GaussianBasisSet>&b2,
			const Ref<GaussianBasisSet>&b3, 
			const Ref<GaussianBasisSet>&b4,	   
			Chemistry::QC::GaussianBasis::IntegralEvaluatorFactoryInterface,
			Chemistry::QC::GaussianBasis::CompositeIntegralDescrInterface );
    ~TwoBodyDerivIntCCA();

    void compute_shell(int,int,int,int,DerivCenters&);
    int log2_shell_bound(int,int,int,int);
    int redundant() const { return redundant_; }
    void set_redundant(int i) { redundant_ = i; }
    unsigned int num_tbint_types() const;
    void copy_buffer(int);
  };
  
}

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
