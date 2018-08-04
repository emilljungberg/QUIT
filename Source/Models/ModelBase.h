/*
 *  ModelBase.h
 *
 *  Copyright (c) 2018 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef MODEL_BASE_H
#define MODEL_BASE_H

#include <string>
#include <vector>

#include <Eigen/Core>

#include "SignalEquations.h"

namespace QI {
namespace Model {

//cdbl and carrd already typedef'd in SignalEquations
typedef const Eigen::VectorXd cvecd;

enum class FieldStrength { Three, Seven, User };

class ModelBase {
public:

protected:
    bool m_scale_to_mean = false;
    Eigen::ArrayXcd scale(const Eigen::ArrayXcd &signal) const;
    Eigen::ArrayXd  scale_mag(const Eigen::ArrayXd  &signal) const;

public:
    virtual std::string Name() const = 0;
    virtual size_t nParameters() const = 0;
    virtual bool ValidParameters(cvecd &params) const = 0;
    virtual const std::vector<std::string> &ParameterNames() const = 0;
    ptrdiff_t ParameterIndex(const std::string &parameter) const;
    virtual Eigen::ArrayXXd Bounds(const FieldStrength f) const = 0;
    virtual Eigen::ArrayXd Default(const FieldStrength f = FieldStrength::Three) const = 0;

    bool scaleToMean() const { return m_scale_to_mean; }
    void setScaleToMean(bool s) { m_scale_to_mean = s; }

    virtual Eigen::VectorXd SSFPEchoMagnitude(cvecd &params, carrd &a, cdbl TR, carrd &phi) const;

    virtual Eigen::VectorXcd MultiEcho(cvecd &params, carrd &TE, cdbl TR) const;
    virtual Eigen::VectorXcd SPGR(cvecd &params, carrd &a, cdbl TR) const;
    virtual Eigen::VectorXcd SPGREcho(cvecd &p, carrd& a, cdbl TR, cdbl TE) const;
    virtual Eigen::VectorXcd SPGRFinite(cvecd &params, carrd &a, cdbl TR, cdbl T_rf, cdbl TE) const;
    virtual Eigen::VectorXcd SPGR_MT(cvecd &p, carrd &satflip, carrd &satf0, cdbl flip, cdbl TR, cdbl Trf) const;
    virtual Eigen::VectorXcd MPRAGE(cvecd &params, cdbl a, cdbl TR, const int Nseg, const int Nk0, cdbl eta, cdbl TI, cdbl TD) const;
    virtual Eigen::VectorXcd AFI(cvecd &params, cdbl a, cdbl TR1, cdbl TR2) const;
    virtual Eigen::VectorXcd SSFP(cvecd &params, carrd &a, cdbl TR, carrd &phi) const;
    virtual Eigen::VectorXcd SSFPEcho(cvecd &params, carrd &a, cdbl TR, carrd &phi) const;
    virtual Eigen::VectorXcd SSFP_GS(cvecd &params, carrd &a, cdbl TR) const;
    virtual Eigen::VectorXcd SSFPFinite(cvecd &params, carrd &a, cdbl TR, cdbl T_rf, carrd &phi) const;
};

#define DECLARE_MODEL_INTERFACE( )\
public:\
    std::string Name() const override;\
    size_t nParameters() const override;\
    bool ValidParameters(cvecd &p) const override;\
    const std::vector<std::string> &ParameterNames() const override;\
    Eigen::ArrayXXd Bounds(const FieldStrength f) const override;\
    Eigen::ArrayXd Default(const FieldStrength f = FieldStrength::Three) const override;\

} // End namespace Model
} // End namespace QI

#endif // MODEL_BASE_H
