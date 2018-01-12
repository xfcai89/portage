/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/


#include <iostream>
#include <memory>
#include <cmath>

#include "portage/interpolate/interpolate_2nd_order.h"

#include "gtest/gtest.h"

#include "portage/support/portage.h"
#include "portage/wonton/mesh/simple_mesh/simple_mesh_wrapper.h"
#include "portage/wonton/state/simple_state/simple_state_wrapper.h"
#include "portage/interpolate/test/simple_intersect_for_tests.h"

/// Second order interpolation of constant cell-centered field with no
/// limiter in 2D

double TOL = 1e-12;

TEST(Interpolate_2nd_Order, Cell_Ctr_Const_No_Limiter_2D) {

  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 5, 5);

  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

  // Define a state vector with constant value and add it to the source state

  std::vector<double> data(ncells_source, 1.25);
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));
  
  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<2>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<2>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<2>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<2, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars");

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each cell on the target
  const double stdval = data[0];
  for (int c = 0; c < ncells_target; ++c) ASSERT_DOUBLE_EQ(stdval, outvals[c]);
}


/// Second order interpolation of linear cell-centered field with no
/// limiting in 2D

TEST(Interpolate_2nd_Order, Cell_Ctr_Lin_No_Limiter_2D) {
  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 5, 5);

  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

// Define a state vector with linear value and add it to the source state

  std::vector<double> data(ncells_source);
  for (int c = 0; c < ncells_source; ++c) {
    Portage::Point<2> cen;
    sourceMeshWrapper.cell_centroid(c, &cen);
    data[c] = cen[0]+cen[1];
  }
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));  

  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<2>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<2>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<2>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<2, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars");

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);
  
  // Make sure we retrieved the correct value for each cell on the target
  
  std::vector<double> stdvals(ncells_target);
  for (int c = 0; c < ncells_target; ++c) {
    Portage::Point<2> cen;
    targetMeshWrapper.cell_centroid(c, &cen);
    stdvals[c] = cen[0]+cen[1];
  }

  for (int c = 0; c < ncells_target; ++c)
    ASSERT_DOUBLE_EQ(stdvals[c], outvals[c]);

}


///*!
//  @brief Second order interpolate of linear cell-centered field with
//  Barth-Jespersen limiting in 2D
// STAR/

TEST(Interpolate_2nd_Order, Cell_Ctr_Lin_BJ_Limiter_2D) {

  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 1.0, 1.0, 5, 5);

  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

  // Define a state vector with linear value and add it to the source state

  double minval = 1e+10, maxval = -1e+10;
  
  std::vector<double> data(ncells_source);
  for (int c = 0; c < ncells_source; ++c) {
    Portage::Point<2> ccen;
    sourceMeshWrapper.cell_centroid(c, &ccen);
    if (ccen[0] < 0.5)
      data[c] = ccen[0]+ccen[1];
    else
      data[c] = 100*ccen[0];
    if (data[c] < minval) minval = data[c];
    if (data[c] > maxval) maxval = data[c];
  }
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));  

  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<2>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<2>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals1(ncells_target);
  std::vector<double> outvals2(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<2>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<2, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars", Portage::NOLIMITER);

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals1.begin(), interpolater);
  
  interpolater.set_interpolation_variable("cellvars", Portage::BARTH_JESPERSEN);

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals2.begin(), interpolater);
  
  // Make sure we retrieved the correct value for each cell on the target

  std::vector<double> stdvals(ncells_target);
  for (int c = 0; c < ncells_target; ++c) {
    Portage::Point<2> cen;
    targetMeshWrapper.cell_centroid(c, &cen);
    stdvals[c] = cen[0]+cen[1];
  }
  
  // Check if we violated the bounds on at least one cell in the
  // unlimited interpolation and if we respected the bounds in all
  // cells in the limited case

  bool outofbounds_unlimited = false;
  for (int c = 0; c < ncells_target; ++c) {
    if (outvals1[c] < minval  || outvals1[c] > maxval) {
      outofbounds_unlimited = true;
      break;
    }
  }

  // Check if we preserved bounds on interior cells. Since we don't limit on
  // boundary cells we have no guarantee their values will be within bounds
  bool inbounds_limited = true;
  for (int c = 0; c < ncells_target; ++c) {
    if (!targetMeshWrapper.on_exterior_boundary(Portage::CELL, c)) {
      if (outvals2[c] < minval - TOL  || outvals2[c] > maxval + TOL) {
        inbounds_limited = false;
        break;
      }
    }
  }
  EXPECT_TRUE(outofbounds_unlimited && inbounds_limited);

}

