/*
  This file is part of the Ristra portage project.
  Please see the license file at the root of this repository, or at:
  https://github.com/laristra/portage/blob/master/LICENSE
*/

#include <iostream>
#include <memory>

#include "gtest/gtest.h"
#ifdef PORTAGE_ENABLE_MPI
#include "mpi.h"
#endif

#include "wonton/mesh/jali/jali_mesh_wrapper.h"
#include "wonton/state/jali/jali_state_wrapper.h"
#include "portage/search/search_kdtree.h"
#include "portage/intersect/intersect_r2d.h"
#include "portage/intersect/intersect_r3d.h"
#include "portage/intersect/simple_intersect_for_tests.h"
#include "portage/interpolate/interpolate_1st_order.h"
#include "Mesh.hh"
#include "MeshFactory.hh"
#include "JaliStateVector.h"
#include "JaliState.h"

#include "portage/driver/coredriver.h"

class PartMismatchTest : public testing::Test {

protected:
  // useful shortcuts
  using Remapper = Portage::CoreDriver<2, Wonton::Entity_kind::CELL,
                                          Wonton::Jali_Mesh_Wrapper,
                                          Wonton::Jali_State_Wrapper>;
  using PartsPair = Portage::Parts<2, Wonton::Entity_kind::CELL,
                                      Wonton::Jali_Mesh_Wrapper,
                                      Wonton::Jali_State_Wrapper>;
  using Partial = Portage::Partial_fixup_type;
  using Empty   = Portage::Empty_fixup_type;

  /**
   * @brief compute cell centroid.
   *
   * @param cell the given cell.
   * @param the mesh
   * @return centroid its centroid.
   */
  Wonton::Point<2> get_centroid(int cell, Wonton::Jali_Mesh_Wrapper const& mesh) {
    Wonton::Point<2> centroid;
    mesh.cell_centroid(cell, &centroid);
    return std::move(centroid);
  };

  /**
   * @brief compute cell density analytically.
   *
   * @param c    the given cell.
   * @param mesh a wrapper to the supporting mesh[source|target].
   * @return the computed cell density.
   */
  double compute_source_density(int c) {
    double const rho_min = 1.;
    double const rho_max = 100.;
    auto centroid = get_centroid(c, source_mesh_wrapper);
    return (centroid[0] < 0.5 ? rho_max : rho_min);
  };

  /**
   * @brief Create a partition based on a threshold value.
   *
   * @param mesh     the current mesh to split
   * @param nb_cells its number of cells
   * @param thresh   x-axis threshold value
   * @param part     source or target mesh parts
   */
  void create_partition(Wonton::Jali_Mesh_Wrapper const& mesh,
                        double thresh, std::vector<int>* part) {

    assert(part != nullptr);
    assert(nb_parts == 2);

    int const nb_cells =
      mesh.num_entities(Wonton::Entity_kind::CELL, Wonton::Entity_type::ALL);
    int const min_heap_size = static_cast<int>(nb_cells / 2);

    for (int i = 0; i < nb_parts; ++i) {
      part[i].clear();
      part[i].reserve(min_heap_size);
    }

    for (int i = 0; i < nb_cells; ++i) {
      auto centroid = get_centroid(i, mesh);
      if (centroid[0] < thresh) {
        part[0].emplace_back(i);
      } else {
        part[1].emplace_back(i);
      }
    }
  }

