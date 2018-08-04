/*
 *  JSON.cpp
 *  Part of the QUantitative Image Toolbox
 *
 *  Copyright (c) 2016 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "JSON.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/error/en.h"
#include "Macro.h"

using namespace std::string_literals;

namespace QI {

rapidjson::Document ReadJSON(std::istream &is) {
    rapidjson::IStreamWrapper isw(is);
    rapidjson::Document doc;
    doc.ParseStream(isw);
    if (doc.HasParseError()) {
        QI_FAIL("JSON Error(offset "s << (unsigned)doc.GetErrorOffset() << "): " << 
                rapidjson::GetParseError_En(doc.GetParseError()));
    }
    return doc;
}

std::ostream &WriteJSON(std::ostream &os, const rapidjson::Document &doc) {
    rapidjson::OStreamWrapper osw(os);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    doc.Accept(writer);
    os << std::endl;
    return os;
}

rapidjson::Value ArrayToJSON(const Eigen::ArrayXd &array,
                            rapidjson::Document::AllocatorType &allocator,
                            const double &scale) {
    rapidjson::Value json_array(rapidjson::kArrayType);
    for (Eigen::Index i = 0; i < array.rows(); i++) {
        double scaled_value = array[i] * scale;
        json_array.PushBack(scaled_value, allocator);
    }
    return json_array;
}

Eigen::ArrayXd ArrayFromJSON(const rapidjson::Value &json_array, const double &scale) {
        if (!json_array.IsArray()) QI_FAIL("Failed to read JSON array");
        Eigen::ArrayXd array(json_array.Size());
        for (rapidjson::SizeType i = 0; i < json_array.Size(); i++) {
            array[i] = json_array[i].GetDouble() * scale;
        }
        return array;
}

} // End namespace QI
