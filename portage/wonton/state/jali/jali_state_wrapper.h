/*
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
*/





#ifndef JALI_MMSTATE_WRAPPER_H_
#define JALI_MMSTATE_WRAPPER_H_

#include <string>
#include <vector>

#include "portage/support/portage.h"

#include "Mesh.hh"       // Jali mesh declarations
#include "JaliState.h"  // Jali-based state manager declarations

/*!
  @file jali_mmstate_wrapper.h
  @brief Wrapper for interfacing with the Jali state manager
 */
namespace Wonton {

using namespace Portage;

/*!
  @class Jali_MMState_Wrapper "jali_mmstate_wrapper.h"
  @brief Provides access to data stored in Jali_State
*/
class Jali_State_Wrapper {
 public:

  /*!
    @brief Constructor of Jali_MMState_Wrapper
    @param[in] jali_state A reference to a Jali::State instance
   */
  Jali_State_Wrapper(Jali::State & jali_state) : jali_state_(jali_state) {}

  /*!
    @brief Copy constructor of Jali_State_Wrapper - not a deep copy
    @param[in] state A reference to another Jali_State_Wrapper instance
   */
  Jali_State_Wrapper(Jali_State_Wrapper & state) :
      jali_state_(state.jali_state_) {}

  /*!
    @brief Assignment operator (disabled) - don't know how to implement (RVG)
   */
  Jali_State_Wrapper & operator=(Jali_State_Wrapper const &) = delete;

  /*!
    @brief Empty destructor
   */
  ~Jali_State_Wrapper() {}


  /*!
    @brief Initialize fields from mesh file
   */
  void init_from_mesh() { jali_state_.init_from_mesh(); }

  /*!
    @brief Export fields to mesh file
   */
  void export_to_mesh() {jali_state_.export_to_mesh(); }


  /*!
    @brief Number of materials in problem
  */

  int num_materials() const {
    return jali_state_.num_materials();
  }

  /*!
    @brief Name of material
  */

  std::string material_name(int matid) const {
    assert(matid >= 0 && matid < jali_state_.num_materials());
    return jali_state_.material_name(matid);
  }

  /*!
    @brief Get cell indices containing a particular material
    @param matid    Index of material (0, num_materials()-1)
    @param matcells Cells contained in that materials
  */

  void mat_get_cells(int matid, std::vector<int> *matcells) const {
    matcells->clear();
    (*matcells) = jali_state_.material_cells(matid);
  }

  /*!
    @brief Get the local index of mesh cell in material cell list
    @param meshcell    Mesh cell ID
    @param matid       Material ID
    @return             Local cell index in material cell list
  */

  int cell_index_in_material(int meshcell, int matid) const {
    return jali_state_.cell_index_in_material(meshcell, matid);
  }



  /*!
    @brief Get a pointer to read-only single-valued data on the mesh
    @param[in] on_what The entity type on which to get the data
    @param[in] var_name The string name of the data field
    @param[in,out] data A vector containing the data
   */

  template <typename T>
  void mesh_get_data(const Entity_kind on_what, const std::string var_name,
                     T const **data) const {
    Jali::StateVector<T, Jali::Mesh> vector;
    if (jali_state_.get<T, Jali::Mesh, Jali::StateVector>(var_name,
                                                    jali_state_.mesh(),
                                                    (Jali::Entity_kind) on_what,
                                                    Jali::Entity_type::ALL,
                                                    &vector)) {
      *data = (T const *) vector.get_raw_data();
    }
    else
      std::cerr << "Could not find state variable " << var_name << "\n";
  }


  /*!
    @brief Get a pointer to read-write single-valued data on the mesh
    @param[in] on_what The entity type on which to get the data
    @param[in] var_name The string name of the data field
    @param[in,out] data A vector containing the data

    Removing the constness of the template parameter allows us to call
    this function and get const data back (e.g. pointer to double const)
    even if the wrapper object is not const. The alternative is to make
    another overloaded operator that is non-const but returns a pointer
    to const data. Thanks StackOverflow!
   */
  template <class T>
  void mesh_get_data(const Entity_kind on_what, const std::string var_name,
                     T **data) {
    using T1 = typename std::remove_const<T>::type;
    Jali::StateVector<T1, Jali::Mesh> vector;
    if (jali_state_.get<T1, Jali::Mesh, Jali::StateVector>(var_name,
                                                    jali_state_.mesh(),
                                                    (Jali::Entity_kind) on_what,
                                                    Jali::Entity_type::ALL,
                                                    &vector)) {
      *data = vector.get_raw_data();
    }
    else
      std::cerr << "Could not find state variable " << var_name << "\n";
  }


