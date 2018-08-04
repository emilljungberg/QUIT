/*
 *  JSON.h
 *  Part of the QUantitative Image Toolbox
 *
 *  Copyright (c) 2016 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef QUIT_JSON_H
#define QUIT_JSON_H

#include <iostream>
#include <Eigen/Dense>
#include "rapidjson/document.h"

namespace QI {

rapidjson::Document ReadJSON(std::istream &is);
std::ostream &WriteJSON(std::ostream &os, const rapidjson::Document &doc);

rapidjson::Value ArrayToJSON(const Eigen::ArrayXd &, rapidjson::Document::AllocatorType &, const double &scale = 1);
Eigen::ArrayXd ArrayFromJSON(const rapidjson::Value &, const double &scale = 1);

} // End namespace QI

#endif // QUIT_JSON_H