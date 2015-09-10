/*---------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *---------------------------------------------------------------------------~*/

#ifndef JALI_MESH_WRAPPER_H_
#define JALI_MESH_WRAPPER_H_

#include <cassert>

#include "Mesh.hh"                      // Jali mesh header


/*!
  \class Jali_Mesh_Wrapper jali_mesh_wrapper.h
  \brief Jali_Mesh_Wrapper implements mesh methods for Jali

  Jali_Mesh_Wrapper implements methods required for Portage mesh
  queries for the Jali mesh infrastructure
*/


class Jali_Mesh_Wrapper {
 public:

  //! Constructor
  Jali_Mesh_Wrapper(Jali::Mesh const & mesh) : 
      mesh_(mesh) 
  {}

  //! Copy constructor (disabled)
  Jali_Mesh_Wrapper(Jali_Mesh_Wrapper const &) = delete;

  //! Assignment operator (disabled)
  Jali_Mesh_Wrapper & operator=(Jali_Mesh_Wrapper const &) = delete;
  
  //! Empty destructor 
  ~Jali_Mesh_Wrapper() {};
  
  //! Number of owned cells in the mesh
  int num_owned_cells() const {
    return mesh_.num_entities(Jali::CELL, Jali::OWNED);
  }

  //! Number of ghost cells in the mesh
  int num_ghost_cells() const {
    return mesh_.num_entities(Jali::CELL, Jali::GHOST);
  }

  //! Get list of nodes for a cell
  void cell_get_nodes(int cellid, std::vector<int> *nodes) const {
    mesh_.cell_get_nodes(cellid, nodes);
  }

  //! 1D version of coords of a node
  void node_get_coordinates(int const nodeid, double *x) const {
    JaliGeometry::Point p;
    mesh_.node_get_coordinates(nodeid, &p);
    assert(p.dim() == 1);
    *x = p[0];
  }

  //! 2D version of coords of a node
  void node_get_coordinates(int const nodeid, 
                            std::pair<double,double> *xy) const {
    JaliGeometry::Point p;
    mesh_.node_get_coordinates(nodeid, &p);
    assert(p.dim() == 2);
    xy->first = p[0];
    xy->second = p[1];
  }

  //! 3D version of coords of a node
  void node_get_coordinates(int const nodeid, 
                            std::tuple<double,double,double> *xyz) const {
    JaliGeometry::Point p;
    mesh_.node_get_coordinates(nodeid, &p);
    assert(p.dim() == 3);
    std::get<0>(*xyz) = p[0];
    std::get<1>(*xyz) = p[1];
    std::get<2>(*xyz) = p[2];
  }

  //! 1D version of coords of nodes of a cell

  void cell_get_coordinates(int const cellid, std::vector<double> *xlist) const {
    assert(mesh_.space_dimension() == 1);

    std::vector<JaliGeometry::Point> plist;
    mesh_.cell_get_coordinates(cellid, &plist);

    // should convert to a std::for_each or std::transform
    xlist->resize(plist.size());
    std::vector<JaliGeometry::Point>::iterator itp = plist.begin();
    std::vector<double>::iterator itx = xlist->begin();
    while (itp != plist.end()) {
      JaliGeometry::Point p = *itp;
      *itx = p[0];
      ++itp;
      ++itx;
    }
  }

  //! 2D version of coords of nodes of a cell

  void cell_get_coordinates(int const cellid, 
                            std::vector<std::pair<double,double> > *xylist) const {
    assert(mesh_.space_dimension() == 2);

    std::vector<JaliGeometry::Point> plist;
    mesh_.cell_get_coordinates(cellid, &plist);

    // should convert to a std::for_each or std::transform
    xylist->resize(plist.size());
    std::vector<JaliGeometry::Point>::iterator itp = plist.begin();
    std::vector<std::pair<double,double> >::iterator itx = xylist->begin();
    while (itp != plist.end()) {
      JaliGeometry::Point p = *itp;
      *itx = std::pair<double,double>(p[0],p[1]);
      ++itp;
      ++itx;
    }
  }

  //! 3D version of coords of nodes of a cell

  void cell_get_coordinates(int const cellid, 
                            std::vector<std::tuple<double,double,double> > *xyzlist) const {
    assert(mesh_.space_dimension() == 3);

    std::vector<JaliGeometry::Point> plist;
    mesh_.cell_get_coordinates(cellid, &plist);

    // should convert to a std::for_each or std::transform

    xyzlist->resize(plist.size());
    std::vector<JaliGeometry::Point>::iterator itp = plist.begin();
    std::vector<std::tuple<double,double,double> >::iterator itx = xyzlist->begin();
    while (itp != plist.end()) {
      JaliGeometry::Point p = *itp;
      *itx = std::tuple<double,double,double>(p[0],p[1],p[2]);
      ++itp;
      ++itx;
    }

  }

 private:
  Jali::Mesh const & mesh_;

}; // class Jali_Mesh_Wrapper


struct pointsToXY
{
	pointsToXY() { }
	std::vector<std::pair<double,double> > operator()(const std::vector<JaliGeometry::Point> ptList){    
		std::vector<std::pair<double, double> > xyList;
		std::for_each(ptList.begin(), ptList.end(), [&xyList](JaliGeometry::Point pt){xyList.emplace_back(pt.x(), pt.y());});								     
		return xyList;
	}
};

struct cellToXY
{
    Jali::Mesh const *mesh;
    cellToXY(const Jali::Mesh* mesh):mesh(mesh){}
    std::vector<std::pair<double, double> > operator()(const Jali::Entity_ID cellID){
        // Get the Jali Points for each candidate cells
        std::vector<JaliGeometry::Point> cellPoints;
        mesh->cell_get_coordinates(cellID, &cellPoints);
        //Change to XY coords for clipper
        return pointsToXY()(cellPoints);
    }
};

#endif // JALI_MESH_WRAPPER_H_
