//
// parenthesis2t.cc
//
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki.toru@gmail.com>
// Maintainer: TS
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

#include <algorithm>
#include <chemistry/qc/ccr12/parenthesis2t.h>

using namespace sc;
  
static ClassDesc Parenthesis2t_cd(
  typeid(Parenthesis2t),"Parenthesis2t",1,"public RefCount"
  ,0,0,0);
  
Parenthesis2t::Parenthesis2t(CCR12_Info* info): z(info){
}


Parenthesis2t::~Parenthesis2t(){
}


double Parenthesis2t::compute_energy(Ref<Parenthesis2tNum> eval_left, 
                                     Ref<Parenthesis2tNum> eval_right){

 double energy = 0.0;
 double dummy = 0.0; // will be used, not be referred

 // precalculating the intermediates
 eval_left->compute_amp(&dummy, 0L, 0L, 0L, 0L, 0L, 0L, 1L);
 eval_right->compute_amp(&dummy, 0L, 0L, 0L, 0L, 0L, 0L, 1L);

 long count = 0L;

 const size_t maxsize1 = z->maxtilesize();
 const size_t maxsize3 = maxsize1 * maxsize1 * maxsize1;
 const size_t size_alloc = maxsize3 * maxsize3;

 double* data_right = z->mem()->malloc_local_double(size_alloc);
 double* data_left = z->mem()->malloc_local_double(size_alloc);
 double* data_left_sorted = z->mem()->malloc_local_double(size_alloc);

 for (long t_p4b = z->noab(); t_p4b<z->noab() + z->nvab(); ++t_p4b){
  for (long t_p5b = t_p4b;     t_p5b<z->noab() + z->nvab(); ++t_p5b){
   for (long t_p6b = t_p5b;     t_p6b<z->noab() + z->nvab(); ++t_p6b){

    for (long t_h1b = 0L;    t_h1b<z->noab(); ++t_h1b){
     for (long t_h2b = t_h1b; t_h2b<z->noab(); ++t_h2b){
      for (long t_h3b  =t_h2b; t_h3b<z->noab(); ++t_h3b){
// this will be updated
       if (count%z->mem()->n() == z->mem()->me()){ 
 
        if(!z->restricted() ||  z->get_spin(t_p4b) + z->get_spin(t_p5b) + z->get_spin(t_p6b)
                              + z->get_spin(t_h1b) + z->get_spin(t_h2b) + z->get_spin(t_h3b) < 9L){
        if(z->get_spin(t_p4b) + z->get_spin(t_p5b) + z->get_spin(t_p6b) == z->get_spin(t_h1b) + z->get_spin(t_h2b) + z->get_spin(t_h3b)){
        if((z->get_sym(t_p4b)^(z->get_sym(t_p5b)^(z->get_sym(t_p6b)^(z->get_sym(t_h1b)^(z->get_sym(t_h2b)^z->get_sym(t_h3b)))))) == 0L){

         const long size = z->get_range(t_p4b) * z->get_range(t_p5b) * z->get_range(t_p6b)
                         * z->get_range(t_h1b) * z->get_range(t_h2b) * z->get_range(t_h3b);

         std::fill(data_right, data_right + size, 0.0);
         std::fill(data_left, data_left + size, 0.0);
 
         eval_left->compute_amp( data_left,  t_h1b, t_h2b, t_h3b, t_p4b, t_p5b, t_p6b, 2L);  
         eval_right->compute_amp(data_right, t_p4b, t_p5b, t_p6b, t_h1b, t_h2b, t_h3b, 2L);  
 
#if 0
         z->sort_indices6(data_left, data_left_sorted, z->get_range(t_h1b), z->get_range(t_h2b), z->get_range(t_h3b),
                                                       z->get_range(t_p4b), z->get_range(t_p5b), z->get_range(t_p6b),
                                                       3, 4, 5, 0, 1, 2, 1.0);
#else
         z->sort_indices2(data_left, data_left_sorted, z->get_range(t_h1b)*z->get_range(t_h2b)*z->get_range(t_h3b),
                                                       z->get_range(t_p4b)*z->get_range(t_p5b)*z->get_range(t_p6b),
                                                       1, 0, 1.0);
#endif
 
         double factor = 1.0;
         if (z->restricted()) factor *= 2.0; 
 
         if      (t_p4b == t_p5b && t_p5b == t_p6b) factor *= 0.166666666666666667;
         else if (t_p4b == t_p5b || t_p5b == t_p6b) factor *= 0.5; 
 
         if      (t_h1b == t_h2b && t_h2b == t_h3b) factor *= 0.166666666666666667; 
         else if (t_h1b == t_h2b || t_h2b == t_h3b) factor *= 0.5; 
 
         long iall = 0L;
         for (long p4 = 0L; p4 < z->get_range(t_p4b); ++p4) {
          const double ep4 = z->get_orb_energy(z->get_offset(t_p4b) + p4);
          for (long p5 = 0L; p5 < z->get_range(t_p5b); ++p5) {
           const double ep5 = z->get_orb_energy(z->get_offset(t_p5b) + p5);
           for (long p6 = 0L; p6 < z->get_range(t_p6b); ++p6) {
            const double ep6 = z->get_orb_energy(z->get_offset(t_p6b) + p6);

            for (long h1 = 0L; h1 < z->get_range(t_h1b); ++h1) {
             const double eh1 = z->get_orb_energy(z->get_offset(t_h1b) + h1);
             for (long h2 = 0L; h2 < z->get_range(t_h2b); ++h2) {
              const double eh2 = z->get_orb_energy(z->get_offset(t_h2b) + h2);
              const double eps = ep4 + ep5 + ep6 - eh1 - eh2;
              for (long h3 = 0L; h3 < z->get_range(t_h3b); ++h3, ++iall) {
               const double eh3 = z->get_orb_energy(z->get_offset(t_h3b) + h3);
 
               const double numerator= data_left_sorted[iall] * data_right[iall] * factor;
               energy += numerator / (eh3 - eps);
              }
             }
            }
           }
          }
         }
        }
        }
        }
       } // end of parallel loop
       ++count;
      }
     }
    }
   }
  }
 }
 z->mem()->free_local_double(data_left_sorted);
 z->mem()->free_local_double(data_left);
 z->mem()->free_local_double(data_right);


 // deallocating the intermediates
 eval_left->compute_amp(&dummy, 0L, 0L, 0L, 0L, 0L, 0L, 3L);
 eval_right->compute_amp(&dummy, 0L, 0L, 0L, 0L, 0L, 0L, 3L);

 z->mem()->sync();
 Ref<MessageGrp> msg_ = MessageGrp::get_default_messagegrp();
 msg_->sum(energy);

 return energy;
}  

