// hmm/tree-accu.cc

// Copyright 2009-2011 Microsoft Corporation
//                2013 Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//  http://www.apache.org/licenses/LICENSE-2.0

// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.
#include "util/kaldi-io.h"
#include "hmm/tree-accu-2D.h"
//#include "hmm/hmm-utils.h"

namespace kaldi {

bool SplitToPhones_2D(const TransitionModel_2D &trans_model,
		const std::vector<int32> &alignment,
		std::vector<std::vector<int32> > *split_alignment) {
		KALDI_ASSERT(split_alignment != NULL);
		split_alignment->clear();
		if (alignment.empty()) return true;  // nothing to split.
		std::vector<size_t> end_points;  // points at which phones end [in an
		// stl iterator sense, i.e. actually one past the last transition-id within
		// each phone]..
		bool was_ok = true;

		for (size_t i = 0; i < alignment.size() - 1; i++) {
			int32 this_state = alignment[i],
				next_state = alignment[i + 1];
			if (this_state == next_state) continue;  // optimization.
			int32 this_phone = trans_model.TransitionStateToPhone(this_state),
				next_phone = trans_model.TransitionStateToPhone(next_state);
			if (this_phone != next_phone) 
				end_points.push_back(i + 1);
		}

		if (!end_points.size())	end_points.push_back(alignment.size()); // 如果样本只有一个音素

		size_t cur_point = 0;
		for (size_t i = 0; i < end_points.size(); i++) {
			split_alignment->push_back(std::vector<int32>());
			// The next if-statement checks if the initial trans-id at the current end
			// point is the initial-state of the current phone if that initial-state
			// is emitting (a cursory check that the alignment is plausible).
			int32 trans_state = alignment[cur_point];
			int32 phone = trans_model.TransitionStateToPhone(trans_state);
			for (size_t j = cur_point; j < end_points[i]; j++)
				split_alignment->back().push_back(alignment[j]);
			cur_point = end_points[i];
		}
		return was_ok;
	}

	void AccumulateTreeStats_2D(const TransitionModel_2D &trans_model,
		const AccumulateTreeStatsInfo &info,
		const std::vector<int32> &alignment,
		const Matrix<BaseFloat> &features,
		std::map<EventType, GaussClusterable*> *stats) {
		std::vector<std::vector<int32> > split_alignment;
		bool ans = SplitToPhones_2D(trans_model, alignment, &split_alignment);
		if (!ans) {
			// Currently, there is no check done here, always true.
			KALDI_WARN << "AccumulateTreeStats_2D: alignment appears to be bad, not using it";
			return;
		}
		int32 cur_pos = 0;
		int32 dim = features.NumCols();
		KALDI_ASSERT(features.NumRows() == static_cast<int32>(alignment.size()));
		for (int32 i = -info.context_width; i < static_cast<int32>(split_alignment.size()); i++) {
			// consider window starting at i, only if i+info.central_position is within
			// list of phones.
			if (i + info.central_position >= 0 &&
				i + info.central_position < static_cast<int32>(split_alignment.size())) {
				//int32 central_phone = MapPhone(info.phone_map, trans_model.TransitionIdToPhone(
				//			split_alignment[i + info.central_position][0]));
				int32 central_phone = trans_model.TransitionStateToPhone(
					split_alignment[i + info.central_position][0]);

				bool is_ctx_dep = !std::binary_search(info.ci_phones.begin(),
					info.ci_phones.end(),
					central_phone);
				EventType evec;
				for (int32 j = 0; j < info.context_width; j++) {
					int32 phone;
					if (i + j >= 0 && i + j < static_cast<int32>(split_alignment.size()))
						phone = trans_model.TransitionStateToPhone(
							split_alignment[i + j][0]);
					else
						phone = 0;  // ContextDependency class uses 0 to mean "out of window";
					  // we also set the phone arbitrarily to 0

					  // Don't add stuff to the event that we don't "allow" to be asked, due
					  // to the central phone being context-independent: check "is_ctx_dep".
					  // Why not just set the value to zero in this
					  // case?  It's for safety.  By omitting the key from the event, we
					  // ensure that there is no way a question can ever be asked that might
					  // give an inconsistent answer in tree-training versus graph-building.
					  // [setting it to zero would have the same effect given the "normal"
					  // recipe but might be less robust to changes in tree-building recipe].
					if (is_ctx_dep || j == info.central_position)
						evec.push_back(std::make_pair(static_cast<EventKeyType>(j), static_cast<EventValueType>(phone)));
				}
				for (int32 j = 0; j < static_cast<int32>(split_alignment[i + info.central_position].size()); j++) {
					// for central phone of this window...
					EventType evec_more(evec);
					int32 pdf_class = trans_model.TransitionStateToForwardPdfClass(
						split_alignment[i + info.central_position][j]);
					// pdf_class will normally by 0, 1 or 2 for 3-state HMM.
					std::pair<EventKeyType, EventValueType> pr(kPdfClass, pdf_class);
					evec_more.push_back(pr);
					std::sort(evec_more.begin(), evec_more.end());  // these must be sorted!
					if (stats->count(evec_more) == 0)
						(*stats)[evec_more] = new GaussClusterable(dim, info.var_floor);

					BaseFloat weight = 1.0;
					(*stats)[evec_more]->AddStats(features.Row(cur_pos), weight);
					cur_pos++;
				}
			}
		}
		KALDI_ASSERT(cur_pos == static_cast<int32>(alignment.size()));
	}

}  // end namespace kaldi
