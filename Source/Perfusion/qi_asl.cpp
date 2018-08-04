/*
 *  qi_asl.cpp
 *
 *  Copyright (c) 2018 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include <Eigen/Dense>

#include "Util.h"
#include "ImageIO.h"
#include "Args.h"
#include "ApplyTypes.h"
#include "CASLSequence.h"

class CASLAlgo : public QI::ApplyVectorF::Algorithm {
protected:
    const QI::CASLSequence m_CASL;
    const double m_T1, m_alpha, m_lambda;
    const int m_inputsize, m_series_size;
    const bool m_average_timeseries;
public:
    CASLAlgo(const QI::CASLSequence& casl,
             const double T1, const double alpha, const double lambda,
             const int inputsize, const bool average, const bool slice_time) :
        m_CASL(casl), m_T1(T1), m_alpha(alpha), m_lambda(lambda),
        m_inputsize(inputsize), m_series_size(inputsize/2),
        m_average_timeseries(average)
    {
        if (!slice_time && (casl.post_label_delay.rows() != 1)) {
            QI_FAIL("More than one post-label delay specified, but not in slice-timing correction mode");
        }
    }

    size_t numInputs() const override  { return 1; }
    size_t numConsts() const override  { return 2; }
    size_t numOutputs() const override { return 1; }
    size_t dataSize() const override   { return m_inputsize; }
    size_t outputSize() const override {
        if (m_average_timeseries) {
            return 1;
        } else {
            return m_series_size;
        }
    }
    TOutput zero() const override {
        TOutput z;
        if (m_average_timeseries) {
            z.SetSize(1);
        } else {
            z.SetSize(m_series_size);
        }
        z.Fill(0.);
        return z;
    }

    std::vector<float> defaultConsts() const override {
        std::vector<float> def(2, 0);
        return def;
    }

    TStatus apply(const std::vector<TInput> &inputs, const std::vector<TConst> &consts,
               const TIndex &index, // Unused
               std::vector<TOutput> &outputs, TOutput &residual,
               TInput &resids, TIterations &its) const override
    {
        const Eigen::Map<const Eigen::ArrayXf, 0, Eigen::InnerStride<>> even(inputs[0].GetDataPointer(), m_series_size, Eigen::InnerStride<>(2));
        const Eigen::Map<const Eigen::ArrayXf, 0, Eigen::InnerStride<>> odd(inputs[0].GetDataPointer() + 1, m_series_size, Eigen::InnerStride<>(2));

        const Eigen::ArrayXd diff = (odd.cast<double>() - even.cast<double>());
        Eigen::ArrayXd SI_PD = odd.cast<double>();
        double T1_tissue = consts[0];
        Eigen::ArrayXd PD;
        if (consts[1] == 0) {
            PD = odd.cast<double>();
        } else {
            PD = Eigen::ArrayXd::Constant(m_series_size, consts[1]);
        }
        if (T1_tissue > 0) {
            PD /= (1 - exp(-m_CASL.TR / T1_tissue));
        }
        const double PLD = (m_CASL.post_label_delay.rows() > 1) ? m_CASL.post_label_delay[index[2]] : m_CASL.post_label_delay[0];
        const Eigen::ArrayXd CBF = (6000 * m_lambda * diff * exp(PLD / m_T1)) / 
                           (2. * m_alpha * m_T1 * PD * (1. - exp(-m_CASL.label_time / m_T1)));
        if (m_average_timeseries) {
            outputs[0][0] = CBF.mean();
        } else {
            for (int i = 0; i < m_series_size; i++) {
                outputs[0][i] = CBF[i];
            }
        }
        residual.Fill(0.);
        resids.Fill(0.);
        its = 0;
        return std::make_tuple(true, "");
    }
};

/*
 * Main
 */
