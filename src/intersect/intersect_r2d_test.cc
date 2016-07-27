#include "intersect_r2d.h"
#include "gtest/gtest.h"
#include "Mesh.hh"
#include "MeshFactory.hh"
#include "portage/wrappers/mesh/jali/jali_mesh_wrapper.h"

/*! 
 * @brief Intersect two cells on two single cell meshes to compute moments.
 * Intersect two cells contained in the mesh: 
 * (0, 0) (2, 0) (2, 2) (0,2) with (1,1) (2,1) (2,2) (1,2)
 * Results should be an area of 1 and a centroid of 1.5, 1.5.
 */
TEST(intersectR2D, simple1) {
  Jali::MeshFactory mf(MPI_COMM_WORLD);
  //Create mesh from 0, 0 to 2.4, 2 1x1
  std::shared_ptr<Jali::Mesh> sm = mf(0, 0, 2, 2, 1,1);
  std::shared_ptr<Jali::Mesh> tm = mf(1,1,2, 2, 1, 1);
  const Portage::Jali_Mesh_Wrapper s(*sm);
  const Portage::Jali_Mesh_Wrapper t(*tm);
  Portage::IntersectR2D<Portage::Jali_Mesh_Wrapper> isect{s , t};
  std::vector<std::vector<double> > moments = isect(0, 0); 
  for(int i=0;i<moments.size();i++){
    for(int j=0;j<moments[i].size();j++){
      std::cout << "i, j, m " << i << ", " << j << ", " << moments[i][j] << std::endl;
    }   
  }

  ASSERT_EQ(moments[0][0], 1);
  ASSERT_EQ(moments[0][1], 1.5);
  ASSERT_EQ(moments[0][2], 1.5);
}

#if 0
//@todo Figure out a way to convert this older test to an intersection test in the current framework
TEST(intersectClipper, convex){
  std::vector<JaliGeometry::Point> cellA, cellB;
  cellA.emplace_back(2,5);
  cellA.emplace_back(10.5,5);
  cellA.emplace_back(10.5, 3.5);
  cellA.emplace_back(2,3.5);

  cellB.emplace_back(2.5,0);
  cellB.emplace_back(8.5,0);
  cellB.emplace_back(8.5,5);
  cellB.emplace_back(5.5,2.5);
  cellB.emplace_back(2.5,4);

  IntersectClipper<Jali_Mesh_Wrapper> isect{Jali_Mesh_Wrapper, Jali_Mesh_Wrapper};
  std::vector<std::vector<double> > moments = isect(cellA, cellB); 
  for(int i=0;i<moments.size();i++){
    for(int j=0;j<moments[i].size();j++){
      std::cout << "i, j, m " << i << ", " << j << ", " << moments[i][j] << std::endl;
    }   
  }
  double eps = 1e-6;
  EXPECT_NEAR(moments[0][0], 1.35, eps);
  EXPECT_NEAR(moments[0][1], 10.665, eps);
  EXPECT_NEAR(moments[0][2], 5.4, eps);
  EXPECT_NEAR(moments[1][0], .25, eps);
  EXPECT_NEAR(moments[1][1], .708333, eps);
  EXPECT_NEAR(moments[1][2], .916667, eps);
}
#endif
