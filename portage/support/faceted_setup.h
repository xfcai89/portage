/*
  This file is part of the Ristra portage project.
  Please see the license file at the root of this repository, or at:
  https://github.com/laristra/portage/blob/master/LICENSE
*/

#ifndef FACETED_SETUP_H
#define FACETED_SETUP_H

#include <vector>
#include <algorithm>
#include <stdexcept>

#include "wonton/mesh/AuxMeshTopology.h"
#include "wonton/support/Point.h"
#include "portage/support/weight.h"
#include "portage/support/portage.h"

namespace Portage {
  namespace Meshfree {
    namespace Weight {

      /** @brief Function to setup faceted weight data give a mesh wrapper. 
       *
       * @param[in] mesh Mesh wrapper from which to pull face normals and distances
       * @param[out] smoothing_lengths the output normals and distances
       * @param[out] extents the output bounding boxes of the cells in @code mesh @endcode
       * @param[in] smoothing_factor multiple of distance from center to edge to use for smoothing length
       * @param[in] boundary_factor same as @code smoothing_factor but for boundaries
       *            zero means use @code smoothing_factor
       * 
       * The smoothing_lengths object has the following structure: 
       * smoothing_lengths[i][j][k] is interpreted as the k-th component of the normal vector 
       * to the j-th face of the i-th cell, for k=0,...,dim-1, where dim is the spatial 
       * dimension of the problem. For k=dim, it is the distance of the j-th face from 
       * the centroid of the i-th cell. For 3D faces that are non-planar, the normal is the 
       * average of the normals to the triangles formed by adjacent vertices and face centroids.  
       */

      template<int DIM, class Mesh_Wrapper> void faceted_setup_cell
        (const Mesh_Wrapper &mesh, 
         Portage::vector<std::vector<std::vector<double>>> &smoothing_lengths,
         Portage::vector<Wonton::Point<DIM>> &extents,
         double smoothing_factor, double boundary_factor)
      {
	if (smoothing_factor<0.) {
	  throw std::runtime_error("smoothing_factor must be non-negative");
	}
	if (boundary_factor<0.) {
	  throw std::runtime_error("boundary_factor must be non-negative");
	}
	
        int ncells=mesh.num_owned_cells();
        smoothing_lengths.resize(ncells);
        extents.resize(ncells);
        
        // get face normal and distance information
        for (int i=0; i<ncells; i++) {
          std::vector<int> faces, fdirs;
          mesh.cell_get_faces_and_dirs(i, &faces, &fdirs);

          int nfaces = faces.size();
          std::vector<std::vector<double>> h = smoothing_lengths[i];
          h.resize(nfaces);

          Wonton::Point<DIM> fcent, ccent, normal, distance;
          std::vector<int> fnodes;
          std::vector<Wonton::Point<DIM>> fncoord;
          for (int j=0; j<nfaces; j++) {
            // get face data
            mesh.face_centroid(faces[j], &fcent);
            mesh.cell_centroid(i, &ccent);
            mesh.face_get_nodes(faces[j], &fnodes);
            fncoord.resize(fnodes.size()+1);
            for (int k=0; k<fnodes.size(); k++) {
              mesh.node_get_coordinates(fnodes[k], &(fncoord[k]));
            }
            fncoord[fnodes.size()] = fncoord[0]; 

            if (DIM==2) {
              // Get 2d face normal 
              std::vector<double> delta(2);
              for (int k=0;k<2;k++) delta[k] = fncoord[1][k] - fncoord[0][k];
              normal[0] =  delta[1];
              normal[1] = -delta[0];
              if (fdirs[j]<0) normal*=-1.0;
              double norm = sqrt(normal[0]*normal[0] + normal[1]*normal[1]);
              normal /= norm;
              h[j].resize(3);
              h[j][0] = normal[0];
              h[j][1] = normal[1];
            } else if (DIM==3) {
              // Get 3d face normal using average cross product, in case face is not flat
              // or has nodes close together.
              std::vector<std::vector<double>> bivec(2,std::vector<double>(3,0.));
              for (int k=0;k<3;k++) {
                bivec[0][k] = fncoord[0][k] - fcent[k];
                normal[k] = 0.;
              }
              for (int k=1; k<=fnodes.size(); k++) {
                for (int m=0;m<3;m++) bivec[1][m] = fncoord[k][m] - fcent[m];
                std::vector<double> cross(3,0.);
                cross[0] =  bivec[1][2]*bivec[0][1] - bivec[1][1]*bivec[0][2];
                cross[1] = -bivec[1][2]*bivec[0][0] + bivec[1][0]*bivec[0][2];
                cross[2] =  bivec[1][1]*bivec[0][0] - bivec[1][0]*bivec[0][1];
                double norm=sqrt(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]);
                for (int m=0;m<3;m++) {
                  cross[m]/=norm;
                }
                normal += cross;
                bivec[0] = bivec[1];
              }
              if (fdirs[j]<0) normal*=-1.0;
              double norm=0.;
              for (int k=0;k<3;k++) norm+=normal[k]*normal[k];
              norm=sqrt(norm);
              normal /= norm;
              h[j].resize(4);
              for (int k=0;k<3;k++) h[j][k] = normal[k];
            }

            distance = Wonton::Point<DIM>(fcent-ccent);
            double smoothing = 0.0;
            for (int k=0; k<DIM; k++) smoothing += 2*distance[k]*normal[k];
            if (smoothing<0.0) {  // when nodes are ordered backwards
              normal = -normal;
              smoothing = -smoothing;
            }
	    double factor;
	    if (mesh.on_exterior_boundary(Wonton::FACE, j) and boundary_factor > 0.) {
	      h[j][DIM] = boundary_factor*smoothing;
	    } else {
	      h[j][DIM] = smoothing_factor*smoothing;
	    }
          }
          smoothing_lengths[i] = h;
        }

