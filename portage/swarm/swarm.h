/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/
#ifndef SWARM_H_INC_
#define SWARM_H_INC_

#include <vector>
#include <memory>
#include <cassert>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <cmath>

#include "portage/support/portage.h"
#include "portage/support/Point.h"
#include "portage/wonton/mesh/flat/flat_mesh_wrapper.h"

namespace Portage {
namespace Meshfree {

using std::string;
using std::shared_ptr;
using std::make_shared;

/*!
 @class Swarm "swarm.h"
 @brief An effective "mesh" class for a collection disconnected points (particles).
 @tparam dim The dimension of the problem space.
 */
template<size_t dim>
class Swarm {
 public:

  // When using PointVec and PointVecPtr in another file, they have to
  // be prefixed by the keyword typename
  
  //  SEE --- https://stackoverflow.com/questions/610245/where-and-why-do-i-have-to-put-the-template-and-typename-keywords

  using PointVecPtr = shared_ptr<vector<Point<dim>>>;
  using PointVec = vector<Point<dim>>;
  using SmoothingLengthPtr = shared_ptr<vector<std::vector<std::vector<double>>>>;  
  using SmoothingLengthUnit = std::vector<std::vector<double>>;  
  
  /*!
   * @brief A particle has a center point and smoothing lengths in each dimension.
   * @param points center points of the particles
   * @param extents the widths of the particle bounding boxes in each dimension
   */
  Swarm(PointVecPtr points)
      : points_(points), 
        npoints_owned_(points_->size()) 
  {}

  /*!
   * @brief A particle has a center point and smoothing lengths in each dimension.
   * @param points center points of the particles
   * @param extents the widths of the particle bounding boxes in each dimension
   */
  Swarm(PointVecPtr points, 
        SmoothingLengthPtr smoothing_lengths)
      : points_(points), 
        npoints_owned_(points_->size(), 
        smoothing_lengths_(smoothing_lengths)) 
  {}

  /*!
   * @brief Create a Swarm from a flat mesh wrapper.
   * @param wrapper Input mesh wrapper
   */
  Swarm(Wonton::Flat_Mesh_Wrapper<double> &wrapper, 
        Portage::Entity_kind entity)
      : points_(NULL), 
        npoints_owned_(0)
  {
    if (entity != NODE and entity != CELL) {
      throw(std::runtime_error("only nodes and cells allowed"));
    }

    if (entity == NODE) {
      npoints_owned_ = wrapper.num_owned_nodes();
      points_ = make_shared<vector<Point<dim>>>(npoints_owned_);
      Point<dim> node;
      for (size_t i=0; i<npoints_owned_; i++) {
        wrapper.node_get_coordinates(i, &node);
        (*points_)[i] = node;
      }
    } else if (entity == CELL) {
      npoints_owned_ = wrapper.num_owned_cells();
      points_ = make_shared<vector<Point<dim>>>(npoints_owned_);
      Point<dim> centroid;
      for (size_t i=0; i<npoints_owned_; i++) {
        wrapper.cell_centroid<dim>(i, &centroid);
        (*points_)[i] = centroid;
      }
    }
  }

  /*!
   * @brief Create a Swarm from a flat mesh wrapper along with smoothing lengths
   * @param wrapper Input mesh wrapper
   */
  Swarm(Wonton::Flat_Mesh_Wrapper<double> &wrapper, 
        Portage::Entity_kind entity,
        SmoothingLengthPtr smoothing_lengths)
      : points_(NULL), 
        npoints_owned_(0),
        smoothing_lengths_(smoothing_lengths)
  {
    if (entity != NODE and entity != CELL) {
      throw(std::runtime_error("only nodes and cells allowed"));
    }

    if (entity == NODE) {
      npoints_owned_ = wrapper.num_owned_nodes();
      points_ = make_shared<vector<Point<dim>>>(npoints_owned_);
      Point<dim> node;
      for (size_t i=0; i<npoints_owned_; i++) {
        wrapper.node_get_coordinates(i, &node);
        (*points_)[i] = node;
      }
    } else if (entity == CELL) {
      npoints_owned_ = wrapper.num_owned_cells();
      points_ = make_shared<vector<Point<dim>>>(npoints_owned_);
      Point<dim> centroid;
      for (size_t i=0; i<npoints_owned_; i++) {
        wrapper.cell_centroid<dim>(i, &centroid);
        (*points_)[i] = centroid;
      }
    }
  }

  /*! @brief Dimensionality of points */
  unsigned int space_dimension() const {
    return dim;
  }

  /*! @brief return the number of particles in the swarm owned by this processor
   * @return the number of owned particles in the swarm
   */
  int num_owned_particles() const {
    return npoints_owned_;
  }

