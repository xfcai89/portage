/*
  This file is part of the Ristra portage project.
  Please see the license file at the root of this repository, or at:
  https://github.com/laristra/portage/blob/master/LICENSE
*/

#ifndef PORTAGE_GENREMAP_DRIVER_H_
#define PORTAGE_GENREMAP_DRIVER_H_

#include <sys/time.h>

#include <algorithm>
#include <vector>
#include <iterator>
#include <string>
#include <utility>
#include <iostream>
#include <type_traits>
#include <memory>
#include <limits>

#ifdef HAVE_TANGRAM
#include "tangram/driver/driver.h"

#include "portage/intersect/dummy_interface_reconstructor.h"
#endif

#include "portage/support/portage.h"

#include "portage/search/search_kdtree.h"
#include "portage/intersect/intersect_r2d.h"
#include "portage/intersect/intersect_r3d.h"
#include "portage/interpolate/interpolate_1st_order.h"
#include "portage/interpolate/interpolate_2nd_order.h"
#include "wonton/mesh/flat/flat_mesh_wrapper.h"
#include "wonton/state/flat/flat_state_mm_wrapper.h"
#include "wonton/support/Point.h"
#include "wonton/state/state_vector_multi.h"
#include "portage/driver/fix_mismatch.h"
#include "portage/driver/coredriver.h"


#ifdef PORTAGE_ENABLE_MPI
#include "portage/distributed/mpi_bounding_boxes.h"
#endif

/*!
  @file genremapdriver.h
  @brief Fancy remapping driver that does EVERYTHING :-)

  Remap mesh and material variables from mesh to mesh (in serial or
  distributed settings) with the option of doing a part-by-part remap
  (remap between sets of source and target entities)
*/

namespace Portage {



using Wonton::CELL;
using Wonton::NODE;
using Wonton::Flat_Mesh_Wrapper;
using Wonton::Flat_State_Wrapper;

/*!
  @class FancyDriver "driver.h"
  @brief FancyDriver provides the API to mapping multi-material data from one mesh to another in a general way

  @tparam SourceMesh A lightweight wrapper to a specific input mesh
  implementation that provides certain functionality.

  @tparam SourceState A lightweight wrapper to a specific input state
  manager implementation that provides certain functionality.

  @tparam TargetMesh A lightweight wrapper to a specific target mesh
  implementation that provides certain functionality.

  @tparam TargetState A lightweight wrapper to a specific target state
  manager implementation that provides certain functionality.

  @tparam InterfaceReconstructorType An interface reconstruction class
  that takes the raw interface reconstruction method, the dimension of
  the problem and the source mesh class as template parameters

  @tparam Matpoly_Splitter A polygon/polyhedron splitting class (returns both pieces of the polygon)

  @tparam Matpoly_Clipper A polygon/polyhedron clipping class (returns only the piece below/behind the clipping plane)

  @tparam CoordinateSystem 
*/
template <int D,
          class SourceMesh, class SourceState,
          class TargetMesh = SourceMesh, class TargetState = SourceState,
          template <class, int, class, class> class InterfaceReconstructorType =
          DummyInterfaceReconstructor,
          class Matpoly_Splitter = void, class Matpoly_Clipper = void,
          class CoordSys = Wonton::DefaultCoordSys
          >
class FancyDriver {
 public:

  // A couple of shorthand notations
  
  using SerialDriverType =
      CoreDriverBase<D, SourceMesh, SourceState,
                     TargetMesh, TargetState,
                     InterfaceReconstructorType,
                     Matpoly_Splitter, Matpoly_Clipper, CoordSys>;
  
  using ParallelDriverType =
      CoreDriverBase<D, Flat_Mesh_Wrapper<>,
                     Flat_State_Wrapper<Flat_Mesh_Wrapper<>>,
                     TargetMesh, TargetState,
                     InterfaceReconstructorType,
                     Matpoly_Splitter, Matpoly_Clipper, CoordSys>;
  
