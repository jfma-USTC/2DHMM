// bin/cluster-phones.cc

// Copyright 2009-2011  Microsoft Corporation

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
#include "tree/context-dep.h"
#include "tree/build-tree.h"
#include "tree/build-tree-utils.h"
#include "tree/context-dep.h"
#include "tree/clusterable-classes.h"
#include "util/text-utils.h"

int main(int argc, char *argv[]) {
	using namespace kaldi;
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage =
			"Data-driven only. Create a pdf-list file, each line is vector of pair [phone hmm_state], and they share the same pdf.\n"
			"Usage:  cluster-bottom-up-compart-to-pdf-list [options] <tree-stats-in> <pdf-list>\n"
			"e.g.: \n"
			" cluster-bottom-up-compart-to-pdf-list --k-means-point=200 1.tacc pdf-list\n";

		int32 max_clust = -1;
		BaseFloat cluster_thresh = 0;

		ParseOptions po(usage);
		po.Register("max-clust", &max_clust, " Maximum number of clusters\n");
		po.Register("cluster-thresh", &cluster_thresh, "provides an alternative mechanism [other than max_clust] to limit the number of leaves.\n");

		po.Read(argc, argv);

		if (po.NumArgs() != 2) {
			po.PrintUsage();
			exit(1);
		}

		std::string stats_rxfilename = po.GetArg(1),
			pdf_list_wxfilename = po.GetArg(2);

		// typedef std::vector<std::pair<EventType, Clusterable*> > BuildTreeStatsType;
		BuildTreeStatsType stats;
		{  // Read tree stats.
			bool binary_in;
			GaussClusterable gc;  // dummy needed to provide type. 
								  // temp var, used to provide type for Clusterable
			Input ki(stats_rxfilename, &binary_in);
			ReadBuildTreeStats(ki.Stream(), binary_in, gc, &stats);
		}
		
		std::vector<std::vector<Clusterable* > > points;
		std::vector<std::vector<size_t > > points_assgin_to_stats;
		BuildTreeStatsType::const_iterator iter = stats.begin(), end = stats.end();
		int32 max_hmm_state_num = -1;
		for (; iter != end; ++iter) {
			const EventType &evec = iter->first;
			for (size_t i = 0; i < evec.size(); i++)
				if (evec[i].first == -1) max_hmm_state_num = std::max(max_hmm_state_num, evec[i].second + 1);
		}
		points.resize(max_hmm_state_num);
		points_assgin_to_stats.resize(max_hmm_state_num);
		iter = stats.begin();
		for (size_t i = 0; iter != end; ++iter, i++) {
			const EventType &evec = iter->first;
			for (size_t j = 0; j < evec.size(); j++)
				if (evec[j].first == -1) {
					points[evec[j].second].push_back(iter->second);
					points_assgin_to_stats[evec[j].second].push_back(i);
				}
		}

		std::vector<std::vector<int32 > > assignment;

		ClusterBottomUpCompartmentalized(points, cluster_thresh, max_clust, NULL, &assignment);

		std::vector<std::vector<int32> > pdf_list;
		for (size_t comp = 0; comp < max_hmm_state_num; comp++) {
			std::vector<std::vector<int32> > pdf_list_temp;
			pdf_list_temp.resize(*std::max_element(assignment[comp].begin(), assignment[comp].end()) + 1);
			for (size_t i = 0; i < assignment[comp].size(); i++) {
				const EventType &evec = stats[points_assgin_to_stats[comp][i]].first;
				int32 phone = 0, hmm_state = -1;
				for (size_t i = 0; i < evec.size(); i++)
					if (evec[i].first == -1) hmm_state = evec[i].second;
					else if (evec[i].first == 0) phone = evec[i].second;
					else KALDI_ERR << "Appear invalid EventKeyType " << evec[i].first
						<< " in BuildTreeStats stats";
				if (phone == 0 || hmm_state == -1)
					KALDI_ERR << " No phone or hmm_state were seen";
				pdf_list_temp[assignment[comp][i]].push_back(phone);
				pdf_list_temp[assignment[comp][i]].push_back(hmm_state); //将属于聚类到用一个类的音素放到同一行
			}
			int32 valid_size = 0;
			for (size_t i = 0; i < pdf_list_temp.size(); i++)
				if (pdf_list_temp[i].size())
					valid_size++;
			pdf_list.resize(pdf_list.size() + valid_size);
			for (size_t i = pdf_list.size() - valid_size, j = 0; j < valid_size; i++, j++)
				pdf_list[i].swap(pdf_list_temp[j]);
		}

		if (!WriteIntegerVectorVectorSimple(pdf_list_wxfilename, pdf_list))
			KALDI_ERR << "Error writing vector of vector of pair [phone hmm_state] to "
			<< PrintableWxfilename(pdf_list_wxfilename);
		else
			KALDI_LOG << "Wrote vector of vector of pair [phone hmm_state] to " << pdf_list_wxfilename;

		DeleteBuildTreeStats(&stats);
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}

