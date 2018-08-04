/*
 *  qimp2rage.cpp
 *
 *  Created by Tobias Wood on 2015/08/24.
 *  Copyright (c) 2015 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include <string>
#include <complex>

#include "itkBinaryFunctorImageFilter.h"
#include "itkExtractImageFilter.h"
#include "itkMaskImageFilter.h"
#include "itkAddImageFilter.h"

#include "ApplyAlgorithmFilter.h"
#include "ImageTypes.h"
#include "Util.h"
#include "ImageIO.h"
#include "Args.h"
#include "MPRAGESequence.h"
#include "Masking.h"

template<class T> class MP2Functor {
public:
    MP2Functor() {}
    ~MP2Functor() {}
    bool operator!=(const MP2Functor &) const { return false; }
    bool operator==(const MP2Functor &other) const { return !(*this != other); }

    inline T operator()(const std::complex<T> &ti1, const std::complex<T> &ti2) const
    {
        const T a1 = abs(ti1);
        const T a2 = abs(ti2);
        return real((conj(ti1)*ti2)/(a1*a1 + a2*a2));
    }
};

template<class T> class SqrSumFunctor {
public:
    SqrSumFunctor() {}
    ~SqrSumFunctor() {}
    bool operator!=(const SqrSumFunctor &) const { return false; }
    bool operator==(const SqrSumFunctor &other) const { return !(*this != other); }

    inline T operator()(const std::complex<T> &ti1, const std::complex<T> &ti2) const
    {
        const T a1 = abs(ti1);
        const T a2 = abs(ti2);
        return a1*a1 + a2*a2;
    }
};

namespace itk {

class MPRAGELookUpFilter : public ImageToImageFilter<QI::VolumeF, QI::VolumeF>
{
public:

protected:
    std::vector<float> m_T1, m_con;

public:
    /** Standard class typedefs. */
    typedef QI::VolumeF                        TImage;
    typedef MPRAGELookUpFilter                 Self;
    typedef ImageToImageFilter<TImage, TImage> Superclass;
    typedef SmartPointer<Self>                 Pointer;
    typedef typename TImage::RegionType        RegionType;

    itkNewMacro(Self); /** Method for creation through the object factory. */
    itkTypeMacro(MPRAGELookUpFilter, ImageToImageFilter); /** Run-time type information (and related methods). */

    void SetInput(const TImage *img) ITK_OVERRIDE {
        this->SetNthInput(0, const_cast<TImage*>(img));
    }

    void SetSequence(QI::MP2RAGESequence &sequence) {
        MP2Functor<double> con;
        m_T1.clear();
        m_con.clear();
        for (float T1 = 0.25; T1 < 4.3; T1 += 0.001) {
            Eigen::Array3d tp; tp << T1, 1.0, 1.0; // Fix B1 and eta
            Eigen::Array2cd sig = sequence.signal(1., T1, 1.0, 1.0);
            double c = con(sig[0], sig[1]);
            m_T1.push_back(T1);
            m_con.push_back(c);
            //cout << m_pars.back().transpose() << " : " << m_cons.back().transpose());
        }
    }

protected:
    MPRAGELookUpFilter() {
        this->SetNumberOfRequiredInputs(1);
        this->SetNumberOfRequiredOutputs(1);
        this->SetNthOutput(0, this->MakeOutput(0));
    }
    ~MPRAGELookUpFilter() {}

    DataObject::Pointer MakeOutput(ProcessObject::DataObjectPointerArraySizeType idx) override {
        //std::cout <<  __PRETTY_FUNCTION__ );
        if (idx == 0) {
            DataObject::Pointer output = (TImage::New()).GetPointer();
            return output.GetPointer();
        } else {
            std::cerr << "No output " << idx << std::endl;
            return NULL;
        }
    }

    void ThreadedGenerateData(const RegionType &region, ThreadIdType /* Unused */) ITK_OVERRIDE {
        //std::cout <<  __PRETTY_FUNCTION__ );
        ImageRegionConstIterator<TImage> inputIter(this->GetInput(), region);
        ImageRegionIterator<TImage> outputIter(this->GetOutput(), region);

        while(!inputIter.IsAtEnd()) {
            const double ival = inputIter.Get();
            double best_distance = std::numeric_limits<double>::max();
            int best_index = 0;
            for (int i = m_T1.size(); i > 0; i--) {
                double distance = fabs(m_con[i] - ival);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = i;
                }
            }
            //cout << "Best index " << best_index << " distance " << best_distance << " pars " << outputs.transpose() << " data " << data_inputs.transpose() << " cons" << m_cons[best_index].transpose());
            outputIter.Set(m_T1[best_index]);
            ++inputIter;
            ++outputIter;
        }
    }

private:
    MPRAGELookUpFilter(const Self &); //purposely not implemented
    void operator=(const Self &);  //purposely not implemented
};

} // End namespace itk

