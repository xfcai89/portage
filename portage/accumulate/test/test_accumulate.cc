/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/
#include <cmath>
#include <vector>
#include <portage/driver/driver_swarm.h>
#include "gtest/gtest.h"

#include "portage/swarm/swarm.h"
#include "portage/accumulate/accumulate.h"
#include "portage/support/portage.h"
#include "portage/support/test_operator_data.h"
#include "wonton/support/Point.h"

using namespace Portage::Meshfree;
using Wonton::Point;

template<int dim>
void test_accumulate(EstimateType etype, Basis::Type btype, WeightCenter center) {

  using Accumulator = Accumulate<dim, Swarm<dim>, Swarm<dim>>;

  // create the source and target swarms input data
  const size_t nside = 6;
  const size_t npoints = powl(nside,dim);
  const double deltax = 1./nside;
  const double smoothing = 2.5*deltax;
  const double jitter = 0.2;

  Portage::vector<Point<dim>> source_points(npoints);
  Portage::vector<Point<dim>> target_points(npoints);

  // set the random engine and generator
  std::random_device device;
  std::mt19937 engine { device() };
  std::uniform_real_distribution<double> generator(0.0, 0.5);

  for (size_t i = 0; i < npoints; i++) {
    size_t offset = 0, index;
    for (size_t k = 0; k < dim; k++) {
      index = (i - offset) / powl(nside, dim - k - 1);
      offset += index * powl(nside, dim - k - 1);

      double delta = 2. * generator(engine) * jitter * deltax;
      Point<dim> pt = source_points[i];
      pt[k] = index * deltax + delta;
      source_points[i] = pt;

      delta = 2. * generator(engine) * jitter * deltax;
      pt = target_points[i];
      pt[k] = index * deltax + delta;
      target_points[i] = pt;
    }
  }

  // create source+target swarms, kernels, geometries, and smoothing lengths
  Swarm<dim> src_swarm(source_points);
  Swarm<dim> tgt_swarm(target_points);
  Portage::vector<Weight::Kernel> kernels(npoints, Weight::B4);
  Portage::vector<Weight::Geometry> geometries(npoints, Weight::TENSOR);

  std::vector<std::vector<double>> default_length(1, std::vector<double>(dim, smoothing));
  SmoothingLengths smoothing_h(npoints, default_length);

  // create the accumulator
  Accumulator accumulator(src_swarm, tgt_swarm, etype, center,
                          kernels, geometries, smoothing_h, btype);

  // check sizes and allocate test array
  auto bsize = Basis::function_size<dim>(btype);
  auto jsize = Basis::jet_size<dim>(btype);
  ASSERT_EQ(jsize[0], bsize);
  ASSERT_EQ(jsize[1], bsize);

  // list of src swarm particles (indices)
  std::vector<int> src_particles(npoints);
  std::iota(src_particles.begin(), src_particles.end(), 0);

  // Loop through target particles
  for (size_t i = 0; i < npoints; i++) {
    std::vector<std::vector<double>> sums(jsize[0], std::vector<double>(jsize[1], 0.));

    // do the accumulation loop for each target particle against all
    // the source particles
    auto shape_vecs = accumulator(i, src_particles);

    if (etype == KernelDensity) {
      for (size_t j = 0; j < npoints; j++) {
        double weight = accumulator.weight(i, j);
        ASSERT_EQ ((shape_vecs[j]).weights[0], weight);
      }
    } else {
      // test for reproducing property
      auto x = tgt_swarm.get_particle_coordinates(i);
      auto jetx = Basis::jet<dim>(btype, x);

      for (size_t j = 0; j < npoints; j++) {
        auto y = src_swarm.get_particle_coordinates(j);
        auto basisy = Basis::function<dim>(btype, y);
        for (size_t k = 0; k < jsize[0]; k++) {
          for (size_t m = 0; m < jsize[1]; m++) {
            sums[k][m] += basisy[k] * (shape_vecs[j]).weights[m];
          }
        }
      }

      for (size_t k = 0; k < jsize[0]; k++) {
        for (size_t m = 0; m < jsize[1]; m++) {
          ASSERT_NEAR(sums[k][m], jetx[k][m], 1.e-11);
        }
      }
    }
  }

}