  /*!
    @brief Constructor for the remap driver.
    @param[in] source_mesh A @c  wrapper to the source mesh.
    @param[in] source_state A @c wrapper for the data that lives on the
    source mesh
    @param[in] target_mesh A @c TargetMesh to the target mesh
    @param[in,out] target_state A @c TargetState for the data that will
    be mapped to the target mesh
    @param[in] source_vars_to_remap  Optional list of source variables to remap
    (if not everything will be remapped)
    @param[in] executor  pointer to an executor allowing us choose between serial and parallel runs
  */
  FancyDriver(SourceMesh const& source_mesh,
              SourceState const& source_state,
              TargetMesh const& target_mesh,
              TargetState& target_state,
              std::vector<std::string> source_vars_to_remap,
              Wonton::Executor_type const *executor = nullptr,
              std::string *errmsg = nullptr)
      : source_mesh_(source_mesh), source_state_(source_state),
        target_mesh_(target_mesh), target_state_(target_state),
        source_vars_to_remap_(source_vars_to_remap),
        dim_(source_mesh.space_dimension()),
        executor_(executor) {

    assert(source_mesh.space_dimension() == target_mesh.space_dimension());

    // Record all the field types we are remapping and all the kinds
    // of entities we are remapping on

    entity_kinds_.clear();
    for (auto const& source_varname : source_vars_to_remap_) {
      Entity_kind onwhat = source_state_.get_entity(source_varname);
      if (std::find(entity_kinds_.begin(), entity_kinds_.end(), onwhat) ==
          entity_kinds_.end())
        entity_kinds_.push_back(onwhat);

      Field_type fieldtype = source_state_.field_type(onwhat, source_varname);
      if (std::find(field_types_.begin(), field_types_.end(), fieldtype) ==
          field_types_.end())
        field_types_.push_back(fieldtype);

      if (fieldtype == Field_type::MULTIMATERIAL_FIELD)
        have_multi_material_fields_ = true;
    }
        
    // Make the internal drivers for each entity kind
    
    instantiate_core_drivers();
  }

  /*!
    @brief Constructor for the remap driver.
    @param[in] sourceMesh A @c  wrapper to the source mesh.
    @param[in] sourceState A @c wrapper for the data that lives on the
    source mesh
    @param[in] targetMesh A @c TargetMesh to the target mesh
    @param[in,out] targetState A @c TargetState for the data that will
    be mapped to the target mesh
  */
  FancyDriver(SourceMesh const& source_mesh,
              SourceState const& source_state,
              TargetMesh const& target_mesh,
              TargetState& target_state,
              Wonton::Executor_type const *executor = nullptr,
              std::string *errmsg = nullptr)
      : source_mesh_(source_mesh), source_state_(source_state),
        target_mesh_(target_mesh), target_state_(target_state),
        dim_(source_mesh.space_dimension()),
        executor_(executor) {

    assert(source_mesh.space_dimension() == target_mesh.space_dimension());

    // if the variables to remap were not listed, assume all variables are
    // to be remapped
    if (source_vars_to_remap_.size() == 0)
      source_vars_to_remap_ = source_state_.names();

    // Record all the field types we are remapping and all the kinds
    // of entities we are remapping on

    entity_kinds_.clear();
    for (auto const& source_varname : source_vars_to_remap_) {
      Entity_kind onwhat = source_state_.get_entity(source_varname);
      if (std::find(entity_kinds_.begin(), entity_kinds_.end(), onwhat) ==
          entity_kinds_.end())
        entity_kinds_.push_back(onwhat);

      Field_type fieldtype = source_state_.field_type(onwhat, source_varname);
      if (std::find(field_types_.begin(), field_types_.end(), fieldtype) ==
          field_types_.end())
        field_types_.push_back(fieldtype);

      if (fieldtype == Field_type::MULTIMATERIAL_FIELD)
        have_multi_material_fields_ = true;
    }

    // Make the core drivers for each entity kind
    
    instantiate_core_drivers();
  }


  /// Copy constructor (disabled)
  FancyDriver(const FancyDriver &) = delete;

  /// Assignment operator (disabled)
  FancyDriver & operator = (const FancyDriver &) = delete;

  /// Destructor
  ~FancyDriver() {}

  /// Is this a distributed (multi-rank) run?

  bool is_distributed_run(Wonton::Executor_type const *executor = nullptr) {
    distributed_ = false;

#ifdef PORTAGE_ENABLE_MPI
    mycomm_ = MPI_COMM_NULL;
    auto mpiexecutor = dynamic_cast<Wonton::MPIExecutor_type const *>(executor);
    if (mpiexecutor && mpiexecutor->mpicomm != MPI_COMM_NULL) {
      mycomm_ = mpiexecutor->mpicomm;
      MPI_Comm_size(mycomm_, &nprocs_);
      if (nprocs_ > 1)
        distributed_ = true;
    }
#endif

    return distributed_;
  }