  /*! @brief return the number of ghost (not owned by this processor)
   * particles in the swarm.
   * @return the number of ghost particles in the swarm that are stored
   * at the end of the particle list i.e., between num_owned_particles
   * and num_owned_particles+num_ghost_particle.  
   */
  int num_ghost_particles() const {
    return points_->size() - npoints_owned_;
  }

  /*! @brief return the number of particles of given type
   * @return the number of owned + ghost particles in the swarm
   */
  int num_particles(Entity_type etype = ALL) const {
    switch (etype) {
      case PARALLEL_OWNED:
        return num_owned_particles();
      case PARALLEL_GHOST:
        return num_ghost_particles();
      case ALL:
        return num_owned_particles() + num_ghost_particles();
      default:
        return 0;
    }
    return 0;
  }


  /*! @brief Get the coordinates of the particle,
   * @param index index of particle of interest
   * @return the particle coordinates
   */
  Point<dim> get_particle_coordinates(const size_t index) const {
    return (*points_)[index];
  }

  /*! @brief Get the smoothing lengths of the particle which
 *           should be available when Scatter scheme is used. ,
   * @param index index of particle of interest
   * @return the particle coordinates
   */
  SmoothingLengthUnit get_particle_smoothing_length(const size_t index) const {
    return (*smoothing_lengths_)[index];
  }

  //! Iterators on mesh entity - begin
  counting_iterator begin(Entity_kind const entity = Entity_kind::PARTICLE,
                          Entity_type const etype = Entity_type::ALL) const {
    assert(entity == PARTICLE);
    int start_index = 0;
    return make_counting_iterator(start_index);
  }

  //! Iterator on mesh entity - end
  counting_iterator end(Entity_kind const entity = Entity_kind::PARTICLE,
                        Entity_type const etype = Entity_type::ALL) const {
    assert(entity == PARTICLE);
    int start_index = 0;
    return (make_counting_iterator(start_index) + num_particles(etype));
  }

  /*! @brief Add new particles to swarm
   * @return 
   */
  void extend_particle_list(std::vector<Point<dim>>& new_pts)
  {
    (*points_).insert((*points_).end(), new_pts.begin(), new_pts.end());
  }
  
  /*! @brief Update smoothing lengths for new particles 
   * @return 
   */
  void update_smoothing_lengths(std::vector<std::vector<std::vector<double>>>& sm_vals)
  {
    int numparts = sm_vals.size();
    for (int i = 0 ; i < numparts; i++)
    {
      std::vector<std::vector<double>> val = sm_vals[i];
      (*smoothing_lengths_).push_back(val);
    }
  }

  void set_smoothing_lengths(SmoothingLengthPtr smoothing_lengths)
  {
    assert(!smoothing_lengths_); 
    smoothing_lengths_ = smoothing_lengths;
  }

  vector<std::vector<std::vector<double>>> get_smoothing_lengths()
  {
    return *smoothing_lengths_;
  }

 private:
  /** the centers of the particles */
  PointVecPtr points_;

  /** the number of owned particles in the swarm */
  size_t npoints_owned_;

