#!/usr/bin/env bash
# This script is executed on Jenkins using
#
#     $WORKSPACE/jenkins/build_matrix_entry.sh <compiler> <build_type>
#
# The exit code determines if the test succeeded or failed.
# Note that the environment variable WORKSPACE must be set (Jenkins
# will do this automatically).

# Exit on error
set -e
# Echo each command
set -x

compiler=$1
build_type=$2

# set modules and install paths

jali_version=0.9.8
tangram_version=133c1db580f
xmof2d_version=0.9

export NGC=/usr/local/codes/ngc
ngc_include_dir=$NGC/private/include

# compiler-specific settings
if [[ $compiler == "intel" ]]; then
  cxxmodule=intel/17.0.1
  openmpi_version=1.10.5
  jali_install_dir=$NGC/private/jali/${jali_version}-intel-17.0.1-openmpi-${openmpi_version}
  tangram_install_dir=$NGC/private/tangram/${tangram_version}-intel-17.0.1-openmpi-${openmpi_version}
  xmof2d_install_dir=$NGC/private/xmof2d/${xmof2d_version}-intel-17.0.1-openmpi-${opempi_version}
elif [[ $compiler == "gcc" ]]; then
  cxxmodule=gcc/5.3.0
  openmpi_version=1.10.3
  jali_install_dir=$NGC/private/jali/${jali_version}-gcc-5.3.0-openmpi-${openmpi_version}
  flecsi_install_prefix=$NGC/private/flecsi/gcc5.3_openmpi1.10.3
  flecsisp_install_prefix=$NGC/private/flecsi-sp/gcc5.3_openmpi1.10.3
  tangram_install_dir=$NGC/private/tangram/${tangram_version}-gcc-5.3.0-openmpi-${openmpi_version}
  xmof2d_install_dir=$NGC/private/xmof2d/${xmof2d_version}-gcc-5.3.0-openmpi-${opempi_version}
fi
  
cmake_build_type=Release
extra_flags=
jali_flags="-D Jali_DIR:FILEPATH=$jali_install_dir/lib"
tangram_flags="-D TANGRAM_DIR:FILEPATH=$tangram_install_dir"
xmof2d_flags="-D XMOF2D_DIR:FILEPATH=$xmof2d_install_dir/share/cmake"
mpi_flags="-D ENABLE_MPI=True"

if [[ $build_type == "debug" ]]; then
  cmake_build_type=Debug
elif [[ $build_type == "serial" ]]; then
  mpi_flags=
  # jali is not available in serial
  jali_flags=
elif [[ $build_type == "thrust" ]]; then
  extra_flags="-D ENABLE_THRUST=True"
elif [[ $build_type == "flecsi" ]]; then
  extra_flags="-D CMAKE_PREFIX_PATH='$flecsi_install_prefix;$flecsisp_install_prefix' \
               -D ENABLE_FleCSI=True"
elif [[ $build_type == "coverage" ]]; then
  extra_flags="-D CMAKE_C_FLAGS='-coverage' \
               -D CMAKE_CXX_FLAGS='-coverage'"
  cmake_build_type=Debug
  export PATH=$NGC/private/bin:${PATH}
fi

export SHELL=/bin/sh

export MODULEPATH=""
. /opt/local/packages/Modules/default/init/sh
module load $cxxmodule
module load cmake # 3.0 or higher is required

if [[ -n "$mpi_flags" ]] ; then
  module load openmpi/${openmpi_version}
  mpi_flags+=" -D CMAKE_C_COMPILER=`which mpicc` \
               -D CMAKE_CXX_COMPILER=`which mpiCC`"
fi

echo $WORKSPACE
cd $WORKSPACE

mkdir build
cd build

cmake \
  -D CMAKE_BUILD_TYPE=$cmake_build_type \
  -D ENABLE_UNIT_TESTS=True \
  -D ENABLE_APP_TESTS=True \
  -D ENABLE_JENKINS_OUTPUT=True \
  -D NGC_INCLUDE_DIR:FILEPATH=$ngc_include_dir \
  $mpi_flags \
  $jali_flags \
  $tangram_flags \
  $xmof2d_flags \
  $extra_flags \
  ..
make -j2
ctest --output-on-failure

if [[ $build_type == "coverage" ]]; then
  gcovr -r .. -x  >coverage.xml
fi