/*


/// Second order interpolation of constant node-centered field with no
/// limiting in 2D

TEST(Interpolate_2nd_Order, Node_Ctr_Const_No_Limiter) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  if (Jali::framework_available(Jali::MSTK))
    mf.framework(Jali::MSTK);
  mf.included_entities({Jali::Entity_kind::EDGE,
                        Jali::Entity_kind::FACE,
                        Jali::Entity_kind::WEDGE,
                        Jali::Entity_kind::CORNER});

  std::shared_ptr<Jali::Mesh> source_mesh = mf(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Jali::Mesh> target_mesh = mf(0.0, 0.0, 1.0, 1.0, 5, 5);

  const int nnodes_source =
      source_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);
  const int nnodes_target =
      target_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);

  Jali::Entity_ID_List const& targetnodes =
      target_mesh->nodes<Jali::Entity_type::ALL>();

  // Create a state object and add the first two vectors to it

  Jali::State source_state(source_mesh);


  // Define two state vectors, one with constant value, the other
  // with a linear function

  std::vector<double> data(nnodes_source, 1.5);
  Jali::StateVector<double> myvec("nodevars", source_mesh,
                                  Jali::Entity_kind::NODE,
                                  Jali::Entity_type::PARALLEL_OWNED,
                                  &(data[0]));
  source_state.add(myvec);

  Wonton::Jali_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Jali_Mesh_Wrapper targetMeshWrapper(*target_mesh);
  Wonton::Jali_State_Wrapper sourceStateWrapper(source_state);

  // Interpolate from source to target mesh

  std::vector<double> outvals(nnodes_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(nnodes_target);

  // Gather the dual cell coordinates for source and target meshes for
  // intersection

  std::vector<std::vector<JaliGeometry::Point>>
      source_dualcell_coords(nnodes_source);
  std::vector<std::vector<JaliGeometry::Point>>
      target_dualcell_coords(nnodes_target);

  // Because the meshes are rectangular we can get away with examining
  // the coordinates of the corners instead of the wedges

  // Also, because we will use only the bounding box of each cell to
  // do the search and intersection we can get away with adding all
  // the coordinates of the corners to list including duplicates

  for (int n = 0; n < nnodes_source; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    source_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      source_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        source_dualcell_coords[n].push_back(coord);
    }
  }

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    target_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      target_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        target_dualcell_coords[n].push_back(coord);
    }
  }


  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<int> xnodes;
    std::vector<std::vector<double>> xweights;

    BOX_INTERSECT::intersection_moments(target_dualcell_coords[n],
                                        source_dualcell_coords,
                                        &xnodes, &xweights);

    std::vector<Portage::Weights_t> wtsvec(xnodes.size());
    for (int i = 0; i < xnodes.size(); ++i) {
      wtsvec[i].entityID = xnodes[i];
      wtsvec[i].weights = xweights[i];
    }
    sources_and_weights[n] = wtsvec;
  }

  // Create Interpolation object

  Portage::Interpolate_2ndOrder<2, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  // Compute the target mesh vals

  interpolater.set_interpolation_variable("nodevars", Portage::NOLIMITER);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each node on the
  // target. Second order interpolation should preserve a linear field
  // exactly but node-centered interpolation has some quirks - the field
  // does not get preserved exactly at the boundary because the source
  // values for boundary dual cells are not specified inside the dual
  // cells but at one of their vertices or edges. So, check only
  // interior nodes

  const double stdval = data[0];
  for (int n = 0; n < nnodes_target; ++n) {
    JaliGeometry::Point coord;
    target_mesh->node_get_coordinates(n, &coord);
    if (fabs(coord[0]) < 1e-16 || fabs(1-coord[0]) < 1e-16 ||
        fabs(coord[1]) < 1e-16 || fabs(1-coord[1]) < 1.e-16)
      continue;
    ASSERT_NEAR(stdval, outvals[n], TOL);
  }
}


// Second order interpolation of linear node-centered field with no
// limiting in 2D

TEST(Interpolate_2nd_Order, Node_Ctr_Lin_No_Limiter) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  if (Jali::framework_available(Jali::MSTK))
    mf.framework(Jali::MSTK);
  mf.included_entities({Jali::Entity_kind::EDGE,
                        Jali::Entity_kind::FACE,
                        Jali::Entity_kind::WEDGE,
                        Jali::Entity_kind::CORNER});

  std::shared_ptr<Jali::Mesh> source_mesh = mf(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Jali::Mesh> target_mesh = mf(0.0, 0.0, 1.0, 1.0, 5, 5);

  const int nnodes_source =
      source_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);
  const int nnodes_target =
      target_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);

  Jali::Entity_ID_List const& targetnodes =
      target_mesh->nodes<Jali::Entity_type::ALL>();

  // Create a state object and add the first two vectors to it

  Jali::State source_state(source_mesh);

  // Define a state vectors representing a linear function

  std::vector<double> data(nnodes_source);
  for (int n = 0; n < nnodes_source; ++n) {
    JaliGeometry::Point coord;
    source_mesh->node_get_coordinates(n, &coord);
    data[n] = coord[0]+coord[1];
  }
  Jali::StateVector<double> myvec("nodevars", source_mesh,
                                  Jali::Entity_kind::NODE,
                                  Jali::Entity_type::PARALLEL_OWNED,
                                  &(data[0]));
  source_state.add(myvec);

  // Create a mesh wrapper

  Wonton::Jali_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Jali_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // Gather the dual cell coordinates for source and target meshes for
  // intersection

  std::vector<std::vector<JaliGeometry::Point>>
      source_dualcell_coords(nnodes_source);
  std::vector<std::vector<JaliGeometry::Point>>
      target_dualcell_coords(nnodes_target);

  // Because the meshes are rectangular we can get away with examining
  // the coordinates of the corners instead of the wedges

  // Also, because we will use only the bounding box of each cell to
  // do the search and intersection we can get away with adding all
  // the coordinates of the corners to list including duplicates

  for (int n = 0; n < nnodes_source; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    source_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      source_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        source_dualcell_coords[n].push_back(coord);
    }
  }

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    target_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      target_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        target_dualcell_coords[n].push_back(coord);
    }
  }

  // Interpolate from source to target mesh

  std::vector<double> outvals(nnodes_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(nnodes_target);

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<int> xnodes;
    std::vector<std::vector<double>> xweights;

    BOX_INTERSECT::intersection_moments(target_dualcell_coords[n],
                                        source_dualcell_coords,
                                        &xnodes, &xweights);

    std::vector<Portage::Weights_t> wtsvec(xnodes.size());
    for (int i = 0; i < xnodes.size(); ++i) {
      wtsvec[i].entityID = xnodes[i];
      wtsvec[i].weights = xweights[i];
    }
    sources_and_weights[n] = wtsvec;
  }


  // Create Interpolation object

  Portage::Interpolate_2ndOrder<2, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper,
                   source_state);

  // Compute the target mesh vals

  interpolater.set_interpolation_variable("nodevars", Portage::NOLIMITER);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each node on the
  // target. Second order interpolation should preserve a linear field
  // exactly but node-centered interpolation has some quirks - the field
  // does not get preserved exactly at the boundary because the source
  // values for boundary dual cells are not specified inside the dual
  // cells but at one of their vertices or edges. So, check only
  // interior nodes

  std::vector<double> stdvals(nnodes_target);
  for (int n = 0; n < nnodes_target; ++n) {
    JaliGeometry::Point coord;
    target_mesh->node_get_coordinates(n, &coord);
    if (fabs(coord[0]) < 1e-16 || fabs(1-coord[0]) < 1e-16 ||
        fabs(coord[1]) < 1e-16 || fabs(1-coord[1]) < 1.e-16)
      continue;
    stdvals[n] = coord[0]+coord[1];
    EXPECT_NEAR(stdvals[n], outvals[n], TOL);
  }
}
*/

