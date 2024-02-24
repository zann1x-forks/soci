//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#define SOCI_SOURCE
#include "soci/type-holder.h"

using namespace soci;
using namespace soci::details;

holder::holder(db_type dt_) : dt(dt_) {
  switch (dt) {
  case soci::db_blob:
  case soci::db_xml:
  case soci::db_string:
    new (&val.s) std::string();
  default:
    break;
  }
}

holder::~holder() {
  switch (dt) {
  case soci::db_blob:
  case soci::db_xml:
  case soci::db_string:
    val.s.~basic_string();
  default:
    break;
  }
}
