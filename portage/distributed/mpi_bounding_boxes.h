/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/

#ifndef MPI_BOUNDING_BOXES_H_
#define MPI_BOUNDING_BOXES_H_

//#define DEBUG_MPI

#include <cassert>
#include <algorithm>
#include <numeric>
#include <memory>
#include <unordered_map>

#include "portage/support/portage.h"
#include "wonton/state/state_vector_uni.h"

#include "mpi.h"

/*!
  @file mpi_bounding_boxes.h
  @brief Distributes source data using MPI based on bounding boxes
 */

namespace Portage {


/*!
  @class MPI_Bounding_Boxes
  @brief Distributes source data using MPI based on bounding boxes

         Currently assumes coordinates and all fields are doubles.
*/
class MPI_Bounding_Boxes {
 public:

  /*!
    @brief Constructor of MPI_Bounding_Boxes
   */
  MPI_Bounding_Boxes() {};


  /*!
    @brief Destructor of MPI_Bounding_Boxes
   */
  ~MPI_Bounding_Boxes() {};


  /*!
    @brief Helper structure containg comms info for a given entity type
   */
  struct comm_info_t {
    //!< Number of total/owned entities in source field
    int sourceNum = 0, sourceNumOwned = 0;
    //!< Number of total/owned entities in new field
    int newNum = 0, newNumOwned = 0;
    //! Array of total/owned send sizes from me to all PEs
    std::vector<int> sendCounts, sendOwnedCounts;
    //! Array of total/owned recv sizes to me from all PEs
    std::vector<int> recvCounts, recvOwnedCounts;
  };


  /*!
    @brief Compute bounding boxes for all partitions, and send source mesh and state
           information to all target partitions with an overlapping bounding box using MPI
    @param[in] source_mesh_flat  Input mesh (must be flat representation)
    @param[in] source_state_flat Input state (must be flat representation)
    @param[in] target_mesh       Target mesh
    @param[in] target_state      Target state (not actually used for now)
   */
  template <class Source_Mesh, class Source_State, class Target_Mesh, class Target_State>
  void distribute(Source_Mesh &source_mesh_flat, Source_State &source_state_flat,
                  Target_Mesh &target_mesh, Target_State &target_state)
  {
    // Get the MPI communicator size and rank information
    int commSize, commRank;
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &commRank);

    int dim = source_mesh_flat.space_dimension();
    assert(dim == target_mesh.space_dimension());

    // Compute the bounding box for the target mesh on this rank, and put it in an
    // array that will later store the target bounding box for each rank
    int targetNumOwnedCells = target_mesh.num_owned_cells();
    std::vector<double> targetBoundingBoxes(2*dim*commSize);
    for (unsigned int i=0; i<2*dim; i+=2)
    {
      targetBoundingBoxes[2*dim*commRank+i+0] = std::numeric_limits<double>::max();
      targetBoundingBoxes[2*dim*commRank+i+1] = -std::numeric_limits<double>::max();
    }
    for (unsigned int c=0; c<targetNumOwnedCells; ++c)
    {
      std::vector<int> nodes;
      target_mesh.cell_get_nodes(c, &nodes);
      int cellNumNodes = nodes.size();
      for (unsigned int j=0; j<cellNumNodes; ++j)
      {
        int n = nodes[j];
        // ugly hack, since dim is not known at compile time
        if (dim == 3)
        {
          Point<3> nodeCoord;
          target_mesh.node_get_coordinates(n, &nodeCoord);
          for (unsigned int k=0; k<dim; ++k)
          {
            if (nodeCoord[k] < targetBoundingBoxes[2*dim*commRank+2*k])
              targetBoundingBoxes[2*dim*commRank+2*k] = nodeCoord[k];
            if (nodeCoord[k] > targetBoundingBoxes[2*dim*commRank+2*k+1])
              targetBoundingBoxes[2*dim*commRank+2*k+1] = nodeCoord[k];
          }
        }
        else if (dim == 2)
        {
          Point<2> nodeCoord;
          target_mesh.node_get_coordinates(n, &nodeCoord);
          for (unsigned int k=0; k<dim; ++k)
          {
            if (nodeCoord[k] < targetBoundingBoxes[2*dim*commRank+2*k])
              targetBoundingBoxes[2*dim*commRank+2*k] = nodeCoord[k];
            if (nodeCoord[k] > targetBoundingBoxes[2*dim*commRank+2*k+1])
              targetBoundingBoxes[2*dim*commRank+2*k+1] = nodeCoord[k];
          }
        }
      } // for j
    } // for c

