// gmmbin/gmm-init-model.cc

// Copyright 2009-2012  Microsoft Corporation  Johns Hopkins University (Author: Daniel Povey)
//                     Johns Hopkins University  (author: Guoguo Chen)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "gmm/am-diag-gmm.h"
#include "hmm/transition-model.h"
//#include "hmm/transition-model-2D.h"
#include "gmm/mle-am-diag-gmm.h"
#include "tree/build-tree-utils.h"
#include "tree/context-dep.h"
#include "tree/clusterable-classes.h"
#include "util/text-utils.h"

//#define SHOW_DETAIL

namespace kaldi {

	void SplitStatsByPdfList(const BuildTreeStatsType &stats,
		const std::map<std::pair<int32, int32>, int32> &map_phone_state_to_pdf,
		// [[phone, hmm_state], pdf_id]
		std::vector<BuildTreeStatsType> *stats_out) {
		BuildTreeStatsType::const_iterator iter, end = stats.end();
		KALDI_ASSERT(stats_out != NULL);
		stats_out->clear();
		size_t pdf_num = 0;
		for (iter = stats.begin(); iter != end; ++iter) {
			const EventType &evec = iter->first;
			int32 phone = 0, hmm_state = -1;
			for (size_t i = 0; i < evec.size(); i++) {
				if (evec[i].first == -1) {
					hmm_state = evec[i].second;
				}
				else if (evec[i].first == 0) {
					phone = evec[i].second;
				}
				else KALDI_ERR << "Appear invalid EventKeyType " << evec[i].first
					<< " in BuildTreeStats stats";
			}
			if (phone == 0 || hmm_state == -1)
				KALDI_ERR << " No phone or hmm_state were seen";
			EventAnswerType pdf;
			//std::map<std::pair<int32, int32>, int32>::iterator iter;
			auto map_iter = map_phone_state_to_pdf.find(std::make_pair(phone, hmm_state));
			if (map_iter == map_phone_state_to_pdf.end()) {
				KALDI_ERR << "SplitStatsByPdfList: could not map event vector " << EventTypeToString(evec)
					<< "if error seen during tree-building, check that "
					<< "--context-width and --central-position match stats, "
					<< "and that phones that are context-independent (CI) during "
					<< "stats accumulation do not share roots with non-CI phones.";
			}
			pdf = map_iter->second;
			pdf_num = std::max(pdf_num, (size_t)(pdf + 1));
		}
		stats_out->resize(pdf_num);
		for (iter = stats.begin(); iter != end; ++iter) {
			const EventType &evec = iter->first;
			int32 phone = 0, hmm_state = -1;
			for (size_t i = 0; i < evec.size(); i++) {
				if (evec[i].first == -1) {
					hmm_state = evec[i].second;
				}
				else if (evec[i].first == 0) {
					phone = evec[i].second;
				}
				else KALDI_ERR << "Appear invalid EventKeyType " << evec[i].first
					<< " in BuildTreeStats stats";
			}
			if (phone == 0 || hmm_state == -1)
				KALDI_ERR << " No phone or hmm_state were seen";
			EventAnswerType pdf;
			//std::map<std::pair<int32, int32>, int32>::iterator iter;
			auto map_iter = map_phone_state_to_pdf.find(std::make_pair(phone, hmm_state));
			if (map_iter == map_phone_state_to_pdf.end()) {
				KALDI_ERR << "SplitStatsByPdfList: could not map event vector " << EventTypeToString(evec)
					<< "if error seen during tree-building, check that "
					<< "--context-width and --central-position match stats, "
					<< "and that phones that are context-independent (CI) during "
					<< "stats accumulation do not share roots with non-CI phones.";
			}
			pdf = map_iter->second;
			(*stats_out)[pdf].push_back(*iter);
		}
	}


