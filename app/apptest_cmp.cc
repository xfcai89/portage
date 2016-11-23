/*---------------------------------------------------------------------------~*
 * Copyright (c) 2014 Los Alamos National Security, LLC
 * All rights reserved.
 *---------------------------------------------------------------------------~*/
#include <sys/time.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cmath>
#include <iostream>
#include <stdexcept>

void print_usage() {
  std::printf("Usage: apptest_cmp file_gold file eps\n");
}

void load_field(std::iostream &s, std::vector<int> &gid,
                std::vector<double> &values) {
  int g;
  double v;
  while (s >> g >> v) {
    gid.push_back(g);
    values.push_back(v);
  }
}

int main(int argc, char** argv) {

  if (argc != 4) {
    print_usage();
    return 1;
  }
  double eps = std::stod(argv[3]);

  std::fstream f1(argv[1]), f2(argv[2]);
  if (!f1) throw std::runtime_error("First file cannot be opened.");
  if (!f2) throw std::runtime_error("Second file cannot be opened.");

  std::vector<int> gid1, gid2;
  std::vector<double> values1, values2;
  load_field(f1, gid1, values1);
  load_field(f2, gid2, values2);

  std::cout << std::scientific;
  std::cout.precision(17);
  std::cout << "Comparing files: " << argv[1] << " " << argv[2] << std::endl;
  std::cout << "Epsilon: " << eps << std::endl;
  std::cout << "Field sizes: " << gid1.size() << " " << gid2.size() <<
    std::endl;
  if (gid1.size() != gid2.size()) {
    throw std::runtime_error("The field sizes do not match.");
  }
  for (int i=0; i < gid1.size(); i++) {
    if (gid1[i] != gid2[i]) {
      std::cout << i << " " << gid1[i] << " " << gid2[i] << std::endl;
      throw std::runtime_error("The field global IDs do not match.");
    }
    if (std::abs(values1[i] - values2[i]) > eps) {
      std::cout << i << " " << values1[i] << " " << values2[i] << std::endl;
      throw std::runtime_error("The field values do not match.");
    }
  }

  return 0;
}
