/*
Copyright (c) 2016, Los Alamos National Security, LLC
All rights reserved.

Copyright 2016. Los Alamos National Security, LLC. This software was produced
under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National
Laboratory (LANL), which is operated by Los Alamos National Security, LLC for
the U.S. Department of Energy. The U.S. Government has rights to use,
reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS ALAMOS
NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified to produce
derivative works, such modified software should be clearly marked, so as not to
confuse it with the version available from LANL.

Additionally, redistribution and use in source and binary forms, with or
without modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of Los Alamos National Security, LLC, Los Alamos
   National Laboratory, LANL, the U.S. Government, nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL
SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef FLAT_STATE_WRAPPER_H_
#define FLAT_STATE_WRAPPER_H_

#include <map>
#include <memory>
#include <vector>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "portage/support/portage.h"
#include "portage/support/Point.h"

/*!
  @file flat_state_wrapper.h
  @brief Wrapper for interfacing with the Flat state manager
 */

namespace Portage {

/*!
  @class Flat_State_Wrapper "flat_state_wrapper.h"
  @brief Stores state data in a flat representation
         
         Currently all fields must be of the same type
*/
template <class T=double>
class Flat_State_Wrapper {
 public:

  //! pair of name and entity to be used as data key
  using pair_t = std::pair<std::string, Entity_kind>;

  /*!
    @brief Constructor of Flat_State_Wrapper
   */
  Flat_State_Wrapper() { };

  /*!
   * @brief Initialize the state wrapper with another state wrapper and a list of names
   * @param[in] input another state wrapper, which need not be for Flat_State.
   * @param[in] var_names a list of state names to initialize
   *
   * Entities and sizes associated with the given name will be obtained from the input state wrapper.
   *
   * A name can be re-used with a different entity, but a name-entity combination
   * must be unique.
   *
   * A name-entity combination must not introduce a new size for that entity if
   * it has previously been encountered.
   *
   * All existing internal data is forgotten.
   */
  template <class State_Wrapper>
  void initialize(State_Wrapper const & input,
                  std::vector<std::string> var_names) 
  {
	  clear(); // forget everything

	  for (unsigned int i=0; i<var_names.size(); i++)
	  {
		  // get name
		  std::string varname = var_names[i];

		  // get entity
		  Entity_kind entity = input.get_entity(varname);

		  // get pointer to data for state from input state wrapper
		  T const* data;
		  input.get_data(entity, varname, &data);

		  // copy input state data into new vector storage
		  size_t dataSize = input.get_data_size(entity, varname);
		  auto vdata = std::make_shared<std::vector<T>>(dataSize);
	      std::copy(data, data+dataSize, vdata->begin());

	      // add to database
		  add_data(entity, varname, vdata);
	  }
  }

  /*!
    @brief Assignment operator (disabled) - don't know how to implement (RVG)
   */
  Flat_State_Wrapper & operator=(Flat_State_Wrapper const &) = delete;

  /*!
    @brief Empty destructor
   */
  ~Flat_State_Wrapper() {};

  /*!
   * @brief Initialize the state wrapper with explicit lists of names, entities and data
   * @param[in] names a list of state names to initialize
   * @param[in] entities entities corresponding to the names
   * @param[in] data the state vectors to be stored
   *
   * A name can be re-used with a different entity, but a name-entity combination
   * must be unique.
   *
   * A name-entity combination must not introduce a new size for that entity if
   * the entity has previously been encountered.
   *
   * All existing internal data is forgotten.
   */
  void initialize(std::vector<std::string> names, std::vector<Entity_kind> entities,
		  	      std::vector<std::shared_ptr<std::vector<T>>> data)
  {
    if (not (names.size() == entities.size() and names.size() == data.size() and data.size() == entities.size())) {
        throw std::runtime_error("argument sizes do not agree");
    }

    clear();

    size_t index;
    for (size_t i=0; i<names.size(); i++) {
    	add_data(entities[i], names[i], data[i]);
    }
  }

  /*!
    @brief Get pointer to scalar data
    @param[in] on_what The entity type on which the data is defined
    @param[in] var_name The string name of the data field
    @param[in,out] data A pointer to an array of data. Null on output if data does not exist.
    *
    * Data is associated with the name-entity combination. Both values must be valid.
   */
  void get_data(const Entity_kind on_what, const std::string var_name, T** data) {
    pair_t pr(var_name, on_what);
    auto iter = name_map_.find(pr);
    if (iter != name_map_.end()) {
      (*data) = (T*)(&((*(state_[iter->second]))[0]));
    } else {
      (*data) = nullptr;
    }
  }