  /** the smoothing length extents for each particle */
  SmoothingLengthPtr smoothing_lengths_ = nullptr; 
};

// Factories for making swarms in 1/2/3 dimensions with random or uniform
// distribution of particles. Reason we are having to return a
// shared_ptr to the Swarm is because of how its used in the
// DriverTest

std::shared_ptr<Swarm<1>> SwarmFactory(double xmin, double xmax,
                                         unsigned int nparticles,
                                         unsigned int distribution) {

  shared_ptr<typename Swarm<1>::PointVec> pts_sp = std::make_shared<typename Swarm<1>::PointVec>(nparticles);
  typename Swarm<1>::PointVec &pts(*pts_sp);
  if (distribution == 0) {  // random distribution of particles
    srand(time(NULL));
    unsigned int rand_state;
    for (size_t i=0; i<nparticles; i++) {
      // for some reason intel does not like these two lines combined when using thrust, i.e. pts[i][0]
      Point<1> pt=pts[i];
      pt[0] = xmin + (xmax-xmin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pts[i] = pt;
    }
  } else { // regular distribution 
    double h = (xmax-xmin)/(nparticles-1);
    for (size_t i=0; i < nparticles; i++) {
      Point<1> pt=pts[i];
      pt[0] = xmin + i*h;
      pts[i] = pt;
    }
    
    if (distribution == 2) { // perturbed regular
      srand(time(NULL));
      unsigned int rand_state;
      for (size_t i=0; i < nparticles; i++) {
        Point<1> pt=pts[i];
        pt[0] += 0.25*h*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
	pt[0] = fmax(xmin,fmin(xmax,pt[0]));
	pts[i] = pt;
      }
    }
  }
  
  std::shared_ptr<Swarm<1>> swarm = make_shared<Swarm<1>>(pts_sp);
  return swarm;
}


std::shared_ptr<Swarm<2>> SwarmFactory(double xmin, double ymin,
                                       double xmax, double ymax,
                                       unsigned int nparticles,
                                       unsigned int distribution) {

  auto pts_sp = std::make_shared<typename Swarm<2>::PointVec>(nparticles);
  typename Swarm<2>::PointVec &pts(*pts_sp);
  if (distribution == 0) {  // random distribution of particles
    srand(time(NULL));
    unsigned int rand_state;
    for (size_t i = 0; i < nparticles; i++) {
      Point<2> pt=pts[i];
      pt[0] = xmin + (xmax-xmin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pt[1] = ymin + (ymax-ymin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pts[i] = pt;
    }
  } else {
    int npdim = sqrt(nparticles);
    if (npdim*npdim != nparticles) {
      std::cerr << "Requested number of particles not a perfect square\n";
      std::cerr << "Generating only " << npdim << "particles in each dimension\n";
    }
    double hx = (xmax-xmin)/(npdim-1);
    double hy = (ymax-ymin)/(npdim-1);
    int n = 0;
    for (size_t i = 0; i < npdim; i++) {
      for (size_t j = 0; j < npdim; j++) {
	Point<2> pt=pts[n];
        pt[0] = xmin + i*hx;
        pt[1] = ymin + j*hy;
	pts[n] = pt;
        n++;
      }
    }
    if (distribution == 2) {
      srand(time(NULL));
      unsigned int rand_state;
      for (size_t i = 0; i < nparticles; i++) {
	Point<2> pt=pts[i];
        pt[0] += 0.25*hx*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
        pt[1] += 0.25*hy*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
	pt[0] = fmax(xmin,fmin(xmax,pt[0]));
	pt[1] = fmax(ymin,fmin(ymax,pt[1]));
	pts[i] = pt;
      }
    }
  }
  
  std::shared_ptr<Swarm<2>> swarm = make_shared<Swarm<2>>(pts_sp);
  return swarm;
}


std::shared_ptr<Swarm<3>> SwarmFactory(double xmin, double ymin, double zmin,
                                       double xmax, double ymax, double zmax,
                                       unsigned int nparticles,
                                       unsigned int distribution) {

  auto pts_sp = std::make_shared<typename Swarm<3>::PointVec>(nparticles);
  typename Swarm<3>::PointVec &pts(*pts_sp);
  if (distribution == 0) {  // Random distribution
    srand(time(NULL));
    unsigned int rand_state;
    for (size_t i = 0; i < nparticles; i++) {
      Point<3> pt=pts[i];
      pt[0] = xmin + (xmax-xmin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pt[1] = ymin + (ymax-ymin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pt[2] = zmin + (zmax-zmin)*(static_cast<double>(rand_r(&rand_state))/RAND_MAX);
      pts[i] = pt;
    }
  } else {
    int npdim = static_cast<int>(pow(nparticles, 1.0/3.0) + 0.5);
    if (npdim*npdim*npdim != nparticles) {
      std::cerr << "Requested number of particles not a perfect cube\n";
      std::cerr << "Generating only " << npdim << " particles in each dimension\n";
    }
    double hx = (xmax-xmin)/(npdim-1);
    double hy = (ymax-ymin)/(npdim-1);
    double hz = (zmax-zmin)/(npdim-1);
    int n = 0;
    for (size_t i = 0; i < npdim; i++) {
      for (size_t j = 0; j < npdim; j++) {
        for (size_t k = 0; k < npdim; k++) {
	  Point<3> pt=pts[n];
          pt[0] = xmin + i*hx;
          pt[1] = ymin + j*hy;
          pt[2] = zmin + k*hz;
	  pts[n] = pt;
          n++;
        }
      }
    }
    if (distribution == 2) {
      srand(time(NULL));
      unsigned int rand_state;
      for (size_t i = 0; i < nparticles; i++) {
	Point<3> pt=pts[i];
        pt[0] += 0.25*hx*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
        pt[1] += 0.25*hy*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
        pt[2] += 0.25*hz*((2*static_cast<double>(rand_r(&rand_state))/RAND_MAX)-1.0);
	pt[0] = fmax(xmin,fmin(xmax,pt[0]));
	pt[1] = fmax(ymin,fmin(ymax,pt[1]));
	pt[2] = fmax(zmin,fmin(zmax,pt[2]));
	pts[i] = pt;
      }
    }
  }
  
  std::shared_ptr<Swarm<3>> swarm = make_shared<Swarm<3>>(pts_sp);
  return swarm;
}

}
}

#endif // SWARM_H_INC_