    // Get the source mesh properties and cordinates
    int sourceNumOwnedCells = source_mesh_flat.num_owned_cells();
    int sourceNumCells = source_mesh_flat.num_owned_cells() + source_mesh_flat.num_ghost_cells();
    int sourceNumOwnedNodes = source_mesh_flat.num_owned_nodes();
    int sourceNumNodes = source_mesh_flat.num_owned_nodes() + source_mesh_flat.num_ghost_nodes();
    // only used in 3D:
    int sourceNumOwnedFaces = -1;
    int sourceNumFaces = -1;
    if (dim == 3)
    {
      sourceNumOwnedFaces = source_mesh_flat.num_owned_faces();
      sourceNumFaces = source_mesh_flat.num_owned_faces() + source_mesh_flat.num_ghost_faces();
    }
    std::vector<double>& sourceCoords = source_mesh_flat.get_coords();
    std::vector<int>& sourceNodeCounts = source_mesh_flat.get_cell_node_counts();
    std::vector<int>& sourceCellGlobalIds = source_mesh_flat.get_global_cell_ids();
    std::vector<int>& sourceNodeGlobalIds = source_mesh_flat.get_global_node_ids();

    // Compute the bounding box for the source mesh on this rank
    std::vector<double> sourceBoundingBox(2*dim);
    for (unsigned int i=0; i<2*dim; i+=2)
    {
      sourceBoundingBox[i+0] = std::numeric_limits<double>::max();
      sourceBoundingBox[i+1] = -std::numeric_limits<double>::max();
    }
    for (unsigned int c=0; c<sourceNumOwnedCells; ++c)
    {
      std::vector<int> nodes;
      source_mesh_flat.cell_get_nodes(c, &nodes);
      int cellNumNodes = nodes.size();
      for (unsigned int j=0; j<cellNumNodes; ++j)
      {
        int n = nodes[j];
        for (unsigned int k=0; k<dim; ++k)
        {
          if (sourceCoords[n*dim+k] < sourceBoundingBox[2*k])
            sourceBoundingBox[2*k] = sourceCoords[n*dim+k];
          if (sourceCoords[n*dim+k] > sourceBoundingBox[2*k+1])
            sourceBoundingBox[2*k+1] = sourceCoords[n*dim+k];
        }
      } // for j
    } // for c

    // Broadcast the target bounding boxes so that each rank knows the bounding boxes for all ranks
    for (unsigned int i=0; i<commSize; i++)
      MPI_Bcast(&(targetBoundingBoxes[0])+i*2*dim, 2*dim, MPI_DOUBLE, i, MPI_COMM_WORLD);

#ifdef DEBUG_MPI
    std::cout << "Target boxes: ";
    for (unsigned int i=0; i<2*dim*commSize; i++) std::cout << targetBoundingBoxes[i] << (i%(2*dim)==2*dim-1?", ":" ");
    std::cout << std::endl;

    std::cout << "Source box " << commRank << ": ";
    for (unsigned int i=0; i<2*dim; i++) std::cout << sourceBoundingBox[i] << " ";
    std::cout << std::endl;
#endif

    // Offset the source bounding boxes by a fudge factor so that we don't send source data to a rank
    // with a target bounding box that is incident to but not overlapping the source bounding box
    const double boxOffset = 2.0*std::numeric_limits<double>::epsilon();
    double min2[dim], max2[dim];
    for (unsigned int k=0; k<dim; ++k)
    {
      min2[k] = sourceBoundingBox[2*k]+boxOffset;
      max2[k] = sourceBoundingBox[2*k+1]-boxOffset;
    }

    // For each target rank with a bounding box that overlaps the bounding box for this rank's partition
    // of the source mesh, we will send it all our source cells; otherwise, we will send it nothing
    std::vector<bool> sendFlags(commSize);
    for (unsigned int i=0; i<commSize; ++i)
    {
      double min1[dim], max1[dim];
      bool sendThis = true;
      for (unsigned int k=0; k<dim; ++k)
      {
        min1[k] = targetBoundingBoxes[2*dim*i+2*k]+boxOffset;
        max1[k] = targetBoundingBoxes[2*dim*i+2*k+1]-boxOffset;
        sendThis = sendThis &&
            ((min1[k] <= min2[k] && min2[k] <= max1[k]) ||
             (min2[k] <= min1[k] && min1[k] <= max2[k]));
      }

      sendFlags[i] = sendThis;
    }

    comm_info_t cellInfo, nodeInfo;
    // only used in 2D:
    comm_info_t cellToNodeInfo;
    // only used in 3D:
    comm_info_t faceInfo, cellToFaceInfo, faceToNodeInfo;