  /*!
    @brief Get pointer to read-only scalar cell data for a particular material
    @param[in] var_name The string name of the data field
    @param[in] matid   Index (not unique identifier) of the material
    @param[out] data   vector containing the values corresponding to cells in the material
   */

  template <class T>
  void mat_get_celldata(const std::string var_name, int matid, T const **data) const {
    Jali::MMStateVector<T, Jali::Mesh> mmvector;
    if (jali_state_.get<T, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                      jali_state_.mesh(),
                                                      Jali::Entity_kind::CELL,
                                                      Jali::Entity_type::ALL,
                                                      &mmvector)) {
      // data copy
      *data = (T const *) mmvector.get_raw_data(matid);
    }
    else
      std::cerr << "Could not find state variable " << var_name << "\n";
  }


  /*!
    @brief Get pointer to read-write scalar data for a particular material
    @param[in] on_what The entity type on which to get the data
    @param[in] var_name The string name of the data field
    @param[in] matid   Index (not unique identifier) of the material
    @param[out] data   vector containing the values corresponding to cells in the material

    Removing the constness of the template parameter allows us to call
    this function and get const data back (e.g. pointer to double const)
    even if the wrapper object is not const. The alternative is to make
    another overloaded operator that is non-const but returns a pointer
    to const data. Thanks StackOverflow!
   */