/// Second order interpolation of constant cell-centered field with no
/// limiting in 3D

TEST(Interpolate_2nd_Order, Cell_Ctr_Const_No_Limiter_3D) {

  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 4, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 5, 5, 5);

  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

  // Define a state vector with constant value and add it to the source state

  std::vector<double> data(ncells_source, 1.25);
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));
  
  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<3>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<3>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<3>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<3, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars");

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each cell on the target
  const double stdval = data[0];
  for (int c = 0; c < ncells_target; ++c) ASSERT_DOUBLE_EQ(stdval, outvals[c]);

}

/// Second order interpolation of linear cell-centered field with no
/// limiting in 3D

TEST(Interpolate_2nd_Order, Cell_Ctr_Lin_No_Limiter_3D) {
  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 4, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 5, 5, 5);

  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

// Define a state vector with linear value and add it to the source state

  std::vector<double> data(ncells_source);
  for (int c = 0; c < ncells_source; ++c) {
    Portage::Point<2> cen;
    sourceMeshWrapper.cell_centroid(c, &cen);
    data[c] = cen[0]+cen[1]+cen[2];
  }
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));  

  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<3>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<3>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<3>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<3, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars");

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);
  
  // Make sure we retrieved the correct value for each cell on the target
  
  std::vector<double> stdvals(ncells_target);
  for (int c = 0; c < ncells_target; ++c) {
    Portage::Point<2> cen;
    targetMeshWrapper.cell_centroid(c, &cen);
    stdvals[c] = cen[0]+cen[1]+cen[2];
  }

  for (int c = 0; c < ncells_target; ++c)
    ASSERT_DOUBLE_EQ(stdvals[c], outvals[c]);

}

