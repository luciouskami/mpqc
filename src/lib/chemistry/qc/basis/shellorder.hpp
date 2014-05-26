//
// shellorder.hpp
//
// Copyright (C) 2013 Drew Lewis
//
// Authors: Drew Lewis
// Maintainer: Drew Lewis and Edward Valeev
//
// This file is part of the MPQC Toolkit.
//
// The MPQC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The MPQC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the MPQC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef CHEMISTRY_QC_BASIS_SHELLORDER_HPP
#define CHEMISTRY_QC_BASIS_SHELLORDER_HPP

#include<vector>
#include<string>
#include<cassert>

#include <Eigen/Dense>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/basis/basis.h>
#include <random>

#include "kcluster.hpp"

namespace mpqc{
namespace TA{

    /**
     * Determines the clustering of shells based on k-means clustering.
     */
    class ShellOrder {

    public:
        using Shell = sc::GaussianBasisSet::Shell;
        using Atom = KCluster::Atom;
        /// Each element represents the shell a new tile starts on.  So if the
        /// vector looks like | 0, 5, 6 ) then tile 0 is from 0-4, tile 1 has 5,
        /// tile 3 has 6.
        using ShellRange = std::vector<std::size_t>;
        using Vector3 = KCluster::Vector3;

        /**
         * Initializes ShellOrder with the atoms from the molecule and the shells
         * from the basis.
         */
        ShellOrder(const sc::Ref<sc::GaussianBasisSet> &basis) :
            clusters_(),
            atoms_(),
            basis_(basis),
            com_(Vector3::Zero(3))
        {
            // Get the molecule.
            sc::Ref<sc::Molecule> mol = basis->molecule();
            com_[0] = mol->center_of_mass()[0];
            com_[1] = mol->center_of_mass()[1];
            com_[2] = mol->center_of_mass()[2];

            // Get the Atoms and their index out of the molecule for the clusters.
            for(auto i = 0; i < mol->natom(); ++i){
               atoms_.push_back(Atom(mol->atom(i), i));
            }
        }

        /**
         * Returns a list of shells ordered by which cluster they belong to.
         * Must pass in the new basis so the shells get the correct parent.
         */
        std::vector<Shell>
        ordered_shells(std::size_t nclusters,
                       const sc::GaussianBasisSet *new_parent_basis){
            // Number of clusters desired.
            nclusters_ = nclusters;

            // Compute clusters using Lloyd's Algorithm
            compute_clusters(nclusters_);

            std::vector<Shell> shells = cluster_shells(new_parent_basis);

            return shells;
        }

        /**
         * Returns a a ShellRange which specifies what shell each tile starts on.
         */
        ShellRange shell_ranges() const {
            return compute_shell_ranges_by_cluster();
        }

    private:
        /*
         * Find clusters using Lloyd's algorithm.
         */
        void compute_clusters(std::size_t nclusters){
            // Make initial guess at clusters
            init_clusters();
            // Search for local minimium in terms of clustering.
            k_means_search();
        }

        /*
         * Determines initial guess for clusters.
         */
        void init_clusters(){
            // Sort atoms in molecule by distance from COM such that 1. Closest
            // . . . N. Farthest.  If two atoms are equidistant then sort based
            // on x, then y, and finally z.
            auto ordering = [&](const Atom &a, const Atom &b){
                // Get position vector for each atom.
                const Vector3 va = Eigen::Map<const Vector3>(a.r(),3);
                const Vector3 vb = Eigen::Map<const Vector3>(b.r(),3);

                // Find distance from center of mass for each atom
                double da = Vector3(va - com_).norm();
                double db = Vector3(vb - com_).norm();

                // Begin to check coordinates
                if(da != db) { // Check not equidistant
                    return (da < db);
                }
                else if(va[0] != vb[0]){ // Check x isn't equidistant
                    return (va[0] < vb[0]);
                }
                else if(va[1] != vb[1]){ // Check y isn't equidistant
                    return (va[1] < vb[1]);
                }
                else if(va[2] != vb[2]){ // Check z isn't equidistant
                    return (va[2] < vb[2]);
                }
                else {
                    return false;
                }
            };

            std::sort(atoms_.begin(), atoms_.end(), ordering);

            // Randomly pick starting atoms for guess
            initial_cluster_guess();

            // Attach atoms to the cluster which they are closest too.
            attach_to_closest_cluster();
        }