// Test the reproducing property of basis integration operators
template<Basis::Type btype, Operator::Type opertype, Operator::Domain domain>
void test_operator(WeightCenter center) {

  // create the source swarm input geometry data
  constexpr int dim = Operator::dimension(domain);
  const size_t nside = 6;
  const size_t npoints = powl(nside,dim);
  const double deltax = 1./nside;
  const double jitter = 0.3;
  const double smoothing = 2.5*(1.+jitter)*deltax;

  using Accumulator = Accumulate<dim, Swarm<dim>, Swarm<dim>>;

  Portage::vector<Point<dim>> source_points(npoints);

  for (size_t i = 0; i < npoints; i++) {
    size_t offset = 0, index;
    for (size_t k = 0; k < dim; k++) {
      index = (i - offset) / powl(nside, dim - k - 1);
      offset += index * powl(nside, dim - k - 1);

      double delta = (((double) rand()) / RAND_MAX) * jitter * deltax;
      Point<dim> pt = source_points[i];
      pt[k] = index * deltax + delta;
      source_points[i] = pt;
    }
  }

  // create the target swarm input geometry data based on integration domains.
  Portage::vector<Point<dim>> target_points(1);
  Portage::vector<std::vector<Point<dim>>> domain_points(1);
  domain_points[0] = reference_points<domain>();
  auto reference_point = reference_points<domain>();
  Point<dim> centroid(std::vector<double>(dim, 0.));

  for (int i = 0; i < reference_point.size(); i++) {
    for (int j = 0; j < dim; j++) {
      Point<dim> pt = reference_point[i];
      centroid[j] += pt[j];
    }
  }

  for (int k = 0; k < dim; k++) {
    centroid[k] /= reference_point.size();
    Point<dim> pt = target_points[0];
    pt[k] = centroid[k];
    target_points[0] = pt;
  }

  Portage::vector<Operator::Domain> domains(1);
  domains[0] = domain;

  // create source+target swarms, kernels, geometries, and smoothing lengths
  Swarm<dim> source_swarm(source_points);
  Swarm<dim> target_swarm(target_points);
  Portage::vector<Weight::Kernel> kernels(npoints, Weight::B4);
  Portage::vector<Weight::Geometry> geometries(npoints, Weight::TENSOR);
  Portage::vector<std::vector<std::vector<double>>> smoothing_h
    (npoints, std::vector<std::vector<double>>(1, std::vector<double>(dim, smoothing)));

  // create the accumulator
  Accumulator accumulator(source_swarm, target_swarm, OperatorRegression,
                          center, kernels, geometries, smoothing_h, btype,
                          opertype, domains, domain_points);

  // check sizes and allocate test array
  auto bsize = Basis::function_size<dim>(btype);
  auto jsize = Operator::Operator<opertype, btype, domain>::operator_size;

  // list of src swarm particles (indices)
  std::vector<int> source_particles(npoints);
  std::iota(source_particles.begin(), source_particles.end(), 0);

  std::vector<std::vector<double>> sums(bsize, std::vector<double>(jsize,0.));

  // do the accumulation loop for each target particle against all
  // the source particles
  std::vector<Portage::Weights_t> shape_vecs = accumulator(0, source_particles);

  for (size_t j = 0; j < npoints; j++) {
    auto y = source_swarm.get_particle_coordinates(j);
    auto basisy = Basis::function<dim>(btype, y);
    for (size_t k = 0; k < bsize; k++) {
      for (size_t m = 0; m < jsize; m++) {
        sums[k][m] += basisy[k] * (shape_vecs[j]).weights[m];
      }
    }
  }

  for (size_t k = 0; k < bsize; k++) {
    for (size_t m = 0; m < jsize; m++) {
      if (opertype == Operator::VolumeIntegral) {
        ASSERT_EQ(jsize, 1);
        using namespace Operator;
        switch (btype) {
          case Basis::Unitary: {
            switch (domain) {
              case Interval:      ASSERT_NEAR(sums[k][m],exactUnitaryInterval[k], 1.e-12);     break;
              case Quadrilateral: ASSERT_NEAR(sums[k][m],exactUnitaryQuadrilateral[k], 1.e-12);break;
              case Triangle:      ASSERT_NEAR(sums[k][m],exactUnitaryTriangle[k], 1.e-12);     break;
              case Hexahedron:    ASSERT_NEAR(sums[k][m],exactUnitaryHexahedron[k], 1.e-12);   break;
              case Wedge:         ASSERT_NEAR(sums[k][m],exactUnitaryWedge[k], 1.e-12);        break;
              case Tetrahedron:   ASSERT_NEAR(sums[k][m],exactUnitaryTetrahedron[k], 1.e-12);  break;
              default: break;
            }
          }
          case Basis::Linear: {
            switch (domain) {
              case Interval:      ASSERT_NEAR(sums[k][m],exactLinearInterval[k], 1.e-12);     break;
              case Quadrilateral: ASSERT_NEAR(sums[k][m],exactLinearQuadrilateral[k], 1.e-12);break;
              case Triangle:      ASSERT_NEAR(sums[k][m],exactLinearTriangle[k], 1.e-12);     break;
              case Hexahedron:    ASSERT_NEAR(sums[k][m],exactLinearHexahedron[k], 1.e-12);   break;
              case Wedge:         ASSERT_NEAR(sums[k][m],exactLinearWedge[k], 1.e-12);        break;
              case Tetrahedron:   ASSERT_NEAR(sums[k][m],exactLinearTetrahedron[k], 1.e-12);  break;
              default: break;
              }
          }
          case Basis::Quadratic: {
            switch (domain) {
              case Interval:      ASSERT_NEAR(sums[k][m],exactQuadraticInterval[k], 1.e-12);     break;
              case Quadrilateral: ASSERT_NEAR(sums[k][m],exactQuadraticQuadrilateral[k], 1.e-12);break;
              case Triangle:      ASSERT_NEAR(sums[k][m],exactQuadraticTriangle[k], 1.e-12);     break;
              case Tetrahedron:   ASSERT_NEAR(sums[k][m],exactQuadraticTetrahedron[k], 1.e-12);  break;
              default: break;
            }
          }
          default: break;
        }
      }
    }
  }
}