    setInfo(&cellInfo, commSize, sendFlags,
            sourceNumCells, sourceNumOwnedCells);

#ifdef DEBUG_MPI
    std::cout << "Received  on rank " << commRank << ", from source rank 0:" << cellInfo.recvCounts[0];
    std::cout << ", from source rank 1:" << cellInfo.recvCounts[1];
    std::cout << ", from source rank 2:" << cellInfo.recvCounts[2];
    std::cout << ",  totaling:" << cellInfo.newNum
              << " ,of which " << (cellInfo.newNum - cellInfo.recvCounts[commRank]) << " were received from different ranks" << std::endl;
#endif

    setInfo(&nodeInfo, commSize, sendFlags,
            sourceNumNodes, sourceNumOwnedNodes);

    if (dim == 2)
    {
      std::vector<int>& sourceCellToNodeList = source_mesh_flat.get_cell_to_node_list();
      std::vector<int>& sourceCellNodeOffsets = source_mesh_flat.get_cell_node_offsets();
      int sizeCellToNodeList = sourceCellToNodeList.size();
      int sizeOwnedCellToNodeList = (
          sourceNumCells == sourceNumOwnedCells ? sizeCellToNodeList :
          sourceCellNodeOffsets[sourceNumOwnedCells]);

      setInfo(&cellToNodeInfo, commSize, sendFlags,
              sizeCellToNodeList, sizeOwnedCellToNodeList);
    }

    if (dim == 3)
    {
      setInfo(&faceInfo, commSize, sendFlags,
              sourceNumFaces, sourceNumOwnedFaces);

      std::vector<int>& sourceCellToFaceList = source_mesh_flat.get_cell_to_face_list();
      std::vector<int>& sourceCellFaceOffsets = source_mesh_flat.get_cell_face_offsets();
      int sizeCellToFaceList = sourceCellToFaceList.size();
      int sizeOwnedCellToFaceList = (
          sourceNumCells == sourceNumOwnedCells ? sizeCellToFaceList :
          sourceCellFaceOffsets[sourceNumOwnedCells]);

      setInfo(&cellToFaceInfo, commSize, sendFlags,
              sizeCellToFaceList, sizeOwnedCellToFaceList);

      std::vector<int>& sourceFaceToNodeList = source_mesh_flat.get_face_to_node_list();
      std::vector<int>& sourceFaceNodeOffsets = source_mesh_flat.get_face_node_offsets();
      int sizeFaceToNodeList = sourceFaceToNodeList.size();
      int sizeOwnedFaceToNodeList = (
          sourceNumFaces == sourceNumOwnedFaces ? sizeFaceToNodeList :
          sourceFaceNodeOffsets[sourceNumOwnedFaces]);

      setInfo(&faceToNodeInfo, commSize, sendFlags,
              sizeFaceToNodeList, sizeOwnedFaceToNodeList);
    }

    // Data structures to hold mesh data received from other ranks
    std::vector<double> newCoords(dim*nodeInfo.newNum);
    std::vector<int> newCellNodeCounts;
    std::vector<int> newCellToNodeList;
    std::vector<int> newCellFaceCounts;
    std::vector<int> newCellToFaceList;
    std::vector<bool> newCellToFaceDirs;
    std::vector<int> newFaceNodeCounts;
    std::vector<int> newFaceToNodeList;
    if (dim == 2)
    {
      newCellNodeCounts.resize(cellInfo.newNum);
      newCellToNodeList.resize(cellToNodeInfo.newNum);
    }
    if (dim == 3)
    {
      newCellFaceCounts.resize(cellInfo.newNum);
      newCellToFaceList.resize(cellToFaceInfo.newNum);
      newCellToFaceDirs.resize(cellToFaceInfo.newNum);
      newFaceNodeCounts.resize(faceInfo.newNum);
      newFaceToNodeList.resize(faceToNodeInfo.newNum);
    }
    std::vector<int> newCellGlobalIds(cellInfo.newNum);
    std::vector<int> newNodeGlobalIds(nodeInfo.newNum);

    // SEND NODE COORDINATES
#ifdef DEBUG_MPI
                std::cout << std::endl << "Source node coordinates (from all received ranks):" << std::endl;
#endif
    moveField(nodeInfo, commRank, commSize, MPI_DOUBLE, dim,
              sourceCoords, &newCoords);
    if (dim == 2)
    {
      // SEND NUMBER OF NODES FOR EACH CELL

#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source nodes per cell:" << std::endl;
#endif
      moveField(cellInfo, commRank, commSize, MPI_INT, 1,
                sourceNodeCounts, &newCellNodeCounts);

      // SEND CELL-TO-NODE MAP
      std::vector<int>& sourceCellToNodeList = source_mesh_flat.get_cell_to_node_list();
#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source cell nodes:" << std::endl;
#endif
      moveField(cellToNodeInfo, commRank, commSize, MPI_INT, 1,
                sourceCellToNodeList, &newCellToNodeList);

    }