        /*
         * Attaches each atom to its closest cluster.
         */
        void attach_to_closest_cluster(){
            // Loop over all the atoms.
            for(const auto atom : atoms_){
                // Guess that first cluster is closest
                double smallest = clusters_[0].distance(atom);

                // To which cluster the atom belongs.
                std::size_t kindex = 0;

                // Loop over kclusters using i = 1 because we already computed 0
                for(auto i = 1; i < nclusters_; ++i){
                    // Compute distance from atom to next kcluster
                    double dist = clusters_[i].distance(atom);

                    // if closer update index info
                    if(dist < smallest){
                        kindex = i;
                        smallest = dist;
                    }
                }

                // Add atom to the closest kcluster
                clusters_[kindex].add_atom(atom);
            }
        }

        // Computes Lloyd's algorith to find a local minimium for the clusters.
        void k_means_search(std::size_t niter = 1000){

          bool converged = false;
          std::size_t iter = 0;
          while(!converged && iter < niter){
            std::vector<Vector3> temp_cluster_centers;
            for(auto &cluster : clusters_){
              temp_cluster_centers.push_back(cluster.center());
              cluster.guess_center();
            }
            attach_to_closest_cluster();

            for(auto i = 0; i < temp_cluster_centers.size(); ++i){
              // Check to see if the cluster center has changed
              auto& old_center = temp_cluster_centers[i];
              auto& new_center = clusters_[i].center();
              auto center_diff = (old_center - new_center).norm();

              // If the center is basically the same then converged = true
              // and go check next center
              converged = (center_diff < 1e-14) ? true : false;

              // As soon as a single center is not converged break out and
              // compute new centers
              if(!converged){break;}
            }
            ++iter;
          }



            //// Initial search for starting minimum
            //for(auto i = 0; i < niter; ++i){

            //   // Recompute the center of the cluster using the centroid of
            //   // the atoms.  Will lose information about which atoms
            //   // go with which center.
            //   for(auto &cluster : clusters_){ cluster.guess_center(); }

            //   attach_to_closest_cluster();

            // }

           sort_clusters(); // Sort the clusters and their atoms
        }


        /*
         * Returns a vector of shells in the order they appear in the clusters.
         * Need to pass in a basis which will become the parent of the new Shells
         */
        std::vector<Shell>
        cluster_shells(const sc::GaussianBasisSet *new_parent_basis) const {

            std::vector<Shell> shells;
            // Loop over clusters
            for(const auto& cluster : clusters_){
                // Loop over atoms in cluster
                for(const auto& atom : cluster.atoms()){
                    // Figure out where in the molecule the atom is located.
                    std::size_t center_ = atom.mol_index();
                    // Figure out how many shells are on the atom.
                    std::size_t nshells_on_atom =
                                    basis_->nshell_on_center(center_);
                    // Loop over the shells on the atom and pack them into
                    // the shell contatiner.
                    for(auto i = 0; i < nshells_on_atom; ++i){
                        shells.push_back(Shell(
                                new_parent_basis, center_,
                                basis_->operator()(center_, i)));
                    }
                }
            }

            return shells;
        }

        ShellRange compute_shell_ranges_by_cluster() const {
          // Initialize Range object
          ShellRange range;
          range.reserve(nclusters_+1);
          range.push_back(0);

          // Loop over clusters
          for(auto i = 0; i < nclusters_; ++i){
            // Holds the number of shells on the cluster.
            std::size_t shells_in_cluster = 0;
            // Loop over atoms
            for(const auto atom : clusters_[i].atoms()){
              // position of atom in molecule
              std::size_t atom_index = atom.mol_index();
              // Get number of shells on the atom.
              shells_in_cluster += basis_->nshell_on_center(atom_index);
            }
            // Compute the Starting Shell of the next tile.
            range.push_back(range.at(i) + shells_in_cluster);
          }

          return range;
        }