int main(int argc, char **argv) {
    args::ArgumentParser parser("Calculates T1/B1 maps from MP2/3-RAGE data\nhttp://github.com/spinicist/QUIT");
    args::Positional<std::string> input_path(parser, "INPUT FILE", "Path to complex MP-RAGE data");
    args::HelpFlag help(parser, "HELP", "Show this help message", {'h', "help"});
    args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
    args::ValueFlag<int> threads(parser, "THREADS", "Use N threads (default=4, 0=hardware limit)", {'T', "threads"}, 4);
    args::ValueFlag<std::string> outarg(parser, "OUTPREFIX", "Add a prefix to output filenames", {'o', "out"});
    args::ValueFlag<std::string> mask(parser, "MASK", "Only process voxels within the mask", {'m', "mask"});
    args::Flag     automask(parser, "AUTOMASK", "Create a mask from the sum of squares image", {'a', "automask"});
    QI::ParseArgs(parser, argc, argv, verbose);
    itk::MultiThreader::SetGlobalDefaultNumberOfThreads(threads.Get());

    QI_LOG(verbose, "Opening input file " << QI::CheckPos(input_path));
    auto inFile = QI::ReadImage<QI::SeriesXF>(QI::CheckPos(input_path));

    auto ti_1 = itk::ExtractImageFilter<QI::SeriesXF, QI::VolumeXF>::New();
    auto ti_2 = itk::ExtractImageFilter<QI::SeriesXF, QI::VolumeXF>::New();
    auto region = inFile->GetLargestPossibleRegion();
    region.GetModifiableSize()[3] = 0;
    ti_1->SetExtractionRegion(region);
    ti_1->SetDirectionCollapseToSubmatrix();
    ti_1->SetInput(inFile);
    region.GetModifiableIndex()[3] = 1;
    ti_2->SetExtractionRegion(region);
    ti_2->SetDirectionCollapseToSubmatrix();
    ti_2->SetInput(inFile);

    QI::VolumeI::Pointer mask_img = nullptr;
    if (automask) {
        QI_LOG(verbose, "Calculating mask");
        auto SqrSumFilter = itk::BinaryFunctorImageFilter<QI::VolumeXF, QI::VolumeXF, QI::VolumeF, SqrSumFunctor<float>>::New();
        SqrSumFilter->SetInput1(ti_1->GetOutput());
        SqrSumFilter->SetInput2(ti_2->GetOutput());
        SqrSumFilter->Update();
        mask_img = QI::ThresholdMask(SqrSumFilter->GetOutput(), 0.025);
    } else if (mask) {
        QI_LOG(verbose, "Reading mask file: " << mask.Get());
        mask_img = QI::ReadImage<QI::VolumeI>(mask.Get());
    }

    QI_LOG(verbose, "Generating MP2 contrasts");
    auto MP2Filter = itk::BinaryFunctorImageFilter<QI::VolumeXF, QI::VolumeXF, QI::VolumeF, MP2Functor<float>>::New();
    MP2Filter->SetInput1(ti_1->GetOutput());
    MP2Filter->SetInput2(ti_2->GetOutput());
    MP2Filter->Update();
    std::string outName = outarg ? outarg.Get() : QI::StripExt(input_path.Get());
    QI_LOG(verbose, "Reading sequence parameters");
    rapidjson::Document input = QI::ReadJSON(std::cin);
    QI::MP2RAGESequence mp2rage_sequence(input["MP2RAGE"]);
    QI_LOG(verbose, "Calculating T1");
    auto apply = itk::MPRAGELookUpFilter::New();
    apply->SetSequence(mp2rage_sequence);
    apply->SetInput(MP2Filter->GetOutput());
    apply->Update();

    if (mask_img) {
        QI_LOG(verbose, "Masking outputs");
        auto masker = itk::MaskImageFilter<QI::VolumeF, QI::VolumeI>::New();
        auto add = itk::AddImageFilter<QI::VolumeF, QI::VolumeF>::New();
        add->SetInput(MP2Filter->GetOutput());
        add->SetConstant(0.5);
        masker->SetInput(add->GetOutput());
        masker->SetMaskImage(mask_img);
        masker->Update();
        QI::WriteImage(masker->GetOutput(), outName + "_contrast" + QI::OutExt());
        masker->SetInput(apply->GetOutput());
        masker->Update();
        QI::WriteImage(masker->GetOutput(0), outName + "_T1" + QI::OutExt());
    } else {
        QI::WriteImage(MP2Filter->GetOutput(), outName + "_contrast" + QI::OutExt());
        QI::WriteImage(apply->GetOutput(0), outName + "_T1" + QI::OutExt());
    }
    QI_LOG(verbose, "Finished.");
    return EXIT_SUCCESS;
}