  /*!
    Does the source mesh need redistribution due to geometric
    mismatch of partitons (different from mismatch of overall domain
    geometry)
  */
  
  bool source_needs_redistribution(Wonton::Executor_type const *executor =
                                   nullptr) {
    // for now if it is a distributed run, we always "redistribute"
    // even if that means copying the data into the flat mesh/state
    // but not moving data around. Eventually, we will determine if
    // we need to redistribute based on the partition check and
    // and construct the flat mesh/state wrappers only if we need to

    return is_distributed_run(executor);
  }


  /*! @brief Compute interpolation weights in advance of actual
    interpolation of variables

    @tparam Search A search method that takes the dimension, source
    mesh class and target mesh class as template parameters

    @tparam Intersect A polyhedron-polyhedron intersection class that
    takes the source and taget mesh classes as template parameters
  */

  template<
    template <int, Entity_kind, class, class> class Search,
    template <Entity_kind, class, class, class,
              template <class, int, class, class> class,
              class, class> class Intersect
    >
  void compute_interpolation_weights() {

    Portage::vector<std::vector<int>> intersection_candidates;
    
    for (Entity_kind onwhat : entity_kinds_) {
      switch (onwhat) {
        case CELL: {
          // find intersection candidates
          intersection_candidates = search<CELL, Search>();

          // Compute moments of intersection
          source_weights_[onwhat] =
              intersect_meshes<CELL, Intersect>(intersection_candidates);

          has_mismatch_ |= check_mesh_mismatch<CELL>(source_weights_[onwhat]);

          if (have_multi_material_fields_) {
            mat_intersection_completed_ = true;
            
            source_weights_by_mat_ =
                intersect_materials<Intersect>(intersection_candidates);
          }
          break;
        }
        case NODE: {
          // find intersection candidates
          intersection_candidates = search<NODE, Search>();

          // Compute moments of intersection
          source_weights_[onwhat] =
              intersect_meshes<NODE, Intersect>(intersection_candidates);

          has_mismatch_ |= check_mesh_mismatch<NODE>(source_weights_[onwhat]);
          break;
        }
        default:
          std::cerr << "Cannot remap on " << to_string(onwhat) << "\n";
      }
    }

  }




  /*!
    @brief search for candidate source entities whose control volumes
     (cells, dual cells) overlap the control volumes of target cells

     @tparam Entity_kind  what kind of entity are we searching on/for

     @tparam Search       search functor

     @returns    vector of candidate cells for each target cell
  */

  template<
    Entity_kind ONWHAT,
    template <int, Entity_kind, class, class> class Search
    >
  Portage::vector<std::vector<int>>                         // return type
  search() {

    search_completed_[ONWHAT] = true;

    if (source_redistributed_)
      return core_driver_parallel_[ONWHAT]->template search<ONWHAT, Search>();
    else
      return core_driver_serial_[ONWHAT]->template search<ONWHAT, Search>();

  }


  /*!
    @brief intersect target control volumes with source control volumes

     @tparam Entity_kind  what kind of entities are we intersecting
 
     @tparam Intersect    intersect functor

     @param[in] candidates Intersection candidates for each target cell

     @returns             vector of weights for each target cell
  */

  template<
    Entity_kind ONWHAT,
    template <Entity_kind, class, class, class,
              template <class, int, class, class> class,
              class, class> class Intersect
    >
  Portage::vector<std::vector<Portage::Weights_t>>         // return type
  intersect_meshes(Portage::vector<std::vector<int>> const& candidates) {

    assert(search_completed_[ONWHAT]);
    mesh_intersection_completed_[ONWHAT] = true;

    if (source_redistributed_)
      return core_driver_parallel_[ONWHAT]->template intersect_meshes<ONWHAT, Intersect>(candidates);
    else
      return core_driver_serial_[ONWHAT]->template intersect_meshes<ONWHAT, Intersect>(candidates);

  }

  /* @brief intersect target cells with source material polygons

     @tparam Entity_kind  what kind of entities are we intersecting

     @tparam Intersect    intersect functor

     @param[in] candidates intersection candidates for each target cells

     @returns vector(s) of weights for each target cell organized by
     material (hence the additional outer std::vector compared to the
     return type of intersect_meshes)
  */