	/// InitAmGmm initializes the GMM with one Gaussian per state.
	void InitAmGmm_from_pdf_list_2D(const BuildTreeStatsType &stats,
		const std::map<std::pair<int32, int32>, int32> &map_phone_state_to_pdf,
		AmDiagGmm *am_gmm,
		const TransitionModel &trans_model,
		BaseFloat var_floor) {
		// Get stats split by tree-leaf ( == pdf):
		std::vector<BuildTreeStatsType> split_stats;
		SplitStatsByPdfList(stats, map_phone_state_to_pdf, &split_stats);

		size_t max_pdf_id_add_1 = 0;
		auto iter = map_phone_state_to_pdf.begin();
		while (iter != map_phone_state_to_pdf.end()) {
			int32 pdf_id = iter->second;
			max_pdf_id_add_1 = std::max(max_pdf_id_add_1, (size_t)(pdf_id + 1));
			iter++;
		}
		split_stats.resize(max_pdf_id_add_1); // ensure that
		// if the last leaf had no stats, this vector still has the right size.

		// Make sure each leaf has stats.
		for (size_t i = 0; i < split_stats.size(); i++) {
			if (split_stats[i].empty()) {
				std::vector<int32> bad_pdfs(1, i), bad_phones;
				GetPhonesForPdfs(trans_model, bad_pdfs, &bad_phones);
				std::ostringstream ss;
				for (int32 idx = 0; idx < bad_phones.size(); idx++)
					ss << bad_phones[idx] << ' ';
				KALDI_WARN << "AcumulateStats has pdf-id " << i
					<< " with no stats; corresponding phone list: " << ss.str();
				/*
				  This probably means you have phones that were unseen in training
				  and were not shared with other phones in the roots file.
				  You should modify your roots file as necessary to fix this.
				  (i.e. share that phone with a similar but seen phone on one line
				  of the roots file). Be sure to regenerate roots.int from roots.txt,
				  if using s5 scripts. To work out the phone, search for
				  pdf-id  i  in the output of show-transitions (for this model). */
			}
		}
		std::vector<Clusterable*> summed_stats;
		SumStatsVec(split_stats, &summed_stats);
		Clusterable *avg_stats = SumClusterable(summed_stats);
		KALDI_ASSERT(avg_stats != NULL && "No stats available in gmm-init-model.");
		for (size_t i = 0; i < summed_stats.size(); i++) {
			GaussClusterable *c =
				static_cast<GaussClusterable*>(summed_stats[i] != NULL ? summed_stats[i] : avg_stats);
			DiagGmm gmm(*c, var_floor);
			am_gmm->AddPdf(gmm);
			BaseFloat count = c->count();
			if (count < 100) {
				std::vector<int32> bad_pdfs(1, i), bad_phones;
				GetPhonesForPdfs(trans_model, bad_pdfs, &bad_phones);
				std::ostringstream ss;
				for (int32 idx = 0; idx < bad_phones.size(); idx++)
					ss << bad_phones[idx] << ' ';
				KALDI_WARN << "Very small count for state " << i << ": "
					<< count << "; corresponding phone list: " << ss.str();
			}
		}
		DeletePointers(&summed_stats);
		delete avg_stats;
	}

	/// Get state occupation counts.
	void GetOccs_from_pdf_list_2D(const BuildTreeStatsType &stats,
		const std::map<std::pair<int32, int32>, int32> &map_phone_state_to_pdf,
		Vector<BaseFloat> *occs) {

		// Get stats split by tree-leaf ( == pdf):
		std::vector<BuildTreeStatsType> split_stats;
		SplitStatsByPdfList(stats, map_phone_state_to_pdf, &split_stats);
		size_t max_pdf_id_add_1 = 0;
		auto iter = map_phone_state_to_pdf.begin();
		while (iter != map_phone_state_to_pdf.end()) {
			int32 pdf_id = iter->second;
			max_pdf_id_add_1 = std::max(max_pdf_id_add_1, (size_t)(pdf_id + 1));
			iter++;
		}
		if (split_stats.size() != max_pdf_id_add_1) {
			KALDI_ASSERT(split_stats.size() < max_pdf_id_add_1);
			split_stats.resize(max_pdf_id_add_1);
		}
		occs->Resize(split_stats.size());
		for (int32 pdf = 0; pdf < occs->Dim(); pdf++)
			(*occs)(pdf) = SumNormalizer(split_stats[pdf]);
	}

}