        /*
         * Returns a ShellRange that contains the shells included on each cluster.
         */
        ShellRange compute_shell_ranges_for_size() const {
            ShellRange range;
            range.reserve(nclusters_+1);
            // First range is easy
            range.push_back(0);

            // First loop over atoms and package them
            std::vector<Atom> range_atoms;
            for(const auto &cluster : clusters_){
              for(const auto atom : cluster.atoms()){
                range_atoms.push_back(atom);
              }
            }

            // Guess how many functions shoule be on each tile
            double nfuncs_average = double(basis_->nbasis())/double(nclusters_);
            double min = basis_->nbasis(); // starting min for algo

            // Will store the atoms that go on each tile
            for(std::size_t i = 0; i < range_atoms.size(); ++i){
              auto first = range_atoms.begin();
              auto last = range_atoms.end();

              std::vector<std::size_t> func_in_cluster;
              std::vector<std::vector<Atom> > atoms_in_cluster(nclusters_);
              for(auto &cluster_atoms : atoms_in_cluster){

                if(first != last){
                  // Compute the last atom on tile based on loop
                  auto tile_end = (i > std::distance(first, last)) ? last :
                                                                    first + i;
                  // Store the number of funcs
                  std::size_t nfuncs = 0;
                  for(auto it = first; it != tile_end; ++it){
                    nfuncs += basis_->nbasis_on_center(it->mol_index());
                    cluster_atoms.push_back(*it); // Count number of atoms
                  }

                  // Imposing a large penalty for havning zero funcs on a tile.
                  nfuncs = (nfuncs != 0) ? nfuncs : 2 * min + 1000;

                  func_in_cluster.push_back(nfuncs);

                  first = tile_end; // Start next tile where this one left off
                }else{
                  func_in_cluster.push_back(0);
                }
              } // End loop over clusters

              // Compute sum of goal funcs minus nfuncs per cluster
              double iteration_guess = 0;
              for(auto nfuncs : func_in_cluster){
                iteration_guess += std::abs(nfuncs - nfuncs_average);
              }

              // See if we have a good grouping
              if(iteration_guess < min){
                // Loop over clusters
                for(auto i = 1; i <= atoms_in_cluster.size(); ++i){
                  // Loop over atoms in each cluster
                  std::size_t shells_in_cluster = 0;
                  for(auto j = 0; j < atoms_in_cluster[i-1].size(); ++j){
                    auto atom_index = atoms_in_cluster[i-1][j].mol_index();
                    shells_in_cluster += basis_->nshell_on_center(atom_index);
                  }
                  range[i] = range[i-1] + shells_in_cluster;
                }
                min = iteration_guess;
              } // end minimization check
            }

            return  range;
        }

        /*
         * Sort clusters on distance from the cluster in front of it.
         */
        void sort_clusters(){
          // Loop over clusters and sort the ones that come after the current one
          // Based on distance from the current one.
          for(auto i = 0; i < nclusters_; ++i){
            auto center = clusters_[i].center();
            auto it = clusters_.begin() + i + 1; // Get iterator to next tile
            auto end = clusters_.end();

            // For clusters after the current one sort their atoms based on
            // closest to the current center. Do this because in the next
            // step we are going to find the cluster with atoms closest
            // to the current one.
            for(auto rest_it = it; rest_it != end; ++rest_it){
              rest_it->sort_atoms(center);
            }

            // Sort clusters after current cluster based on distance
            // of first atom to current cluster
            std::stable_sort(it, end, [&](const KCluster &a,
                                          const KCluster &b){
                Vector3 a_first_atom_center = a.atoms().front().center();
                Vector3 b_first_atom_center = b.atoms().front().center();
                return (a_first_atom_center - center).norm() <
                       (b_first_atom_center - center).norm();
              }
            );

          }
        }

        void initial_cluster_guess(const std::size_t seed = 90){
            // Initialize the kcluster guess by randomly guessing first cluster
            // and then using k-means++ algorithm to choose rest of clusters.
            clusters_.reserve(nclusters_);

            // To start off we will favor all atoms equally
            std::vector<std::size_t> weights(atoms_.size(), 1);

            // Create a generator
            std::default_random_engine generator(seed); // Always same seed if user doesn't change it

            // Guess the initial centers
            for(auto i = 0; i < nclusters_; ++i){
              // Create the distribution
              std::discrete_distribution<unsigned int> dist(weights.begin(),
                                                            weights.end());
              // Choose atom
              unsigned int atom_index = dist(generator);
              clusters_.push_back(Vector3(atoms_[atom_index].r(0),
                                          atoms_[atom_index].r(1),
                                          atoms_[atom_index].r(2) )
                                  );

              // Copy the center we just assigned.
              auto last_center = atoms_[atom_index].center();

              // Set the weight of the atom we picked to zero
              weights[atom_index] = 0;

              // If the weight of the atom isn't zero set it to D^2 from the previouse center
              for(auto z = 0; z < weights.size(); ++z){
                if(weights[z] != 0){
                  // Perform D^2 weighting look up k-means++ algorithm
                  weights[z] = std::pow((last_center - atoms_[z].center()).norm(),2);
                }
              }

            }
        }

    private:
        std::size_t nclusters_ = 0;
        std::vector<Atom> atoms_;
        Vector3 com_;
        std::vector<KCluster> clusters_; // Note that KCluster contains fixed
                                         // Eigen objects, but these are not vectorizable
                                         // Thus we don't have to worry about Alignment issues.
        sc::Ref<sc::GaussianBasisSet> basis_;
    };

} // namespace TA
} // namespace mpqc

#endif /* CHEMISTRY_QC_BASIS_SHELLORDER_HPP */
