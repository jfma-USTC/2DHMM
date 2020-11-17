// bin/show-transitions.cc
//
// Copyright 2009-2011  Microsoft Corporation
//                2014  Johns Hopkins University (author: Daniel Povey)

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

#include "hmm/transition-model.h"
#include "util/common-utils.h"


int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;

    const char *usage =
        "\n"
        "Usage:  show-tied-result [options] <phones-symbol-table> <transition/model-file> <tied-result>\n"
        "e.g.: \n"
        " show-tied-result ark:phones.txt final.mdl ark:tied_result.txt\n"
		" [options]       : Each line in <tied-result> is like\n"
		" --output-mode=1 : ' hmm-state : phone1 phone2 | phone3 phone4 ', where the hmm-state of phone1/2 share the same pdf [Default Value]\n"
		" --output-mode=2 : ' pdf-class : [phone1 hmm-state1] [phone2 hmm-state2] | xxx ', where hmm-state1 of phone1 and hmm-state2 of phone2 share the same pdf\n";

    ParseOptions po(usage);
	int32 output_mode = 1;
	po.Register("output-mode", &output_mode, "Choose which form to show result");// 对ParseOptions对象注册命令行选项(Option的结构体有它自己的注册函数)
    po.Read(argc, argv);

    if (po.NumArgs() != 3) {
      po.PrintUsage();
      exit(1);
    }

    std::string phones_symtab_filename = po.GetArg(1),
        transition_model_filename = po.GetArg(2),
        output_filename = po.GetOptArg(3);

    TransitionModel trans_model;
    ReadKaldiObject(transition_model_filename, &trans_model);
	HmmTopology topo = trans_model.GetTopo();

	SequentialInt32VectorReader phones_symtab_reader(phones_symtab_filename);
	TokenVectorWriter tied_result_writer(output_filename);

	int32 other_error = 1;
	size_t max_phone_id = 0;
	std::vector<std::string> phone_id2phone_name;
	for (; !phones_symtab_reader.Done(); phones_symtab_reader.Next()) {
		std::string phone_name = phones_symtab_reader.Key();
		const std::vector<int32> &phone_id_vector = phones_symtab_reader.Value();
		// phone_id should only contain one id
		if (phone_id_vector.size() != 1) {
			KALDI_WARN << "Not standrd phone list, " << phone_name << " have muliple or zero phone_id.";
			other_error++;
			continue;
		}
		size_t phone_id = static_cast<size_t>(phone_id_vector[0]);
		KALDI_ASSERT(phone_id >= 0 && phone_id <= 100000);
		max_phone_id = std::max(max_phone_id, phone_id);
	}

	phone_id2phone_name.resize(max_phone_id + 1, "KALDI_NOT_A_PHONE");

	SequentialInt32VectorReader phones_symtab_reader2(phones_symtab_filename);
	for (; !phones_symtab_reader2.Done(); phones_symtab_reader2.Next()) {
		std::string phone_name = phones_symtab_reader2.Key();
		const std::vector<int32> &phone_id_vector = phones_symtab_reader2.Value();
		size_t phone_id = static_cast<size_t>(phone_id_vector[0]);

		if (phone_id2phone_name[phone_id] == "KALDI_NOT_A_PHONE") {
			phone_id2phone_name[phone_id] = phone_name;
		}
		else {
			KALDI_WARN << "Phone " << phone_name << " have the same phone-id with " << phone_id2phone_name[phone_id] << ", this should be a error";
			other_error++;
			continue;
		}
	}

	int32 num_pdf = trans_model.NumPdfs(); // 一共有多少的GMM，注意，pdf_id标号从0到num_pdf-1
	std::vector<int32> phone2num_pdf_classes; // 一共phone_num+1个元素，下标从1到phone_num为有效
	std::vector<int32> phone2trans_state; // 一共phone_num+1个元素，下标从1到phone_num为有效
	topo.GetPhoneToNumPdfClasses(&phone2num_pdf_classes);
	//if (max_phone_id != (phone2num_pdf_classes.size() - 1)) {
	//	KALDI_ERR << "phones.txt has " << max_phone_id << " phones, but topo info shows there is only " << phone2num_pdf_classes.size() - 1 << " phones instead.";
	//}
	phone2trans_state.resize(phone2num_pdf_classes.size(), -1); // Store the first trans_state for each phone.
	int32 trans_state_num = 0, // 一共有多少的trans_state
		max_trans_state_num = 0; // 一个音素中最多有多少trans_state
	for (size_t phone = 0; phone < phone2num_pdf_classes.size(); phone++) {
		if (phone2num_pdf_classes[phone] != -1) {
			phone2trans_state[phone] = trans_state_num + 1;
			trans_state_num += phone2num_pdf_classes[phone];
			max_trans_state_num = std::max(phone2num_pdf_classes[phone], max_trans_state_num);
		}
	}
	if (num_pdf > trans_state_num) {
		KALDI_ERR << "trans_model has " << num_pdf << " pdfs, but topo info shows there is only " << trans_state_num << " trans_state instead.";
	}

	if (output_mode == 1) { // hmm-state : phone1 phone2 | phone3 phone4
		std::vector<std::vector<int32> > pdf2phone_list(num_pdf);
		int32 hmm_state = 0;
		while (hmm_state < max_trans_state_num) {
			for (size_t pdf = 0; pdf < num_pdf; pdf++) 
				pdf2phone_list[pdf].clear();
			for (size_t phone_id = 1; phone_id <= phone2num_pdf_classes.size() - 1; phone_id++)
				if (phone2trans_state[phone_id] != -1 && hmm_state < phone2num_pdf_classes[phone_id]) { // 当这个音素有trans_state且存在对应的hmm_state时
					int32 trans_state = phone2trans_state[phone_id] + hmm_state; // 得到了当前音素的当前hmm_state对应的trans_state
					int32 pdf_id = trans_model.TransitionStateToForwardPdf(trans_state);
					KALDI_ASSERT(pdf_id >= 0 && pdf_id < num_pdf);
					pdf2phone_list[pdf_id].push_back(phone_id);
				}
			// 现在我们得到了当前hmm_state对应的所有pdf2phone的二维列表
			for (size_t pdf = 0; pdf < num_pdf; pdf++)
				if (pdf2phone_list[pdf].size()) {
					std::vector<std::string> pdf2phone_list_str;
					for (size_t phone_idx = 0; phone_idx < pdf2phone_list[pdf].size(); phone_idx++) 
						pdf2phone_list_str.push_back(phone_id2phone_name[pdf2phone_list[pdf][phone_idx]]);
					tied_result_writer.Write(std::to_string(hmm_state), pdf2phone_list_str);
				}
			hmm_state++;
		}
	}
	else if (output_mode == 2) { // pdf-class : [phone1 hmm-state1] [phone2 hmm-state2] | xxx
		std::vector<std::vector<std::pair<size_t, size_t> > > pdf2phone_hmm_state_list(num_pdf);
		for (size_t phone_id = 1; phone_id <= phone2num_pdf_classes.size() - 1; phone_id++)
			if (phone2trans_state[phone_id] != -1)
				for (size_t hmm_state = 0; hmm_state < max_trans_state_num; hmm_state++)
					if (hmm_state < phone2num_pdf_classes[phone_id]) { // 当这个音素有trans_state且存在对应的hmm_state时
						int32 trans_state = phone2trans_state[phone_id] + hmm_state; // 得到了当前音素的当前hmm_state对应的trans_state
						int32 pdf_id = trans_model.TransitionStateToForwardPdf(trans_state);
						KALDI_ASSERT(pdf_id >= 0 && pdf_id < num_pdf);
						pdf2phone_hmm_state_list[pdf_id].push_back(std::make_pair(hmm_state, phone_id));
					}
		for (size_t pdf = 0; pdf < num_pdf; pdf++) {
			std::vector<std::string> pdf2phone_hmm_state_list_str;
			std::sort(pdf2phone_hmm_state_list[pdf].begin(), pdf2phone_hmm_state_list[pdf].end());
			for (size_t pair_idx = 0; pair_idx < pdf2phone_hmm_state_list[pdf].size(); pair_idx++) {
				pdf2phone_hmm_state_list_str.push_back(phone_id2phone_name[pdf2phone_hmm_state_list[pdf][pair_idx].second]);
				pdf2phone_hmm_state_list_str.push_back(std::to_string(pdf2phone_hmm_state_list[pdf][pair_idx].first));
			}
			tied_result_writer.Write(std::to_string(pdf), pdf2phone_hmm_state_list_str);
		}
	}
	else {
		KALDI_ERR << "output_mode should be 1 or 2, not " << output_mode ;
	}

  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

