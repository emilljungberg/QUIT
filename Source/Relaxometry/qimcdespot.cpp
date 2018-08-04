/*
 *  qmcdespot.cpp
 *
 *  Created by Tobias Wood on 03/06/2015.
 *  Copyright (c) 2015 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <fstream>
#include <Eigen/Dense>
#include <unsupported/Eigen/LevenbergMarquardt>
#include <unsupported/Eigen/NumericalDiff>

#include "itkTimeProbe.h"

#include "ApplyTypes.h"
#include "Util.h"
#include "Args.h"
#include "ImageIO.h"
#include "IO.h"
#include "Models.h"
#include "SequenceGroup.h"
#include "RegionContraction.h"

struct MCDSRCFunctor {
    const QI::SequenceGroup &m_sequence;
    const Eigen::ArrayXd m_data, m_weights;
    const std::shared_ptr<QI::Model::ModelBase> m_model;

    MCDSRCFunctor(std::shared_ptr<QI::Model::ModelBase> m, QI::SequenceGroup &s,
                  const Eigen::ArrayXd &d, const Eigen::ArrayXd &w) :
        m_sequence(s), m_data(d), m_weights(w), m_model(m)
    {
        assert(static_cast<size_t>(m_data.rows()) == m_sequence.size());
    }

    int inputs() const { return m_model->nParameters(); }
    int values() const { return m_sequence.size(); }

    bool constraint(const Eigen::VectorXd &params) const {
        return m_model->ValidParameters(params);
    }

    Eigen::ArrayXd residuals(const Eigen::Ref<Eigen::VectorXd> &params) const {
        const Eigen::ArrayXd s = (m_sequence.signal(m_model, params)).abs();
        return m_data - s;
    }
    double operator()(const Eigen::Ref<Eigen::VectorXd> &params) const {
        return (residuals(params) * m_weights).square().sum();
    }
};

struct SRCAlgo : public QI::ApplyF::Algorithm {
    Eigen::ArrayXXd m_bounds;
    std::shared_ptr<QI::Model::ModelBase> m_model = nullptr;
    QI::SequenceGroup &m_sequence;
    QI::Model::FieldStrength m_tesla = QI::Model::FieldStrength::Three;
    int m_iterations = 0;
    size_t m_samples = 5000, m_retain = 50;
    bool m_gauss = true;

    SRCAlgo(std::shared_ptr<QI::Model::ModelBase>&m, Eigen::ArrayXXd &b,
            QI::SequenceGroup &s, int mi) :
        m_bounds(b), m_model(m), m_sequence(s), m_iterations(mi)
    {}

    size_t numInputs() const override  { return m_sequence.count(); }
    size_t numOutputs() const override { return m_model->nParameters(); }
    size_t dataSize() const override   { return m_sequence.size(); }

    void setModel(std::shared_ptr<QI::Model::ModelBase> &m) { m_model = m; }
    void setSequence(QI::SequenceGroup &s) { m_sequence = s; }
    void setBounds(Eigen::ArrayXXd &b) { m_bounds = b; }
    void setIterations(const int i) { m_iterations = i; }
    float zero() const override { return 0.f; }

    void setGauss(bool g) { m_gauss = g; }
    size_t numConsts() const override  { return 2; }
    std::vector<float> defaultConsts() const override {
        std::vector<float> def(2);
        def[0] = NAN; def[1] = 1.0f; // f0, B1
        return def;
    }

    TStatus apply(const std::vector<TInput> &inputs, const std::vector<TConst> &consts,
               const TIndex &, // Unused
               std::vector<TOutput> &outputs, TConst &residual,
               TInput &resids, TIterations &its) const override
    {
        Eigen::ArrayXd data(dataSize());
        int dataIndex = 0;
        for (size_t i = 0; i < inputs.size(); i++) {
            Eigen::Map<const Eigen::ArrayXf> this_data(inputs[i].GetDataPointer(), inputs[i].Size());
            if (m_model->scaleToMean()) {
                data.segment(dataIndex, this_data.rows()) = this_data.cast<double>() / this_data.abs().mean();
            } else {
                data.segment(dataIndex, this_data.rows()) = this_data.cast<double>();
            }
            dataIndex += this_data.rows();
        }
        Eigen::ArrayXd thresh(m_model->nParameters()); thresh.setConstant(0.05);
        const double f0 = consts[0];
        const double B1 = consts[1];
        Eigen::ArrayXXd localBounds = m_bounds;
        Eigen::ArrayXd weights = Eigen::ArrayXd::Ones(m_sequence.size());
        if (std::isfinite(f0)) { // We have an f0 map, add it to the fitting bounds
            localBounds.row(m_model->ParameterIndex("f0")) += f0;
            weights = m_sequence.weights(f0);
        }
        localBounds.row(m_model->ParameterIndex("B1")).setConstant(B1);
        MCDSRCFunctor func(m_model, m_sequence, data, weights);
        QI::RegionContraction<MCDSRCFunctor> rc(func, localBounds, thresh, m_samples, m_retain, m_iterations, 0.02, m_gauss, false);
        Eigen::ArrayXd pars(m_model->nParameters());
        if (!rc.optimise(pars)) {
            return std::make_tuple(false, "Region contraction failed");
        }
        
        for (size_t i = 0; i < m_model->nParameters(); i++) {
            outputs[i] = pars[i];
        }
        Eigen::ArrayXf r = func.residuals(pars).cast<float>();
        residual = sqrt(r.square().sum() / r.rows());
        resids = itk::VariableLengthVector<float>(r.data(), r.rows());
        its = rc.contractions();
        return std::make_tuple(true, "");
    }
};

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv) {
    Eigen::initParallel();
    args::ArgumentParser parser("Calculates MWF & other parameter maps from mcDESPOT data\n"
                                "All times (e.g. T1, TR) are in SECONDS. All angles are in degrees.\n"
                                "http://github.com/spinicist/QUIT");
    args::PositionalList<std::string> input_paths(parser, "INPUT FILES", "Input image files");
    args::HelpFlag help(parser, "HELP", "Show this help message", {'h', "help"});
    args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
    args::ValueFlag<int> threads(parser, "THREADS", "Use N threads (default=4, 0=hardware limit)", {'T', "threads"}, 4);
    args::ValueFlag<std::string> outarg(parser, "OUTPREFIX", "Add a prefix to output filenames", {'o', "out"});
    args::ValueFlag<std::string> f0(parser, "f0", "f0 map (Hertz)", {'f', "f0"});
    args::ValueFlag<std::string> B1(parser, "B1", "B1 map (ratio)", {'b', "B1"});
    args::ValueFlag<std::string> mask(parser, "MASK", "Only process voxels within the mask", {'m', "mask"});
    args::ValueFlag<std::string> subregion(parser, "SUBREGION", "Process subregion starting at voxel I,J,K with size SI,SJ,SK", {'s', "subregion"});
    args::Flag resids(parser, "RESIDS", "Write out residuals for each data-point", {'r', "resids"});
    args::ValueFlag<std::string> modelarg(parser, "MODEL", "Select model to fit - 1/2/2nex/3/3_f0/3nex, default 3", {'M', "model"}, "3");
    args::Flag scale(parser, "SCALE", "Normalize signals to mean (a good idea)", {'S', "scale"});
    args::ValueFlag<char> algorithm(parser, "ALGO", "Select (S)tochastic or (G)aussian Region Contraction", {'a', "algo"}, 'G');
    args::ValueFlag<int> its(parser, "ITERS", "Max iterations, default 4", {'i',"its"}, 4);
    args::ValueFlag<char> field(parser, "FIELD STRENGTH", "Specify field-strength for fitting regions - 3/7/u for user input", {'t', "tesla"}, '3');
    QI::ParseArgs(parser, argc, argv, verbose);

    std::vector<QI::VectorVolumeF::Pointer> images;
    for (auto &input_path : QI::CheckList(input_paths)) {
        QI_LOG(verbose, "Reading file: " << input_path );
        auto image = QI::ReadVectorImage<float>(input_path);
        image->DisconnectPipeline(); // This step is really important.
        images.push_back(image);
    }
    rapidjson::Document input = QI::ReadJSON(std::cin);
    QI::SequenceGroup sequences(input["Sequences"]);
    if (sequences.count() != images.size()) {
        QI_FAIL("Sequence group size " << sequences.count() << " does not match images size " << images.size());
    }

    std::shared_ptr<QI::Model::ModelBase> model = nullptr;
    if (modelarg.Get() == "1")         { model = std::make_shared<QI::Model::OnePool>(); }
    else if (modelarg.Get() == "2")    { model = std::make_shared<QI::Model::TwoPool>(); }
    else if (modelarg.Get() == "2nex") { model = std::make_shared<QI::Model::TwoPool_NoExchange>(); }
    else if (modelarg.Get() == "3")    { model = std::make_shared<QI::Model::ThreePool>(); }
    else if (modelarg.Get() == "3_f0") { model = std::make_shared<QI::Model::ThreePool_f0>(); }
    else if (modelarg.Get() == "3nex") { model = std::make_shared<QI::Model::ThreePool_NoExchange>(); }
    else {
        QI_FAIL("Invalid model " << modelarg.Get() << " specified.");
    }
    QI_LOG(verbose, "Using " << model->Name() << " model.");
    if (scale) {
        QI_LOG(verbose, "Mean-scaling selected");
        model->setScaleToMean(scale);
    }

    Eigen::ArrayXXd bounds = model->Bounds(QI::Model::FieldStrength::Three);
    Eigen::ArrayXd start = model->Default(QI::Model::FieldStrength::Three);
    switch (field.Get()) {
        case '3':
            bounds = model->Bounds(QI::Model::FieldStrength::Three);
            start = model->Default(QI::Model::FieldStrength::Three);
            break;
        case '7':
            bounds = model->Bounds(QI::Model::FieldStrength::Seven);
            start = model->Bounds(QI::Model::FieldStrength::Seven);
            break;
        case 'u': {
            auto lower_bounds = QI::ArrayFromJSON(input["lower_bounds"]);
            auto upper_bounds = QI::ArrayFromJSON(input["upper_bounds"]);
            bounds.col(0) = lower_bounds;
            bounds.col(1) = upper_bounds;
        } break;
    default:
        QI_FAIL("Unknown boundaries type " << field.Get());
    }

    auto apply = QI::ApplyF::New();
    switch (algorithm.Get()) {
        case 'S': {
            QI_LOG(verbose, "Using SRC algorithm" );
            std::shared_ptr<SRCAlgo> algo = std::make_shared<SRCAlgo>(model, bounds, sequences, its.Get());
            algo->setGauss(false);
            apply->SetAlgorithm(algo);
        } break;
        case 'G': {
            QI_LOG(verbose, "Using GRC algorithm" );
            std::shared_ptr<SRCAlgo> algo = std::make_shared<SRCAlgo>(model, bounds, sequences, its.Get());
            algo->setGauss(true);
            apply->SetAlgorithm(algo);
        } break;
        default:
            QI_FAIL("Unknown algorithm type " << algorithm.Get());
    }
    apply->SetOutputAllResiduals(resids);
    apply->SetVerbose(verbose);
    apply->SetPoolsize(threads.Get());
    apply->SetSplitsPerThread(threads.Get() < 8 ? 8 : threads.Get()); // mcdespot with a mask & threads is a very unbalanced algorithm
    for (size_t i = 0; i < images.size(); i++) {
        apply->SetInput(i, images[i]);
    }
    if (f0) apply->SetConst(0, QI::ReadImage(f0.Get()));
    if (B1) apply->SetConst(1, QI::ReadImage(B1.Get()));
    if (mask) apply->SetMask(QI::ReadImage(mask.Get()));
    if (subregion) apply->SetSubregion(QI::RegionArg(args::get(subregion)));

    // Need this here so the bounds.txt file will have the correct prefix
    std::string outPrefix = outarg.Get() + model->Name() + "_";
    std::ofstream boundsFile(outPrefix + "bounds.txt");
    boundsFile << "Names: ";
    for (size_t p = 0; p < model->nParameters(); p++) {
        boundsFile << model->ParameterNames()[p] << "\t";
    }
    boundsFile << std::endl << "Bounds:\n" << bounds.transpose() << std::endl;
    boundsFile.close();
    QI_LOG(verbose, "Bounds:\n" <<  bounds.transpose());
    QI_LOG(verbose, "Processing");
    if (verbose) {
        auto monitor = QI::GenericMonitor::New();
        apply->AddObserver(itk::ProgressEvent(), monitor);
    }
    apply->Update();
    QI_LOG(verbose, "Elapsed time was " << apply->GetTotalTime() << "s" <<
                    "Writing results files.");
    for (size_t i = 0; i < model->nParameters(); i++) {
        QI::WriteImage(apply->GetOutput(i), outPrefix + model->ParameterNames()[i] + QI::OutExt());
    }
    QI::WriteImage(apply->GetResidualOutput(), outPrefix + "residual" + QI::OutExt());
    if (resids) {
        QI::WriteVectorImage(apply->GetAllResidualsOutput(), outPrefix + "all_residuals" + QI::OutExt());
    }
    QI::WriteImage(apply->GetIterationsOutput(), outPrefix + "iterations" + QI::OutExt());
    return EXIT_SUCCESS;
}