    if (dim == 3)
    {
      // SEND NUMBER OF FACES FOR EACH CELL
      std::vector<int>& sourceCellFaceCounts = source_mesh_flat.get_cell_face_counts();
#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source faces per cell:" << std::endl;
#endif
      moveField(cellInfo, commRank, commSize, MPI_INT, 1,
                sourceCellFaceCounts, &newCellFaceCounts);

      // SEND CELL-TO-FACE MAP
      // For this array only, pack up face IDs + dirs and send together
      std::vector<int>& sourceCellToFaceList = source_mesh_flat.get_cell_to_face_list();
      std::vector<bool>& sourceCellToFaceDirs = source_mesh_flat.get_cell_to_face_dirs();
      int size = sourceCellToFaceList.size();
      for (unsigned int j=0; j<size; ++j)
      {
        int f = sourceCellToFaceList[j];
        int dir = static_cast<int>(sourceCellToFaceDirs[j]);
        sourceCellToFaceList[j] = (f << 1) | dir;
      }
#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source cell faces:" << std::endl;
#endif
      moveField(cellToFaceInfo, commRank, commSize, MPI_INT, 1,
                sourceCellToFaceList, &newCellToFaceList);

      // Unpack face IDs and dirs
      for (unsigned int j=0; j<newCellToFaceList.size(); ++j)
      {
        int fd = newCellToFaceList[j];
        newCellToFaceList[j] = fd >> 1;
        newCellToFaceDirs[j] = fd & 1;
      }

      // SEND NUMBER OF NODES FOR EACH FACE
      std::vector<int>& sourceFaceNodeCounts = source_mesh_flat.get_face_node_counts();
#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source nodes per face:" << std::endl;
#endif
      moveField(faceInfo, commRank, commSize, MPI_INT, 1,
                sourceFaceNodeCounts, &newFaceNodeCounts);

      // SEND FACE-TO-NODE MAP
      std::vector<int>& sourceFaceToNodeList = source_mesh_flat.get_face_to_node_list();
#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source face nodes:" << std::endl;
#endif
      moveField(faceToNodeInfo, commRank, commSize, MPI_INT, 1,
                sourceFaceToNodeList, &newFaceToNodeList);
    }

    // SEND GLOBAL CELL IDS

#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source global cell ids:" << std::endl;
#endif
    moveField(cellInfo, commRank, commSize, MPI_INT, 1,
              sourceCellGlobalIds, &newCellGlobalIds);

    // SEND GLOBAL NODE IDS

#ifdef DEBUG_MPI
                        std::cout << std::endl << "Source global node ids:" << std::endl;
#endif
    moveField(nodeInfo, commRank, commSize, MPI_INT, 1,
              sourceNodeGlobalIds, &newNodeGlobalIds);

    // SEND FIELD VALUES
    
    // is this a multimaterial problem? the following is error prone. Just because
    // we have more than one material, it may not be. Also, a node can have just
    // one material and this will fail, but for the moment, it will work. I just realized
    // that num_materials is ambiguous. It could refer to the number of materials 
    // registered with the state manager (as in a global registry), but more likely it is the number
    // of materials on this node
    int nmats = source_state_flat.num_materials();
    comm_info_t num_mats_info, num_mat_cells_info;
    std::vector<int> all_material_ids, all_material_shapes, all_material_cells;

