
// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "reconstructed_regions.hpp"

#include <openMVG/features/image_describer.hpp>
#include <nonFree/sift/SIFT_describer.hpp>
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/sfm/pipelines/localization/SfM_Localizer.hpp>
#include <openMVG/stl/stlMap.hpp>
#include <openMVG/voctree/vocabulary_tree.hpp>
#include <openMVG/voctree/database.hpp>


namespace openMVG {
namespace localization {

//@fixme find a better place or maje the class template?
typedef openMVG::features::Descriptor<unsigned char, 128> DescriptorFloat;
typedef Reconstructed_Regions<features::SIOPointFeature, unsigned char, 128> Reconstructed_RegionsT;

class VoctreeLocalizer
{
  
public:
  
  bool init(const std::string &sfmFilePath,
            const std::string &descriptorsFolder,
            const std::string &vocTreeFilepath,
            const std::string &weightsFilepath);
  
  // loadSfmData(const std::string & sfmDataPath)

  /**
   * @brief Load all the Descriptors who have contributed to the reconstruction.
   */
  bool loadReconstructionDescriptors(
    const sfm::SfM_Data & sfm_data,
    const std::string & feat_directory);
  
  /**
  * @brief Try to localize an image in the database
  *
  * @param[in] image_size the w,h image size
  * @param[in] optional_intrinsics camera intrinsic if known (else nullptr)
  * @param[in] query_regions the image regions (type must be the same as the database)
  * @param[out] pose found pose
  * @param[out] resection_data matching data (2D-3D and inliers; optional)
  * @return True if a putative pose has been estimated
  */
  bool Localize( const image::Image<unsigned char> & imageGray,
                const cameras::IntrinsicBase * optional_intrinsics,
                const features::Regions & query_regions,
                geometry::Pose3 & pose,
                sfm::Image_Localizer_Match_Data * resection_data = nullptr);

private:
  /**
   * @brief Load the vocabulary tree.
   */
  bool initDatabase(const std::string & vocTreeFilepath,
                                    const std::string & weightsFilepath,
                                    const std::string & feat_directory);

  
  
public:
  Hash_Map<IndexT, Reconstructed_RegionsT > _regions_per_view;
  
  sfm::SfM_Data _sfm_data;
  
  // the feature extractor
  // @fixme do we want a generic image describer>
  features::SIFT_Image_describer _image_describer;
  
  // the vocabulary tree
  voctree::VocabularyTree<DescriptorFloat> _voctree;
  
  // the database
  voctree::Database _database;
  
  // this maps the docId in the database with the view index of the associated
  // image
  std::map<voctree::DocId, IndexT> _mapDocIdToView;
  
};

}
}
