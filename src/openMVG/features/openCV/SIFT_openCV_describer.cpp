#include "SIFT_openCV_describer.hpp"

#include "openMVG/image/image.hpp"
#include "openMVG/system/timer.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/xfeatures2d.hpp>

namespace openMVG {
namespace features {

bool SIFT_openCV_Params::Set_configuration_preset(EDESCRIBER_PRESET preset)
{
    switch(preset)
    {
      case LOW_PRESET:
        contrastThreshold = 0.01;
        maxTotalKeypoints = 1000;
        break;
      case MEDIUM_PRESET:
        contrastThreshold = 0.005;
        maxTotalKeypoints = 5000;
        break;
      case NORMAL_PRESET:
        contrastThreshold = 0.005;
        edgeThreshold = 15;
        maxTotalKeypoints = 10000;
        break;
      case HIGH_PRESET:
        contrastThreshold = 0.005;
        edgeThreshold = 20;
        maxTotalKeypoints = 20000;
        break;
      case ULTRA_PRESET:
        contrastThreshold = 0.005;
        edgeThreshold = 20;
        maxTotalKeypoints = 40000;
        break;
    }
    return true;
}

bool SIFT_openCV_ImageDescriber::Describe(const image::Image<unsigned char>& image,
                                          std::unique_ptr<Regions> &regions,
                                          const image::Image<unsigned char> * mask)
{
  // Convert for opencv
  cv::Mat img;
  cv::eigen2cv(image.GetMat(), img);

  // Create a SIFT detector
  std::vector< cv::KeyPoint > v_keypoints;
  cv::Mat m_desc;
  std::size_t maxDetect = 0; //< No max value by default
  if(_params.maxTotalKeypoints)
    if(!_params.gridSize) //< If no grid filtering, use opencv to limit the number of features
      maxDetect = _params.maxTotalKeypoints;

  cv::Ptr<cv::Feature2D> siftdetector = cv::xfeatures2d::SIFT::create(maxDetect, _params.nOctaveLayers, _params.contrastThreshold, _params.edgeThreshold, _params.sigma);

  // Detect SIFT keypoints
  auto detect_start = std::chrono::steady_clock::now();
  siftdetector->detect(img, v_keypoints);
  auto detect_end = std::chrono::steady_clock::now();
  auto detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);

  std::cout << "SIFT: contrastThreshold: " << _params.contrastThreshold << ", edgeThreshold: " << _params.edgeThreshold << std::endl;
  std::cout << "Detect SIFT: " << detect_elapsed.count() << " milliseconds." << std::endl;
  std::cout << "Image size: " << img.cols << " x " << img.rows << std::endl;
  std::cout << "Grid size: " << _params.gridSize << ", maxTotalKeypoints: " << _params.maxTotalKeypoints << std::endl;
  std::cout << "Number of detected features: " << v_keypoints.size() << std::endl;

  // cv::KeyPoint::response: the response by which the most strong keypoints have been selected.
  // Can be used for the further sorting or subsampling.
  std::sort(v_keypoints.begin(), v_keypoints.end(), [](const cv::KeyPoint& a, const cv::KeyPoint& b) { return a.size > b.size; });