  /**
   * @brief Compute the expected remapped density on target mesh.
   *
   * It depends on Partial/empty mismatch fixup schemes being used:
   * - locally-conservative/leave-empty: [100.0|100.0| 50.0|  0.0|1.0]
   * - locally-conservative/extrapolate: [100.0|100.0| 50.0| 50.0|1.0]
   * - constant/leave-empty:             [100.0|100.0|100.0|  0.0|1.0]
   * - constant/extrapolate:             [100.0|100.0|100.0|100.0|1.0]
   * - shifted-conservative/leave-empty: [ 83.3| 83.3| 83.3|  0.0|2.5]
   * - shifted-conservative/extrapolate: [ 62.5| 62.5| 62.5| 62.5|2.5]
   *
   * @tparam partial_fixup_type the Partial mismatch fixup scheme to use
   * @tparam empty_fixup_type   the empty cell fixup scheme to use
   * @param cell                the given cell
   * @return the expected remapped density value
   */
  double get_expected_remapped_density(int const cell,
                                       Partial partial_fixup,
                                       Empty   empty_fixup) {

    // get cell position
    auto centroid = get_centroid(cell, target_mesh_wrapper);

    switch (partial_fixup) {
      case Partial::LOCALLY_CONSERVATIVE: {
        if (empty_fixup == Empty::LEAVE_EMPTY) {
          if (centroid[0] < 0.4) {
            return 100.;
          } else if (centroid[0] < 0.6) {
            return 50.;
          } else if (centroid[0] < 0.8) {
            return 0.;
          } else {
            return 1.;
          }
        } else /* EXTRAPOLATE */ {
          if (centroid[0] < 0.4) {
            return 100.;
          } else if (centroid[0] < 0.8) {
            return 50.;
          } else {
            return 1.;
          }
        }
      } case Partial::CONSTANT: {
        if (empty_fixup == Empty::LEAVE_EMPTY) {
          if (centroid[0] < 0.6) {
            return 100.;
          } else if (centroid[0] < 0.8) {
            return 0.;
          } else {
            return 1.;
          }
        } else /* EXTRAPOLATE */ {
          if (centroid[0] < 0.8) {
            return 100.;
          } else {
            return 1.;
          }
        }
      } case Partial::SHIFTED_CONSERVATIVE: {

        /* Correct target cell density for shifted-conservative fixup scheme. */
        auto compute_shifted_density = [](double mass_source,
                                          double unit_mass_target,
                                          double unit_volume_target,
                                          int nb_target_cells) {
          double mass_delta = (unit_mass_target * nb_target_cells) - mass_source;
          double discrepancy = mass_delta / nb_target_cells;
          return (unit_mass_target - discrepancy) / unit_volume_target;
        };

        if (empty_fixup == Empty::LEAVE_EMPTY) {
          if (centroid[0] < 0.6) {
            return compute_shifted_density(50, 20, 0.2, 3);  // 83.35
          } else if(centroid[0] < 0.8) {
            return 0.;
          } else {
            return compute_shifted_density(0.5, 0.2, 0.2, 1);  // 2.5
          }
        } else /* EXTRAPOLATE */ {
          if (centroid[0] < 0.8) {
            return compute_shifted_density(50, 20, 0.2, 4);  // 62.5
          } else {
            return compute_shifted_density(0.5, 0.2, 0.2, 1);  // 2.5
          }
        } // end extrapolate
      } default: throw std::runtime_error("Invalid Partial fixup type");
    }
  }

public:


  /**
   * @brief Setup each test-case.
   *
   * It creates both source and target meshes,
   * then computes and assigns a density field on source mesh,
   * then creates parts couples for both source and target meshes.
   */
  PartMismatchTest()
    : source_mesh(Jali::MeshFactory(MPI_COMM_WORLD)(0.0, 0.0, 1.0, 1.0, 4, 4)),
      target_mesh(Jali::MeshFactory(MPI_COMM_WORLD)(0.0, 0.0, 1.0, 1.0, 5, 5)),
      source_state(Jali::State::create(source_mesh)),
      target_state(Jali::State::create(target_mesh)),
      source_mesh_wrapper(*source_mesh),
      target_mesh_wrapper(*target_mesh),
      source_state_wrapper(*source_state),
      target_state_wrapper(*target_state)
  {
    // get rid of long namespaces
    auto const CELL = Wonton::Entity_kind::CELL;
    auto const ALL  = Wonton::Entity_type::ALL;

    // compute and add density field to the source mesh
    int const nb_cells = source_mesh_wrapper.num_entities(CELL, ALL);
    double source_density[nb_cells];
    for (int c = 0; c < nb_cells; c++) {
      source_density[c] = compute_source_density(c);
    }

    source_state_wrapper.mesh_add_data(CELL, "density", source_density);
    target_state_wrapper.mesh_add_data<double>(CELL, "density", 0.);

    // create source and target mesh parts
    create_partition(source_mesh_wrapper, 0.5, source_cells);
    create_partition(target_mesh_wrapper, 0.8, target_cells);

        // set parts
    for (int i = 0; i < nb_parts; ++i) {
      parts.emplace_back(source_mesh_wrapper, target_mesh_wrapper,
                         source_state_wrapper,target_state_wrapper,
                         source_cells[i], target_cells[i], nullptr);
    }
  }

