/*
  This file is part of the Ristra portage project.
  Please see the license file at the root of this repository, or at:
  https://github.com/laristra/portage/blob/master/LICENSE
*/
#include <vector>
#include <memory>
#include <cstdlib>
#include <string>
#include <exception>

#include "gtest/gtest.h"

#include "portage/wonton/state/simple_state/simple_state_wrapper.h"
#include "portage/wonton/mesh/simple_mesh/simple_mesh_wrapper.h"

#include "portage/swarm/swarm.h"
#include "portage/swarm/swarm_state.h"

#include "portage/support/portage.h"

TEST(SwarmState, basic) {
  using std::make_shared;
  using Portage::Meshfree::SwarmState;

  const size_t npoints = 10;
  std::vector<Portage::Point<3>> points(npoints), extents(npoints);

  // set up swarm
  double h = 0.01;
  srand(time(NULL));
  for (int i = 0; i < npoints; i++) {
    points[i] = Portage::Point<3>(
        (static_cast<double>(rand()) / RAND_MAX),
        (static_cast<double>(rand()) / RAND_MAX),
        (static_cast<double>(rand()) / RAND_MAX));
  }
  auto p_ptr = std::make_shared<Portage::vector<Portage::Point<3>>>(points);
  auto swarm = Portage::Meshfree::Swarm<3>(p_ptr);

  // create state
  SwarmState<3> state(swarm);
  ASSERT_EQ(state.get_size(), npoints);

  // create state fields
  SwarmState<3>::DblVecPtr dbl_field1 =
      make_shared<SwarmState<3>::DblVec>(npoints, 0.);
  SwarmState<3>::DblVecPtr dbl_field2 =
      make_shared<SwarmState<3>::DblVec>(npoints, 0.);
  SwarmState<3>::DblVecPtr bad_dbl_field =
      make_shared<SwarmState<3>::DblVec>(npoints+5, 0.);
  SwarmState<3>::IntVecPtr int_field1 =
      make_shared<SwarmState<3>::IntVec>(npoints, 0.);
  SwarmState<3>::IntVecPtr int_field2 =
      make_shared<SwarmState<3>::IntVec>(npoints, 0.);
  SwarmState<3>::IntVecPtr bad_int_field =
      make_shared<SwarmState<3>::IntVec>(npoints+5, 0.);

  // fill in fields
  for (size_t i=0; i < npoints; i++) {
    (*dbl_field1)[i] = i+.1;
    (*dbl_field2)[i] = i+.01;
    (*int_field1)[i] = i+10;
    (*int_field2)[i] = i+100;
  }

  // add the fields to the state
  state.add_field("d1", dbl_field1);
  state.add_field("d2", dbl_field2);
  state.add_field("i1", int_field1);
  state.add_field("i2", int_field2);

  // check that fields are correct
  SwarmState<3>::DblVecPtr d1p, d2p;
  SwarmState<3>::IntVecPtr i1p, i2p;
  state.get_field("d1", d1p);
  state.get_field("d2", d2p);
  state.get_field("i1", i1p);
  state.get_field("i2", i2p);
  for (size_t i=0; i < npoints; i++) {
    ASSERT_EQ((*d1p)[i], (*dbl_field1)[i]);
    ASSERT_EQ((*d2p)[i], (*dbl_field2)[i]);
    ASSERT_EQ((*i1p)[i], (*int_field1)[i]);
    ASSERT_EQ((*i2p)[i], (*int_field2)[i]);
  }

  // check names lists are correct
  std::vector<std::string>
      dnames = state.field_names_double();
  std::vector<std::string>
      inames = state.field_names_int();
  ASSERT_EQ(dnames.size(), 2);
  ASSERT_EQ(dnames[0], "d1");
  ASSERT_EQ(dnames[1], "d2");
  ASSERT_EQ(inames.size(), 2);
  ASSERT_EQ(inames[0], "i1");
  ASSERT_EQ(inames[1], "i2");

  // check failure on adding field twice
  try {
    state.add_field("d1", dbl_field1);
    ASSERT_FALSE(true);
  } catch (std::exception err) {
    ASSERT_TRUE(true);
  }
  try {
    state.add_field("i1", int_field1);
    ASSERT_FALSE(true);
  } catch (std::exception err) {
    ASSERT_TRUE(true);
  }

  // check failure on adding bad fields
  try {
    state.add_field("bad", bad_dbl_field);
    ASSERT_FALSE(true);
  } catch (...) {
    ASSERT_TRUE(true);
  }
  try {
    state.add_field("bad", bad_int_field);
    ASSERT_FALSE(true);
  } catch (...) {
    ASSERT_TRUE(true);
  }

  // check creation by size alone
  SwarmState<3> state2(npoints);
  state2.add_field("d1", dbl_field1);
  SwarmState<3>::DblVecPtr d1p2;
  state2.get_field("d1", d1p2);
  ASSERT_EQ(d1p2->size(), npoints);
}