  /*!
    @brief Get pointer to scalar data
    @param[in] on_what The entity type on which the data is defined
    @param[in] var_name The string name of the data field
    @param[in,out] data A pointer to an array of data. Null on output if data does not exist.
    *
    * Data is associated with the name-entity combination. Both values must be valid.
   */
  void get_data(const Entity_kind on_what, const std::string var_name, T const **data) const {
    pair_t pr(var_name, on_what);
    auto iter = name_map_.find(pr);
    if (iter != name_map_.end()) {
      (*data) = (T const *)(&((*(state_[iter->second]))[0]));
    } else {
      (*data) = nullptr;
    }
  }

  /*!
    @brief Get the entity type on which the given field is defined
    @param[in] var_name The name of the data field
    @return The Entity_kind enum for the entity type on which the field is defined
   *
   * If the name has previously been associated with more than one entity, then the
   * entity that was most recently associated will be returned. To avoid this
   * ambiguity, please provide entity hints in the field name for yourself.
   *
   * This function is provided to make the class compatible with other state wrappers.
   */
  Entity_kind get_entity(std::string &var_name) {
    return entity_map_[var_name];
  }

  /*!
    @brief Get size for entity
  */
  size_t get_entity_size(Entity_kind ent) {
    return entity_size_map_[ent];
  }

  /*!
   * @brief Get index for entity and name
   */
  size_t get_vector_index(Entity_kind ent, std::string name) {
    pair_t pair(name, ent);
    return name_map_[pair];
  }

  /*!
    @brief Get the data vector
  */
  std::shared_ptr<std::vector<T>> get_vector(size_t index)
  {
    return state_[index]; 
  }

  /*!
    @brief Get gradients
  */
  std::shared_ptr<std::vector<Portage::Point3>> get_gradients(size_t index)
  {
    return gradients_[index];
  }

  /*!
    @brief Get the number of data vectors
  */
  size_t get_num_vectors() { return state_.size(); }

  /*!
    @brief Add a gradient field
  */
  void add_gradients(std::shared_ptr<std::vector<Portage::Point3>> new_grad)
  {
    if (new_grad->size() <= 0) return;
    gradients_.push_back(new_grad); 
  }

  /*! 
    @brief Get field stride
  */
  size_t get_field_stride(size_t index)
  {
    return 1;
  }

  /*!
    @brief Get the number of gradient vectors
  */
  size_t get_num_gradients() { return gradients_.size(); }

private:
  std::vector<std::shared_ptr<std::vector<T>>> state_;
  std::map<pair_t, size_t> name_map_;
  std::map<std::string, Entity_kind> entity_map_;
  std::map<Entity_kind, size_t> entity_size_map_;
  std::vector<std::shared_ptr<std::vector<Portage::Point3>>> gradients_;

  /*!
   * @brief Forget all internal data so initialization can start over
   */
  void clear(){
	  state_.clear();
	  name_map_.clear();
	  entity_map_.clear();
	  entity_size_map_.clear();
	  gradients_.clear();
  }

  /*!
   @brief Add a scalar data field
   @param[in] on_what The entity type on which the data is defined
   @param[in] var_name The name of the data field
   @param[in] value vector of data to add
   *
   * If data for name-entity combination already exists, then replace data.
   * Size of data must match previous size recorded for requested entity.
   * If entity has not been seen before, make that entity's size equal to that of data.
   */
  void add_data(const Entity_kind on_what, const std::string var_name, std::shared_ptr<std::vector<T>> data) {
	  // if we have seen this entity before - check size match, else store this size
	  auto sziter = entity_size_map_.find(on_what);
	  if (sziter != entity_size_map_.end()) {
		  if (sziter->second != data->size()) {
			  throw std::runtime_error(std::string("variable ")+var_name+" has incompatible size on add");
		  }
	  } else {
		  entity_size_map_[on_what] = data->size();
	  }

	  // store data and update internal book-keeping
	  pair_t pair(var_name, on_what);
	  auto iter = name_map_.find(pair);
	  if (iter == name_map_.end()) {  // have not seen this entity-name combo before, add data
		  state_.push_back(data);
		  name_map_[pair] = state_.size() - 1;
		  entity_map_[var_name] = on_what;
		  entity_size_map_[on_what] = data->size();
	  } else { // have seen the entity-name combo already, replace data. already checked size.
		  std::copy(data->begin(), data->end(), state_[iter->second]->begin());
	  }
  }
}; // Flat_State_Wrapper

} // namespace Portage

#endif // FLAT_STATE_WRAPPER_H_
