/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/

#include "portage/support/portage.h"
#include "portage/state/state_vector_uni.h"
#include "portage/state/state_vector_multi.h"
#include "portage/state/state_manager.h"

#include "portage/simple_mesh/simple_mesh.h"
#include "portage/wonton/mesh/simple_mesh/simple_mesh_wrapper.h"

#include "gtest/gtest.h"

using namespace Portage;

TEST(StateManager, test1){

	std::string name1{"field1"};
	std::vector<std::vector<double>> data1 {{1.,2.,3.},{4.,5.},{6.}};	
	double *temp1[3];
	for (int i=0; i<3; i++)temp1[i]=data1[i].data();
	StateVectorMulti<double> sv1(name1, temp1);

	double **sv1_data = sv1.get_data();
	for (int i=0; i<data1.size(); i++) {
		for (int j=0; j<data1[i].size(); j++){
			ASSERT_EQ(sv1_data[i][j],data1[i][j]);
		}
	}  


	std::string name2{"field2"};
	std::vector<std::vector<int>> data2 {{-1,-2,-3},{-4,-5}};	
	int *temp2[2];
	for (int i=0; i<2; i++)temp2[i]=data2[i].data();
	StateVectorMulti<int> sv2(name2, temp2);

	std::vector<StateVectorBase*> state {&sv1, &sv2};

	int **sv2_data = sv2.get_data();
	for (int i=0; i<data2.size(); i++) {
		for (int j=0; j<data2[i].size(); j++){
			ASSERT_EQ(sv2_data[i][j],data2[i][j]);
		}
	}  
	
	// now check through state and dynamic casting
	double **sv1_data_state = (dynamic_cast<StateVectorMulti<double>*>(state[0]))->get_data();
	for (int i=0; i<data1.size(); i++) {
		for (int j=0; j<data1[i].size(); j++){
			ASSERT_EQ(sv1_data_state[i][j],data1[i][j]);
		}
	}  

	int **sv2_data_state = (dynamic_cast<StateVectorMulti<int>*>(state[1]))->get_data();
	for (int i=0; i<data2.size(); i++) {
		for (int j=0; j<data2[i].size(); j++){
			ASSERT_EQ(sv2_data_state[i][j],data2[i][j]);
		}
	}  

	// now check through state and static
	double **sv1_data_state2 = (static_cast<StateVectorMulti<double>*>(state[0]))->get_data();
	for (int i=0; i<data1.size(); i++) {
		for (int j=0; j<data1[i].size(); j++){
			ASSERT_EQ(sv1_data_state2[i][j],data1[i][j]);
		}
	}  

	int **sv2_data_state2 = (static_cast<StateVectorMulti<int>*>(state[1]))->get_data();
	for (int i=0; i<data2.size(); i++) {
		for (int j=0; j<data2[i].size(); j++){
			ASSERT_EQ(sv2_data_state2[i][j],data2[i][j]);
		}
	}  

}

TEST(StateManager, manageMeshField1){

	using namespace Wonton;

	// create the mesh
	Simple_Mesh mesh{0., 0., 1., 1., 1, 1};
	
	// create the wrapper
	Simple_Mesh_Wrapper wrapper{mesh};
	
	// create the state manager
	StateManager<Simple_Mesh_Wrapper> manager{wrapper};
	
	// print the counts to make sure everything is working
	manager.print_counts();
	
	// create the data vector
	std::vector<double> data {1.};	

	// create a uni state vector
	StateVectorUni<> v("field", Field_type::MESH_FIELD, data.data());
	
	// make sure the data access is correct
	ASSERT_EQ(data[0], v.get_data()[0]);
	
	// add the data to the state manager
	//manager.add

	
}






