    if (nmats>0){
    
      std::cout << "in distribute, this a multimaterial problem with " << nmats << " materials\n";
      
      /////////////////////////////////////////////////////////
      // get the material ids across all nodes
      /////////////////////////////////////////////////////////
      
      // set the info for the number of materials on each node
      setInfo(&num_mats_info, commSize, sendFlags, nmats, nmats);
      
      // get the sorted material ids on this node
      std::vector<int> material_ids=source_state_flat.get_material_ids();
      
      // resize the post distribute material id vector
      all_material_ids.resize(num_mats_info.newNum);

      // sendData
      moveData(commRank, commSize, MPI_INT, 1, 0, num_mats_info.sourceNum, 0,
        num_mats_info.sendCounts, num_mats_info.recvCounts,
        material_ids, &all_material_ids
      );
      
      /////////////////////////////////////////////////////////
      // get the material cell shapes across all nodes
      /////////////////////////////////////////////////////////      

      // get the sorted material shapes on this node
      std::vector<int> material_shapes=source_state_flat.get_material_shapes();
      
      // resize the post distribute material id vector
      all_material_shapes.resize(num_mats_info.newNum);

      // sendData
      moveData(commRank, commSize, MPI_INT, 1, 0, num_mats_info.sourceNum, 0,
        num_mats_info.sendCounts, num_mats_info.recvCounts,
        material_shapes, &all_material_shapes
      );
     
      /////////////////////////////////////////////////////////
      // get the material cell ids across all nodes
      /////////////////////////////////////////////////////////
      
      // get the total number of material cell id's on this node
      int nmatcells = source_state_flat.num_material_cells();
      
      // set the info for the number of materials on each node
      setInfo(&num_mat_cells_info, commSize, sendFlags, nmatcells, nmatcells);
      
      // get the sorted material ids on this node
      std::vector<int> material_cells=source_state_flat.get_material_cells();
      
      // resize the post distribute material id vector
      all_material_cells.resize(num_mat_cells_info.newNum);

      // sendData
      moveData(commRank, commSize, MPI_INT, 1, 0, num_mat_cells_info.sourceNum, 0,
        num_mat_cells_info.sendCounts, num_mat_cells_info.recvCounts,
        material_cells, &all_material_cells
      );
      
		  /////////////////////////////////////////////////////////
      // compute the cell map from local id to global id back to
      // first appearance of the local id. This code was copied
      // verbatim from flat_mesh_wrapper.h
      /////////////////////////////////////////////////////////

		  // Global to local maps for cells and nodes
		  std::map<int, int> globalCellMap;
		  std::vector<int> cellUniqueRep(newCellGlobalIds.size());
		  for (unsigned int i=0; i<newCellGlobalIds.size(); ++i) {
		    auto itr = globalCellMap.find(newCellGlobalIds[i]);
		    if (itr == globalCellMap.end()) {
		      globalCellMap[newCellGlobalIds[i]] = i;
		      cellUniqueRep[i] = i;
		    }
		    else {
		      cellUniqueRep[i] = itr->second;
		    }
		  }
		  
		  /////////////////////////////////////////////////////////
      // Fix the material cell indices to account for the fact
      // that they are local indices on the different nodes and 
      // need to be consistent within this flat mesh. FixListIndices
      // below is hard coded to work with ghost cells which we don't have
      // so I'm going to do this inline.
      /////////////////////////////////////////////////////////
      
      // get the local cell index offsets
      std::vector<int> offsets(commSize);
      offsets[0]=0;
 			std::partial_sum(cellInfo.recvCounts.begin(),cellInfo.recvCounts.end()-1,
      	offsets.begin()+1);     
      	
    	// make life easy by keeping a running counter
    	int running_counter=0;
      	
      // loop over the ranks
      for (int rank=0; rank<commSize; ++rank){
      	
      	// get the cell offset for this rank
      	int this_offset = offsets[rank];
      	
      	// get the number of material cells for this rank
      	int nmat_cells = num_mat_cells_info.recvCounts[rank];
      	
      	// loop over material cells on this rank
      	for (int i=0; i<nmat_cells; ++i){
      		
      		// offset the indices in all_material_cells
      		all_material_cells[running_counter] += this_offset;
      		
      		// use cellUniqueRep to map each cell down to its first appearance
      		all_material_cells[running_counter++] = cellUniqueRep[all_material_cells[running_counter]];
      	}	
      }
      
		  /////////////////////////////////////////////////////////
      // We need to turn the flattened material cells into a correctly shaped
      // ragged right structure for use as the material cells in the flat
      // state wrapper. Just as in the flat state mesh field (and associated
      // cell ids), we aren't removing duplicates, just concatnating by material
      /////////////////////////////////////////////////////////
      
      // allocate the material indices
      std::unordered_map<int,std::vector<int>> material_indices;
      
      // reset the running counter
      running_counter=0;
      
      // loop over material ids on different nodes
      for (int i=0; i<all_material_ids.size(); ++i){
      
      	// get the current working material
      	int mat_id = all_material_ids[i];
      	
      	// get the current number of material cells for this material
      	int nmat_cells = all_material_shapes[i];
      	
      	// get or create a reference to the correct material cell vector
      	std::vector<int>& these_material_cells = material_indices[mat_id];
      	
      	// loop over the correct number of material cells
      	for (int j=0; j<nmat_cells; ++j){
      		these_material_cells.push_back(all_material_cells[running_counter++]);
      	}
      	
      }
      
      // We are reusing the material cells and cell materials. Since we are using
      // maps and sets we want to make sure that we are starting clean and not
      // adding to cruft that is already there.
      source_state_flat.clear_material_cells();
      
      // add the material indices by keys
      for ( auto& kv: material_indices){
      	source_state_flat.mat_add_cells(kv.first, kv.second);
      }
      
    }