int main(int argc, char *argv[]) {
	using namespace kaldi;
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage =
			"Initialize GMM from pdf-list file and tree stats\n"
			"Usage:  gmm-init-model-from-pdf-list [options] <pdf-list> <tree-stats-in> <topo-file> <model-out>\n"
			"NOTE that each line of <pdf-list> is a vector of [phone_id hmm_state], and they share the same pdf.\n"
			"e.g. 4 0 18 0 22 2 \n 4 1 6 1 14 1 \n ..."
			"e.g.: \n"
			"  gmm-init-model-from-pdf-list pdf-list-file treeacc topo 1.mdl\n";

		bool binary = true;
		double var_floor = 0.01;
		std::string occs_out_filename;


		ParseOptions po(usage);
		po.Register("binary", &binary, "Write output in binary mode");
		po.Register("write-occs", &occs_out_filename, "File to write state "
			"occupancies to.");
		po.Register("var-floor", &var_floor, "Variance floor used while "
			"initializing Gaussians");

		po.Read(argc, argv);

		if (po.NumArgs() != 4 && po.NumArgs() != 6) {
			po.PrintUsage();
			exit(1);
		}

		std::string
			pdf_list_filename = po.GetArg(1),
			stats_filename = po.GetArg(2),
			topo_filename = po.GetArg(3),
			model_out_filename = po.GetArg(4);

		std::vector<std::vector< int32 > > pdf_list_raw;
		if (!ReadIntegerVectorVectorSimple(pdf_list_filename, &pdf_list_raw))
			KALDI_ERR << "Could not read pdf_list from "
			<< PrintableRxfilename(pdf_list_filename);

		BuildTreeStatsType stats;
		{
			bool binary_in;
			GaussClusterable gc;  // dummy needed to provide type.
			Input ki(stats_filename, &binary_in);
			ReadBuildTreeStats(ki.Stream(), binary_in, gc, &stats);
		}
		KALDI_LOG << "Number of separate statistics is " << stats.size();

		HmmTopology topo;
		ReadKaldiObject(topo_filename, &topo);

		std::vector<std::vector<std::pair<int32, int32> > > pdf_list;
		int32 not_empty_line = 0;
		for (size_t i = 0; i < pdf_list_raw.size(); i++)
			if (!pdf_list_raw[i].empty()) {
				for (size_t j = 0; j < pdf_list_raw[i].size(); j += 2) {
					int32 phone = pdf_list_raw[i][j];
					if (j + 1 == pdf_list_raw.size())
						KALDI_ERR << "pdf_list_raw has even number in line " << i + 1;
					int32 hmm_state = pdf_list_raw[i][j + 1];
					pdf_list.resize(not_empty_line + 1);
					pdf_list[not_empty_line].push_back(std::make_pair(phone, hmm_state));
				}
				not_empty_line++;
			}

#ifdef SHOW_DETAIL
		std::cout << "Start build model\n";
#endif // SHOW_DETAIL

		TransitionModel trans_model(pdf_list, topo);

#ifdef SHOW_DETAIL
		std::cout << "End build model\n";
#endif // SHOW_DETAIL

		std::map<std::pair<int32, int32>, int32> map_phone_state_to_pdf; // map from [phone hmm_state] to pdf_id

		const std::vector<int32> &phones = topo.GetPhones(); // 注意，这里的phones都是按升序排序好的，e.g. 1 2 3 4 5 ... 3000
		KALDI_ASSERT(!phones.empty());
		int32 max_phone_id = *std::max_element(phones.begin(), phones.end());
		std::vector<int32> num_pdf_classes(1 + max_phone_id, -1);
		for (size_t i = 0; i < phones.size(); i++)
			num_pdf_classes[phones[i]] = topo.NumPdfClasses(phones[i]);

		for (size_t i = 0; i < phones.size(); i++) {
			int32 phone = phones[i];
			for (size_t j = 0; j < num_pdf_classes[phone]; j++) {
				int32 hmm_state = j;
				int32 trans_state = trans_model.PairToState(phone, hmm_state);
				if (trans_state == -1) KALDI_ERR << "Cannot map pair(" << phone << " " << hmm_state << ") to trans_state";
				int32 pdf_id = trans_model.TransitionStateToForwardPdf(trans_state);
				map_phone_state_to_pdf[std::make_pair(phone, hmm_state)] = pdf_id;
			}
		}

#ifdef SHOW_DETAIL
		std::cout << "Start InitAmGmm\n";
#endif // SHOW_DETAIL

		// Now, the summed_stats will be used to initialize the GMM.
		AmDiagGmm am_gmm;
		InitAmGmm_from_pdf_list_2D(stats, map_phone_state_to_pdf, &am_gmm, trans_model, var_floor);  // Normal case: initialize 1 Gauss/model from tree stats.

#ifdef SHOW_DETAIL
		std::cout << "End InitAmGmm\n";
#endif // SHOW_DETAIL

		if (!occs_out_filename.empty()) {  // write state occs
			Vector<BaseFloat> occs;
			GetOccs_from_pdf_list_2D(stats, map_phone_state_to_pdf, &occs);
			Output ko(occs_out_filename, binary);
			occs.Write(ko.Stream(), binary);
		}

#ifdef SHOW_DETAIL
		std::cout << "End GetOccs\n";
#endif // SHOW_DETAIL

		{
			Output ko(model_out_filename, binary);
			trans_model.Write(ko.Stream(), binary);
			am_gmm.Write(ko.Stream(), binary);
		}
		KALDI_LOG << "Wrote model.";

		DeleteBuildTreeStats(&stats);
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}

