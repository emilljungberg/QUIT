/*
 *  qidream.cpp
 *
 *  Created by Tobias Wood on 2015/06/04.
 *  Copyright (c) 2015 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include <string>

#include "ImageTypes.h"
#include "Util.h"
#include "ImageIO.h"
#include "Args.h"

#include "itkExtractImageFilter.h"
#include "itkDivideImageFilter.h"
#include "itkBinaryFunctorImageFilter.h"

template<class TPixel> class DREAM {
public:
    DREAM() {}
    ~DREAM() {}
    bool operator!=(const DREAM &) const { return false; }
    bool operator==(const DREAM &other) const { return !(*this != other); }

    inline TPixel operator()(const TPixel &fid,
                             const TPixel &ste) const
    {
        TPixel alpha = atan(sqrt(2.*ste/fid)) * 180. / M_PI;
        return alpha;
    }
};

int main(int argc, char **argv) {
    args::ArgumentParser parser("Calculates a B1 (flip-angle) map from DREAM data.\nhttp://github.com/spinicist/QUIT");

    args::Positional<std::string> input_file(parser, "DREAM_FILE", "Input file. Must have 2 volumes (FID and STE)");

    args::HelpFlag help(parser, "HELP", "Show this help menu", {'h', "help"});
    args::Flag     verbose(parser, "VERBOSE", "Print more information", {'v', "verbose"});
    args::ValueFlag<int> threads(parser, "THREADS", "Use N threads (default=4, 0=hardware limit)", {'T', "threads"}, 4);
    args::ValueFlag<std::string> out_prefix(parser, "OUTPREFIX", "Add a prefix to output filenames", {'o', "out"});
    args::ValueFlag<char> order(parser, "ORDER", "Volume order - f/s/v - fid/ste/vst first", {'O', "order"}, 'f');
    args::ValueFlag<std::string> mask(parser, "MASK", "Only process voxels within the mask", {'m', "mask"});
    args::ValueFlag<double> alpha(parser, "ALPHA", "Nominal flip-angle (default 55)", {'a', "alpha"}, 55);
    args::ValueFlag<std::string> subregion(parser, "SUBREGION", "Process subregion starting at voxel I,J,K with size SI,SJ,SK", {'s', "subregion"});
    QI::ParseArgs(parser, argc, argv, verbose);

    itk::MultiThreader::SetGlobalDefaultNumberOfThreads(threads.Get());

    QI_LOG(verbose, "Opening input file " << QI::CheckPos(input_file));
    auto inFile = QI::ReadImage<QI::SeriesF>(QI::CheckPos(input_file));

    auto fid_volume = itk::ExtractImageFilter<QI::SeriesF, QI::VolumeF>::New();
    auto ste_volume = itk::ExtractImageFilter<QI::SeriesF, QI::VolumeF>::New();
    auto region = inFile->GetLargestPossibleRegion();
    region.GetModifiableSize()[3] = 0;
    switch (order.Get()) {
    case 'f': region.GetModifiableIndex()[3] = 0; break;
    case 's':
    case 'v': region.GetModifiableIndex()[3] = 1; break;
    }
    fid_volume->SetExtractionRegion(region);
    fid_volume->SetInput(inFile);
    fid_volume->SetDirectionCollapseToSubmatrix();
    // Swap to other volume
    region.GetModifiableIndex()[3] = (region.GetIndex()[3] + 1) % 2;
    ste_volume->SetExtractionRegion(region);
    ste_volume->SetInput(inFile);
    ste_volume->SetDirectionCollapseToSubmatrix();

    auto dream = itk::BinaryFunctorImageFilter<QI::VolumeF,
                                               QI::VolumeF,
                                               QI::VolumeF,
                                               DREAM<float>>::New();
    dream->SetInput1(fid_volume->GetOutput());
    dream->SetInput2(ste_volume->GetOutput());

    auto B1 = itk::DivideImageFilter<QI::VolumeF, QI::VolumeF, QI::VolumeF>::New();
    B1->SetInput1(dream->GetOutput());
    B1->SetConstant2(alpha.Get());
    B1->Update();
    QI::WriteImage(dream->GetOutput(), out_prefix.Get() + "DREAM_angle" + QI::OutExt());
    QI::WriteImage(B1->GetOutput(), out_prefix.Get() + "DREAM_B1" + QI::OutExt());
    QI_LOG(verbose, "Finished." );
    return EXIT_SUCCESS;
}