    // Send and receive each field to be remapped
    for (std::string field_name : source_state_flat.names())
    {

      // this is a serialized version of the field and is not a pointer to the
      // original field
      std::vector<double> sourceField = source_state_flat.serialize(field_name);
      
      // get the field stride
      int sourceFieldStride = source_state_flat.get_field_stride(field_name);

      // Currently only cell and node fields are supported
      comm_info_t info;
      if (source_state_flat.get_entity(field_name) == Entity_kind::NODE){
      	// node mesh field
      	info = nodeInfo;
      } else if (source_state_flat.field_type(Entity_kind::CELL, field_name) == Wonton::Field_type::MESH_FIELD){
      	// mesh cell field
     		info = cellInfo;
     	} else {
     		// multi material field
     		info = num_mat_cells_info;
     	}
                           
      // allocate storage for the new distribute data, note that this data
      // is still serialized and raw doubles and will need to be deserialized
      std::vector<double> newField(sourceFieldStride*info.newNum);

#ifdef DEBUG_MPI
      std::cout << std::endl << "Source user field \"" << field_name << "\":" << std::endl;
#endif

			// move the field
      moveField(info, commRank, commSize,
                MPI_DOUBLE, sourceFieldStride,
                sourceField, &newField);
      
      // deserialize the field
      source_state_flat.deserialize(field_name, newField, all_material_ids, all_material_shapes);
           
      // put the received field back into the state vector
      
      // get a pointer to the actual data in the state vector
      //std::shared_ptr<Wonton::StateVectorUni<double>> pv =
      //	std::dynamic_pointer_cast<Wonton::StateVectorUni<double>>(source_state_flat.get(field_name));
      
      // get the actual data vector in the state manager by reference
      //std::vector<double>& data = pv->get_data();
      
      // resize the state vector data to hold the new number
      // be careful when we get to MM data, the counts and size are not the same
      // when the stride!=1
      //data.resize(info.newNum);
      
      // deserialize the data
      //auto new_data = source_state_flat.deserialize(field_name, newField);
      //std::copy(new_data.begin(), new_data.end(), data.begin());

      // We will now use the received source state as our new source state on this partition
      //sourceField.resize(newField.size());
      //std::copy(newField.begin(), newField.end(), sourceField.begin());

    } // for field_name

#ifdef DEBUG_MPI
    for (std::string field_name : source_state_flat.names())
    {
      std::vector<double> sourceField = source_state_flat.serialize(field_name);
      std::cout  << "****\nJust distributed source user field \"" << field_name << "\" (rank " << commRank <<"):" << std::endl;
    	for (double x: sourceField){
    	  std::cout << x << " ";
    	}
    	std::cout << "\n***\n" <<std::endl;
    } // diagnostic print
#endif

    // We will now use the received source mesh data as our new source mesh on this partition
    // TODO:  replace with swap
    sourceCoords = newCoords;
    sourceCellGlobalIds = newCellGlobalIds;
    source_mesh_flat.set_node_global_ids(newNodeGlobalIds);
    source_mesh_flat.set_num_owned_cells(cellInfo.newNumOwned);
    source_mesh_flat.set_num_owned_nodes(nodeInfo.newNumOwned);

    if (dim == 2)
    {
      source_mesh_flat.set_cell_node_counts(newCellNodeCounts);
      fixListIndices(cellToNodeInfo, nodeInfo, commSize, &newCellToNodeList);
      source_mesh_flat.set_cell_to_node_list(newCellToNodeList);
    }

    if (dim == 3)
    {
      source_mesh_flat.set_cell_face_counts(newCellFaceCounts);
      source_mesh_flat.set_face_node_counts(newFaceNodeCounts);
      source_mesh_flat.set_num_owned_faces(faceInfo.newNumOwned);

      fixListIndices(cellToFaceInfo, faceInfo, commSize, &newCellToFaceList);
      source_mesh_flat.set_cell_to_face_list(newCellToFaceList);
      source_mesh_flat.set_cell_to_face_dirs(newCellToFaceDirs);

      fixListIndices(faceToNodeInfo, nodeInfo, commSize, &newFaceToNodeList);
      source_mesh_flat.set_face_to_node_list(newFaceToNodeList);
    }

    // Finish initialization using redistributed data
    source_mesh_flat.finish_init();

#ifdef DEBUG_MPI
    std::cout << "Sizes on rank " << commRank << ",  newNum: " << cellInfo.newNum << ", targetNumOwned: " << targetNumOwnedCells
              << ", sourceNum: " << cellInfo.sourceNum << ", sourceCoords: " << sourceCoords.size() << std::endl;

    for (unsigned int i=0; i<sourceCellGlobalIds.size(); i++)
      std::cout << sourceCellGlobalIds[i] << " ";
    std::cout << std::endl;
    for (unsigned int i=0; i<sourceCoords.size(); i++)
    {
      std::cout << sourceCoords[i] << " " ;
      if (i % 24 == 23) std::cout << std::endl;
    }
    std::cout << std::endl << std::endl;
#endif

#ifdef DEBUG_MPI
    for (std::string field_name : source_state_flat.names())
    {
      std::vector<double> sourceField = source_state_flat.serialize(field_name);
      std::cout  << "****\nThe very end of distribute() source user field \"" << field_name << "\" (rank " << commRank <<"):" << std::endl;
    	for (double x: sourceField){
    	  std::cout << x << " ";
    	}
    	std::cout << "\n***\n" <<std::endl;
    } // diagnostic print
#endif
  } // distribute

