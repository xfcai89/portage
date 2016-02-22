/*---------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *---------------------------------------------------------------------------~*/

#ifndef REMAP_1STORDER_H_
#define REMAP_1STORDER_H_


#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "portage/wrappers/mesh/jali/jali_mesh_wrapper.h"
#include "portage/wrappers/state/jali/jali_state_wrapper.h"

namespace Portage {

/*!
  @class Remap_1stOrder remap_1st_order.h
  @brief Remap_1stOrder does a 1st order remap of scalars
  @tparam MeshType The type of the mesh wrapper used to access mesh info
  @tparam StateType The type of the state manager used to access data.
  @tparam OnWhatType The type of entity-based data we wish to remap; e.g. does
  it live on nodes, cells, edges, etc.

  Viewed simply, the value at target cell is the weighted average of
  values on from source entities and therefore, this can work for
  cell-cell, particle-cell, cell-particle and particle-particle remap.

  In the context of remapping from one cell-based mesh to another (as
  opposed to particles to cells), it is assumed that scalars are
  provided at cell centers. A piecewise constant "reconstruction" of
  the quantity is assumed over cells which means that the integral
  value over the cell or any piece of the cell is just the average
  multiplied by the volume of the cell or its piece. Then the integral
  value over a target cell is the sum of the integral values of some
  source cells (or their pieces) and the weights to be specified in
  the call are the areas/volumes of the donor cells (pieces). If an
  exact intersection is performed between the target cell and the
  source mesh, these weights are the area/volumes of the intersection
  pieces. So, in this sense, this is the Cell-Intersection-Based
  Donor-Cell (CIB/DC) remap referred to in the Shashkov, Margolin
  paper [1]. This remap is 1st order accurate and positivity
  preserving (target cell values will be positive if the field is
  positive on the source mesh).

  [1] Margolin, L.G. and Shashkov, M.J. "Second-order sign-preserving
  conservative interpolation (remapping) on general grids." Journal of
  Computational Physics, v 184, n 1, pp. 266-298, 2003.

  [2] Dukowicz, J.K. and Kodis, J.W. "Accurate Conservative Remapping
  (Rezoning) for Arbitrary Lagrangian-Eulerian Computations," SIAM
  Journal on Scientific and Statistical Computing, Vol. 8, No. 3,
  pp. 305-321, 1987.

  @todo Template on variable type??

*/

template<typename MeshType, typename StateType, Entity_kind on_what>
class Remap_1stOrder {
 public:
  /*!
    @brief Constructor.
    @param[in] source_mesh The input mesh.
    @param[in] source_state The state manager for data on the input mesh.
    @param[in] on_what The location where the data lives; e.g. on cells, nodes, edges, etc.
    @param[in] remap_var_name The string name of the variable to remap.
   */
  Remap_1stOrder(MeshType const & source_mesh, StateType const & source_state,
                 std::string const remap_var_name) :
      source_mesh_(source_mesh),
      source_state_(source_state),
      remap_var_name_(remap_var_name),
      source_vals_(NULL)
  {
    source_state.get_data(on_what, remap_var_name, &source_vals_);
  }


  /// Copy constructor (disabled)
  Remap_1stOrder(const Remap_1stOrder &) = delete;

  /// Assignment operator (disabled)
  Remap_1stOrder & operator = (const Remap_1stOrder &) = delete;

  /// Destructor
  ~Remap_1stOrder() {}


  /*!
    @brief Functor to do the actual remap calculation.
    @param[in] sources_and_weights A pair of two vectors.
    @c sources_and_weights.first() is the vector of source entity indices
    in the source mesh that will contribute to the current target mesh entity.
    @c sources_and_weights.second() is the vector of vector weights for each
    of the source mesh entities in @c sources_and_weights.first().  Each element
    of the weights vector is a moment of the source data over the target
    entity; for first order remap, only the first element (or zero'th moment)
    of the weights vector (i.e. the volume of intersection) is used. Source
    entities may be repeated in the list if the intersection of a target entity
    and a source entity consists of two or more disjoint pieces

    @todo Cleanup the datatype for sources_and_weights - it is somewhat confusing.
    @todo SHOULD WE USE boost::tuple FOR SENDING IN SOURCES_AND_WEIGHTS SO
    THAT WE CAN USE boost::zip_iterator TO ITERATOR OVER IT?

   */

  double
  operator() (std::pair<std::vector<int> const &,
              std::vector<std::vector<double>> const &> cells_and_weights)
      const;

 private:
  MeshType const & source_mesh_;
  StateType const & source_state_;
  std::string const & remap_var_name_;
  double * source_vals_;
};


/*!
  @brief 1st order remap operator on general entity types
  @param[in] sources_and_weights Pair containing vector of contributing source entities and vector of contribution weights
*/

template<typename MeshType, typename StateType, Entity_kind on_what>
double Remap_1stOrder<MeshType, StateType, on_what> :: operator()
    (std::pair<std::vector<int> const &,
     std::vector< std::vector<double> > const &> sources_and_weights) const {

  std::vector<int> const & source_cells = sources_and_weights.first;
  int nsrccells = source_cells.size();
  if (!nsrccells) {
    std::cerr << "ERROR: No source cells contribute to target cell?" <<
        std::endl;
    return 0.0;
  }

  std::vector<std::vector<double>> const & weights = sources_and_weights.second;
  if (weights.size() < nsrccells) {
    std::cerr << "ERROR: Not enough weights provided for remapping " <<
        std::endl;
    return 0.0;
  }

  // contribution of the source cell is its field value weighted by
  // its "weight" (in this case, its 0th moment/area/volume)

  /// @todo Should use zip_iterator here but I am not sure I know how to

  double val = 0.0;
  double sumofweights = 0.0;
  for (int j = 0; j < nsrccells; ++j) {
    int srccell = source_cells[j];
    std::vector<double> pair_weights = weights[j];

    val += source_vals_[srccell] * pair_weights[0];  // 1st order
    sumofweights += pair_weights[0];
  }

  // Normalize the value by sum of all the weights

  val /= sumofweights;

  return val;
}


}  // namespace Portage

#endif  // REMAP_1ST_ORDER_H_
