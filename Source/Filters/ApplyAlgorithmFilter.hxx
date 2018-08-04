#ifndef APPLYALGORITHMFILTER_HXX
#define APPLYALGORITHMFILTER_HXX

#include "itkObjectFactory.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkImageRegionConstIterator.h"
#include "itkProgressReporter.h"
#include "itkImageRegionSplitterSlowDimension.h"

#include "ApplyAlgorithmFilter.h"

namespace itk {

template<typename TI, typename TO, typename TC, typename TM>
ApplyAlgorithmFilter<TI, TO, TC, TM>::ApplyAlgorithmFilter() {
    //std::cout <<  __PRETTY_FUNCTION__ << endl;
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetAlgorithm(const std::shared_ptr<Algorithm> &a) {
    //std::cout <<  __PRETTY_FUNCTION__ << endl;
    m_algorithm = a;
    // Inputs go: Data 0, Data 1, ..., Mask, Const 0, Const 1, ...
    // Only the data inputs are required, the others are optional
    this->SetNumberOfRequiredInputs(a->numInputs());
    // Outputs go: Parameter 0, Parameter 1, ..., AllResiduals, Residual, Iterations
    // Need to be this way because at some ITK assumes 1st output is of TOutputImage
    this->SetNumberOfRequiredOutputs(m_algorithm->numOutputs()+ExtraOutputs);
    for (size_t i = 0; i < (m_algorithm->numOutputs()+ExtraOutputs); i++) {
        this->SetNthOutput(i, this->MakeOutput(i));
    }
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetAlgorithm() const -> std::shared_ptr<const Algorithm>{ return m_algorithm; }

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetPoolsize(const size_t n) {
    if (n > 0) {
        m_poolsize = n;
    } else {
        m_poolsize = std::thread::hardware_concurrency();
    }
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetSplitsPerThread(const size_t n) {
    if (n > 0) {
        m_splitsPerThread = n;
    } else {
        m_splitsPerThread = m_poolsize;
    }
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetSubregion(const TRegion &sr) {
    if (m_verbose) std::cout << "Setting subregion to: " << std::endl << sr << std::endl;
    m_subregion = sr;
    m_hasSubregion = true;
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetVerbose(const bool v) { m_verbose = v; }

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetOutputAllResiduals(const bool r) { m_allResiduals = r; }

template<typename TI, typename TO, typename TC, typename TM>
RealTimeClock::TimeStampType ApplyAlgorithmFilter<TI, TO, TC, TM>::GetTotalTime() const { return m_elapsedTime; }

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetInput(unsigned int i, const TInputImage *image) {
    if (!m_algorithm) itkExceptionMacro("No algorithm set, do not know number of valid inputs");
    if (i < m_algorithm->numInputs()) {
        this->SetNthInput(i, const_cast<TInputImage*>(image));
    } else {
        itkExceptionMacro("Requested input " << i << " does not exist (" << m_algorithm->numInputs() << " inputs)");
    }
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetConst(const size_t i, const TConstImage *image) {
    if (i < m_algorithm->numConsts()) {
        this->SetNthInput(m_algorithm->numInputs() + 1 + i, const_cast<TConstImage*>(image));
    } else {
        itkExceptionMacro("Requested const input " << i << " does not exist (" << m_algorithm->numConsts() << " const inputs)");
    }
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::SetMask(const TMaskImage *image) {
    this->SetNthInput(m_algorithm->numInputs(), const_cast<TMaskImage *>(image));
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetInput(const size_t i) const -> typename TInputImage::ConstPointer {
    if (i < m_algorithm->numInputs()) {
        return static_cast<const TInputImage *> (this->ProcessObject::GetInput(i));
    } else {
        itkExceptionMacro("Requested input " << i << " does not exist (" << m_algorithm->numInputs() << " inputs)");
    }
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetConst(const size_t i) const -> typename TConstImage::ConstPointer {
    if (i < m_algorithm->numConsts()) {
        size_t index = m_algorithm->numInputs() + 1 + i;
        return static_cast<const TConstImage *> (this->ProcessObject::GetInput(index));
    } else {
        itkExceptionMacro("Requested const input " << i << " does not exist (" << m_algorithm->numConsts() << " const inputs)");
    }
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetMask() const -> typename TMaskImage::ConstPointer {
    return static_cast<const TMaskImage *>(this->ProcessObject::GetInput(m_algorithm->numInputs()));
}

template<typename TI, typename TO, typename TC, typename TM>
DataObject::Pointer ApplyAlgorithmFilter<TI, TO, TC, TM>::MakeOutput(ProcessObject::DataObjectPointerArraySizeType idx) {
    DataObject::Pointer output;
    if (idx < m_algorithm->numOutputs()) {
        output = (TOutputImage::New()).GetPointer();
    } else if (idx == (m_algorithm->numOutputs() + AllResidualsOutputOffset)) {
        auto img = TInputImage::New();
        output = img;
    } else if (idx == (m_algorithm->numOutputs() + ResidualOutputOffset)) {
        auto img = TOutputImage::New();
        output = img;
    } else if (idx == (m_algorithm->numOutputs() + IterationsOutputOffset)) {
        auto img = TIterationsImage::New();
        output = img;
    } else {
        itkExceptionMacro("Attempted to create output " << idx << ", index too high");
    }
    return output.GetPointer();
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetOutput(const size_t i) -> TOutputImage *{
    if (i < m_algorithm->numOutputs()) {
        return dynamic_cast<TOutputImage *>(this->ProcessObject::GetOutput(i));
    } else {
        itkExceptionMacro("Requested output " << std::to_string(i) << " is past maximum (" << std::to_string(m_algorithm->numOutputs()) << ")");
    }
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetAllResidualsOutput() -> TInputImage *{
    return dynamic_cast<TInputImage *>(this->ProcessObject::GetOutput(m_algorithm->numOutputs()+AllResidualsOutputOffset));
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetResidualOutput() -> TOutputImage *{
    return dynamic_cast<TOutputImage *>(this->ProcessObject::GetOutput(m_algorithm->numOutputs()+ResidualOutputOffset));
}

template<typename TI, typename TO, typename TC, typename TM>
auto ApplyAlgorithmFilter<TI, TO, TC, TM>::GetIterationsOutput() -> TIterationsImage *{
    return dynamic_cast<TIterationsImage *>(this->ProcessObject::GetOutput(m_algorithm->numOutputs()+IterationsOutputOffset));
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::GenerateOutputInformation() {
    Superclass::GenerateOutputInformation();
    size_t size = 0;
    for (size_t i = 0; i < m_algorithm->numInputs(); i++) {
        size += this->GetInput(i)->GetNumberOfComponentsPerPixel();
    }
    if (m_algorithm->dataSize() != size) {
        itkExceptionMacro("Sequence size (" << m_algorithm->dataSize() << ") does not match input size (" << size << ")");
    }
    if (size == 0) {
        itkExceptionMacro("Total input size cannot be 0");
    }

    auto input     = this->GetInput(0);
    auto region    = input->GetLargestPossibleRegion();
    auto spacing   = input->GetSpacing();
    auto origin    = input->GetOrigin();
    auto direction = input->GetDirection();
    if (m_verbose) std::cout << "Allocating output memory" << std::endl;
    for (size_t i = 0; i < m_algorithm->numOutputs(); i++) {
        auto op = this->GetOutput(i);
        op->SetRegions(region);
        op->SetSpacing(spacing);
        op->SetOrigin(origin);
        op->SetDirection(direction);
        op->SetNumberOfComponentsPerPixel(m_algorithm->outputSize());
        op->Allocate(true);
        if (m_verbose) std::cout << "Allocated output " << i << std::endl;
    }
    if (m_allResiduals) {
        if (m_verbose) std::cout << "Allocating residuals memory" << std::endl;
        auto r = this->GetAllResidualsOutput();
        r->SetRegions(region);
        r->SetSpacing(spacing);
        r->SetOrigin(origin);
        r->SetDirection(direction);
        r->SetNumberOfComponentsPerPixel(size);
        r->Allocate(true);
    }
    if (m_verbose) std::cout << "Allocating total residual memory" << std::endl;
    auto r = this->GetResidualOutput();
    r->SetRegions(region);
    r->SetSpacing(spacing);
    r->SetOrigin(origin);
    r->SetDirection(direction);
    r->SetNumberOfComponentsPerPixel(m_algorithm->outputSize());
    r->Allocate(true);
    if (m_verbose) std::cout << "Allocating iterations memory" << std::endl;
    auto i = this->GetIterationsOutput();
    i->SetRegions(region);
    i->SetSpacing(spacing);
    i->SetOrigin(origin);
    i->SetDirection(direction);
    i->Allocate(true);
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::GenerateData() {
    auto fullRegion = this->GetInput(0)->GetLargestPossibleRegion();
    if (m_hasSubregion) {
        if (fullRegion.IsInside(m_subregion)) {
            fullRegion = m_subregion;
        } else {
            itkExceptionMacro("Specified subregion is not entirely inside image.");
        }
    }

    auto splitter = ImageRegionSplitterSlowDimension::New();
    auto splits = splitter->GetNumberOfSplits(fullRegion, m_poolsize * m_splitsPerThread);
    if (m_verbose) std::cout << "Number of splits: " << splits << std::endl;
    TimeProbe clock;
    clock.Start();
    {   // Use scope-based instantiation to wait for the threadpool to stop
        QI::ThreadPool threadPool(m_poolsize);
        for (unsigned int split = 0; split < splits; split++) {
            auto task = [=] {
                auto thread_region = fullRegion;
                splitter->GetSplit(split, splits, thread_region);
                this->ThreadedGenerateData(thread_region, split);
            };
            threadPool.enqueue(task);
            if (m_verbose) std::cout << "Starting split " << split << std::endl;
        }
    }
    clock.Stop();
    m_elapsedTime = clock.GetTotal();
    if (m_verbose) std::cout << "Finished all splits" << std::endl;
}

template<typename TI, typename TO, typename TC, typename TM>
void ApplyAlgorithmFilter<TI, TO, TC, TM>::ThreadedGenerateData(const TRegion &region, ThreadIdType /* Unused */) {
    ImageRegionConstIterator<TMaskImage> maskIter;
    const auto mask = this->GetMask();
    if (mask) {
        maskIter = ImageRegionConstIterator<TMaskImage>(mask, region);
    }
    
    std::vector<ImageRegionConstIterator<TInputImage>> dataIters(m_algorithm->numInputs());
    for (size_t i = 0; i < m_algorithm->numInputs(); i++) {
        dataIters[i] = ImageRegionConstIterator<TInputImage>(this->GetInput(i), region);
    }

    std::vector<ImageRegionConstIterator<TConstImage>> constIters(m_algorithm->numConsts());
    for (size_t i = 0; i < m_algorithm->numConsts(); i++) {
        typename TConstImage::ConstPointer c = this->GetConst(i);
        if (c) {
            constIters[i] = ImageRegionConstIterator<TConstImage>(c, region);
        }
    }
    std::vector<ImageRegionIterator<TOutputImage>> outputIters(m_algorithm->numOutputs());
    for (size_t i = 0; i < m_algorithm->numOutputs(); i++) {
        outputIters[i] = ImageRegionIterator<TOutputImage>(this->GetOutput(i), region);
    }
    ImageRegionIterator<TInputImage> allResidualsIter;
    if (m_allResiduals) {
        allResidualsIter = ImageRegionIterator<TInputImage>(this->GetAllResidualsOutput(), region);
    }
    // Keep the index on this one to report voxel locations where algorithm fails.
    ImageRegionIteratorWithIndex<TOutputImage> residualIter(this->GetResidualOutput(), region);
    ImageRegionIterator<TIterationsImage> iterationsIter(this->GetIterationsOutput(), region);

    while(!dataIters[0].IsAtEnd()) {
        if (!mask || maskIter.Get()) {
            std::vector<TInputPixel> inputs(m_algorithm->numInputs());
            std::vector<TOutputPixel> outputs(m_algorithm->numOutputs());
            for (size_t i = 0; i < outputs.size(); i++) {
                outputs[i] = m_algorithm->zero();
            }
            std::vector<TConstPixel> constants = m_algorithm->defaultConsts();
            for (size_t i = 0; i < constIters.size(); i++) {
                if (this->GetConst(i)) {
                    constants[i] = constIters[i].Get();
                }
            }
            TOutputPixel residual = m_algorithm->zero();
            TInputPixel resids;
            if (m_allResiduals) {
                resids.SetSize(this->GetAllResidualsOutput()->GetNumberOfComponentsPerPixel());
            } else {
                resids.SetSize(0);
            }
            TIterations iterations{0};

            for (size_t i = 0; i < m_algorithm->numInputs(); i++) {
                inputs[i] = dataIters[i].Get();
            }
            typename Algorithm::TStatus status = m_algorithm->apply(inputs, constants, residualIter.GetIndex(),
                                                outputs, residual, resids, iterations);
            if (!std::get<0>(status)) {
                std::cerr << "Algorithm failed for voxel " << residualIter.GetIndex() << ": " << std::get<1>(status) << std::endl;
            }
            for (size_t i = 0; i < m_algorithm->numOutputs(); i++) {
                outputIters[i].Set(outputs[i]);
            }
            residualIter.Set(residual);
            if (m_allResiduals) {
                allResidualsIter.Set(resids);
            }
            iterationsIter.Set(iterations);
        } else {
            for (size_t i = 0; i < m_algorithm->numOutputs(); i++) {
                outputIters[i].Set(m_algorithm->zero());
            }
            if (m_allResiduals) {
                VariableLengthVector<float> residZeros(m_algorithm->dataSize()); residZeros.Fill(0.);
                allResidualsIter.Set(residZeros);
            }
            residualIter.Set(m_algorithm->zero());
            iterationsIter.Set(0);
        }
        
        if (this->GetMask())
            ++maskIter;
        for (size_t i = 0; i < m_algorithm->numInputs(); i++) {
            ++dataIters[i];
        }
        for (size_t i = 0; i < m_algorithm->numConsts(); i++) {
            if (this->GetConst(i))
                ++constIters[i];
        }
        for (size_t i = 0; i < m_algorithm->numOutputs(); i++) {
            ++outputIters[i];
        }
        if (m_allResiduals)
            ++allResidualsIter;
        ++residualIter;
        ++iterationsIter;
    }
}
} // namespace ITK

#endif // APPLYALGORITHMFILTER_HXX