 private:

  /*!
    @brief Compute fields needed to do comms for a given entity type
    @param[in] info              Info data structure to be filled
    @param[in] commSize          Total number of MPI ranks
    @param[in] sendFlags         Array of flags:  do I send to PE n?
    @param[in] sourceNum         Number of entities (total) on this rank
    @param[in] sourceNumOwned    Number of owned entities on this rank
   */
  void setInfo(comm_info_t* info,
               const int commSize,
               const std::vector<bool>& sendFlags,
               const int sourceNum,
               const int sourceNumOwned)
  {
    // Set my counts of all entities and owned entities
    info->sourceNum = sourceNum;
    info->sourceNumOwned = sourceNumOwned;

    // Each rank will tell each other rank how many indexes it is going to send it
    info->sendCounts.resize(commSize);
    info->recvCounts.resize(commSize);
    for (unsigned int i=0; i<commSize; i++)
      info->sendCounts[i] = sendFlags[i] ? info->sourceNum : 0;
    MPI_Alltoall(&(info->sendCounts[0]), 1, MPI_INT,
                 &(info->recvCounts[0]), 1, MPI_INT, MPI_COMM_WORLD);

    // Each rank will tell each other rank how many owned indexes it is going to send it
    info->sendOwnedCounts.resize(commSize);
    info->recvOwnedCounts.resize(commSize);
    for (unsigned int i=0; i<commSize; i++)
      info->sendOwnedCounts[i] = sendFlags[i] ? info->sourceNumOwned : 0;
    MPI_Alltoall(&(info->sendOwnedCounts[0]), 1, MPI_INT,
                 &(info->recvOwnedCounts[0]), 1, MPI_INT, MPI_COMM_WORLD);

    // Compute the total number of indexes this rank will receive from all ranks
    for (unsigned int i=0; i<commSize; i++)
      info->newNum += info->recvCounts[i];
    for (unsigned int i=0; i<commSize; i++)
      info->newNumOwned += info->recvOwnedCounts[i];
  } // setInfo


  /*!
    @brief Move values for a single data field to all ranks as needed
    @tparam[in] T                C++ type of data to be moved
    @param[in] info              Info struct for entity type of field
    @param[in] commRank          MPI rank of this PE
    @param[in] commSize          Total number of MPI ranks
    @param[in] mpiType           MPI type of data (MPI_???) to be moved
    @param[in] stride            Stride of data field
    @param[in] sourceData        Array of (old) source data
    @param[in] newData           Array of new source data
   */
  template<typename T>
  void moveField(const comm_info_t& info,
                 const int commRank, const int commSize,
                 const MPI_Datatype mpiType, const int stride,
                 const std::vector<T>& sourceData,
                 std::vector<T>* newData)
  {
    // Perform two rounds of sends: the first for owned cells, and the second for ghost cells
    std::vector<int> recvGhostCounts(commSize);
    std::vector<int> sendGhostCounts(commSize);

    for (unsigned int i=0; i<commSize; i++)
    {
      recvGhostCounts[i] = info.recvCounts[i] - info.recvOwnedCounts[i];
      sendGhostCounts[i] = info.sendCounts[i] - info.sendOwnedCounts[i];
    }

    moveData(commRank, commSize, mpiType, stride,
             0, info.sourceNumOwned,
             0,
             info.sendOwnedCounts, info.recvOwnedCounts,
             sourceData, newData);
    moveData(commRank, commSize, mpiType, stride,
             info.sourceNumOwned, info.sourceNum,
             info.newNumOwned,
             sendGhostCounts, recvGhostCounts,
             sourceData, newData);

#ifdef DEBUG_MPI
    std::cout << "Number of values on rank " << commRank << ": " << (*newData).size() << std::endl;
    for (unsigned int f=0; f<(*newData).size(); f++)
      std::cout << (*newData)[f] << " ";
    std::cout << std::endl << std::endl;
#endif
  } // moveField