/*!
  @brief Unit test for constructor with Simple_State_Wrapper in 3D using cells
*/
TEST(SwarmState, Simple_State_Wrapper) {
  std::shared_ptr<Portage::Simple_Mesh> mesh_ptr = 
    std::make_shared<Portage::Simple_Mesh>(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 2, 2, 2);
  Portage::Simple_Mesh &mesh(*mesh_ptr);
  Wonton::Simple_Mesh_Wrapper mesh_wrapper(mesh);

  Portage::Simple_State sstate(mesh_ptr);
  int ncells = mesh_wrapper.num_owned_cells();
  int nnodes = mesh_wrapper.num_owned_nodes();
  std::vector<double> &cfield1 = *new std::vector<double>(ncells);
  std::vector<double> &nfield1 = *new std::vector<double>(nnodes);
  for (int i=0; i < ncells; i++) {
    cfield1[i] = 1.;
  }
  for (int i=0; i < nnodes; i++) {
    nfield1[i] = 2.;
  }
  sstate.add("nf1", Portage::NODE, &nfield1[0]);
  sstate.add("cf1", Portage::CELL, &cfield1[0]);
  Wonton::Simple_State_Wrapper state_wrapper(sstate);

  // create swarm state from mesh state wrapper for cells
  {
    std::shared_ptr<Portage::Meshfree::SwarmState<3>> state_ptr = 
      Portage::Meshfree::SwarmStateFactory<3,Wonton::Simple_State_Wrapper>
      (state_wrapper, Portage::CELL);
    Portage::Meshfree::SwarmState<3> &state(*state_ptr);

    // test size
    ASSERT_EQ(state.get_size(), ncells);

    // check data fields
    std::vector<std::string> intnames = state.field_names_int();
    ASSERT_EQ(intnames.size(), 0);
    std::vector<std::string> dblnames = state.field_names_double();
    ASSERT_EQ(dblnames.size(), 1);
    ASSERT_EQ(dblnames[0], "cf1");
    Portage::Meshfree::SwarmState<3>::DblVecPtr field;
    state.get_field("cf1", field);
    for (int i=0; i < ncells; i++) {
      ASSERT_EQ((*field)[i], 1.0);
    }
  }

  // create swarm state from mesh state wrapper for nodes
  {  
    std::shared_ptr<Portage::Meshfree::SwarmState<3>> state_ptr = 
       Portage::Meshfree::SwarmStateFactory<3,Wonton::Simple_State_Wrapper>
      (state_wrapper, Portage::NODE);
    Portage::Meshfree::SwarmState<3> &state(*state_ptr);

    // test size
    ASSERT_EQ(state.get_size(), nnodes);

    // check data fields
    std::vector<std::string> intnames = state.field_names_int();
    ASSERT_EQ(intnames.size(), 0);
    auto dblnames = state.field_names_double();
    ASSERT_EQ(dblnames.size(), 1);
    ASSERT_EQ(dblnames[0], "nf1");
    Portage::Meshfree::SwarmState<3>::DblVecPtr field;
    state.get_field("nf1", field);
    for (int i=0; i < nnodes; i++) {
      ASSERT_EQ((*field)[i], 2.0);
    }
  }
}