  // Grid filtering of the keypoints to ensure a global repartition
  if(_params.gridSize && _params.maxTotalKeypoints)
  {
    // Only filter features if we have more features than the maxTotalKeypoints
    if(v_keypoints.size() > _params.maxTotalKeypoints)
    {
      std::vector< cv::KeyPoint > filtered_keypoints;
      std::vector< cv::KeyPoint > rejected_keypoints;
      filtered_keypoints.reserve(std::min(v_keypoints.size(), _params.maxTotalKeypoints));
      rejected_keypoints.reserve(v_keypoints.size());

      cv::Mat countFeatPerCell(_params.gridSize, _params.gridSize, cv::DataType<std::size_t>::type, cv::Scalar(0));
      const std::size_t keypointsPerCell = _params.maxTotalKeypoints / countFeatPerCell.total();
      const double regionWidth = image.Width() / double(countFeatPerCell.cols);
      const double regionHeight = image.Height() / double(countFeatPerCell.rows);

      std::cout << "Grid filtering -- keypointsPerCell: " << keypointsPerCell
                << ", regionWidth: " << regionWidth
                << ", regionHeight: " << regionHeight << std::endl;

      for(const cv::KeyPoint& keypoint: v_keypoints)
      {
        const std::size_t cellX = std::min(std::size_t(keypoint.pt.x / regionWidth), _params.gridSize);
        const std::size_t cellY = std::min(std::size_t(keypoint.pt.y / regionHeight), _params.gridSize);
        // std::cout << "- keypoint.pt.x: " << keypoint.pt.x << ", keypoint.pt.y: " << keypoint.pt.y << std::endl;
        // std::cout << "- cellX: " << cellX << ", cellY: " << cellY << std::endl;
        // std::cout << "- countFeatPerCell: " << countFeatPerCell << std::endl;
        // std::cout << "- gridSize: " << _params.gridSize << std::endl;

        const std::size_t count = countFeatPerCell.at<std::size_t>(cellX, cellY);
        countFeatPerCell.at<std::size_t>(cellX, cellY) = count + 1;
        if(count < keypointsPerCell)
          filtered_keypoints.push_back(keypoint);
        else
          rejected_keypoints.push_back(keypoint);
      }
      // If we don't have enough features (less than maxTotalKeypoints) after the grid filtering (empty regions in the grid for example).
      // We add the best other ones, without repartition constraint.
      if( filtered_keypoints.size() < _params.maxTotalKeypoints )
      {
        const std::size_t remainingElements = std::min(rejected_keypoints.size(), _params.maxTotalKeypoints - filtered_keypoints.size());
        std::cout << "Grid filtering -- Copy remaining points: " << remainingElements << std::endl;
        filtered_keypoints.insert(filtered_keypoints.end(), rejected_keypoints.begin(), rejected_keypoints.begin() + remainingElements);
      }

      v_keypoints.swap(filtered_keypoints);
    }
  }
  std::cout << "Number of features: " << v_keypoints.size() << std::endl;

  // Compute SIFT descriptors
  auto desc_start = std::chrono::steady_clock::now();
  siftdetector->compute(img, v_keypoints, m_desc);
  auto desc_end = std::chrono::steady_clock::now();
  auto desc_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(desc_end - desc_start);
  std::cout << "Compute descriptors: " << desc_elapsed.count() << " milliseconds." << std::endl;

  Allocate(regions);

  // Build alias to cached data
  SIFT_Regions * regionsCasted = dynamic_cast<SIFT_Regions*>(regions.get());
  // Reserve some memory for faster keypoint saving
  regionsCasted->Features().reserve(v_keypoints.size());
  regionsCasted->Descriptors().reserve(v_keypoints.size());

  // Prepare a column vector with the sum of each descriptor
  cv::Mat m_siftsum;
  cv::reduce(m_desc, m_siftsum, 1, cv::REDUCE_SUM);

  // Copy keypoints and descriptors in the regions
  int cpt = 0;
  for(std::vector< cv::KeyPoint >::const_iterator i_kp = v_keypoints.begin();
      i_kp != v_keypoints.end();
      ++i_kp, ++cpt)
  {
    SIOPointFeature feat((*i_kp).pt.x, (*i_kp).pt.y, (*i_kp).size, (*i_kp).angle);
    regionsCasted->Features().push_back(feat);

    Descriptor<unsigned char, 128> desc;
    for(int j = 0; j < 128; j++)
    {
      desc[j] = static_cast<unsigned char>(512.0*sqrt(m_desc.at<float>(cpt, j)/m_siftsum.at<float>(cpt, 0)));
    }
    regionsCasted->Descriptors().push_back(desc);
  }

  return true;
}

} //namespace features
} //namespace openMVG