  /**
   * @brief Do the test.
   *
   * @param partial_fixup the partially filled cell fixup scheme to use.
   * @param empty_fixup   the empty cells fixup scheme to use.
   */
  void unitTest(Partial partial_fixup, Empty empty_fixup) {

    using Wonton::Entity_kind;
    using Wonton::Entity_type;

    // Perform remapping without redistribution
    Remapper remapper(source_mesh_wrapper, source_state_wrapper,
                      target_mesh_wrapper, target_state_wrapper);

    auto candidates = remapper.search<Portage::SearchKDTree>();
    auto source_weights = remapper.intersect_meshes<Portage::IntersectR2D>(candidates);

    for (int i = 0; i < nb_parts; ++i) {
      // test for mismatch and compute volumes
      parts[i].test_mismatch(source_weights);

      // interpolate density part-by-part while fixing mismatched values
      remapper.interpolate_mesh_var<double, Portage::Interpolate_1stOrder>(
        "density", "density", source_weights,
        lower_bound, higher_bound, Portage::DEFAULT_LIMITER,
        partial_fixup, empty_fixup, Portage::DEFAULT_CONSERVATION_TOL,
        Portage::DEFAULT_MAX_FIXUP_ITER, &(parts[i])
      );
    }

    // Finally check that we got the right target density values
    double* remapped;
    target_state_wrapper.mesh_get_data(Entity_kind::CELL, "density", &remapped);

    for (int i = 0; i < nb_parts; ++i) {
      for (auto&& c : target_cells[i]) {
        auto obtained = remapped[c];
        auto expected = get_expected_remapped_density(c, partial_fixup, empty_fixup);
#if DEBUG_PART_BY_PART
        auto centroid = get_centroid(c, target_mesh_wrapper);
        std::printf("target[%02d]: (x=%.1f, y=%.1f), "
                    "remapped: %7.3f, expected: %7.3f\n",
                    c, centroid[0], centroid[1], obtained, expected);
#endif
        ASSERT_NEAR(obtained, expected, epsilon);
      }
    }
  }

protected:
  // useful constants
  static constexpr int nb_parts = 2;
  static constexpr double higher_bound = std::numeric_limits<double>::max();
  static constexpr double lower_bound  = -higher_bound;
  static constexpr double epsilon = 1.E-10;

  // Source and target meshes and states
  std::shared_ptr<Jali::Mesh>  source_mesh;
  std::shared_ptr<Jali::Mesh>  target_mesh;
  std::shared_ptr<Jali::State> source_state;
  std::shared_ptr<Jali::State> target_state;

  // Wrappers for interfacing with the underlying mesh data structures
  Wonton::Jali_Mesh_Wrapper  source_mesh_wrapper;
  Wonton::Jali_Mesh_Wrapper  target_mesh_wrapper;
  Wonton::Jali_State_Wrapper source_state_wrapper;
  Wonton::Jali_State_Wrapper target_state_wrapper;

  // source and target parts couples
  std::vector<PartsPair> parts;
  std::vector<int> source_cells[2];
  std::vector<int> target_cells[2];
};


TEST_F(PartMismatchTest, LocallyConservative_LeaveEmpty) {
  unitTest(Partial::LOCALLY_CONSERVATIVE, Empty::LEAVE_EMPTY);
}

TEST_F(PartMismatchTest, LocallyConservative_Extrapolate) {
  unitTest(Partial::LOCALLY_CONSERVATIVE, Empty::EXTRAPOLATE);
}

TEST_F(PartMismatchTest, Constant_LeaveEmpty) {
  unitTest(Partial::CONSTANT, Empty::LEAVE_EMPTY);
}

TEST_F(PartMismatchTest, Constant_Extrapolate) {
  unitTest(Partial::CONSTANT, Empty::EXTRAPOLATE);
}

TEST_F(PartMismatchTest, ShiftedConservative_LeaveEmpty) {
  unitTest(Partial::SHIFTED_CONSERVATIVE, Empty::LEAVE_EMPTY);
}

TEST_F(PartMismatchTest, ShiftedConservative_Extrapolate) {
  unitTest(Partial::SHIFTED_CONSERVATIVE, Empty::EXTRAPOLATE);
}