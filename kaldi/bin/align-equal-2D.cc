// bin/align-equal.cc

// Copyright 2009-2013  Microsoft Corporation
//                      Johns Hopkins University (Author: Daniel Povey)

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
#include "hmm/transition-model-2D.h"

/** @brief Write equally spaced alignments of utterances (to get training started).
*/
int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage = "Write equally spaced alignments of utterances "
			"(to get training started)\n"
			"Usage:  align-equal <model-in> <block-rspecifier> <transcriptions-rspecifier> "
			"<alignments-wspecifier>\n"
			"e.g.: \n"
			" align-equal 0.mdl ark:block 'ark:text2phone.pl lexicon.txt phones.txt text|'"
			" ark,t:equal.ali\n";

		ParseOptions po(usage);
		po.Read(argc, argv);

		if (po.NumArgs() != 4) {
			po.PrintUsage();
			exit(1);
		}

		std::string model_in_filename = po.GetArg(1); // .mdl主要为了获取state-map以及对应的trans_state
		std::string block_rspecifier = po.GetArg(2); // 读取每个sample的block-map信息
		std::string transcript_rspecifier = po.GetArg(3); // 读取每个sample的phone序列
		std::string alignment_wspecifier = po.GetArg(4); // 写入每个sample对应的alignment vector

		TransitionModel_2D trans_model;
		{
			bool binary; // binary的值通过ki实例化的语句来判定，二进制为1，文本格式为0
			Input ki(model_in_filename, &binary); // Kaldi根据开头是否"\0B"来判断是二进制还是文本格式，同时准备好一个流ki
			trans_model.Read(ki.Stream(), binary);// 从.mdl流里读取属于TransitionModel类的对象到trans_model
		}
		const HmmTopology_2D topo = trans_model.GetTopo();

		SequentialInt32VectorReader block_info_reader(block_rspecifier);
		RandomAccessInt32VectorReader transcript_reader(transcript_rspecifier);
		Int32VectorWriter alignment_writer(alignment_wspecifier);

		int32 done = 0, no_transcript = 0, other_error = 0;
		for (; !block_info_reader.Done(); block_info_reader.Next()) {
			std::string key = block_info_reader.Key();
			const std::vector<int32> &block_info = block_info_reader.Value();
			// 如果某个sample的block-information不是三个【row_num col_num tol_num】，则错误数+1，跳过这个sample
			if (block_info.size() != 3) {
				KALDI_WARN << "Block information size supposed to be 3, but get "
					<< block_info.size() << " from " << key << " instead.";
				other_error++;
				continue;
			}
			size_t block_row_num = static_cast<size_t>(block_info[0]),
				  block_col_num = static_cast<size_t>(block_info[1]),
				  block_tol_num = static_cast<size_t>(block_info[2]);
			// 如果某个sample的block-information不正确【row_num*col_num！=tol_num】，则错误数+1，跳过这个sample
			if (block_tol_num != block_row_num * block_col_num) {
				KALDI_WARN << "Block rows number * cols number not equal to total number, sample name:" << key
					<< "rows:" << block_row_num << " cols:" << block_col_num << " total:" << block_tol_num;
				other_error++;
				continue;
			}
			// 如果这个sample有text对应的phone序列则进行处理
			if (transcript_reader.HasKey(key)) {
				const std::vector<int32> &transcript = transcript_reader.Value(key);
				//TODO:暂时不接受超过一个phone的sample
				if (transcript.size() != 1) {
					KALDI_WARN << "Currently could only handle single phone for each sample, but get "
						<< transcript.size() << " phones for " << key;
					other_error++;
					continue;
				}
				int32 phone_id = transcript[0],//该样本对应的phone
					state_row_num = topo.TopologyShapeRowsForPhone(phone_id),//该phone对应的state-map总行数
					state_col_num = topo.TopologyShapeColsForPhone(phone_id),//该phone对应的state-map总列数
					hmm_state, //某一block对应的state在该phone中的index，0-based
					trans_state; //该state在所有state中的序号
				//std::cout << key << " <-> phone " << phone_id << " has " << state_row_num << " * " << state_col_num << " state-map " << "\n";
				//如果这个sample的block-map行数/列数有一者小于state-map，则无法提供equal-ali-2D，跳过这个sample
				if (block_row_num < state_row_num || block_col_num < state_col_num) {
					KALDI_WARN << "Sample name: " << key << " corrspanding phone: " << phone_id
						<< " have state map for " << state_row_num << " rows " << state_col_num << " cols "
						<< "but only have " << block_row_num << " rows " << block_col_num << " cols frames, "
						<< "cannot create proper equal-alignment.";
						other_error++;
					continue;
				}
				size_t step_rows = static_cast<size_t>(floor(block_row_num / state_row_num)),//每state-map向下移动一格对应在block-map中移动的格数
					step_cols = static_cast<size_t>(floor(block_col_num / state_col_num)),//每state-map向右移动一格对应在block-map中移动的格数
					state_row, //state-map中的row_index，0-based
					state_col; //state-map中的col_index，0-based
				//用来记录每个block对应的trans_state对应关系，这里的block被按行拉伸成一个vector
				//std::cout << "step_rows = " << step_rows << ", step_cols = " << step_cols << "\n";
				std::vector<int32> blocks2states(block_tol_num);
				for (size_t row_i = 0; row_i < block_row_num; row_i++) {
					for (size_t col_j = 0; col_j < block_col_num; col_j++) {
						//size_t should_row = static_cast<size_t>(ceil((row_i + 1) / step_rows));
						//std::cout << "row_i = "<<row_i<<" static_cast<size_t>(ceil((row_i + 1) / step_rows)) "<<should_row <<"\n";
						state_row = std::min(static_cast<size_t>(state_row_num), static_cast<size_t>(ceil((float)(row_i + 1) / step_rows))) - 1;
						state_col = std::min(static_cast<size_t>(state_col_num), static_cast<size_t>(ceil((float)(col_j + 1) / step_cols))) - 1;
						hmm_state = static_cast<int32>(state_row * state_col_num + state_col);
						trans_state = trans_model.PairToState(phone_id, hmm_state);
						//std::cout << "block row "<<row_i<<" block col "<<col_j<<" has state-map: "<<state_row<<" * "<<state_col
						//	<<" and hmm-state "<<hmm_state<<" is trans_state: "<<trans_state<<"\n";
						blocks2states[row_i*block_col_num + col_j] = trans_state;
					}
				}
				alignment_writer.Write(key, blocks2states);
				done++;
			}
			else {
				KALDI_WARN << "AlignEqual2D: no blocks number info for utterance " << key;
				no_transcript++;
			}
		}
		if (done != 0 && no_transcript == 0 && other_error == 0) {
			KALDI_LOG << "Success: done " << done << " utterances.";
		}
		else {
			KALDI_WARN << "Computed " << done << " alignments; " << no_transcript
				<< " transcripts size not equal to 1, " << other_error
				<< " had other errors.";
		}
		if (done != 0) return 0;
		else return 1;
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}