/// Second order interpolation of discontinuous cell-centered field with
/// Barth-Jespersen limiting in 3D

TEST(Interpolate_2nd_Order, Cell_Ctr_BJ_Limiter_3D) {

  // Create simple meshes
  
  std::shared_ptr<Portage::Simple_Mesh> source_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 4, 4, 4);
  std::shared_ptr<Portage::Simple_Mesh> target_mesh =
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 5, 5, 5);


  // Create mesh wrappers
  
  Wonton::Simple_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Simple_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // count cells
  
  const int ncells_source =
      sourceMeshWrapper.num_owned_cells();
  const int ncells_target =
      targetMeshWrapper.num_owned_cells();

  // Create a state object

  Portage::Simple_State source_state(source_mesh);

  // Define a state vector with linear value and add it to the source state

  double minval = 1e+10, maxval = -1e+10;
  
  std::vector<double> data(ncells_source);
  for (int c = 0; c < ncells_source; ++c) {
    Portage::Point<3> ccen;
    sourceMeshWrapper.cell_centroid(c, &ccen);
    if (ccen[0] < 0.5)
      data[c] = ccen[0]+ccen[1]+ccen[2];
    else
      data[c] = 100*ccen[0];
    if (data[c] < minval) minval = data[c];
    if (data[c] > maxval) maxval = data[c];
  }
  source_state.add("cellvars", Portage::Entity_kind::CELL, &(data[0]));  

  // Create state wrapper
  
  Wonton::Simple_State_Wrapper sourceStateWrapper(source_state);

  // Gather the cell coordinates as Portage Points for source and target meshes 
  // for intersection. The outer vector is the cells, the inner vector is the
  // points of the vertices of that cell.

  std::vector<std::vector<Portage::Point<3>>>
      source_cell_coords(ncells_source);
  std::vector<std::vector<Portage::Point<3>>>
      target_cell_coords(ncells_target);

  // Actually get the Portage::Points
  
  for (int c = 0; c < ncells_source; ++c)
    source_mesh->cell_get_coordinates(c, &(source_cell_coords[c]));
  for (int c = 0; c < ncells_target; ++c)
    target_mesh->cell_get_coordinates(c, &(target_cell_coords[c]));

  // Interpolate from source to target mesh using the independent calculation
  // in simple_intersect_for_tests.h

  std::vector<double> outvals1(ncells_target);
  std::vector<double> outvals2(ncells_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(ncells_target);
      
  // Loop over target cells
      
  for (int c = 0; c < ncells_target; ++c) {
  
    std::vector<int> xcells;
    std::vector<std::vector<double>> xwts;

		// Compute the moments
		// xcells is the source cell indices that intersect
		// xwts is the moments vector for each cell that intersects
		
    BOX_INTERSECT::intersection_moments<3>(target_cell_coords[c],
                                        source_cell_coords,
                                        &xcells, &xwts);

    // Pack the results into a vector of true Portage::Weights_t
    
    std::vector<Portage::Weights_t> wtsvec(xcells.size());
    for (int i = 0; i < xcells.size(); ++i) {
      wtsvec[i].entityID = xcells[i];
      wtsvec[i].weights = xwts[i];
    }
    
    // Put the weights in final form
    
    sources_and_weights[c] = wtsvec;
  }
  
  // Now do it the Portage way
  
  // Create Interpolation object

  Portage::Interpolate_2ndOrder<3, Portage::CELL,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_Mesh_Wrapper,
                                Wonton::Simple_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper, sourceStateWrapper);

  interpolater.set_interpolation_variable("cellvars", Portage::NOLIMITER);

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals1.begin(), interpolater);
  
  interpolater.set_interpolation_variable("cellvars", Portage::BARTH_JESPERSEN);

      
  Portage::transform(targetMeshWrapper.begin(Portage::Entity_kind::CELL), 
                     targetMeshWrapper.end(Portage::Entity_kind::CELL),
                     sources_and_weights.begin(),
                     outvals2.begin(), interpolater);
  
  // Make sure we retrieved the correct value for each cell on the target

  std::vector<double> stdvals(ncells_target);
  for (int c = 0; c < ncells_target; ++c) {
    Portage::Point<3> cen;
    targetMeshWrapper.cell_centroid(c, &cen);
    stdvals[c] = cen[0]+cen[1]+cen[2];
  }
  
  // Check if we violated the bounds on at least one cell in the
  // unlimited interpolation and if we respected the bounds in all
  // cells in the limited case

  bool outofbounds_unlimited = false;
  for (int c = 0; c < ncells_target; ++c) {
    if (outvals1[c] < minval  || outvals1[c] > maxval) {
      outofbounds_unlimited = true;
      break;
    }
  }

  // Check if we preserved bounds on interior cells. Since we don't limit on
  // boundary cells we have no guarantee their values will be within bounds
  bool inbounds_limited = true;
  for (int c = 0; c < ncells_target; ++c) {
    if (!targetMeshWrapper.on_exterior_boundary(Portage::CELL, c)) {
      if (outvals2[c] < minval - TOL  || outvals2[c] > maxval + TOL) {
        inbounds_limited = false;
        break;
      }
    }
  }
  EXPECT_TRUE(outofbounds_unlimited);
  EXPECT_TRUE(inbounds_limited);

}

