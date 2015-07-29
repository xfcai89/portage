/*--------------------------------------------------------------------------~~*
 * Copyright (c) 2014 Los Alamos National Security, LLC
 * All rights reserved.
 *--------------------------------------------------------------------------~~*/

#include "search.h"

#include "Mesh.hh"

#include <cstdio>

namespace Portage {

void Search::search(const Jali::Mesh* inputMesh, const Jali::Mesh* targetMesh)
{
    std::printf("in Search::search(inputMesh, targetMesh)...\n");
    std::printf("  inputMesh::mesh_type(): %d", inputMesh->mesh_type());
    std::printf("  targetMesh::mesh_type(): %d", targetMesh->mesh_type());
} // Search::search

} // namespace Portage

/*-------------------------------------------------------------------------~--*
 * Formatting options for Emacs and vim.
 *
 * mode:c++
 * indent-tabs-mode:t
 * c-basic-offset:4
 * tab-width:4
 * vim: set tabstop=4 shiftwidth=4 expandtab :
 *-------------------------------------------------------------------------~--*/
