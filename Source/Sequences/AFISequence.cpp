/*
 *  AFI.cpp
 *
 *  Copyright (c) 2018 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "AFISequence.h"
#include "Macro.h"

namespace QI {

Eigen::ArrayXcd AFISequence::signal(std::shared_ptr<Model::ModelBase> m, const Eigen::VectorXd &p) const {
    return m->AFI(p, FA, TR1, TR2);
}

AFISequence::AFISequence(const rapidjson::Value &json) {
    if (json.IsNull()) QI_FAIL("Could not read sequence: " << name());
    TR1 = json["TR1"].GetDouble();
    TR2 = json["TR2"].GetDouble();
    FA = json["FA"].GetDouble() * M_PI / 180;
}

rapidjson::Value AFISequence::toJSON(rapidjson::Document::AllocatorType &a) const {
    rapidjson::Value json(rapidjson::kObjectType);
    json.AddMember("TR1", TR1, a);
    json.AddMember("TR2", TR2, a);
    json.AddMember("FA", FA * 180 / M_PI, a);
    return json;
}

} // End namespace QI