/*


/// Second order interpolation of constant node-centered field with no
/// limiting in 3D

TEST(Interpolate_2nd_Order, Node_Ctr_Const_No_Limiter_3D) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  if (Jali::framework_available(Jali::MSTK))
    mf.framework(Jali::MSTK);
  mf.included_entities({Jali::Entity_kind::EDGE,
                        Jali::Entity_kind::FACE,
                        Jali::Entity_kind::WEDGE,
                        Jali::Entity_kind::CORNER});

  std::shared_ptr<Jali::Mesh> source_mesh = mf(0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
                                               4, 4, 4);
  std::shared_ptr<Jali::Mesh> target_mesh = mf(0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
                                               5, 5, 5);

  const int nnodes_source =
      source_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);
  const int nnodes_target =
      target_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);

  Jali::Entity_ID_List const& targetnodes =
      target_mesh->nodes<Jali::Entity_type::ALL>();

  // Create a state object and add the first two vectors to it

  Jali::State source_state(source_mesh);

  // Define a state vectors representing a linear function

  const double nodeval = 3.0;
  std::vector<double> data(nnodes_source, nodeval);
  Jali::StateVector<double> myvec("nodevars", source_mesh,
                                  Jali::Entity_kind::NODE,
                                  Jali::Entity_type::PARALLEL_OWNED,
                                  &(data[0]));
  source_state.add(myvec);

  // Create a mesh wrapper

  Wonton::Jali_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Jali_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // Gather the dual cell coordinates for source and target meshes for
  // intersection

  std::vector<std::vector<JaliGeometry::Point>>
      source_dualcell_coords(nnodes_source);
  std::vector<std::vector<JaliGeometry::Point>>
      target_dualcell_coords(nnodes_target);

  // Because the meshes are rectangular we can get away with examining
  // the coordinates of the corners instead of the wedges

  // Also, because we will use only the bounding box of each cell to
  // do the search and intersection we can get away with adding all
  // the coordinates of the corners to list including duplicates

  for (int n = 0; n < nnodes_source; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    source_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      source_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        source_dualcell_coords[n].push_back(coord);
    }
  }

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    target_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      target_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        target_dualcell_coords[n].push_back(coord);
    }
  }

  // Interpolate from source to target mesh

  std::vector<double> outvals(nnodes_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(nnodes_target);

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<int> xnodes;
    std::vector<std::vector<double>> xweights;

    BOX_INTERSECT::intersection_moments(target_dualcell_coords[n],
                                        source_dualcell_coords,
                                        &xnodes, &xweights);

    std::vector<Portage::Weights_t> wtsvec(xnodes.size());
    for (int i = 0; i < xnodes.size(); ++i) {
      wtsvec[i].entityID = xnodes[i];
      wtsvec[i].weights = xweights[i];
    }
    sources_and_weights[n] = wtsvec;
  }

  // Create Interpolation object

  Portage::Interpolate_2ndOrder<3, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper,
                   source_state);

  // Compute the target mesh vals

  interpolater.set_interpolation_variable("nodevars", Portage::NOLIMITER);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each node on the
  // target Second order interpolation should preserve a linear field
  // exactly but node-centered interpolation has some quirks - the field
  // does not get preserved exactly at the boundary because the source
  // values for boundary dual cells are not specified inside the dual
  // cells but at one of their vertices or edges. So, check only
  // interior nodes

  for (int n = 0; n < nnodes_target; ++n) {
    JaliGeometry::Point coord;
    target_mesh->node_get_coordinates(n, &coord);
    if (fabs(coord[0]) < 1e-16 || fabs(1-coord[0]) < 1e-16 ||
        fabs(coord[1]) < 1e-16 || fabs(1-coord[1]) < 1.e-16 ||
        fabs(coord[2]) < 1e-16 || fabs(1-coord[2]) < 1.e-16)
      continue;
    EXPECT_NEAR(nodeval, outvals[n], TOL);
  }
}


TEST(Interpolate_2nd_Order, Node_Ctr_Lin_No_Limiter_3D) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  if (Jali::framework_available(Jali::MSTK))
    mf.framework(Jali::MSTK);
  mf.included_entities({Jali::Entity_kind::EDGE,
                        Jali::Entity_kind::FACE,
                        Jali::Entity_kind::WEDGE,
                        Jali::Entity_kind::CORNER});

  std::shared_ptr<Jali::Mesh> source_mesh = mf(0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
                                               4, 4, 4);
  std::shared_ptr<Jali::Mesh> target_mesh = mf(0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
                                               5, 5, 5);

  const int nnodes_source =
      source_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);
  const int nnodes_target =
      target_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);

  Jali::Entity_ID_List const& targetnodes =
      target_mesh->nodes<Jali::Entity_type::ALL>();

  // Create a state object and add the first two vectors to it

  Jali::State source_state(source_mesh);

  // Define a state vectors representing a linear function

  std::vector<double> data(nnodes_source);
  for (int n = 0; n < nnodes_source; ++n) {
    JaliGeometry::Point coord;
    source_mesh->node_get_coordinates(n, &coord);
    data[n] = coord[0]+coord[1]+coord[2];
  }
  Jali::StateVector<double> myvec("nodevars", source_mesh,
                                  Jali::Entity_kind::NODE,
                                  Jali::Entity_type::PARALLEL_OWNED,
                                  &(data[0]));
  source_state.add(myvec);

  // Create a mesh wrapper

  Wonton::Jali_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Jali_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // Gather the dual cell coordinates for source and target meshes for
  // intersection

  std::vector<std::vector<JaliGeometry::Point>>
      source_dualcell_coords(nnodes_source);
  std::vector<std::vector<JaliGeometry::Point>>
      target_dualcell_coords(nnodes_target);

  // Because the meshes are rectangular we can get away with examining
  // the coordinates of the corners instead of the wedges

  // Also, because we will use only the bounding box of each cell to
  // do the search and intersection we can get away with adding all
  // the coordinates of the corners to list including duplicates

  for (int n = 0; n < nnodes_source; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    source_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      source_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        source_dualcell_coords[n].push_back(coord);
    }
  }

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    target_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      target_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        target_dualcell_coords[n].push_back(coord);
    }
  }

  // Interpolate from source to target mesh

  std::vector<double> outvals(nnodes_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(nnodes_target);

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<int> xnodes;
    std::vector<std::vector<double>> xweights;

    BOX_INTERSECT::intersection_moments(target_dualcell_coords[n],
                                        source_dualcell_coords,
                                        &xnodes, &xweights);


    std::vector<Portage::Weights_t> wtsvec(xnodes.size());
    for (int i = 0; i < xnodes.size(); ++i) {
      wtsvec[i].entityID = xnodes[i];
      wtsvec[i].weights = xweights[i];
    }
    sources_and_weights[n] = wtsvec;
  }


  // Create Interpolation object

  Portage::Interpolate_2ndOrder<3, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
      interpolater(sourceMeshWrapper, targetMeshWrapper,
                   source_state);

  // Compute the target mesh vals

  interpolater.set_interpolation_variable("nodevars", Portage::NOLIMITER);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals.begin(), interpolater);

  // Make sure we retrieved the correct value for each node on the
  // target Second order interpolation should preserve a linear field
  // exactly but node-centered interpolation has some quirks - the field
  // does not get preserved exactly at the boundary because the source
  // values for boundary dual cells are not specified inside the dual
  // cells but at one of their vertices or edges. So, check only
  // interior nodes

  std::vector<double> stdvals(nnodes_target);
  for (int n = 0; n < nnodes_target; ++n) {
    JaliGeometry::Point coord;
    target_mesh->node_get_coordinates(n, &coord);
    if (fabs(coord[0]) < 1e-16 || fabs(1-coord[0]) < 1e-16 ||
        fabs(coord[1]) < 1e-16 || fabs(1-coord[1]) < 1.e-16 ||
        fabs(coord[2]) < 1e-16 || fabs(1-coord[2]) < 1.e-16)
      continue;
    stdvals[n] = coord[0]+coord[1]+coord[2];
    EXPECT_NEAR(stdvals[n], outvals[n], TOL);
  }
}


// Second order interpolation of discontinuous node-centered field with
// Barth-Jespersen limiting in 3D

TEST(Interpolate_2nd_Order, Node_Ctr_BJ_Limiter_3D) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  if (Jali::framework_available(Jali::MSTK))
    mf.framework(Jali::MSTK);
  mf.included_entities({Jali::Entity_kind::EDGE,
                        Jali::Entity_kind::FACE,
                        Jali::Entity_kind::WEDGE,
                        Jali::Entity_kind::CORNER});

  std::shared_ptr<Jali::Mesh> source_mesh = mf(0.0, 0.0, 1.0, 1.0, 4, 4);
  std::shared_ptr<Jali::Mesh> target_mesh = mf(0.0, 0.0, 1.0, 1.0, 5, 5);

  const int nnodes_source =
      source_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);
  const int nnodes_target =
      target_mesh->num_entities(Jali::Entity_kind::NODE,
                                Jali::Entity_type::PARALLEL_OWNED);

  Jali::Entity_ID_List const& targetnodes =
      target_mesh->nodes<Jali::Entity_type::ALL>();

  // Create a state object and add the first two vectors to it

  Jali::State source_state(source_mesh);

  // Define a state vector representing two piecewise linear functions,
  // where the right half has 100 times the value it would
  // have in the left linear function

  std::vector<double> data(nnodes_source);
  double minval = 1e+10, maxval = -1e+10;
  for (int n = 0; n < nnodes_source; ++n) {
    JaliGeometry::Point coord;
    source_mesh->node_get_coordinates(n, &coord);
    if (coord[0] >= 0.5)
      data[n] = 100*(coord[0]+coord[1]+coord[2]);
    else
      data[n] = coord[0]+coord[1]+coord[2];
    if (data[n] < minval) minval = data[n];
    if (data[n] > maxval) maxval = data[n];
  }
  Jali::StateVector<double> myvec("nodevars", source_mesh,
                                  Jali::Entity_kind::NODE,
                                  Jali::Entity_type::PARALLEL_OWNED,
                                  &(data[0]));
  source_state.add(myvec);

  // Create a mesh wrapper

  Wonton::Jali_Mesh_Wrapper sourceMeshWrapper(*source_mesh);
  Wonton::Jali_Mesh_Wrapper targetMeshWrapper(*target_mesh);

  // Gather the dual cell coordinates for source and target meshes for
  // intersection

  std::vector<std::vector<JaliGeometry::Point>>
                         source_dualcell_coords(nnodes_source);
  std::vector<std::vector<JaliGeometry::Point>>
                         target_dualcell_coords(nnodes_target);

  // Because the meshes are rectangular we can get away with examining
  // the coordinates of the corners instead of the wedges

  // Also, because we will use only the bounding box of each cell to
  // do the search and intersection we can get away with adding all
  // the coordinates of the corners to list including duplicates

  for (int n = 0; n < nnodes_source; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    source_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      source_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        source_dualcell_coords[n].push_back(coord);
    }
  }

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<JaliGeometry::Point> dualcoords;
    std::vector<int> corners;
    target_mesh->node_get_corners(n, Jali::Entity_type::ALL, &corners);

    for (auto cn : corners) {
      std::vector<JaliGeometry::Point> cncoords;
      target_mesh->corner_get_coordinates(cn, &cncoords);
      for (auto coord : cncoords)
        target_dualcell_coords[n].push_back(coord);
    }
  }

  // Interpolate from source to target mesh

  std::vector<double> outvals1(nnodes_target);
  std::vector<double> outvals2(nnodes_target);
  std::vector<std::vector<Portage::Weights_t>>
      sources_and_weights(nnodes_target);

  for (int n = 0; n < nnodes_target; ++n) {
    std::vector<int> xnodes;
    std::vector<std::vector<double>> xweights;

    BOX_INTERSECT::intersection_moments(target_dualcell_coords[n],
                                        source_dualcell_coords,
                                        &xnodes, &xweights);


    std::vector<Portage::Weights_t> wtsvec(xnodes.size());
    for (int i = 0; i < xnodes.size(); ++i) {
      wtsvec[i].entityID = xnodes[i];
      wtsvec[i].weights = xweights[i];
    }
    sources_and_weights[n] = wtsvec;
  }

  // Create Interpolation objects - one with no limiter and one with limiter

  Portage::Interpolate_2ndOrder<2, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
                         interpolater1(sourceMeshWrapper, targetMeshWrapper,
                                       source_state);

  Portage::Interpolate_2ndOrder<2, Portage::NODE,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_Mesh_Wrapper,
                                Wonton::Jali_State_Wrapper>
                         interpolater2(sourceMeshWrapper, targetMeshWrapper,
                                       source_state);

  // Compute the target mesh vals

  interpolater1.set_interpolation_variable("nodevars", Portage::NOLIMITER);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals1.begin(), interpolater1);

  // Compute the target mesh vals

  interpolater2.set_interpolation_variable("nodevars", Portage::BARTH_JESPERSEN);

  Portage::transform(targetnodes.begin(), targetnodes.end(),
                     sources_and_weights.begin(),
                     outvals2.begin(), interpolater2);

  // Check if we violated the bounds on at least one node in the
  // unlimited interpolate and if we respected the bounds on all nodes in
  // the limited case

  bool outofbounds_unlimited = false;
  for (int n = 0; n < nnodes_target; ++n) {
    if (outvals1[n] < minval  || outvals1[n] > maxval) {
      outofbounds_unlimited = true;
      break;
    }
  }

  // Check if we preserved bounds on interior nodes. Since we don't limit at
  // boundary nodes we have no guarantee their values will be within bounds
  bool inbounds_limited = true;
  for (int n = 0; n < nnodes_target; ++n) {
    if (!targetMeshWrapper.on_exterior_boundary(Portage::NODE, n)) {
      if (outvals2[n] < minval  || outvals2[n] > maxval) {
        inbounds_limited = false;
        break;
      }
    }
  }

  EXPECT_TRUE(outofbounds_unlimited);
  EXPECT_TRUE(inbounds_limited);
}

*/
