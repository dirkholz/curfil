#ifndef CURFIL_RANDOMTREEIMAGE_H
#define CURFIL_RANDOMTREEIMAGE_H

#include <algorithm>
#include <assert.h>
#include <boost/make_shared.hpp>
#include <cuv/ndarray.hpp>
#include <list>
#include <stdint.h>
#include <vector>

#include "image.h"
#include "random_tree.h"

namespace curfil {

class XY {
public:
    XY() :
            x(0), y(0) {
    }
    XY(int x, int y) :
            x(x), y(y) {
    }
    XY(const XY& other) :
            x(other.x), y(other.y) {
    }

    XY& operator=(const XY& other) {
        x = other.x;
        y = other.y;
        return (*this);
    }

    XY normalize(const Depth& depth) const {
        assert(depth.isValid());
        int newX = static_cast<int>(x / depth.getFloatValue());
        int newY = static_cast<int>(y / depth.getFloatValue());
        return XY(newX, newY);
    }

    bool operator==(const XY& other) const {
        return (x == other.x && y == other.y);
    }

    bool operator!=(const XY& other) const {
        return !(*this == other);
    }

    int getX() const {
        return x;
    }

    int getY() const {
        return y;
    }

private:
    int x, y;
};

typedef XY Region;
typedef XY Offset;
typedef XY Point;

class PixelInstance {
public:

    PixelInstance(const RGBDImage* image, const LabelType& label, uint16_t x, uint16_t y) :
            image(image), label(label), point(x, y), depth(Depth::INVALID) {
        assert(image != NULL);
        assert(image->inImage(x, y));
        if (!image->hasIntegratedDepth()) {
            throw std::runtime_error("image is not integrated");
        }

        int aboveValid = (y > 0) ? image->getDepthValid(x, y - 1) : 0;
        int leftValid = (x > 0) ? image->getDepthValid(x - 1, y) : 0;
        int aboveLeftValid = (x > 0 && y > 0) ? image->getDepthValid(x - 1, y - 1) : 0;

        int valid = image->getDepthValid(x, y) - (leftValid + aboveValid - aboveLeftValid);
        assert(valid == 0 || valid == 1);

        if (valid == 1) {
            Depth above = (y > 0) ? image->getDepth(x, y - 1) : Depth(0);
            Depth left = (x > 0) ? image->getDepth(x - 1, y) : Depth(0);
            Depth aboveLeft = (x > 0 && y > 0) ? image->getDepth(x - 1, y - 1) : Depth(0);

            depth = image->getDepth(x, y) - (left + above - aboveLeft);
            assert(depth.isValid());
        } else {
            assert(!depth.isValid());
        }
    }

    PixelInstance(const RGBDImage* image, const LabelType& label, const Depth& depth,
            uint16_t x, uint16_t y) :
            image(image), label(label), point(x, y), depth(depth) {
        assert(image != NULL);
        assert(image->inImage(x, y));
        assert(depth.isValid());
    }

    const RGBDImage* getRGBDImage() const {
        return image;
    }

    int width() const {
        return image->getWidth();
    }

    int height() const {
        return image->getHeight();
    }

    uint16_t getX() const {
        return static_cast<uint16_t>(point.getX());
    }

    uint16_t getY() const {
        return static_cast<uint16_t>(point.getY());
    }

