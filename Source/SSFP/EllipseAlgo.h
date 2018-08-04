/*
 *  EllipseAlgo.h
 *
 *  Copyright (c) 2016, 2017 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef QI_ELLIPSE_ALGO_H
#define QI_ELLIPSE_ALGO_H

#include <memory>
#include <complex>
#include <array>
#include <vector>
#include <string>

#include "Eigen/Dense"

#include "Macro.h"
#include "ApplyTypes.h"
#include "Util.h"
#include "SSFPSequence.h"
#include "ApplyAlgorithmFilter.h"

namespace QI {

class EllipseAlgo : public QI::ApplyVectorXFVectorF::Algorithm {
protected:
    bool m_debug = false;
    const QI::SSFPEllipseSequence &m_seq;
    TOutput m_zero;
    virtual Eigen::ArrayXd apply_internal(const Eigen::ArrayXcf &input, const double flip, const double TR, const Eigen::ArrayXd &phi, const bool debug, float &residual) const = 0;
public:
    EllipseAlgo(const QI::SSFPEllipseSequence &seq, bool debug);

    size_t numInputs() const override { return 1; }
    size_t numConsts() const override { return 1; }
    size_t numOutputs() const override { return 5; }
    const std::vector<std::string> & names() const {
        static std::vector<std::string> _names = {"G", "a", "b", "theta_0", "phi_rf"};
        return _names;
    }
    size_t dataSize() const override { return m_seq.size(); }
    size_t outputSize() const override { return m_seq.FA.rows(); }
    std::vector<float> defaultConsts() const override {
        std::vector<float> def(1, 1.);
        return def;
    }
    TOutput zero() const override { return m_zero; }
    TStatus apply(const std::vector<TInput> &inputs, const std::vector<TConst> &consts,
               const TIndex &, // Unused
               std::vector<TOutput> &outputs, TOutput &residual,
               TInput &resids, TIterations &its) const override;
};

} // End namespace QI

#endif // QI_ELLIPSE_ALGO_H