        // get extent information
        for (int i=0; i<ncells; i++) {
          // calculate the min and max of coordinates
          std::vector<int> node_indices;
          mesh.cell_get_nodes(i, &node_indices);
          Wonton::Point<DIM> cmin, cmax, first_coords; 
          mesh.node_get_coordinates(node_indices[0], &first_coords);
          for (int k=0; k<DIM; k++) {cmin[k] = first_coords[k]; cmax[k] = first_coords[k];}
          for (int j=1; j<node_indices.size(); j++) {
            Wonton::Point<DIM> node_coords;
            mesh.node_get_coordinates(node_indices[j], &node_coords);
            for (int k=0; k<DIM; k++) {
              cmin[k] = std::min(node_coords[k], cmin[k]);
              cmax[k] = std::max(node_coords[k], cmax[k]);
            }
          }

          // subtract to get extents
          Wonton::Point<DIM> dx;
          for (int k=0; k<DIM; k++) dx[k] = cmax[k] - cmin[k];

          // multiply by smoothing_factor
          for (int k=0; k<DIM; k++) dx[k] *= 2.*smoothing_factor;

          extents[i] = dx;
        }
      }


      /** @brief Function to setup faceted weight data give a set of mesh wrappers. 
       *
       * @param[in]  meshes Mesh wrapper pointers from which to pull face normals and distances
       * @param[out] smoothing_lengths the output normals and distances
       * @param[out] extents the output bounding boxes of the cells in @code mesh @endcode
       * @param[in]  smoothing_factor multiple of distance from center to edge to use for smoothing length
       * @param[in]  boundary_factor same as @code smoothing_factor but for boundaries
       *             zero means use @code smoothing_factor
       * 
       * The smoothing_lengths object has the following structure: 
       * smoothing_lengths[i][j][k] is interpreted as the k-th component of the normal vector 
       * to the j-th face of the i-th cell, for k=0,...,dim-1, where dim is the spatial 
       * dimension of the problem. For k=dim, it is the distance of the j-th face from 
       * the centroid of the i-th cell. For 3D faces that are non-planar, the normal is the 
       * average of the normals to the triangles formed by adjacent vertices and face centroids.  
       */

      template<int DIM, class Mesh_Wrapper> void faceted_setup_cell
        (std::vector<Mesh_Wrapper*> meshes, 
         Portage::vector<std::vector<std::vector<double>>> &smoothing_lengths,
         Portage::vector<Wonton::Point<DIM>> &extents,
         double smoothing_factor, double boundary_factor) 
      {
        size_t total_cells=0;
        for ((const Mesh_Wrapper)* meshptr: meshes) {
          const Mesh_Wrapper &mesh(*meshptr);
          total_cells += mesh.num_owned_cells();
        }

        smoothing_lengths.resize(total_cells);
        extents.resize(total_cells);

        size_t offset = 0;
        for ((const Mesh_Wrapper)* meshptr: meshes) {
          const Mesh_Wrapper &mesh(*meshptr);
          size_t ncells = mesh.num_owned_cells();
          Portage::vector<std::vector<std::vector<double>>> sl;
          Portage::vector<Wonton::Point<DIM>> ex;
          faceted_setup_cell<DIM, Mesh_Wrapper>(mesh,sl,ex,smoothing_factor,boundary_factor);
          for (size_t i=0; i<ncells; i++) {
            smoothing_lengths[offset+i] = sl[i];
            extents[offset+i] = ex[i];
          }
          offset += ncells;
        }
      }

    } // namespace Weight
  } // namespace Meshfree
} // namespace Portage
#endif