  template <class T>
  void mat_get_celldata(const std::string var_name, int matid, T **data) {
    using T1 = typename std::remove_const<T>::type;

    Jali::MMStateVector<T1, Jali::Mesh> mmvector;
    if (jali_state_.get<T1, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                      jali_state_.mesh(),
                                                      Jali::Entity_kind::CELL,
                                                      Jali::Entity_type::ALL,
                                                      &mmvector)) {
      // data copy
      *data = mmvector.get_raw_data(matid);
    }
    else
      std::cerr << "Could not find state variable " << var_name << "\n";
  }


  /*!
   @brief Add a scalar single valued data field
   @param[in] on_what The entity type on which the data is defined
   @param[in] var_name The name of the data field
   @param[in] values Initialize with this array of values
   */
  template <class T>
  void mesh_add_data(const Entity_kind on_what, const std::string var_name,
                      T const * const values) {
    jali_state_.add(var_name, jali_state_.mesh(), (Jali::Entity_kind) on_what,
                    Jali::Entity_type::ALL, values);
  }

  /*!
   @brief Add a scalar single valued data field with uniform values
   @param[in] on_what The entity type on which the data is defined
   @param[in] var_name The name of the data field
   @param[in] value Initialize with this value
   */
  template <class T>
  void mesh_add_data(const Entity_kind on_what, const std::string var_name,
                      const T value) {
    // Compiler needs some help deducing template parameters here
    jali_state_.add<T, Jali::Mesh, Jali::StateVector>(var_name,
                                                    jali_state_.mesh(),
                                                    (Jali::Entity_kind) on_what,
                                                    Jali::Entity_type::ALL,
                                                    value);
  }


  /*!
   @brief Add a scalar multi-valued data field on cells and initialize its
   material data to a single value
   @param[in] var_name The name of the data field
   @param[in] value Initialize with this value

   The 2D array will be read and values copied according to which materials
   are contained in which cells. If a material+cell combination is not active
   providing a value for the array will have no effect. 
   */
  template <class T>
  void mat_add_celldata(const std::string var_name, const T value) {
    jali_state_.add(var_name, jali_state_.mesh(), Jali::Entity_kind::CELL,
                    Jali::Entity_type::ALL, value);
  }


  /*!
   @brief Add a scalar multi-valued data field on cells and initialize its
   material data according to a 2D array
   @param[in] var_name The name of the data field
   @param[in] layout  Whether 2D array is laid out with first index being the cell (CELL_CENRIC) or material (MATERIAL CENTRIC)
   @param[in] value Initialize with this value

   The 2D array will be read and values copied according to which materials
   are contained in which cells. If a material+cell combination is not active
   providing a value for the array will have no effect. 
   */
  template <class T>
  void mat_add_celldata(const std::string var_name,
                        T const **values = nullptr,
                        Data_layout layout = Data_layout::MATERIAL_CENTRIC) {
    jali_state_.add(var_name, jali_state_.mesh(), Jali::Entity_kind::CELL,
                    Jali::Entity_type::ALL, (Jali::Data_layout) layout, values);
  }


  /*!
   @brief Add a scalar multi-valued data field on cells and add data to one of
   its materials
   @param[in] var_name The name of the data field
   @param[in] matid  Index of material in the problem
   @param[in] layout Data layout - 
   @param[in] values Initialize with this array of values

   Subsequent calls to this function with the same name will find the added
   field and just add the data.
   */
  template <class T>
  void mat_add_celldata(const std::string var_name, int matid,
                    T const * const values) {
    auto it = jali_state_.find<T, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                        jali_state_.mesh(),
                                                        Jali::Entity_kind::CELL,
                                                        Jali::Entity_type::ALL);

    if (it == jali_state_.end()) {
      Jali::MMStateVector<T, Jali::Mesh>& mmvec =
          jali_state_.add<T, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                  jali_state_.mesh(),
                                                  Jali::Entity_kind::CELL,
                                                  Jali::Entity_type::ALL);
      std::vector<T>& matdata = mmvec.get_matdata(matid);
      int nmatcells = matdata.size();
      matdata.assign(values, values+nmatcells);
    } else {
      Jali::MMStateVector<T, Jali::Mesh>& mmvec =
          *(std::dynamic_pointer_cast<Jali::MMStateVector<T, Jali::Mesh>>(*it));
      std::vector<T>& matdata = mmvec.get_matdata(matid);
      int nmatcells = matdata.size();
      matdata.assign(values, values+nmatcells);
    }
  }


  /*!
   @brief Add a scalar multi-valued data field on cells and initialize one of
   its material data to a uniform value
   @param[in] var_name The name of the data field
   @param[in] matid Index of material in the problem
   @param[in] value Initialize with this value

   Subsequent calls to this function with the same name will find the added
   field and just add the data.
   */
  template <class T>
  void mat_add_celldata(const std::string var_name, int matid, const T value) {
    auto it = jali_state_.find<T, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                        jali_state_.mesh(),
                                                        Jali::Entity_kind::CELL,
                                                        Jali::Entity_type::ALL);

    if (it == jali_state_.end()) {
      Jali::MMStateVector<T, Jali::Mesh>& mmvec =
          jali_state_.add<T, Jali::Mesh, Jali::MMStateVector>(var_name,
                                                  jali_state_.mesh(),
                                                  Jali::Entity_kind::CELL,
                                                  Jali::Entity_type::ALL);
      std::vector<T>& matdata = mmvec.get_matdata(matid);
      int nmatcells = matdata.size();
      matdata.assign(nmatcells, value);
    } else {
      Jali::MMStateVector<T, Jali::Mesh>& mmvec =
          *(std::dynamic_pointer_cast<Jali::MMStateVector<T, Jali::Mesh>>(*it));
      std::vector<T>& matdata = mmvec.get_matdata(matid);
      int nmatcells = matdata.size();
      matdata.assign(nmatcells, value);
    }
  }
    


  /*!
    @brief Add cells to material (or add material to cells)
    @param[in] matid  Material ID
    @param[in] newcells Vector of new cells in material
  */

  void mat_add_cells(int matid, std::vector<int> const & newcells) {
    jali_state_.add_cells_to_material(matid, newcells);
  }


  /*!
    @brief Remove cells from material (or remove material from cells)
    @param[in] matid  Material ID
    @param[in] matcells Vector of to be removed cells
  */
  
  void mat_rem_cells(int matid, std::vector<int> const & delcells) {
    jali_state_.rem_cells_from_material(matid, delcells);
  }


  /*!
    @brief Add a material to state
    @param[in] matname  Name of material
    @param[in] matcells Cells containing the material
   */
  void add_material(std::string const& matname,
                    std::vector<int> const& matcells) {
    jali_state_.add_material(matname, matcells);
  }

  /*!
    @brief Get the entity type on which the given field is defined
    @param[in] var_name The string name of the data field
    @return The Entity_kind enum for the entity type on which the field is defined

    @todo  THIS ASSUMES ONLY DOUBLE VECTORS - WE HAVE TO ACCOUNT FOR OTHER TYPES
           OR WE HAVE TO GENERALIZE THE FIND FUNCTION!!!
   */
  Entity_kind get_entity(const std::string var_name) const {

    // ****** CHANGE WHEN JALISTATE find FUNCTION IS FIXED TO NOT NEED
    // ****** THE DATA TYPE

    Jali::State::const_iterator it =
        jali_state_.find<double, Jali::Mesh>(var_name, jali_state_.mesh());
    if (it != jali_state_.cend()) {
      std::shared_ptr<Jali::BaseStateVector> vector = *it;
      if (vector)
        return (Portage::Entity_kind) vector->entity_kind();
    }

    std::cerr << "Could not find state variable " << var_name << "\n";
    return Portage::UNKNOWN_KIND;
  }


  /*!
    @brief Get the data size for the given field
    @param[in] on_what  The entity type on which the data field is defined
    @param[in] var_name The string name of the data field
    @return The data size for the field with the given name on the given entity type

    @todo  THIS ASSUMES ONLY DOUBLE VECTORS - WE HAVE TO ACCOUNT FOR OTHER TYPES
    OR WE HAVE TO GENERALIZE THE FIND FUNCTION!!!
         
    For multi-material state, this will give the number of materials for now
   */
  int get_data_size(const Entity_kind on_what,
                    const std::string var_name) const {

    // ****** CHANGE WHEN JALISTATE find FUNCTION IS FIXED TO NOT NEED
    // ****** THE DATA TYPE

    Jali::State::const_iterator it =
        jali_state_.find<double, Jali::Mesh>(var_name, jali_state_.mesh(),
                                        (Jali::Entity_kind) on_what);
    if (it != jali_state_.cend()) {
      std::shared_ptr<Jali::BaseStateVector> vector = *it;
      if (vector)
        return (vector->size());
    }

    std::cerr << "Could not find state variable " << var_name << "\n";
    return 0;
  }


