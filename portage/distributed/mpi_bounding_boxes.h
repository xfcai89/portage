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
#include <map>
#include <vector>

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
  MPI_Bounding_Boxes() {}


  /*!
    @brief Destructor of MPI_Bounding_Boxes
   */
  ~MPI_Bounding_Boxes() {}


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

    int dim = dim_ = source_mesh_flat.space_dimension();
    assert(dim == target_mesh.space_dimension());


    // sendFlags, which partitions to send data
    // this is computed via intersection of whole partition bounding boxes
    std::vector<bool> sendFlags(commSize);   
    compute_sendflags(source_mesh_flat, target_mesh, sendFlags);        

    // set counts for cells
    comm_info_t cellInfo;
    int sourceNumOwnedCells = source_mesh_flat.num_owned_cells();
    int sourceNumCells = sourceNumOwnedCells + source_mesh_flat.num_ghost_cells();
    setSendRecvCounts(&cellInfo, commSize, sendFlags,sourceNumCells, sourceNumOwnedCells);
    
    // set counts for nodes
    comm_info_t nodeInfo;
    int sourceNumOwnedNodes = source_mesh_flat.num_owned_nodes();
    int sourceNumNodes = sourceNumOwnedNodes + source_mesh_flat.num_ghost_nodes();
    setSendRecvCounts(&nodeInfo, commSize, sendFlags,sourceNumNodes, sourceNumOwnedNodes);


    ///////////////////////////////////////////////////////
    // always distributed
    ///////////////////////////////////////////////////////

    // SEND GLOBAL CELL IDS
    std::vector<int>& sourceCellGlobalIds = source_mesh_flat.get_global_cell_ids();
    std::vector<int> newCellGlobalIds(cellInfo.newNum);
    sendField(cellInfo, commRank, commSize, MPI_INT, 1,
              sourceCellGlobalIds, &newCellGlobalIds);

    // SEND GLOBAL NODE IDS
    std::vector<int>& sourceNodeGlobalIds = source_mesh_flat.get_global_node_ids();
    std::vector<int> newNodeGlobalIds(nodeInfo.newNum);
    sendField(nodeInfo, commRank, commSize, MPI_INT, 1,
              sourceNodeGlobalIds, &newNodeGlobalIds);

    // Create maps from old uid's to old and new indices
    create_maps(newCellGlobalIds, uidToOldCell_, uidToNewCell_);
    create_maps(newNodeGlobalIds, uidToOldNode_, uidToNewNode_);

    // SEND NODE COORDINATES
    std::vector<double>& sourceCoords = source_mesh_flat.get_coords();
    std::vector<double> newCoords(dim*nodeInfo.newNum);
    sendField(nodeInfo, commRank, commSize, MPI_DOUBLE, dim,
              sourceCoords, &newCoords);
              
    // merge and set coordinates in the flat mesh
    sourceCoords = merge_data(newCoords, uidToOldNode_, dim_);
    

    ///////////////////////////////////////////////////////
    // 2D distributed
    ///////////////////////////////////////////////////////
        
    if (dim == 2)
    {
                
      // send cell node counts
      std::vector<int>& sourceCellNodeCounts = source_mesh_flat.get_cell_node_counts();
      std::vector<int> newCellNodeCounts(cellInfo.newNum);
      sendField(cellInfo, commRank, commSize, MPI_INT, 1,
                sourceCellNodeCounts, &newCellNodeCounts);
                
      // merge and set cell node counts
      sourceCellNodeCounts=merge_data( newCellNodeCounts, uidToOldCell_);


      // mesh data references
      std::vector<int>& sourceCellNodeOffsets = source_mesh_flat.get_cell_node_offsets();
      std::vector<int>& sourceCellToNodeList = source_mesh_flat.get_cell_to_node_list();
      
      int sizeCellToNodeList = sourceCellToNodeList.size();
      int sizeOwnedCellToNodeList = (
          sourceNumCells == sourceNumOwnedCells ? sizeCellToNodeList :
          sourceCellNodeOffsets[sourceNumOwnedCells]);

      comm_info_t cellToNodeInfo;
      setSendRecvCounts(&cellToNodeInfo, commSize, sendFlags,
              sizeCellToNodeList, sizeOwnedCellToNodeList);
              
      // send cell to node lists
      std::vector<int> newCellToNodeList(cellToNodeInfo.newNum);
      sendField(cellToNodeInfo, commRank, commSize, MPI_INT, 1,
                to_uid(sourceCellToNodeList, sourceNodeGlobalIds), &newCellToNodeList);

      // merge and map cell node lists
      sourceCellToNodeList = merge_lists(newCellToNodeList, newCellNodeCounts, 
        uidToOldCell_, uidToNewNode_);

    }


    ///////////////////////////////////////////////////////
    // 3D distributed
    ///////////////////////////////////////////////////////
        
    if (dim == 3)
    {
      int sourceNumOwnedFaces = source_mesh_flat.num_owned_faces();
      int sourceNumFaces = sourceNumOwnedFaces + source_mesh_flat.num_ghost_faces();
      
      comm_info_t faceInfo;
      setSendRecvCounts(&faceInfo, commSize, sendFlags,sourceNumFaces, sourceNumOwnedFaces);

      // SEND GLOBAL FACE IDS
      std::vector<int>& sourceFaceGlobalIds = source_mesh_flat.get_global_face_ids();
      std::vector<int> newFaceGlobalIds(faceInfo.newNum);
      sendField(faceInfo, commRank, commSize, MPI_INT, 1,
                sourceFaceGlobalIds, &newFaceGlobalIds);                  

      // Create map from old uid's to old and new indices
      create_maps(newFaceGlobalIds, uidToOldFace_, uidToNewFace_);

      // mesh data references
      std::vector<int>& sourceCellFaceOffsets = source_mesh_flat.get_cell_face_offsets();
      std::vector<int>& sourceCellToFaceList = source_mesh_flat.get_cell_to_face_list();
     
      int sizeCellToFaceList = sourceCellToFaceList.size();
      int sizeOwnedCellToFaceList = (
          sourceNumCells == sourceNumOwnedCells ? sizeCellToFaceList :
          sourceCellFaceOffsets[sourceNumOwnedCells]);

      comm_info_t cellToFaceInfo;
      setSendRecvCounts(&cellToFaceInfo, commSize, sendFlags,
              sizeCellToFaceList, sizeOwnedCellToFaceList);

      // SEND NUMBER OF FACES FOR EACH CELL

      std::vector<int>& sourceCellFaceCounts = source_mesh_flat.get_cell_face_counts();
      std::vector<int> newCellFaceCounts(cellInfo.newNum);
      sendField(cellInfo, commRank, commSize, MPI_INT, 1,
                sourceCellFaceCounts, &newCellFaceCounts);

      // merge and set cell face counts
      sourceCellFaceCounts=merge_data( newCellFaceCounts, uidToOldCell_);
      
      // SEND CELL-TO-FACE MAP
      // map the cell face list vector to uid's
      std::vector<int> sourceCellToFaceList__= to_uid(sourceCellToFaceList, sourceFaceGlobalIds);
      
      // For this array only, pack up face IDs + dirs and send together
      std::vector<bool>& sourceCellToFaceDirs = source_mesh_flat.get_cell_to_face_dirs();
      for (unsigned int j=0; j<sourceCellToFaceList.size(); ++j)
      {
        int f = sourceCellToFaceList__[j];
        int dir = static_cast<int>(sourceCellToFaceDirs[j]);
        sourceCellToFaceList[j] = (f << 1) | dir;
      }

      std::vector<int> newCellToFaceList(cellToFaceInfo.newNum);
      sendField(cellToFaceInfo, commRank, commSize, MPI_INT, 1,
                sourceCellToFaceList, &newCellToFaceList);

      // Unpack face IDs and dirs
      std::vector<bool> newCellToFaceDirs(cellToFaceInfo.newNum);
      for (unsigned int j=0; j<newCellToFaceList.size(); ++j)
      {
        int fd = newCellToFaceList[j];
        newCellToFaceList[j] = fd >> 1;
        newCellToFaceDirs[j] = fd & 1;
      }

      
      // merge and map cell face lists
      sourceCellToFaceList = merge_lists(newCellToFaceList, newCellFaceCounts, 
        uidToOldCell_, uidToNewFace_);
        
      // merge cell face directions
      sourceCellToFaceDirs = merge_lists(newCellToFaceDirs, newCellFaceCounts, 
        uidToOldCell_);
      

      
      // mesh data references
      std::vector<int>& sourceFaceNodeOffsets = source_mesh_flat.get_face_node_offsets();
      std::vector<int>& sourceFaceToNodeList = source_mesh_flat.get_face_to_node_list();
      
      int sizeFaceToNodeList = sourceFaceToNodeList.size();
      int sizeOwnedFaceToNodeList = (
          sourceNumFaces == sourceNumOwnedFaces ? sizeFaceToNodeList :
          sourceFaceNodeOffsets[sourceNumOwnedFaces]);

      comm_info_t faceToNodeInfo;
      setSendRecvCounts(&faceToNodeInfo, commSize, sendFlags,
              sizeFaceToNodeList, sizeOwnedFaceToNodeList);                 
      
      // SEND NUMBER OF NODES FOR EACH FACE
      std::vector<int>& sourceFaceNodeCounts = source_mesh_flat.get_face_node_counts();
      std::vector<int> newFaceNodeCounts(faceInfo.newNum);
      sendField(faceInfo, commRank, commSize, MPI_INT, 1,
                sourceFaceNodeCounts, &newFaceNodeCounts);

      // SEND FACE-TO-NODE MAP
      std::vector<int> newFaceToNodeList(faceToNodeInfo.newNum);
      sendField(faceToNodeInfo, commRank, commSize, MPI_INT, 1,
                to_uid(sourceFaceToNodeList, sourceNodeGlobalIds), &newFaceToNodeList); 
                
      // merge and set face node counts
      sourceFaceNodeCounts=merge_data( newFaceNodeCounts, uidToOldFace_);
      
      // merge and map face node lists
      sourceFaceToNodeList = merge_lists(newFaceToNodeList, newFaceNodeCounts, 
        uidToOldFace_, uidToNewNode_);

      // merge face global ids
      sourceFaceGlobalIds = merge_data(newFaceGlobalIds, uidToOldFace_);
      
      // set counts for faces in the flat mesh
      source_mesh_flat.set_num_owned_faces(sourceFaceGlobalIds.size());
                                  
    }
    
    // SEND FIELD VALUES
    
    // multimaterial state info
    int nmats = source_state_flat.num_materials();

    // Is the a multimaterial problem? If so we need to pass the cell indices
    // in addition to the field values
    if (nmats>0){
    
      std::cout << "in distribute, this a multimaterial problem with " << nmats << " materials\n";
      
      /////////////////////////////////////////////////////////
      // get the material ids across all nodes
      /////////////////////////////////////////////////////////
      
      // set the info for the number of materials on each node
      comm_info_t num_mats_info;
      setSendRecvCounts(&num_mats_info, commSize, sendFlags, nmats, nmats);
      
      // get the sorted material ids on this node
      std::vector<int> material_ids=source_state_flat.get_material_ids();
      
      // get all material ids across
      all_material_ids_.resize(num_mats_info.newNum);
      
      // send all materials to all nodes, num_mats_info.recvCounts is the shape
      sendData(commRank, commSize, MPI_INT, 1, 0, num_mats_info.sourceNum, 0,
        num_mats_info.sendCounts, num_mats_info.recvCounts,
        material_ids, &all_material_ids_
      );
      
      /////////////////////////////////////////////////////////
      // get the material cell shapes across all nodes
      /////////////////////////////////////////////////////////      

      // get the sorted material shapes on this node
      std::vector<int> material_shapes=source_state_flat.get_material_shapes();
      
      // resize the post distribute material id vector
      all_material_shapes_.resize(num_mats_info.newNum);

      // send all material shapes to all nodes
      sendData(commRank, commSize, MPI_INT, 1, 0, num_mats_info.sourceNum, 0,
        num_mats_info.sendCounts, num_mats_info.recvCounts,
        material_shapes, &all_material_shapes_
      );
     
      /////////////////////////////////////////////////////////
      // get the lists of material cell ids across all nodes
      /////////////////////////////////////////////////////////
      
      // get the total number of material cell id's on this node
      int nmatcells = source_state_flat.num_material_cells();
      
      // set the info for the number of materials on each node
      setSendRecvCounts(&num_mat_cells_info_, commSize, sendFlags, nmatcells, nmatcells);
      
      // get the sorted material ids on this node
      std::vector<int> material_cells=source_state_flat.get_material_cells();
      
      // resize the post distribute material id vector
      all_material_cells_.resize(num_mat_cells_info_.newNum);

      // send material cells to all nodes, but first translate to gid
      sendData(commRank, commSize, MPI_INT, 1, 0, num_mat_cells_info_.sourceNum, 0,
        num_mat_cells_info_.sendCounts, num_mat_cells_info_.recvCounts,
        to_uid(material_cells,sourceCellGlobalIds), &all_material_cells_
      );
     
      
      /////////////////////////////////////////////////////////
      // We need to turn the flattened material cells into a correctly shaped
      // ragged right structure for use as the material cells in the flat
      // state wrapper. We aren't removing duplicates yet, just concatenating by material
      /////////////////////////////////////////////////////////
      
      // allocate the material indices
      std::unordered_map<int,std::vector<int>> material_indices;
      
      // reset the running counter
      int running_counter=0;
      
      // loop over material ids on different nodes
      for (int i=0; i<all_material_ids_.size(); ++i){
      
        // get the current working material
        int mat_id = all_material_ids_[i];
        
        // get the current number of material cells for this material
        int nmat_cells = all_material_shapes_[i];
        
        // get or create a reference to the correct material cell vector
        std::vector<int>& these_material_cells = material_indices[mat_id];
        
        // loop over the correct number of material cells
        for (int j=0; j<nmat_cells; ++j){
            these_material_cells.push_back(all_material_cells_[running_counter++]);
        }
          
      }
      
      // cell material indices are added not replaced by vector so we need to
      // start clean
      source_state_flat.clear_material_cells();
      
      // merge the material cells and convert to local id (uid sort order)
      for (auto &kv: material_indices){
      
        // get the material id
        int m = kv.first;
        
        // create the maps between uid and index within the material
        // uidToOldIndexInMaterial_ maps from uid to index in unmerged data
        // this will be reused to remap data fields
        create_maps(kv.second, uidToOldIndexInMaterial_[m], uidToNewIndexInMaterial_[m]);
        
        // compute unique uid's with  material
        // material cells (uid's) have duplicates and need to be merged
        // this step merges the cell uid's within a material so each cell is unique
        std::vector<int> mat_cell_indices = merge_data(kv.second, uidToOldIndexInMaterial_[m]);
        
        // material cells are unique within the material but currently uid's
        // the uid's need to be converted to a local cell id on the partition
        std::vector<int> local_mat_cell_indices;
        local_mat_cell_indices.reserve(mat_cell_indices.size());
        for (auto uid: mat_cell_indices) 
          local_mat_cell_indices.push_back(uidToNewCell_[uid]);
        
        // add the material cells to the state manager
        source_state_flat.mat_add_cells(kv.first, local_mat_cell_indices);
      }
      
    }

    // Send and receive each field to be remapped
    for (std::string field_name : source_state_flat.names())
    {

      // this is a packed local version of the field and is not a pointer to the
      // original field
      std::vector<double> sourceField = source_state_flat.pack(field_name);
      
      // get the field stride
      int sourceFieldStride = source_state_flat.get_field_stride(field_name);

      comm_info_t info;
      if (source_state_flat.get_entity(field_name) == Entity_kind::NODE){
          // node mesh field
          info = nodeInfo;
      } else if (source_state_flat.field_type(Entity_kind::CELL, field_name) == Wonton::Field_type::MESH_FIELD){
          // mesh cell field
          info = cellInfo;
      } else {
         // multi material field
         info = num_mat_cells_info_;
      }
                           
      // allocate storage for the new distribute data, note that this data
      // is still packed and raw doubles and will need to be unpacked
      std::vector<double> newField(sourceFieldStride*info.newNum);

      // send the field
      sendField(info, commRank, commSize, MPI_DOUBLE, sourceFieldStride,
        sourceField, &newField);
      
      // merge the data before unpacking
      if (source_state_flat.get_entity(field_name) == Entity_kind::NODE){
      
        // node mesh field
        std::vector<double> tempNewField = merge_data(newField, uidToOldNode_, sourceFieldStride);
        
        // unpack the field, has the correct data types, but still is not merged
        source_state_flat.unpack(field_name, tempNewField);
        
      } else if (source_state_flat.field_type(Entity_kind::CELL, field_name) == Wonton::Field_type::MESH_FIELD){
      
        // mesh cell field
        std::vector<double> tempNewField = merge_data(newField, uidToOldCell_, sourceFieldStride);
        
        // unpack the field, has the correct data types, but still is not merged
        source_state_flat.unpack(field_name, tempNewField);
        
      } else {
      
        // unpack the field, has the correct data types, but still is not merged
        source_state_flat.unpack(field_name, newField, 
          all_material_ids_, all_material_shapes_, uidToOldIndexInMaterial_);
        
      }
         
           
    } 

    // need to do this at the end, because converting to 
    // uid uses the global id's and we don't want to modify them before we are
    // done converting the old relationships
    // merge global ids and set in the flat mesh    
    sourceCellGlobalIds = merge_data(newCellGlobalIds, uidToOldCell_);
    sourceNodeGlobalIds = merge_data(newNodeGlobalIds, uidToOldNode_);

    // set counts for cells and nodes in the flat mesh
    source_mesh_flat.set_num_owned_cells(sourceCellGlobalIds.size());
    source_mesh_flat.set_num_owned_nodes(sourceNodeGlobalIds.size());

    // Finish initialization using redistributed data
    source_mesh_flat.finish_init();


  } // distribute

  private:
 
  int dim_;
  
  // maps between uid and new and old indices
  std::map<int,int> uidToOldNode_, uidToNewNode_;
  std::map<int,int> uidToOldFace_, uidToNewFace_;
  std::map<int,int> uidToOldCell_, uidToNewCell_;
  
  // MM maps for material cells by material
  std::map<int,std::map<int,int>>uidToOldIndexInMaterial_;
  std::map<int,std::map<int,int>>uidToNewIndexInMaterial_;
  
  // vectors for unpacking multimaterial data
  std::vector<int> all_material_ids_;
  std::vector<int> all_material_shapes_;
  std::vector<int>all_material_cells_;

  // comm info data tha is potentially needed in two different scopes
  comm_info_t num_mat_cells_info_;


  /*!
    @brief Compute fields needed to do comms for a given entity type
    @param[in] info              Info data structure to be filled
    @param[in] commSize          Total number of MPI ranks
    @param[in] sendFlags         Array of flags:  do I send to PE n?
    @param[in] sourceNum         Number of entities (total) on this rank
    @param[in] sourceNumOwned    Number of owned entities on this rank
   */
  void setSendRecvCounts(comm_info_t* info,
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
  } // setSendRecvCounts


  /*!
    @brief Send values for a single data field to all ranks as needed
    @tparam[in] T                C++ type of data to be sent
    @param[in] info              Info struct for entity type of field
    @param[in] commRank          MPI rank of this PE
    @param[in] commSize          Total number of MPI ranks
    @param[in] mpiType           MPI type of data (MPI_???) to be sent
    @param[in] stride            Stride of data field
    @param[in] sourceData        Array of (old) source data
    @param[in] newData           Array of new source data
   */
  template<typename T>
  void sendField(const comm_info_t& info,
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

    sendData(commRank, commSize, mpiType, stride,
             0, info.sourceNumOwned,
             0,
             info.sendOwnedCounts, info.recvOwnedCounts,
             sourceData, newData);
    sendData(commRank, commSize, mpiType, stride,
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
  } // sendField


  /*!
    @brief Send values for a single range of data to all ranks as needed
    @tparam[in] T                C++ type of data to be sent
    @param[in] commRank          MPI rank of this PE
    @param[in] commSize          Total number of MPI ranks
    @param[in] mpiType           MPI type of data (MPI_???) to be sent
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
  void sendData(const int commRank, const int commSize,
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
  } // sendData


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

  template <class Source_Mesh, class Target_Mesh>        
  void compute_sendflags(Source_Mesh & source_mesh_flat, Target_Mesh &target_mesh, 
              std::vector<bool> &sendFlags){
                
    // Get the MPI communicator size and rank information
    int commSize, commRank;
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &commRank);

    int dim = source_mesh_flat.space_dimension();

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
    std::vector<double>& sourceCoords = source_mesh_flat.get_coords();

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
  }

  std::vector<int> to_uid(std::vector<int> const& in, vector<int>const& uid){
    std::vector<int> result;
    result.reserve(in.size());
    for (auto x:in) result.push_back(uid[x]);
    return result;
  }

  
  void create_maps(std::vector<int>const& uids, std::map<int,int>& uidToOld,
    std::map<int,int>& uidToNew){
    
    for (int i=0; i<uids.size(); ++i)
      if (uidToOld.find(uids[i])==uidToOld.end())
        uidToOld[uids[i]]=i;
    int i=0;
    for (auto& kv:uidToOld) uidToNew[kv.first]=i++;
  }

  
  template<class T>
  std::vector<T> merge_data(std::vector<T>const& in, 
    std::map<int,int>const& toOld, int stride=1){
    
    std::vector<T> result;
    result.reserve(toOld.size()*stride);
    for (auto& kv: toOld)
      for (int d=0; d<stride; ++d)
        result.push_back(in[stride*kv.second + d]);
    return result;
  }

  
  std::vector<int> merge_lists(std::vector<int>const& in, std::vector<int> const & counts,
    std::map<int,int>const& uidToOld, std::map<int,int>const& uidToNew){
    
    std::vector<int> offsets(counts.size());
    std::partial_sum(counts.begin(), counts.end()-1, offsets.begin()+1);
    std::vector<int> result;
    result.reserve(uidToOld.size()*(dim_+1)); // estimate, lower bound
    for (auto &kv: uidToOld)
      for (int i=0; i<counts[kv.second]; ++i)
        result.push_back(uidToNew.at(in[offsets[kv.second]+i]));
    return result;
  }

  
  // lists are data, so don't merge
  template<class T>
  std::vector<T> merge_lists(std::vector<T>const& in, std::vector<int> const & counts,
    std::map<int,int>const& uidToOld){
    
    std::vector<int> offsets(counts.size());
    std::partial_sum(counts.begin(), counts.end()-1, offsets.begin()+1);
    std::vector<T> result;
    result.reserve(uidToOld.size()*(dim_+1)); // estimate, lower bound
    for (auto &kv: uidToOld)
      for (int i=0; i<counts[kv.second]; ++i)
        result.push_back(in[offsets[kv.second]+i]);
    return result;
  }
  

}; // MPI_Bounding_Boxes

} // namespace Portage

#endif // MPI_Bounding_Boxes_H_
