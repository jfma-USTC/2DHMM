// gmmbin/gmm-align.cc

// Copyright 2009-2012  Microsoft Corporation
//           2012-2014 Johns Hopkins University (Author: Daniel Povey)

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
#include "hmm/transition-model-2D.h"

//#define SHOW_DETAIL

namespace kaldi {
	template <class T>
	void print_vector(std::vector<T> &VECTOR, std::string vector_name = "", int col_num = -1) {
		if (vector_name != "")
			std::cout << vector_name << " : \n";
		if (col_num == -1) {
			for (size_t i = 0; i < VECTOR.size(); i++) {
				std::cout << VECTOR[i] << " ";
			}
			std::cout << "\n";
		}
		else if (VECTOR.size() % col_num != 0) {
			std::cout << "this vetor, size() = " << VECTOR.size() << " cannot exact divide " << col_num << "\n";
			for (size_t i = 0; i < VECTOR.size(); i++) {
				std::cout << VECTOR[i] << " ";
			}
			std::cout << "\n";
		}
		else {
			for (size_t i = 0; i < VECTOR.size(); i++) {
				std::cout << VECTOR[i] << " ";
				if ((i + 1) % col_num == 0) {
					std::cout << "\n";
				}
			}
		}
		std::cout << "\n";
	}
} // end namespace kaldi

int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage =
			"Given most likely phones_id sequences to samples, compute wer .\n"
			"Usage:   gmm-wer-2D [options] <phones-id-rspecifier> <transcriptions-rspecifier> <out-wer-file>\n"
			"e.g.: \n"
			" gmm-wer-2D 1.phones_id \"ark:text2phone.pl lexicon.txt phones.txt text wer.txt\" \n"
			"[options]:\n"
			"--top-num1=N1                  print topN1 WER to log file\n";
		ParseOptions po(usage);
		int32 top_num1 = -1;

		po.Register("top-num1", &top_num1, "print top(top-num1) WER to log file");

		po.Read(argc, argv);

		if (po.NumArgs() != 3) {
			po.PrintUsage();
			exit(1);
		}

		std::string phones_seq_rspecifier = po.GetArg(1);
		std::string transcript_rspecifier = po.GetArg(2);
		std::string out_wer_file = po.GetArg(3);

		SequentialInt32VectorReader phones_seq_reader(phones_seq_rspecifier);
		RandomAccessInt32VectorReader transcript_reader(transcript_rspecifier);
		std::fstream wer_file;
		wer_file.open(out_wer_file.c_str(), std::ios::out);
		if (!wer_file) {
			fprintf(stdout, "can not open %s!\n", out_wer_file.c_str());
			return -1;
		}

		int32 num_done = 0, num_err = 0;
		int32 top1_right_num = 0,
			top5_right_num = 0,
			top10_right_num = 0,
			top_num1_right_num = 0;
		if (top_num1 == 1 || top_num1 == 5 || top_num1 == 10) {
			KALDI_WARN << "top_num1 " << top_num1 << " already exist in default topN num\n";
			top_num1 = -1;
		}
		for (; !phones_seq_reader.Done(); phones_seq_reader.Next()) {
			std::string utt = phones_seq_reader.Key();
			const std::vector<int32> &phones_seq = phones_seq_reader.Value();
			if (phones_seq.size() == 0) {
				KALDI_WARN << "No phones_seq found for utterance " << utt;
				num_err++;
				continue;
			}
			// 如果这个sample没有text信息则跳过
			if (!transcript_reader.HasKey(utt)) {
				KALDI_WARN << "No transcript found for utterance " << utt;
				num_err++;
				continue;
			}
			const std::vector<int32> &transcript = transcript_reader.Value(utt);
			//TODO:暂时不接受超过一个phone的sample
			if (transcript.size() != 1) {
				KALDI_WARN << "Currently could only handle single phone for each sample, but get "
					<< transcript.size() << " phones for " << utt;
				num_err++;
				continue;
			}
			int32 true_phone_id = transcript[0]; //该样本对应的phone
			if (phones_seq[0] == true_phone_id)
				top1_right_num++;
			for (int32 i = 0; i < std::min(5, static_cast<int32>(phones_seq.size())); i++)
				if (phones_seq[i] == true_phone_id)
					top5_right_num++;
			for (int32 i = 0; i < std::min(10, static_cast<int32>(phones_seq.size())); i++)
				if (phones_seq[i] == true_phone_id)
					top10_right_num++;
			for (int32 i = 0; i < std::min(top_num1, static_cast<int32>(phones_seq.size())); i++)
				if (phones_seq[i] == true_phone_id)
					top_num1_right_num++;
			num_done++;
		}
		BaseFloat top1_wer = 1 - ((BaseFloat)top1_right_num) / ((BaseFloat)num_done);
		BaseFloat top5_wer = 1 - ((BaseFloat)top5_right_num) / ((BaseFloat)num_done);
		BaseFloat top10_wer = 1 - ((BaseFloat)top10_right_num) / ((BaseFloat)num_done);
		BaseFloat top_num1_wer = 1 - ((BaseFloat)top_num1_right_num) / ((BaseFloat)num_done);
		if (top_num1 == -1) {
			wer_file << "Done samples: "  << num_done << std::endl;
			wer_file << "Error samples: " << num_err << std::endl;
			wer_file << "Top1_right samples: " << top1_right_num << std::endl;
			wer_file << "Top5_right samples: " << top5_right_num << std::endl;
			wer_file << "Top10_right samples: " << top10_right_num << std::endl;
			wer_file << "Top1_wer: " << top1_wer << std::endl;
			wer_file << "Top5_wer: " << top5_wer << std::endl;
			wer_file << "Top10_wer: " << top10_wer << std::endl;
		} else {
			wer_file << "Done samples: " << num_done << std::endl;
			wer_file << "Error samples: " << num_err << std::endl;
			wer_file << "Top1_right samples: " << top1_right_num << std::endl;
			wer_file << "Top5_right samples: " << top5_right_num << std::endl;
			wer_file << "Top10_right samples: " << top10_right_num << std::endl;
			wer_file << "Top"<< top_num1 <<"_right samples: " << top_num1_right_num << std::endl;
			wer_file << "Top1_wer: " << top1_wer << std::endl;
			wer_file << "Top5_wer: " << top5_wer << std::endl;
			wer_file << "Top10_wer: " << top10_wer << std::endl;
			wer_file << "Top" << top_num1 << "_wer: " << top_num1_wer << std::endl;
		}
		return (num_done != 0 ? 0 : 1);
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}