// test the pointwise estimation capability

TEST(accumulate, 1d_KUG) {
  test_accumulate<1>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 2d_KUG) {
  test_accumulate<2>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 3d_KUG) {
  test_accumulate<3>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 1d_KUS) {
  test_accumulate<1>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 2d_KUS) {
  test_accumulate<2>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 3d_KUS) {
  test_accumulate<3>(EstimateType::KernelDensity,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 1d_RUG) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 2d_RUG) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 3d_RUG) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Gather);
}

TEST(accumulate, 1d_RUS) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 2d_RUS) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 3d_RUS) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Unitary,
                     WeightCenter::Scatter);
}

TEST(accumulate, 1d_RLG) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Gather);
}

TEST(accumulate, 2d_RLG) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Gather);
}

TEST(accumulate, 3d_RLG) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Gather);
}

TEST(accumulate, 1d_RLS) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Scatter);
}

TEST(accumulate, 2d_RLS) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Scatter);
}

TEST(accumulate, 3d_RLS) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Linear,
                     WeightCenter::Scatter);
}

TEST(accumulate, 1d_RQG) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Gather);
}

TEST(accumulate, 2d_RQG) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Gather);
}

TEST(accumulate, 3d_RQG) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Gather);
}

TEST(accumulate, 1d_RQS) {
  test_accumulate<1>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Scatter);
}

TEST(accumulate, 2d_RQS) {
  test_accumulate<2>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Scatter);
}

TEST(accumulate, 3d_RQS) {
  test_accumulate<3>(EstimateType::LocalRegression,
                     Basis::Type::Quadratic,
                     WeightCenter::Scatter);
}

// test the operator capability

TEST(operator, UnitaryInterval) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral,
 		            Operator::Interval>(WeightCenter::Scatter);
}

TEST(operator, UnitaryQuadrilateral) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral, 
		Operator::Quadrilateral>
    (WeightCenter::Scatter);
}

TEST(operator, UnitaryTriangle) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral, 
		Operator::Triangle>
    (WeightCenter::Scatter);
}

TEST(operator, UnitaryHexahedron) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral, 
		Operator::Hexahedron>
    (WeightCenter::Scatter);
}

TEST(operator, UnitaryWedge) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral, 
		Operator::Wedge>
    (WeightCenter::Scatter);
}

TEST(operator, UnitaryTetrahedron) {
  test_operator<Basis::Type::Unitary,
                Operator::VolumeIntegral, 
		Operator::Tetrahedron>
    (WeightCenter::Scatter);
}

TEST(operator, LinearInterval) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Interval>
    (WeightCenter::Scatter);
}

TEST(operator, LinearQuadrilateral) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Quadrilateral>
    (WeightCenter::Scatter);
}

TEST(operator, LinearTriangle) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Triangle>
    (WeightCenter::Scatter);
}

TEST(operator, LinearHexahedron) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Hexahedron>
    (WeightCenter::Scatter);
}

TEST(operator, LinearWedge) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Wedge>
    (WeightCenter::Scatter);
}

TEST(operator, LinearTetrahedron) {
  test_operator<Basis::Type::Linear,
                Operator::VolumeIntegral, 
		Operator::Tetrahedron>
    (WeightCenter::Scatter);
}

TEST(operator, QuadraticInterval) {
  test_operator<Basis::Type::Quadratic,
                Operator::VolumeIntegral, 
		Operator::Interval>
    (WeightCenter::Scatter);
}

TEST(operator, QuadraticQuadrilateral) {
  test_operator<Basis::Type::Quadratic,
                Operator::VolumeIntegral, 
		Operator::Quadrilateral>
    (WeightCenter::Scatter);
}

TEST(operator, QuadraticTriangle) {
  test_operator<Basis::Type::Quadratic,
                Operator::VolumeIntegral, 
		Operator::Triangle>
    (WeightCenter::Scatter);
}

TEST(operator, QuadraticTetrahedron) {
  test_operator<Basis::Type::Quadratic,
                Operator::VolumeIntegral, 
		Operator::Tetrahedron>
    (WeightCenter::Scatter);
}