  template<
    template <Entity_kind, class, class, class,
              template <class, int, class, class> class,
              class, class> class Intersect
    >
  std::vector<Portage::vector<std::vector<Portage::Weights_t>>>
  intersect_materials(Portage::vector<std::vector<int>> const& candidates) {

    mat_intersection_completed_ = true;

    if (source_redistributed_)
      return core_driver_parallel_[CELL]->template intersect_materials<Intersect>(candidates);
    else
      return core_driver_serial_[CELL]->template intersect_materials<Intersect>(candidates);

  }


  /*!
    @brief Check if meshes are mismatched 

    @tparam ONWHAT on what kind of entity are we checking mismatch

    @param[in] source_weights Intersection moments/weights 

    @returns whether the mesh has mismatch
  */
  template<Entity_kind ONWHAT>
  bool check_mesh_mismatch(Portage::vector<std::vector<Weights_t>> const& source_weights) {

    if (source_redistributed_)
      return core_driver_parallel_[ONWHAT]->template check_mesh_mismatch<ONWHAT>(source_weights);
    else
      return core_driver_serial_[ONWHAT]->template check_mesh_mismatch<ONWHAT>(source_weights);

  }
  
  
 
  /*!
    Interpolate a mesh variable of type T residing on entity kind
    ONWHAT using previously computed intersection weights (same
    variable name on source and target)

    @tparam T  type of variable being remapped (default double) - underlying
    interpolator must be able to handle this type

    @tparam ONWHAT  Entity kind on which variable resides

    @tparam Interpolate  Interpolate functor to do the actual interpolation

    @param[in] srcvarname   Variable name on source and target meshes

    @param[in] lower_bound  Lower bound for variable

    @param[in] upper_bound  Upper bound for variable

    @param[in] limiter      Limiter to use for second order reconstruction

    @param[in] partial_fixup_type  Method to populate fields on partially filled target entities (cells or dual cells)

    @param[in] empty_fixup_type    Method to populate fields on empty target entities (cells or dual cells)

    @param[in] conservation_tol   Tolerance to which source and target integral quantities are to be matched

    @param[in] max_fixup_iter     Max number of iterations for global repair

    See support/portage.h for options on limiter, partial_fixup_type and
    empty_fixup_type
  */

  template<typename T = double,
           Entity_kind ONWHAT,
           template<int, Entity_kind, class, class, class,
                    template<class, int, class, class> class,
                    class, class, class> class Interpolate
           >
  void interpolate(std::string srcvarname,
                   T lower_bound, T upper_bound,
                   Limiter_type limiter = DEFAULT_LIMITER,
                   Partial_fixup_type partial_fixup_type = DEFAULT_PARTIAL_FIXUP_TYPE,
                   Empty_fixup_type empty_fixup_type = DEFAULT_EMPTY_FIXUP_TYPE,
                   double conservation_tol = DEFAULT_CONSERVATION_TOL,
                   int max_fixup_iter = DEFAULT_MAX_FIXUP_ITER) {

    interpolate<T, ONWHAT, Interpolate>(srcvarname, srcvarname,
                                        lower_bound, upper_bound,
                                        limiter,
                                        partial_fixup_type, empty_fixup_type,
                                        conservation_tol, max_fixup_iter);
  }

  /*!
    Interpolate a mesh variable of type T residing on entity kind
    ONWHAT using previously computed intersection weights (different
    variable name on source and target)

    @tparam T  type of variable being remapped (default double) - underlying
    interpolator must be able to handle this type

    @param[in] srcvarname   Variable name on source mesh

    @param[in] trgvarname   Variable name on target mesh

    @param[in] lower_bound  Lower bound for variable

    @param[in] upper_bound  Upper bound for variable

    @param[in] limiter      Limiter to use for second order reconstruction

    @param[in] partial_fixup_type  Method to populate fields on partially filled target entities (cells or dual cells)

    @param[in] empty_fixup_type    Method to populate fields on empty target entities (cells or dual cells)

    @param[in] conservation_tol   Tolerance to which source and target integral quantities are to be matched

    @param[in] max_fixup_iter     Max number of iterations for global repair

    See support/portage.h for options on limiter, partial_fixup_type and
    empty_fixup_type
  */