    FeatureResponseType averageRegionColor(const Offset& offset, const Region& region, uint8_t channel) const {

        assert(region.getX() >= 0);
        assert(region.getY() >= 0);

        assert(image->hasIntegratedColor());

        const int width = std::max(1, region.getX());
        const int height = std::max(1, region.getY());

        int x = getX() + offset.getX();
        int y = getY() + offset.getY();

        int leftX = x - width;
        int rightX = x + width;
        int upperY = y - height;
        int lowerY = y + height;

        if (leftX < 0 || rightX >= image->getWidth() || upperY < 0 || lowerY >= image->getHeight()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        assert(inImage(x, y));

        Point upperLeft(leftX, upperY);
        Point upperRight(rightX, upperY);
        Point lowerLeft(leftX, lowerY);
        Point lowerRight(rightX, lowerY);

        FeatureResponseType lowerRightPixel = getColor(lowerRight, channel);
        FeatureResponseType lowerLeftPixel = getColor(lowerLeft, channel);
        FeatureResponseType upperRightPixel = getColor(upperRight, channel);
        FeatureResponseType upperLeftPixel = getColor(upperLeft, channel);

        FeatureResponseType sum = (lowerRightPixel - upperRightPixel) + (upperLeftPixel - lowerLeftPixel);

        return sum;
    }

    FeatureResponseType averageRegionDepth(const Offset& offset, const Region& region) const {
        assert(region.getX() >= 0);
        assert(region.getY() >= 0);

        assert(image->hasIntegratedDepth());

        const int width = std::max(1, region.getX());
        const int height = std::max(1, region.getY());

        int x = getX() + offset.getX();
        int y = getY() + offset.getY();

        int leftX = x - width;
        int rightX = x + width;
        int upperY = y - height;
        int lowerY = y + height;

        if (leftX < 0 || rightX >= image->getWidth() || upperY < 0 || lowerY >= image->getHeight()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        assert(inImage(x, y));

        Point upperLeft(leftX, upperY);
        Point upperRight(rightX, upperY);
        Point lowerLeft(leftX, lowerY);
        Point lowerRight(rightX, lowerY);

        int upperLeftValid = getDepthValid(upperLeft);
        int upperRightValid = getDepthValid(upperRight);
        int lowerRightValid = getDepthValid(lowerRight);
        int lowerLeftValid = getDepthValid(lowerLeft);

        int numValid = (lowerRightValid - upperRightValid) + (upperLeftValid - lowerLeftValid);
        assert(numValid >= 0);

        if (numValid == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const int lowerRightDepth = getDepth(lowerRight).getIntValue();
        const int lowerLeftDepth = getDepth(lowerLeft).getIntValue();
        const int upperRightDepth = getDepth(upperRight).getIntValue();
        const int upperLeftDepth = getDepth(upperLeft).getIntValue();

        int sum = (lowerRightDepth - upperRightDepth) + (upperLeftDepth - lowerLeftDepth);
        FeatureResponseType feat = sum / static_cast<FeatureResponseType>(1000);
        return (feat / numValid);
    }

    LabelType getLabel() const {
        return label;
    }

    const Depth getDepth() const {
        return depth;
    }

    WeightType getWeight() const {
        return 1;
    }

private:
    const RGBDImage* image;
    LabelType label;
    Point point;
    Depth depth;

    float getColor(const Point& pos, uint8_t channel) const {
        if (!inImage(pos)) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        assert(image->hasIntegratedColor());
        return image->getColor(pos.getX(), pos.getY(), channel);
    }

    Depth getDepth(const Point& pos) const {
        if (!inImage(pos)) {
            return Depth::INVALID;
        }
        assert(image->hasIntegratedDepth());
        const Depth depth = image->getDepth(pos.getX(), pos.getY());
        // include zero as it is an integral
        assert(depth.getIntValue() >= 0);
        return depth;
    }

    int getDepthValid(const Point& pos) const {
        return image->getDepthValid(pos.getX(), pos.getY());
    }

    bool inImage(int x, int y) const {
        return image->inImage(x, y);
    }

    bool inImage(const Point& pos) const {
        return inImage(pos.getX(), pos.getY());
    }
};

enum FeatureType {
    DEPTH = 0, COLOR = 1
};

class ImageFeatureFunction {

public:

    ImageFeatureFunction(FeatureType featureType,
            const Offset& offset1,
            const Region& region1,
            const uint8_t channel1,
            const Offset& offset2,
            const Region& region2,
            const uint8_t channel2) :
            featureType(featureType),
                    offset1(offset1),
                    region1(region1),
                    channel1(channel1),
                    offset2(offset2),
                    region2(region2),
                    channel2(channel2) {
        if (offset1 == offset2) {
            throw std::runtime_error("illegal feature: offset1 equals offset2");
        }
        assert(isValid());
    }

    ImageFeatureFunction() :
            featureType(), offset1(), region1(), channel1(), offset2(), region2(), channel2() {
    }

    int getSortKey() const {
        int32_t sortKey = 0;
        sortKey |= static_cast<uint8_t>(getType() & 0x03) << 30; // 2 bit for the type
        sortKey |= static_cast<uint8_t>(getChannel1() & 0x0F) << 26; // 4 bit for channel1
        sortKey |= static_cast<uint8_t>(getChannel2() & 0x0F) << 22; // 4 bit for channel2
        sortKey |= static_cast<uint8_t>((getOffset1().getY() + 127) & 0xFF) << 14; // 8 bit for offset1.y
        sortKey |= static_cast<uint8_t>((getOffset1().getX() + 127) & 0xFF) << 6; // 8 bit for offset1.x
        return sortKey;
    }

    FeatureType getType() const {
        return featureType;
    }

    std::string getTypeString() const {
        switch (featureType) {
            case COLOR:
                return "color";
            case DEPTH:
                return "depth";
            default:
                throw std::runtime_error("unknown feature");
        }
    }

    bool isValid() const {
        return (offset1 != offset2);
    }

    FeatureResponseType calculateFeatureResponse(const PixelInstance& instance) const {
        assert(isValid());
        switch (featureType) {
            case DEPTH:
                return calculateDepthFeature(instance);
            case COLOR:
                return calculateColorFeature(instance);
            default:
                assert(false);
                break;
        }
        return 0;
    }

    const Offset& getOffset1() const {
        return offset1;
    }

    const Region& getRegion1() const {
        return region1;
    }

    uint8_t getChannel1() const {
        return channel1;
    }

    const Offset& getOffset2() const {
        return offset2;
    }

    const Region& getRegion2() const {
        return region2;
    }

    uint8_t getChannel2() const {
        return channel2;
    }

    bool operator!=(const ImageFeatureFunction& other) const {
        return !(*this == other);
    }

    bool operator==(const ImageFeatureFunction& other) const;

private:
    FeatureType featureType;

    Offset offset1;
    Region region1;
    uint8_t channel1;

    Offset offset2;
    Region region2;
    uint8_t channel2;

    FeatureResponseType calculateColorFeature(const PixelInstance& instance) const {

        const Depth depth = instance.getDepth();
        if (!depth.isValid()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        FeatureResponseType a = instance.averageRegionColor(offset1.normalize(depth), region1.normalize(depth),
                channel1);
        if (isnan(a))
            return a;

        FeatureResponseType b = instance.averageRegionColor(offset2.normalize(depth), region2.normalize(depth),
                channel2);
        if (isnan(b))
            return b;

        return (a - b);
    }

    FeatureResponseType calculateDepthFeature(const PixelInstance& instance) const {

        const Depth depth = instance.getDepth();
        if (!depth.isValid()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        FeatureResponseType a = instance.averageRegionDepth(offset1.normalize(depth), region1.normalize(depth));
        if (isnan(a)) {
            return a;
        }

        FeatureResponseType b = instance.averageRegionDepth(offset2.normalize(depth), region2.normalize(depth));
        if (isnan(b)) {
            return b;
        }

        assert(a > 0);
        assert(b > 0);

        return (a - b);
    }

};

template<class memory_space>
class ImageFeaturesAndThresholds {

public:

    cuv::ndarray<int8_t, memory_space> m_features;
    cuv::ndarray<float, memory_space> m_thresholds;

private:

    explicit ImageFeaturesAndThresholds(const cuv::ndarray<int8_t, memory_space>& features,
            const cuv::ndarray<float, memory_space>& thresholds) :
            m_features(features), m_thresholds(thresholds) {
    }

public:

    explicit ImageFeaturesAndThresholds(size_t numFeatures, size_t numThresholds,
            boost::shared_ptr<cuv::allocator> allocator) :
            m_features(11, numFeatures, allocator), m_thresholds(numThresholds, numFeatures, allocator) {
    }

    template<class other_memory_space>
    explicit ImageFeaturesAndThresholds(const ImageFeaturesAndThresholds<other_memory_space>& other) :
            m_features(other.features().copy()), m_thresholds(other.thresholds().copy()) {
    }

    template<class other_memory_space>
    ImageFeaturesAndThresholds& operator=(const ImageFeaturesAndThresholds<other_memory_space>& other) {
        m_features = other.features().copy();
        m_thresholds = other.thresholds().copy();
        return (*this);
    }

    ImageFeaturesAndThresholds copy() const {
        return ImageFeaturesAndThresholds(m_features.copy(), m_thresholds.copy());
    }

    const cuv::ndarray<int8_t, memory_space> features() const {
        return m_features;
    }

    cuv::ndarray<int8_t, memory_space> features() {
        return m_features;
    }

    cuv::ndarray<int8_t, memory_space> types() {
        return m_features[cuv::indices[0][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> offset1X() {
        return m_features[cuv::indices[1][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> offset1Y() {
        return m_features[cuv::indices[2][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> offset2X() {
        return m_features[cuv::indices[3][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> offset2Y() {
        return m_features[cuv::indices[4][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> region1X() {
        return m_features[cuv::indices[5][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> region1Y() {
        return m_features[cuv::indices[6][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> region2X() {
        return m_features[cuv::indices[7][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> region2Y() {
        return m_features[cuv::indices[8][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> channel1() {
        return m_features[cuv::indices[9][cuv::index_range()]];
    }

    cuv::ndarray<int8_t, memory_space> channel2() {
        return m_features[cuv::indices[10][cuv::index_range()]];
    }

    cuv::ndarray<float, memory_space> thresholds() {
        return this->m_thresholds;
    }

    const cuv::ndarray<int8_t, memory_space> types() const {
        return m_features[cuv::indices[0][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> offset1X() const {
        return m_features[cuv::indices[1][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> offset1Y() const {
        return m_features[cuv::indices[2][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> offset2X() const {
        return m_features[cuv::indices[3][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> offset2Y() const {
        return m_features[cuv::indices[4][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> region1X() const {
        return m_features[cuv::indices[5][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> region1Y() const {
        return m_features[cuv::indices[6][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> region2X() const {
        return m_features[cuv::indices[7][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> region2Y() const {
        return m_features[cuv::indices[8][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> channel1() const {
        return m_features[cuv::indices[9][cuv::index_range()]];
    }

    const cuv::ndarray<int8_t, memory_space> channel2() const {
        return m_features[cuv::indices[10][cuv::index_range()]];
    }

    const cuv::ndarray<float, memory_space> thresholds() const {
        return this->m_thresholds;
    }

    double getThreshold(size_t threshNr, size_t featNr) const {
        return m_thresholds(threshNr, featNr);
    }

    void setFeatureFunction(size_t feat, const ImageFeatureFunction& feature) {

        types()(feat) = static_cast<int8_t>(feature.getType());

        offset1X()(feat) = feature.getOffset1().getX();
        offset1Y()(feat) = feature.getOffset1().getY();
        offset2X()(feat) = feature.getOffset2().getX();
        offset2Y()(feat) = feature.getOffset2().getY();

        region1X()(feat) = feature.getRegion1().getX();
        region1Y()(feat) = feature.getRegion1().getY();
        region2X()(feat) = feature.getRegion2().getX();
        region2Y()(feat) = feature.getRegion2().getY();

        channel1()(feat) = feature.getChannel1();
        channel2()(feat) = feature.getChannel2();

        assert(getFeatureFunction(feat) == feature);
    }

    ImageFeatureFunction getFeatureFunction(size_t feat) const {
        const Offset offset1(offset1X()(feat), offset1Y()(feat));
        const Offset offset2(offset2X()(feat), offset2Y()(feat));
        const Offset region1(region1X()(feat), region1Y()(feat));
        const Offset region2(region2X()(feat), region2Y()(feat));
        return ImageFeatureFunction(static_cast<FeatureType>(static_cast<int8_t>(types()(feat))),
                offset1, region1,
                channel1()(feat),
                offset2, region2,
                channel2()(feat));
    }

};

template<class memory_space>
class Samples {

public:
    cuv::ndarray<int, memory_space> data;

    float* depths;
    int* sampleX;
    int* sampleY;
    int* imageNumbers;
    uint8_t* labels;

    // does not copy data
    Samples(const Samples& samples) :
            data(samples.data),
                    depths(reinterpret_cast<float*>(data[cuv::indices[0][cuv::index_range()]].ptr())),
                    sampleX(reinterpret_cast<int*>(data[cuv::indices[1][cuv::index_range()]].ptr())),
                    sampleY(reinterpret_cast<int*>(data[cuv::indices[2][cuv::index_range()]].ptr())),
                    imageNumbers(reinterpret_cast<int*>(data[cuv::indices[3][cuv::index_range()]].ptr())),
                    labels(reinterpret_cast<uint8_t*>(data[cuv::indices[4][cuv::index_range()]].ptr())) {
    }

    // copies the data
    template<class T>
    Samples(const Samples<T>& samples, cudaStream_t stream) :
            data(samples.data, stream),
                    depths(reinterpret_cast<float*>(data[cuv::indices[0][cuv::index_range()]].ptr())),
                    sampleX(reinterpret_cast<int*>(data[cuv::indices[1][cuv::index_range()]].ptr())),
                    sampleY(reinterpret_cast<int*>(data[cuv::indices[2][cuv::index_range()]].ptr())),
                    imageNumbers(reinterpret_cast<int*>(data[cuv::indices[3][cuv::index_range()]].ptr())),
                    labels(reinterpret_cast<uint8_t*>(data[cuv::indices[4][cuv::index_range()]].ptr())) {
    }

    Samples(size_t numSamples, boost::shared_ptr<cuv::allocator>& allocator) :
            data(5, numSamples, allocator),
                    depths(reinterpret_cast<float*>(data[cuv::indices[0][cuv::index_range()]].ptr())),
                    sampleX(reinterpret_cast<int*>(data[cuv::indices[1][cuv::index_range()]].ptr())),
                    sampleY(reinterpret_cast<int*>(data[cuv::indices[2][cuv::index_range()]].ptr())),
                    imageNumbers(reinterpret_cast<int*>(data[cuv::indices[3][cuv::index_range()]].ptr())),
                    labels(reinterpret_cast<uint8_t*>(data[cuv::indices[4][cuv::index_range()]].ptr()))
    {
        assert_equals(imageNumbers, data.ptr() + 3 * numSamples);
        assert_equals(labels, reinterpret_cast<uint8_t*>(data.ptr() + 4 * numSamples));
    }

};

class ImageFeatureEvaluation {
public:
    // box_radius: > 0, half the box side length to uniformly sample
    //    (dx,dy) offsets from.
    ImageFeatureEvaluation(const size_t treeId, const TrainingConfiguration& configuration) :
            treeId(treeId), configuration(configuration),
                    imageWidth(0), imageHeight(0),
                    sampleDataAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("sampleData")),
                    featuresAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("feature")),
                    keysIndicesAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("keysIndices")),
                    scoresAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("scores")),
                    countersAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("counters")),
                    featureResponsesAllocator(boost::make_shared<cuv::pooled_cuda_allocator>("featureResponses")) {
        assert(configuration.getBoxRadius() > 0);
        assert(configuration.getRegionSize() > 0);

        initDevice();
    }

    std::vector<SplitFunction<PixelInstance, ImageFeatureFunction> > evaluateBestSplits(RandomSource& randomSource,
            const std::vector<std::pair<boost::shared_ptr<RandomTree<PixelInstance, ImageFeatureFunction> >,
                    std::vector<const PixelInstance*> > >& samplesPerNode);

    std::vector<std::vector<const PixelInstance*> > prepare(const std::vector<const PixelInstance*>& samples,
            RandomTree<PixelInstance, ImageFeatureFunction>& node, cuv::host_memory_space);

    std::vector<std::vector<const PixelInstance*> > prepare(const std::vector<const PixelInstance*>& samples,
            RandomTree<PixelInstance, ImageFeatureFunction>& node, cuv::dev_memory_space, bool keepMutexLocked = true);

    ImageFeaturesAndThresholds<cuv::host_memory_space> generateRandomFeatures(
            const std::vector<const PixelInstance*>& batches,
            int seed, const bool sort, cuv::host_memory_space);

    ImageFeaturesAndThresholds<cuv::dev_memory_space> generateRandomFeatures(
            const std::vector<const PixelInstance*>& batches,
            int seed, const bool sort, cuv::dev_memory_space);

    template<class memory_space>
    void sortFeatures(ImageFeaturesAndThresholds<memory_space>& featuresAndThresholds,
            const cuv::ndarray<int, memory_space>& keysIndices) const;

    template<class memory_space>
    cuv::ndarray<WeightType, memory_space> calculateFeatureResponsesAndHistograms(
            RandomTree<PixelInstance, ImageFeatureFunction>& node,
            const std::vector<std::vector<const PixelInstance*> >& batches,
            const ImageFeaturesAndThresholds<memory_space>& featuresAndThresholds,
            cuv::ndarray<FeatureResponseType, cuv::host_memory_space>* featureResponsesHost = 0);

    template<class memory_space>
    cuv::ndarray<ScoreType, cuv::host_memory_space> calculateScores(
            const cuv::ndarray<WeightType, memory_space>& counters,
            const ImageFeaturesAndThresholds<memory_space>& featuresAndThresholds,
            const cuv::ndarray<WeightType, memory_space>& histogram);

private:

    void selectDevice();

    void initDevice();

    void copyFeaturesToDevice();

    Samples<cuv::dev_memory_space> copySamplesToDevice(const std::vector<const PixelInstance*>& samples,
            cudaStream_t stream);

    const ImageFeatureFunction sampleFeature(RandomSource& randomSource,
            const std::vector<const PixelInstance*>&) const;

    const size_t treeId;
    const TrainingConfiguration& configuration;

    unsigned int imageWidth;
    unsigned int imageHeight;

    boost::shared_ptr<cuv::allocator> sampleDataAllocator;
    boost::shared_ptr<cuv::allocator> featuresAllocator;
    boost::shared_ptr<cuv::allocator> keysIndicesAllocator;
    boost::shared_ptr<cuv::allocator> scoresAllocator;
    boost::shared_ptr<cuv::allocator> countersAllocator;
    boost::shared_ptr<cuv::allocator> featureResponsesAllocator;
};

class RandomTreeImage {
public:

    RandomTreeImage(int id, const TrainingConfiguration& configuration);

    RandomTreeImage(boost::shared_ptr<RandomTree<PixelInstance, ImageFeatureFunction> > tree,
            const TrainingConfiguration& configuration,
            const cuv::ndarray<WeightType, cuv::host_memory_space>& classLabelPriorDistribution);

    void train(const std::vector<LabeledRGBDImage>& trainLabelImages,
            RandomSource& randomSource, size_t subsampleCount);

    void test(const RGBDImage* image, LabelImage& prediction) const;

    void normalizeHistograms(const double histogramBias);

    const boost::shared_ptr<RandomTree<PixelInstance, ImageFeatureFunction> >& getTree() const {
        return tree;
    }

    const cuv::ndarray<WeightType, cuv::host_memory_space>& getClassLabelPriorDistribution() const {
        return classLabelPriorDistribution;
    }

    size_t getId() const {
        return id;
    }

    bool shouldIgnoreLabel(const LabelType& label) const;

private:

    void doTrain(RandomSource& randomSource, size_t numClasses,
            std::vector<const PixelInstance*>& subsamples);

    bool finishedTraining;
    size_t id;

    const TrainingConfiguration configuration;

    boost::shared_ptr<RandomTree<PixelInstance, ImageFeatureFunction> > tree;

    cuv::ndarray<WeightType, cuv::host_memory_space> classLabelPriorDistribution;

    void calculateLabelPriorDistribution(const std::vector<LabeledRGBDImage>& trainLabelImages);

    std::vector<PixelInstance> subsampleTrainingDataPixelUniform(
            const std::vector<LabeledRGBDImage>& trainLabelImages,
            RandomSource& randomSource, size_t subsampleCount) const;

    std::vector<PixelInstance> subsampleTrainingDataClassUniform(
            const std::vector<LabeledRGBDImage>& trainLabelImages,
            RandomSource& randomSource, size_t subsampleCount) const;

};

}

std::ostream& operator<<(std::ostream& os, const curfil::RandomTreeImage& tree);

std::ostream& operator<<(std::ostream& os, const curfil::ImageFeatureFunction& featureFunction);

std::ostream& operator<<(std::ostream& os, const curfil::XY& xy);

#endif