#if 0
  /*!
    @brief Get the data type of the given field
    @param[in] var_name The string name of the data field
    @return A reference to the type_info struct for the field's data type
   */
  const std::type_info& get_type(const std::string var_name) const {

    Jali::State::const_iterator it =
        jali_state_.find(var_name, jali_state_.mesh());
    if (it != jali_state_.cend()) {
      std::shared_ptr<Jali::BaseStateVector> vector = *it;
      if (vector)
        return vector->get_type();
    }

    std::cerr << "Could not find state variable " << var_name << "\n";
    return typeid(0);
  }
#endif

  /*!
    @brief Begin iterator on vector names
    @return Begin iterator on vector of strings
   */
  std::vector<std::string>::iterator names_begin() const {
    return jali_state_.names_begin();
  }

  /*!
    @brief End iterator on vector names
    @return End iterator on vector of strings
   */
  std::vector<std::string>::iterator names_end() const {
    return jali_state_.names_end();
  }

  /*!
    @brief Typedef for permutation iterator on vector of strings
   */
  typedef Jali::State::string_permutation string_permutation;

  /*!
    @brief Begin iterator on vector names of specific entity type
    @param[in] on_what The desired entity type
    @return Permutation iterator to start of string vector
   */
  string_permutation names_entity_begin(Entity_kind const on_what) const {
    return jali_state_.names_entity_begin((Jali::Entity_kind)on_what);
  }

  /*!
    @brief End iterator on vector of names of specific entity type
    @param[in] on_what The desired entity type
   */
  string_permutation names_entity_end(Entity_kind const on_what) const {
    return jali_state_.names_entity_end((Jali::Entity_kind)on_what);
  }

 private:

  Jali::State & jali_state_;

};  // Jali_State_Wrapper

}  // namespace Wonton

#endif  // JALI_STATE_WRAPPER_H_
