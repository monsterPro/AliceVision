
// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "openMVG/matching_image_collection/Matcher_Regions_AllInMemory.hpp"
#include "openMVG/matching/matcher_brute_force.hpp"
#include "openMVG/matching/matcher_kdtree_flann.hpp"
#include "openMVG/matching/matcher_cascade_hashing.hpp"
#include "openMVG/matching/regions_matcher.hpp"
#include "openMVG/matching_image_collection/Matcher.hpp"
#include <openMVG/config.hpp>

#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "third_party/progress/progress.hpp"

namespace openMVG {
namespace matching_image_collection {

using namespace openMVG::matching;
using namespace openMVG::features;

ImageCollectionMatcher_Generic::ImageCollectionMatcher_Generic(
  float distRatio, EMatcherType matcherType)
  : IImageCollectionMatcher()
  , _f_dist_ratio(distRatio)
  , _matcherType(matcherType)
{
}

void ImageCollectionMatcher_Generic::Match(
  const sfm::SfM_Data & sfm_data,
  const features::RegionsPerView& regionsPerView,
  const Pair_Set & pairs,
  features::EImageDescriberType descType,
  matching::PairwiseMatches & map_PutativesMatches)const // the pairwise photometric corresponding points
{
#if OPENMVG_IS_DEFINED(OPENMVG_USE_OPENMP)
  OPENMVG_LOG_DEBUG("Using the OPENMP thread interface");
#endif
  const bool b_multithreaded_pair_search = (_matcherType == CASCADE_HASHING_L2);
  // -> set to true for CASCADE_HASHING_L2, since OpenMP instructions are not used in this matcher

  C_Progress_display my_progress_bar( pairs.size() );

  // Sort pairs according the first index to minimize the MatcherT build operations
  typedef std::map<size_t, std::vector<size_t> > Map_vectorT;
  Map_vectorT map_Pairs;
  for (Pair_Set::const_iterator iter = pairs.begin(); iter != pairs.end(); ++iter)
  {
    map_Pairs[iter->first].push_back(iter->second);
  }

  // Perform matching between all the pairs
  for (Map_vectorT::const_iterator iter = map_Pairs.begin();
    iter != map_Pairs.end(); ++iter)
  {
    const size_t I = iter->first;
    const std::vector<size_t> & indexToCompare = iter->second;

    const features::Regions & regionsI = regionsPerView.getRegions(I, descType);
    if (regionsI.RegionCount() == 0)
    {
      my_progress_bar += indexToCompare.size();
      continue;
    }

    // Initialize the matching interface
    matching::RegionsDatabaseMatcher matcher(_matcherType, regionsI);

    #pragma omp parallel for schedule(dynamic) if(b_multithreaded_pair_search)
    for (int j = 0; j < (int)indexToCompare.size(); ++j)
    {
      const size_t J = indexToCompare[j];

      const features::Regions &regionsJ = regionsPerView.getRegions(J, descType);
      if (regionsJ.RegionCount() == 0
          || regionsI.Type_id() != regionsJ.Type_id())
      {
        #pragma omp critical
        ++my_progress_bar;
        continue;
      }

      IndMatches vec_putatives_matches;
      matcher.Match(_f_dist_ratio, regionsJ, vec_putatives_matches);
      #pragma omp critical
      {
        ++my_progress_bar;
        if (!vec_putatives_matches.empty())
        {
          map_PutativesMatches[std::make_pair(I,J)].emplace(descType, std::move(vec_putatives_matches));
        }
      }
    }
  }
}

} // namespace openMVG
} // namespace matching_image_collection
