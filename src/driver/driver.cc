/*--------------------------------------------------------------------------~~*
 * Copyright (c) 2014 Los Alamos National Security, LLC
 * All rights reserved.
 *--------------------------------------------------------------------------~~*/

#include "driver.h"

#include <cstdio>

#include "search.h"
#include "intersect.h"
#include "remap.h"

#include "Mesh.hh"
#include "MeshFactory.hh"


namespace NGC {
namespace Remap {

void Driver::run()
{
    std::printf("in Driver::run()...\n");

    // Search for possible intersections.
    Search s;
    s.search(0.0, 0.0);

    // Calculate the overlap of actual intersections.
    Intersect isect;
    isect.intersect();

    // Remap from inputMesh_ to targetMesh_
    Remap r;
    r.remap();

} // Driver::run

} // namespace Remap
} // namespace NGC

/*-------------------------------------------------------------------------~--*
 * Formatting options for Emacs and vim.
 *
 * mode:c++
 * indent-tabs-mode:t
 * c-basic-offset:4
 * tab-width:4
 * vim: set tabstop=4 shiftwidth=4 expandtab :
 *-------------------------------------------------------------------------~--*/