  template<typename T = double,
           Entity_kind ONWHAT,
           template<int, Entity_kind, class, class, class,
                    template<class, int, class, class> class,
                    class, class, class> class Interpolate
           >
  void interpolate(std::string srcvarname, std::string trgvarname,
                   T lower_bound, T upper_bound,
                   Limiter_type limiter = DEFAULT_LIMITER,
                   Partial_fixup_type partial_fixup_type = DEFAULT_PARTIAL_FIXUP_TYPE,
                   Empty_fixup_type empty_fixup_type = DEFAULT_EMPTY_FIXUP_TYPE,
                   double conservation_tol = DEFAULT_CONSERVATION_TOL,
                   int max_fixup_iter = DEFAULT_MAX_FIXUP_ITER) {
    
    assert(source_state_.get_entity(srcvarname) == ONWHAT);
    assert(mesh_intersection_completed_[ONWHAT]);

    if (std::find(source_vars_to_remap_.begin(), source_vars_to_remap_.end(),
                  srcvarname) == source_vars_to_remap_.end()) {
      std::cerr << "Cannot remap source variable " << srcvarname <<
          " - not specified in initial variable list in the constructor \n";
      return;
    }

    if (source_redistributed_) {

      auto & driver = core_driver_parallel_[ONWHAT];
      
      if (source_state_.field_type(ONWHAT, srcvarname) ==
          Field_type::MULTIMATERIAL_FIELD) {

        assert(mat_intersection_completed_);
        assert(ONWHAT == CELL);
        
        driver->template interpolate_mat_var<T, Interpolate>
            (srcvarname, trgvarname, source_weights_by_mat_,
             lower_bound, upper_bound, limiter, partial_fixup_type,
             empty_fixup_type, conservation_tol, max_fixup_iter);

      } else {

        assert(mesh_intersection_completed_[ONWHAT]);

        driver->template interpolate_mesh_var<T, ONWHAT, Interpolate>
            (srcvarname, trgvarname, source_weights_[ONWHAT],
             lower_bound, upper_bound, limiter, partial_fixup_type,
             empty_fixup_type, conservation_tol, max_fixup_iter);
      }

    } else {

      auto & driver = core_driver_serial_[ONWHAT];
      
      if (source_state_.field_type(ONWHAT, srcvarname) ==
          Field_type::MULTIMATERIAL_FIELD) {

        assert(mat_intersection_completed_);
        assert(ONWHAT == CELL);
        
        driver->template interpolate_mat_var<T, Interpolate>
            (srcvarname, trgvarname, source_weights_by_mat_,
             lower_bound, upper_bound, limiter, partial_fixup_type,
             empty_fixup_type, conservation_tol, max_fixup_iter);

      } else {

        assert(mesh_intersection_completed_[ONWHAT]);
        
        driver->template interpolate_mesh_var<T, ONWHAT, Interpolate>
            (srcvarname, trgvarname, source_weights_[ONWHAT],
             lower_bound, upper_bound, limiter, partial_fixup_type,
             empty_fixup_type, conservation_tol, max_fixup_iter);

      }
      
    }

  }  // interpolate

  
 private:

  // Inputs specified by calling app
  Wonton::Executor_type const *executor_;
  SourceMesh const& source_mesh_;
  TargetMesh const& target_mesh_;
  SourceState const& source_state_;
  TargetState& target_state_;
  unsigned int dim_;
  double voldifftol_ = 100*std::numeric_limits<double>::epsilon();
  double consttol_ =  100*std::numeric_limits<double>::epsilon();


  // Component variables
  bool distributed_ = false;  // default is serial
  bool source_redistributed_ = false;  // Did we redistribute the source mesh?
  int comm_rank_ = 0;
  int nprocs_ = 1;

#ifdef PORTAGE_ENABLE_MPI
  MPI_Comm mycomm_ = MPI_COMM_NULL;
#endif

  std::vector<std::string> source_vars_to_remap_;
  std::vector<Entity_kind> entity_kinds_;
  std::vector<Field_type> field_types_;

  // Whether meshes are mismatched
  bool has_mismatch_ = false;

  // Whether we are remapping multimaterial fields
  bool have_multi_material_fields_ = false;

  // Track what steps are completed
  std::map<Entity_kind, bool> search_completed_;
  std::map<Entity_kind, bool> mesh_intersection_completed_;
  bool mat_intersection_completed_ = false;

  // Pointers to core drivers designed to work on a particular
  // entity kind on native mesh/state. These are suitable for serial
  // runs or parallel runs with no redistribution (which are
  // equivalent to a serial run on each processor with no
  // communication)

  std::map<Entity_kind, std::unique_ptr<SerialDriverType>> core_driver_serial_;

