#ifndef APPLYALGOFILTER_H
#define APPLYALGOFILTER_H

#include <vector>
#include <tuple>
#include "itkImageToImageFilter.h"
#include "itkVariableLengthVector.h"
#include "itkVectorImage.h"
#include "itkTimeProbe.h"
#include "ThreadPool.h"

namespace itk{

template<typename TInputImage_,
         typename TOutputImage_,
         typename TConstImage_,
         typename TMaskImage_>
class ApplyAlgorithmFilter : public ImageToImageFilter<TInputImage_, TOutputImage_> {
public:
    typedef TInputImage_  TInputImage;
    typedef TOutputImage_ TOutputImage;
    typedef TConstImage_  TConstImage;
    typedef TMaskImage_   TMaskImage;

    typedef typename TInputImage::PixelType  TInputPixel;
    typedef typename TOutputImage::PixelType TOutputPixel;
    typedef typename TConstImage::PixelType  TConstPixel;
    typedef int TIterations;
    typedef Image<TIterations, TInputImage::ImageDimension> TIterationsImage;

    typedef ApplyAlgorithmFilter                          Self;
    typedef ImageToImageFilter<TInputImage, TOutputImage> Superclass;
    typedef SmartPointer<Self>                            Pointer;
    typedef typename TInputImage::RegionType              TRegion;
    typedef typename TRegion::IndexType                   TIndex;

    itkNewMacro(Self); /** Method for creation through the object factory. */
    itkTypeMacro(ApplyAlgorithmFilter, ImageToImageFilter); /** Run-time type information (and related methods). */

    class Algorithm {
    public:
        using TInput = TInputPixel;
        using TOutput = TOutputPixel;
        using TConst = TConstPixel;
        using TIndex = ApplyAlgorithmFilter::TIndex;           // Make TIndex available to algorithm subclasses
        using TIterations = ApplyAlgorithmFilter::TIterations; // Make TIterations available to algorithm subclasses
        typedef std::tuple<bool, std::string> TStatus;         // Allow a return message when fitting fails
        virtual size_t numInputs() const = 0;  // The number of inputs that will be concatenated into the data vector
        virtual size_t numConsts() const = 0;  // Number of constant input parameters/variables
        virtual size_t numOutputs() const = 0; // Number of output parameters/variables
        virtual size_t outputSize() const { return 1; }; // Size outputs, overload for vector outputs
        virtual size_t dataSize() const = 0;   // The expected size of the concatenated input
        virtual std::vector<TConst> defaultConsts() const = 0;    // Give some default constants for when the user does not supply them
        virtual TStatus apply(const std::vector<TInput> &inputs,
                              const std::vector<TConst> &consts,
                              const TIndex &index,
                              std::vector<TOutput> &outputs,
                              TOutput &residual, TInput &resids,
                              TIterations &iterations) const = 0; // Apply the algorithm to the data from one voxel. Return false to indicate algorithm failed.
        virtual TOutput zero() const = 0; // Hack, to supply a zero for masked voxels
    };

    void SetAlgorithm(const std::shared_ptr<Algorithm> &a);
    std::shared_ptr<const Algorithm> GetAlgorithm() const;

    void SetInput(unsigned int i, const TInputImage *img) override;
    typename TInputImage::ConstPointer GetInput(const size_t i) const;
    void SetConst(const size_t i, const TConstImage *img);
    typename TConstImage::ConstPointer GetConst(const size_t i) const;
    void SetMask(const TMaskImage *mask);
    typename TMaskImage::ConstPointer GetMask() const;

    void SetPoolsize(const size_t nThreads);
    void SetSplitsPerThread(const size_t nSplits);
    void SetSubregion(const TRegion &sr); 
    void SetVerbose(const bool v);
    void SetOutputAllResiduals(const bool r); 
    
    TOutputImage     *GetOutput(const size_t i);
    TOutputImage     *GetResidualOutput();
    TInputImage      *GetAllResidualsOutput();
    TIterationsImage *GetIterationsOutput();

    RealTimeClock::TimeStampType GetTotalTime() const;

protected:
    ApplyAlgorithmFilter();
    virtual ~ApplyAlgorithmFilter() {}
    DataObject::Pointer MakeOutput(ProcessObject::DataObjectPointerArraySizeType idx) ITK_OVERRIDE;

    std::shared_ptr<Algorithm> m_algorithm;
    bool m_verbose = false, m_hasSubregion = false, m_allResiduals = false;
    size_t m_poolsize = 1, m_splitsPerThread = 1;
    TRegion m_subregion;

    RealTimeClock::TimeStampType m_elapsedTime = 0.0;
    static const int ResidualOutputOffset = 0;
    static const int IterationsOutputOffset = 1;
    static const int AllResidualsOutputOffset = 2;
    static const int ExtraOutputs = 3;

    virtual void GenerateData() ITK_OVERRIDE;
    /* Doing my own threading so override both of these */
    virtual void GenerateOutputInformation() ITK_OVERRIDE;
    virtual void ThreadedGenerateData(const TRegion &region, ThreadIdType threadId) ITK_OVERRIDE;

private:
    ApplyAlgorithmFilter(const Self &); //purposely not implemented
    void operator=(const Self &);  //purposely not implemented

};

}

#endif // APPLYAGLOFILTER_H