  /*!
    @brief Move values for a single range of data to all ranks as needed
    @tparam[in] T                C++ type of data to be moved
    @param[in] commRank          MPI rank of this PE
    @param[in] commSize          Total number of MPI ranks
    @param[in] mpiType           MPI type of data (MPI_???) to be moved
    @param[in] stride            Stride of data field
    @param[in] sourceStart       Start location in source (send) data
    @param[in] sourceEnd         End location in source (send) data
    @param[in] newStart          Start location in new (recv) data
    @param[in] curSendCounts     Array of send sizes from me to all PEs
    @param[in] curRecvCounts     Array of recv sizes to me from all PEs
    @param[in] sourceData        Array of (old) source data
    @param[in] newData           Array of new source data
   */
  template<typename T>
  void moveData(const int commRank, const int commSize,
                const MPI_Datatype mpiType, const int stride,
                const int sourceStart, const int sourceEnd,
                const int newStart,
                const std::vector<int>& curSendCounts,
                const std::vector<int>& curRecvCounts,
                const std::vector<T>& sourceData,
                std::vector<T>* newData)
  {
    // Each rank will do a non-blocking receive from each rank from
    // which it will receive data values
    int writeOffset = newStart;
    int myOffset;
    std::vector<MPI_Request> requests;
    for (unsigned int i=0; i<commSize; i++)
    {
      if ((i != commRank) && (curRecvCounts[i] > 0))
      {
        MPI_Request request;
        MPI_Irecv((void *)&((*newData)[stride*writeOffset]),
                  stride*curRecvCounts[i], mpiType, i,
                  MPI_ANY_TAG, MPI_COMM_WORLD, &request);
        requests.push_back(request);
      }
      else if (i == commRank)
      {
        myOffset = writeOffset;
      }
      writeOffset += curRecvCounts[i];
    }

    // Copy data values that will stay on this rank into the
    // proper place in the new vector
    if (curRecvCounts[commRank] > 0)
      std::copy(sourceData.begin() + stride*sourceStart,
                sourceData.begin() + stride*sourceEnd,
                newData->begin() + stride*myOffset);

    // Each rank will send its data values to appropriate ranks
    for (unsigned int i=0; i<commSize; i++)
    {
      if ((i != commRank) && (curSendCounts[i] > 0))
      {
        MPI_Send((void *)&(sourceData[stride*sourceStart]),
                 stride*curSendCounts[i], mpiType, i, 0, MPI_COMM_WORLD);
      }
    }

    // Wait for all receives to complete
    if (requests.size() > 0)
    {
      std::vector<MPI_Status> statuses(requests.size());
      MPI_Waitall(requests.size(), &(requests[0]), &(statuses[0]));
    }
  } // moveData

  //! Correct a map to account for concatenated lists
  void fixListIndices(const comm_info_t& mapInfo,
              const comm_info_t& rangeInfo,
              const int commSize,
              std::vector<int>* newMap)
  {
    // compute corrections for each rank
    std::vector<int> ownedOffsets(commSize), ghostOffsets(commSize);
    ownedOffsets[0] = 0;
    std::partial_sum(rangeInfo.recvOwnedCounts.begin(),
                     rangeInfo.recvOwnedCounts.end()-1,
                     ownedOffsets.begin()+1);
    std::vector<int> recvGhostCounts(commSize);
    for (unsigned int i=0; i<commSize; ++i)
      recvGhostCounts[i] =
          rangeInfo.recvCounts[i] - rangeInfo.recvOwnedCounts[i];
    ghostOffsets[0] = 0;
    std::partial_sum(recvGhostCounts.begin(), recvGhostCounts.end()-1,
                     ghostOffsets.begin()+1);
    for (unsigned int i=0; i<commSize; ++i)
      ghostOffsets[i] += rangeInfo.newNumOwned - rangeInfo.recvOwnedCounts[i];

    // correct owned entities, one rank at a time
    int base = 0;
    for (unsigned int i=0; i<commSize; ++i) {
      int ownedCount = mapInfo.recvOwnedCounts[i];
      int ownedNodeCount = rangeInfo.recvOwnedCounts[i];
      for (int j=0; j<ownedCount; ++j) {
        int n = (*newMap)[base + j];
        (*newMap)[base + j] +=
            (n < ownedNodeCount ? ownedOffsets[i] : ghostOffsets[i]);
      }
      base += ownedCount;
    } // for i

    // correct ghost entities, one rank at a time
    for (unsigned int i=0; i<commSize; ++i) {
      int ownedCount = mapInfo.recvOwnedCounts[i];
      int ghostCount = mapInfo.recvCounts[i] - ownedCount;
      int ownedNodeCount = rangeInfo.recvOwnedCounts[i];
      for (int j=0; j<ghostCount; ++j) {
        int n = (*newMap)[base + j];
        (*newMap)[base + j] +=
            (n < ownedNodeCount ? ownedOffsets[i] : ghostOffsets[i]);
      }
      base += ghostCount;
    } // for i

  } // fixListIndices

}; // MPI_Bounding_Boxes

} // namespace Portage

#endif // MPI_Bounding_Boxes_H_