  // Pointers to core drivers designed to work on a particular
  // entity kind on flattened mesh/state (suitable for parallel runs
  // involving redistribution)


  std::map<Entity_kind, std::unique_ptr<ParallelDriverType>> core_driver_parallel_;

  // Weights of intersection b/w target entities and source entities
  // Each intersection is between the control volume (cell, dual cell)
  // of a target and source entity.

  //  over all entity kinds (CELL, NODE, etc.)
  //   ||
  //   ||               for all target entities
  //   ||                  of a particular kind
  //   ||                        ||
  //   ||                        ||     intersection moments list
  //   ||                        ||     for each target entity
  //   ||                        ||           ||
  //   \/                        \/           \/
  std::map<Entity_kind, Portage::vector<std::vector<Weights_t>>> source_weights_;

  // Weights of intersection b/w target CELLS and source material polygons
  // Each intersection is between a target cell and material polygon in
  // a source cell for a particular material

  //  for each material
  //   ||
  //   ||      for all target entities
  //   ||         of a particular kind
  //   ||               ||
  //   ||               ||     intersection moments for
  //   ||               ||     each target entity
  //   ||               ||           ||
  //   \/               \/           \/
  std::vector<Portage::vector<std::vector<Weights_t>>> source_weights_by_mat_;

  /*!
    @brief Instantiate core drivers that abstract away whether we
    are using a redistributed or native source mesh/state

    executor  An executor encoding parallel run parameters (if its parallel executor)
  */

  int instantiate_core_drivers(Wonton::Executor_type const *executor =
                               nullptr) {
    std::string message;

    for (auto const& onwhat : entity_kinds_) {
      search_completed_[onwhat] = false;
      mesh_intersection_completed_[onwhat] = false;
    }
    
    // Default is serial run (if MPI is not enabled or the
    // communicator is not defined or the number of processors is 1)
#ifdef PORTAGE_ENABLE_MPI
    if (source_needs_redistribution(executor)) {

      // Create a new mesh wrapper that we can use for redistribution
      // of the source mesh as necessary (so that every target cell
      // sees any source cell that it overlaps with)

      // IN FACT, WE SHOULD DO THE BOUNDING BOX OVERLAP CHECK FIRST
      // AND ONLY IF WE DETERMINE THAT THE SOURCE MESH NEEDS TO BE
      // DISTRIBUTED WE SHOULD CREATE THE FLAT MESH WRAPPER AND INVOKE
      // REDISTRIBUTION; OTHERWISE, WE JUST INVOKE REMAP WITH THE
      // ORIGINAL WRAPPER

      Flat_Mesh_Wrapper<> source_mesh_flat_;
      source_mesh_flat_.initialize(source_mesh_);

      Flat_State_Wrapper<Flat_Mesh_Wrapper<>> source_state_flat_(source_mesh_flat_);
      source_state_flat_.initialize(source_state_, source_vars_to_remap_);

      auto mpiexecutor = dynamic_cast<Wonton::MPIExecutor_type const *>(executor);
      MPI_Bounding_Boxes distributor(mpiexecutor);
      distributor.distribute(source_mesh_flat_, source_state_flat_,
                             target_mesh_, target_state_);

      source_redistributed_ = true;

      for (Entity_kind onwhat : entity_kinds_)
        core_driver_parallel_[onwhat] =
            make_core_driver<D, Flat_Mesh_Wrapper<>,
                             Flat_State_Wrapper<Flat_Mesh_Wrapper<>>,
                             TargetMesh, TargetState,
                             InterfaceReconstructorType,
                             Matpoly_Splitter, Matpoly_Clipper, CoordSys
                             >(onwhat,
                               source_mesh_flat_, source_state_flat_,
                               target_mesh_, target_state_, executor_);
    }
    else
#endif
    {
      for (Entity_kind onwhat : entity_kinds_)
        core_driver_serial_[onwhat] =
            make_core_driver<D, SourceMesh, SourceState,
                             TargetMesh, TargetState,
                             InterfaceReconstructorType,
                             Matpoly_Splitter, Matpoly_Clipper, CoordSys
                             >(onwhat,
                               source_mesh_, source_state_,
                               target_mesh_, target_state_, executor_);
    }
  }  // FancyDriver::instantiate_core_drivers

};  // FancyDriver


}  // namespace Portage

#endif  // PORTAGE_GENREMAP_DRIVER_H_