int main(int argc, char **argv) {
    Eigen::initParallel();
    args::ArgumentParser parser("Calculates CBF from ASL data.\nhttp://github.com/spinicist/QUIT");
    args::Positional<std::string> input_path(parser, "ASL_FILE", "Input ASL file");
    args::HelpFlag help(parser, "HELP", "Show this help message", {'h', "help"});
    args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
    args::ValueFlag<int> threads(parser, "THREADS", "Use N threads (default=4, 0=hardware limit)", {'T', "threads"}, 4);
    args::ValueFlag<std::string> outarg(parser, "OUTPREFIX", "Add a prefix to output filename", {'o', "out"});
    args::ValueFlag<std::string> mask(parser, "MASK", "Only process voxels within the mask", {'m', "mask"});
    args::Flag average(parser, "AVERAGE", "Average the time-series", {'a', "average"});
    args::Flag slice_time(parser, "SLICE TIME CORRECTION", "Apply slice-time correction (number of post-label delays must match number of slices)", {'s', "slicetime"});
    args::ValueFlag<double> T1_blood(parser, "BLOOD T1", "Value of blood T1 to use (seconds), default 1.65 for 3T", {'b', "blood"}, 1.65);
    args::ValueFlag<std::string> T1_tissue_path(parser, "TISSUE T1", "Path to tissue T1 map (units are seconds)", {'t', "tissue"});
    args::ValueFlag<std::string> PD_path(parser, "PROTON DENSITY", "Path to PD image", {'p', "pd"});
    args::ValueFlag<double> alpha(parser, "ALPHA", "Labelling efficiency, default 0.9", {'a', "alpha"}, 0.9);
    args::ValueFlag<double> lambda(parser, "LAMBDA", "Blood-brain partition co-efficent, default 0.9 mL/g", {'l', "lambda"}, 0.9);
    args::ValueFlag<std::string> subregion(parser, "SUBREGION", "Process subregion starting at voxel I,J,K with size SI,SJ,SK", {'s', "subregion"});
    QI::ParseArgs(parser, argc, argv, verbose);
    QI_LOG(verbose, "Reading ASL data from: " << QI::CheckPos(input_path));
    auto input = QI::ReadVectorImage(QI::CheckPos(input_path));

    QI::VolumeF::Pointer PD_image = ITK_NULLPTR;
    if (PD_path) {
        QI_LOG(verbose, "Reading proton density map: " << PD_path.Get());
        PD_image = QI::ReadImage(PD_path.Get());
    }

    QI::VolumeF::Pointer T1_tissue = ITK_NULLPTR;
    if (T1_tissue_path) {
        QI_LOG(verbose, "Reading tissue T1 map: " << T1_tissue_path.Get());
        T1_tissue = QI::ReadImage(T1_tissue_path.Get());
    }
    rapidjson::Document json = QI::ReadJSON(std::cin);
    QI::CASLSequence sequence(json["CASL"]);
    std::cout << sequence.post_label_delay.transpose() << std::endl;
    const auto n_slices = input->GetLargestPossibleRegion().GetSize()[2];
    if (slice_time && (n_slices != static_cast<size_t>(sequence.post_label_delay.rows()))) {
        QI_FAIL("Number of post-label delays " << sequence.post_label_delay.rows() << " does not match number of slices " << n_slices);
    }
    std::shared_ptr<CASLAlgo> algo = std::make_shared<CASLAlgo>(sequence, T1_blood.Get(), alpha.Get(), lambda.Get(),
                                                                input->GetNumberOfComponentsPerPixel(), average, slice_time);
    auto apply = QI::ApplyVectorF::New();
    apply->SetVerbose(verbose);
    apply->SetAlgorithm(algo);
    apply->SetConst(0, T1_tissue);
    apply->SetConst(1, PD_image);
    apply->SetOutputAllResiduals(false);
    QI_LOG(verbose, "Using " << threads.Get() << " threads" );
    apply->SetPoolsize(threads.Get());
    apply->SetSplitsPerThread(threads.Get());
    apply->SetInput(0, input);
    if (mask) apply->SetMask(QI::ReadImage(mask.Get()));
    if (subregion) {
        apply->SetSubregion(QI::RegionArg(args::get(subregion)));
    }
    QI_LOG(verbose, "Processing");
    if (verbose) {
        auto monitor = QI::GenericMonitor::New();
        apply->AddObserver(itk::ProgressEvent(), monitor);
    }
    apply->Update();
    QI_LOG(verbose, "Elapsed time was " << apply->GetTotalTime() << "s" <<
                    "Writing results files.");
    const std::string outPrefix = outarg ? outarg.Get() : QI::Basename(input_path.Get());
    QI::WriteVectorImage(apply->GetOutput(0), outPrefix + "_CBF" + QI::OutExt());
    return EXIT_SUCCESS;
}
