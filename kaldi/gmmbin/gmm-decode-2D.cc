// gmmbin/gmm-decode-2D.cc

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
	// this function returns the logprob that 'data' is emitted from 'pdf'
	// Refered to gmm/decodable-am-diag-gmm.cc:28 ( LogLikelihoodZeroBased ) 
	BaseFloat LogLikelihood_Temp(const DiagGmm &pdf, const VectorBase<BaseFloat> &data) {
		// check if everything is in order
		if (pdf.Dim() != data.Dim()) {
			KALDI_ERR << "Dim mismatch: data dim = " << data.Dim()
				<< " vs. model dim = " << pdf.Dim();
		}
		// Should check before entering this function ! ! 
		if (!pdf.valid_gconsts()) {
			KALDI_ERR << ": Must call ComputeGconsts() before computing likelihood.";
		}

		BaseFloat log_sum_exp_prune = -1.0;

		Vector<BaseFloat> data_squared(data);
		data_squared.CopyFromVec(data);
		data_squared.ApplyPow(2.0);

		Vector<BaseFloat> loglikes(pdf.gconsts());  // need to recreate for each pdf
		// loglikes +=  means * inv(vars) * data.
		loglikes.AddMatVec(1.0, pdf.means_invvars(), kNoTrans, data, 1.0);
		// loglikes += -0.5 * inv(vars) * data_sq.
		loglikes.AddMatVec(-0.5, pdf.inv_vars(), kNoTrans, data_squared, 1.0);

		BaseFloat log_sum = loglikes.LogSumExp(log_sum_exp_prune);
		if (KALDI_ISNAN(log_sum) || KALDI_ISINF(log_sum))
			KALDI_ERR << "Invalid answer (overflow or invalid variances/features?)";

		return log_sum;
	}

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

	template <class T>
	void print_matrix(std::vector< std::vector<T> > &MATRIX, std::string matrix_name = "", int matrix_col_num = -1, int vector_col_num = -1) {
		if (matrix_name != "")
			std::cout << matrix_name << " : \n";
		if (matrix_col_num != -1 && vector_col_num != -1 && MATRIX.size() % matrix_col_num == 0 && MATRIX[0].size() % vector_col_num == 0) {
			for (size_t row_i = 0; row_i < MATRIX.size(); row_i += matrix_col_num) {
				for (size_t col_j = 0; col_j < MATRIX[row_i].size(); col_j += vector_col_num) {
					for (size_t temp_i = row_i; temp_i < row_i + matrix_col_num; temp_i++) {
						for (size_t temp_j = col_j; temp_j < col_j + vector_col_num; temp_j++) {
							std::cout << MATRIX[temp_i][temp_j] << " ";
						}
						std::cout << "\t";
					}
					std::cout << "\n";
				}
				std::cout << "\n";
			}
		}
		else {
			for (size_t row = 0; row < MATRIX.size(); row++) {
				for (size_t col = 0; col < MATRIX[row].size(); col++)
					std::cout << MATRIX[row][col] << " ";
				std::cout << "\n";
			}
			std::cout << "\n";
		}
	}

	template <typename T>
	std::vector<size_t> sort_indexes(const std::vector<T> &v) {
		// 初始化索引向量
		std::vector<size_t> idx(v.size());
		//使用iota对向量赋0~？的连续值
		std::iota(idx.begin(), idx.end(), 0);

		// 通过比较v的值对索引idx进行排序
		std::sort(idx.begin(), idx.end(),
			[&v](size_t i1, size_t i2) {return v[i1] > v[i2]; });
		return idx;
	}

	double sample2phone_likelihood(
		const TransitionModel_2D &trans_model, 
		const AmDiagGmm &am_gmm,
		const Matrix<BaseFloat> &features, 
		const std::vector<int32> &block_info,
		int32 phone_id,
		std::vector<int32> &blocks2states,
		bool need_alignment = false) {
		
		const HmmTopology_2D &topo = trans_model.GetTopo();
		size_t block_row_num = static_cast<size_t>(block_info[0]);
		size_t block_col_num = static_cast<size_t>(block_info[1]);
		size_t block_tol_num = static_cast<size_t>(block_info[2]);
		
		const HmmTopology_2D::TopologyEntry_2D &entry = topo.TopologyForPhone(phone_id);//entry for this phone
		size_t state_row_num = static_cast<size_t>(topo.TopologyShapeRowsForPhone(phone_id)),//该phone对应的state-map总行数
			state_col_num = static_cast<size_t>(topo.TopologyShapeColsForPhone(phone_id)),//该phone对应的state-map总列数
			state_tol_num = entry.size() - 1; // how many emitting HmmStates that this entry has.
		int32 trans_state; //该state在所有state中的序号

		KALDI_ASSERT(state_tol_num == state_row_num * state_col_num);
		/* explanation of three most important matrixs in-order
			// Store the biggest prob of max_{StateOf_LeftBlock, StateOf_TopBlock}{P[block_row_emitted_by_state_col, O_past | Gmm-Hmm model]}
			// Store argmax_{state of left block}{when block row is emitted by state col}   [first col should be none-sense]
			// Store argmax_{state of top block}{when block row is emitted by state col}   [first row should be none-sense]
		*/
		std::vector< std::vector<BaseFloat> > log_delta_matrix(block_tol_num, std::vector<BaseFloat>(state_tol_num, 0.0));
		std::vector< std::vector<size_t> > most_like_state_top(block_tol_num, std::vector<size_t>(state_tol_num, -1));//当某一block由某一state产生时，这一block上方的block对应的state最可能是什么
		std::vector< std::vector<size_t> > most_like_state_left(block_tol_num, std::vector<size_t>(state_tol_num, -1));//当某一block由某一state产生时，这一block左侧的block对应的state最可能是什么

		// 设置初始概率
#ifdef SHOW_DETAIL
		std::cout << "start doing matrix prepare...\n";
#endif // SHOW_DETAIL
		std::vector<BaseFloat> log_pai_top_down(state_tol_num, Log(0.1)); // the prob that the first block row emitted by every states in this phone
		std::vector<BaseFloat> log_pai_left_right(state_tol_num, Log(0.1)); // the prob that the first block col emitted by every states in this phone
		BaseFloat first_row_state_pai = Log(1.0 / static_cast<BaseFloat>(state_col_num)),
			first_col_state_pai = Log(1.0 / static_cast<BaseFloat>(state_row_num));
		for (size_t i = 0; i < state_col_num; i++)
			log_pai_top_down[i] = first_row_state_pai;
		for (size_t i = 0; i < state_tol_num; i += state_col_num)
			log_pai_left_right[i] = first_col_state_pai;

#ifdef SHOW_DETAIL
		print_vector(log_pai_top_down, "log_pai_top_down", state_col_num);
		print_vector(log_pai_left_right, "log_pai_left_right", state_col_num);
#endif // SHOW_DETAIL

		std::vector< std::vector<BaseFloat> > log_trans_prob_top_down(state_tol_num, std::vector<BaseFloat>(state_tol_num, 0.0));
		std::vector< std::vector<BaseFloat> > log_trans_prob_left_right(state_tol_num, std::vector<BaseFloat>(state_tol_num, 0.0));
		BaseFloat log_prob_no_trans = -10000; //不存在对应的转移弧时的对数转移概率，换算到正常转移概率为 exp{ -10000 } ~= 0
		for (size_t from = 0; from < state_tol_num; from++) {
			for (size_t to = 0; to < state_tol_num; to++) {
				int32 trans_state_from = trans_model.PairToState(phone_id, from);
				int32 trans_state_to = trans_model.PairToState(phone_id, to);
				int32 trans_id = trans_model.StatePairToTransitionId_TopDown(trans_state_from, trans_state_to);
#ifdef SHOW_DETAIL
				std::cout << "phone_id : " << phone_id << ", from state: " << trans_state_from << "(" << from << ")"
					<< " to state: " << trans_state_to << "(" << to << ") has trans_id_top_down: " << trans_id << "\n";
#endif // SHOW_DETAIL
				if (trans_id == -1)
					log_trans_prob_top_down[from][to] = log_prob_no_trans;
				else
					log_trans_prob_top_down[from][to] = trans_model.GetTransitionLogProb_TopDown(trans_id);
			}
		}
#ifdef SHOW_DETAIL
		print_matrix(log_trans_prob_top_down, "log_trans_prob_top_down", state_col_num, state_col_num);
#endif

		for (size_t from = 0; from < state_tol_num; from++) {
			for (size_t to = 0; to < state_tol_num; to++) {
				int32 trans_state_from = trans_model.PairToState(phone_id, from);
				int32 trans_state_to = trans_model.PairToState(phone_id, to);
				int32 trans_id = trans_model.StatePairToTransitionId_LeftRight(trans_state_from, trans_state_to);
#ifdef SHOW_DETAIL
				std::cout << "phone_id : " << phone_id << ", from state: " << trans_state_from << "(" << from << ")"
					<< " to state: " << trans_state_to << "(" << to << ") has trans_id_left_right: " << trans_id << "\n";
#endif
				if (trans_id == -1)
					log_trans_prob_left_right[from][to] = log_prob_no_trans;
				else
					log_trans_prob_left_right[from][to] = trans_model.GetTransitionLogProb_LeftRight(trans_id);
			}
		}
#ifdef SHOW_DETAIL
		print_matrix(log_trans_prob_left_right, "log_trans_prob_left_right", state_col_num, state_col_num);
#endif			
		// Store trans_state for every state in this phone  
		std::vector<int32> trans_states_for_this_phone(state_tol_num);
		// Store pdf_id for every state in this phone  
		std::vector<int32> pdf_index_for_this_phone(state_tol_num);
		for (size_t i = 0; i < state_tol_num; i++) {
			trans_state = trans_model.PairToState(phone_id, static_cast<int32>(i)); //这个phone中第i个state对应的trans_state值
			trans_states_for_this_phone[i] = trans_state;
			pdf_index_for_this_phone[i] = trans_model.TransitionStateToForwardPdf(trans_state);
		}
#ifdef SHOW_DETAIL
		print_vector(trans_states_for_this_phone, "trans_states_for_this_phone", state_col_num);
		print_vector(pdf_index_for_this_phone, "pdf_index_for_this_phone", state_col_num);
#endif		
		int32 which_block_row = 0, which_block_col = 0,
			which_block_index = which_block_row * block_col_num + which_block_col;

		std::vector< std::vector<BaseFloat> > LogProb_blocks2states(block_tol_num, std::vector<BaseFloat>(state_tol_num, 0.0));

		for (size_t hmm_state = 0; hmm_state < state_tol_num; hmm_state++) {
			const DiagGmm &pdf = am_gmm.GetPdf(pdf_index_for_this_phone[hmm_state]);//pdf_index should be 0-based
			for (size_t block = 0; block < block_tol_num; block++) {
				LogProb_blocks2states[block][hmm_state] = LogLikelihood_Temp(pdf, features.Row(static_cast<MatrixIndexT>(block)));
			}
		}

#ifdef SHOW_DETAIL
		print_matrix(LogProb_blocks2states, "LogProb_blocks2states", block_col_num, state_col_num);
		std::cout << "start compute matrix ...\n";
#endif

		std::vector<size_t> possible_state_final_block; // only the state which can trans to SE both from_left_right & from_top_down can be the state of last block.
		std::vector<size_t> possible_state_last_row; // only the state which can trans to SE from_top_down can be the state of last block row.
		std::vector<size_t> possible_state_last_col; // only the state which can trans to SE from_left_right can be the state of last block col.
		for (size_t hmm_state = 0; hmm_state < state_tol_num; hmm_state++) {
			int32 from_trans_state = trans_model.PairToState(phone_id, static_cast<int32>(hmm_state));
			int32 tid_top_down = trans_model.StatePairToTransitionId_TopDown(from_trans_state, 0),  // transition identifier.
				tid_left_right = trans_model.StatePairToTransitionId_LeftRight(from_trans_state, 0);  // transition identifier.
			if (tid_top_down != -1 && tid_left_right != -1)
				possible_state_final_block.push_back(hmm_state);
			if (tid_top_down != -1)
				possible_state_last_row.push_back(hmm_state);
			if (tid_left_right != -1)
				possible_state_last_col.push_back(hmm_state);
		}
		KALDI_ASSERT(possible_state_final_block.size() && "none state can trans to SE from both direction, please check your topo file");
		KALDI_ASSERT(possible_state_last_row.size() && "none state can trans to SE from_top_down, please check your topo file");
		KALDI_ASSERT(possible_state_last_col.size() && "none state can trans to SE from_left_right, please check your topo file");

		//------------------------compute the first block------------------------//
		which_block_index = 0;
		for (size_t i = 0; i < state_tol_num; i++) {
			log_delta_matrix[which_block_index][i] = (log_pai_top_down[i] + log_pai_left_right[i]) / 2 +
				LogProb_blocks2states[which_block_index][i];
		}
#ifdef SHOW_DETAIL
		std::cout << "first block done!\n";
#endif
		//------------compute the first row, from second col to the end in block map-----------//
		which_block_row = 0;
		for (which_block_col = 1; which_block_col < block_col_num; which_block_col++) {
			which_block_index = which_block_row * block_col_num + which_block_col;
			if (which_block_col == block_col_num - 1) {
				for (size_t i = 0; i < possible_state_last_col.size(); i++) {
					// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
					size_t to = possible_state_last_col[i];
					size_t max_index = -1;
					BaseFloat max_sum = -1000000, temp_sum = -1000000;
					for (size_t from = 0; from < state_tol_num; from++) {
						temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][to];
						if (temp_sum > max_sum) {
							max_index = from;
							max_sum = temp_sum;
						}
					}
					most_like_state_left[which_block_index][to] = max_index;
					log_delta_matrix[which_block_index][to] = (log_pai_top_down[i] + max_sum) / 2 +
						LogProb_blocks2states[which_block_index][to];
				}
			}
			else {
				for (size_t i = 0; i < state_tol_num; i++) {
					// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
					size_t max_index = -1;
					BaseFloat max_sum = -1000000, temp_sum = -1000000;
					for (size_t from = 0; from < state_tol_num; from++) {
						temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][i];
						if (temp_sum > max_sum) {
							max_index = from;
							max_sum = temp_sum;
						}
					}
					most_like_state_left[which_block_index][i] = max_index;
					log_delta_matrix[which_block_index][i] = (log_pai_top_down[i] + max_sum) / 2 +
						LogProb_blocks2states[which_block_index][i];
				}
			}
		}
#ifdef SHOW_DETAIL
		std::cout << "first row done!\n";
#endif
		//------------compute the first col, from second row to the end in block map-----------//
		which_block_col = 0;
		for (which_block_row = 1; which_block_row < block_row_num; which_block_row++) {
			which_block_index = which_block_row * block_col_num + which_block_col;

			if (which_block_row == block_row_num - 1) {
				for (size_t i = 0; i < possible_state_last_row.size(); i++) {
					// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
					size_t to = possible_state_last_row[i];
					size_t max_index = -1;
					BaseFloat max_sum = -1000000, temp_sum = -1000000;
					for (size_t from = 0; from < state_tol_num; from++) {
						temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][to];
						if (temp_sum > max_sum) {
							max_index = from;
							max_sum = temp_sum;
						}
					}
					most_like_state_top[which_block_index][to] = max_index;

					log_delta_matrix[which_block_index][to] = (log_pai_left_right[to] + max_sum) / 2 +
						LogProb_blocks2states[which_block_index][to];
				}
			}
			else {
				for (size_t i = 0; i < state_tol_num; i++) {
					// 处理 log_delta_{top_block}{top_block_state} + log_trans{current_block_state | top_block_state}
					size_t max_index = -1;
					BaseFloat max_sum = -1000000, temp_sum = -1000000;
					for (size_t from = 0; from < state_tol_num; from++) {
						temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][i];
						if (temp_sum > max_sum) {
							max_index = from;
							max_sum = temp_sum;
						}
					}
					most_like_state_top[which_block_index][i] = max_index;

					log_delta_matrix[which_block_index][i] = (log_pai_left_right[i] + max_sum) / 2 +
						LogProb_blocks2states[which_block_index][i];
				}
			}
		}
#ifdef SHOW_DETAIL
		std::cout << "first col done!\n";
#endif
		//------------------------compute left blocks------------------------//
		for (which_block_row = 1; which_block_row < block_row_num; which_block_row++) {
			for (which_block_col = 1; which_block_col < block_col_num; which_block_col++) {
				which_block_index = which_block_row * block_col_num + which_block_col;

				if (which_block_row == block_row_num - 1 && which_block_col == block_col_num - 1) {
					for (size_t i = 0; i < possible_state_final_block.size(); i++) {
						// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
						size_t to = possible_state_final_block[i];
						size_t max_index = -1;
						BaseFloat max_sum_left = -1000000, max_sum_top = -1000000, temp_sum = -1000000;
						for (size_t j = 0; j < possible_state_last_row.size(); j++) {
							size_t from = possible_state_last_row[j];
							temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][to];
							if (temp_sum > max_sum_left) {
								max_index = from;
								max_sum_left = temp_sum;
							}
						}
						most_like_state_left[which_block_index][to] = max_index;
						// 处理 log_delta_{top_block}{top_block_state} + log_trans{current_block_state | top_block_state}
						max_index = -1; max_sum_top = -1000000; temp_sum = -1000000;
						for (size_t j = 0; j < possible_state_last_col.size(); j++) {
							size_t from = possible_state_last_col[j];
							temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][to];
							if (temp_sum > max_sum_top) {
								max_index = from;
								max_sum_top = temp_sum;
							}
						}
						most_like_state_top[which_block_index][to] = max_index;

						log_delta_matrix[which_block_index][to] = (max_sum_left + max_sum_top) / 2 +
							LogProb_blocks2states[which_block_index][to];
					}
				}
				else if (which_block_row == block_row_num - 1) {
					for (size_t i = 0; i < possible_state_last_row.size(); i++) {
						// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
						size_t to = possible_state_last_row[i];
						size_t max_index = -1;
						BaseFloat max_sum_left = -1000000, max_sum_top = -1000000, temp_sum = -1000000;
						for (size_t j = 0; j < possible_state_last_row.size(); j++) {
							size_t from = possible_state_last_row[j];
							temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][to];
							if (temp_sum > max_sum_left) {
								max_index = from;
								max_sum_left = temp_sum;
							}
						}
						most_like_state_left[which_block_index][to] = max_index;
						// 处理 log_delta_{top_block}{top_block_state} + log_trans{current_block_state | top_block_state}
						max_index = -1; max_sum_top = -1000000; temp_sum = -1000000;
						for (size_t from = 0; from < state_tol_num; from++) {
							temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][to];
							if (temp_sum > max_sum_top) {
								max_index = from;
								max_sum_top = temp_sum;
							}
						}
						most_like_state_top[which_block_index][to] = max_index;

						log_delta_matrix[which_block_index][to] = (max_sum_left + max_sum_top) / 2 +
							LogProb_blocks2states[which_block_index][to];
					}
				}
				else if (which_block_col == block_col_num - 1) {
					for (size_t i = 0; i < possible_state_last_col.size(); i++) {
						// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
						size_t to = possible_state_last_col[i];
						size_t max_index = -1;
						BaseFloat max_sum_left = -1000000, max_sum_top = -1000000, temp_sum = -1000000;
						for (size_t from = 0; from < state_tol_num; from++) {
							temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][to];
							if (temp_sum > max_sum_left) {
								max_index = from;
								max_sum_left = temp_sum;
							}
						}
						most_like_state_left[which_block_index][to] = max_index;
						// 处理 log_delta_{top_block}{top_block_state} + log_trans{current_block_state | top_block_state}
						max_index = -1; max_sum_top = -1000000; temp_sum = -1000000;
						for (size_t j = 0; j < possible_state_last_col.size(); j++) {
							size_t from = possible_state_last_col[j];
							temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][to];
							if (temp_sum > max_sum_top) {
								max_index = from;
								max_sum_top = temp_sum;
							}
						}
						most_like_state_top[which_block_index][to] = max_index;

						log_delta_matrix[which_block_index][to] = (max_sum_left + max_sum_top) / 2 +
							LogProb_blocks2states[which_block_index][to];
					}
				}
				else {
					for (size_t i = 0; i < state_tol_num; i++) {
						// 处理 log_delta_{left_block}{left_block_state} + log_trans{current_block_state | left_block_state}
						size_t max_index = -1;
						BaseFloat max_sum_left = -1000000, max_sum_top = -1000000, temp_sum = -1000000;
						for (size_t from = 0; from < state_tol_num; from++) {
							temp_sum = log_delta_matrix[which_block_index - 1][from] + log_trans_prob_left_right[from][i];
							if (temp_sum > max_sum_left) {
								max_index = from;
								max_sum_left = temp_sum;
							}
						}
						most_like_state_left[which_block_index][i] = max_index;
						// 处理 log_delta_{top_block}{top_block_state} + log_trans{current_block_state | top_block_state}
						max_index = -1; max_sum_top = -1000000; temp_sum = -1000000;
						for (size_t from = 0; from < state_tol_num; from++) {
							temp_sum = log_delta_matrix[which_block_index - block_col_num][from] + log_trans_prob_top_down[from][i];
							if (temp_sum > max_sum_top) {
								max_index = from;
								max_sum_top = temp_sum;
							}
						}
						most_like_state_top[which_block_index][i] = max_index;

						log_delta_matrix[which_block_index][i] = (max_sum_left + max_sum_top) / 2 +
							LogProb_blocks2states[which_block_index][i];
					}
				}
			}
		}
		//-------------------all temp matrix OK--------------------//
#ifdef SHOW_DETAIL
		std::cout << "all temp matrix done!\n";
		print_matrix(log_delta_matrix, "log_delta_matrix", block_col_num, state_col_num);
		print_matrix(most_like_state_top, "most_like_state_top", block_col_num, state_col_num);
		print_matrix(most_like_state_left, "most_like_state_left", block_col_num, state_col_num);
		std::cout << "start decoding ...\n";
#endif

		//-------------------start decoding ...--------------------//
		
		//-------------------compute the log-like of this sample--------------------//
		//-------------------compute the trans_state corresponding to last-row-last-col block--------------------//
		size_t max_index = -1;
		BaseFloat max_log_like = -1000000;

		for (size_t final_state_index = 0; final_state_index < possible_state_final_block.size(); final_state_index++) {
			size_t final_state = possible_state_final_block[final_state_index];
			if (log_delta_matrix[block_tol_num - 1][final_state] > max_log_like) {
				max_index = final_state;
				max_log_like = log_delta_matrix[block_tol_num - 1][final_state];
			}
		}
		
		if (need_alignment) {
			KALDI_ASSERT(blocks2states.size() == block_tol_num && "blocks2states has not been initialized, size don't match.");
			std::vector<int32> blocks2indices(block_tol_num, 0);
			blocks2indices[block_tol_num - 1] = max_index;
			blocks2states[block_tol_num - 1] = trans_states_for_this_phone[max_index];
			//-------------------compute the trans_state corresponding to last-row-but-not-last-col blocks--------------------//
#ifdef SHOW_DETAIL
			std::cout << "start decoding last-row-but-not-last-col blocks ...\n";
#endif
			which_block_row = block_row_num - 1;
			for (which_block_col = block_col_num - 2; which_block_col >= 0; which_block_col--) {
				which_block_index = which_block_row * block_col_num + which_block_col;
				blocks2indices[which_block_index] = most_like_state_left[which_block_index + 1][blocks2indices[which_block_index + 1]];
				blocks2states[which_block_index] = trans_states_for_this_phone[blocks2indices[which_block_index]];
			}
			//-------------------compute the trans_state corresponding to last-col-but-not-last-row blocks--------------------//
#ifdef SHOW_DETAIL
			std::cout << "start decoding last-col-but-not-last-row blocks ...\n";
#endif
			which_block_col = block_col_num - 1;
			for (which_block_row = block_row_num - 2; which_block_row >= 0; which_block_row--) {
				which_block_index = which_block_row * block_col_num + which_block_col;
				blocks2indices[which_block_index] = most_like_state_top[which_block_index + block_col_num][blocks2indices[which_block_index + block_col_num]];
				blocks2states[which_block_index] = trans_states_for_this_phone[blocks2indices[which_block_index]];
			}
			//-------------------compute the trans_state corresponding to left blocks--------------------//
#ifdef SHOW_DETAIL
			std::cout << "start decoding left blocks...\n";
#endif
			for (which_block_row = block_row_num - 2; which_block_row >= 0; which_block_row--) {
				for (which_block_col = block_col_num - 2; which_block_col >= 0; which_block_col--) {
					which_block_index = which_block_row * block_col_num + which_block_col;
					BaseFloat delta_down = log_delta_matrix[which_block_index][most_like_state_top[which_block_index + block_col_num][blocks2indices[which_block_index + block_col_num]]],
						delta_right = log_delta_matrix[which_block_index][most_like_state_left[which_block_index + 1][blocks2indices[which_block_index + 1]]];
					if (delta_down > delta_right) {
						blocks2indices[which_block_index] = most_like_state_top[which_block_index + block_col_num][blocks2indices[which_block_index + block_col_num]];
						blocks2states[which_block_index] = trans_states_for_this_phone[blocks2indices[which_block_index]];
					}
					else {
						blocks2indices[which_block_index] = most_like_state_left[which_block_index + 1][blocks2indices[which_block_index + 1]];
						blocks2states[which_block_index] = trans_states_for_this_phone[blocks2indices[which_block_index]];
					}
				}
			}
#ifdef SHOW_DETAIL
			std::cout << "decoding done!\n";
			print_vector(blocks2indices, "blocks2indices", block_col_num);
			print_vector(blocks2states, "blocks2states", block_col_num);
#endif
		}

		return max_log_like;
	}

} // end namespace kaldi

int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage =
			"Align features given [GMM-based] models.\n"
			"Usage:   gmm-align-2D [options] <model-in> <feature-rspecifier> "
			"<block-rspecifier> <alignments-wspecifier> <possible_phones_descend>\n"
			"e.g.: \n"
			" gmm-decode-2D 1.mdl block scp:train.scp ark,t:1.ali ark,t:1.phones\n";
		ParseOptions po(usage);
		BaseFloat acoustic_scale = 1.0;

		po.Register("acoustic-scale", &acoustic_scale, "Scaling factor for acoustic likelihoods");

		po.Read(argc, argv);

		if (po.NumArgs() != 5) {
			po.PrintUsage();
			exit(1);
		}

		std::string model_in_filename = po.GetArg(1);
		std::string feature_rspecifier = po.GetArg(2);
		std::string block_rspecifier = po.GetArg(3);
		std::string alignment_wspecifier = po.GetArg(4);
		std::string likelihood_wspecifier = po.GetArg(5);

		TransitionModel_2D trans_model;
		AmDiagGmm am_gmm;
		{
			bool binary;
			Input ki(model_in_filename, &binary);
			trans_model.Read(ki.Stream(), binary);
			am_gmm.Read(ki.Stream(), binary);
		}
		const HmmTopology_2D &topo = trans_model.GetTopo();

		SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
		RandomAccessInt32VectorReader block_info_reader(block_rspecifier);
		Int32VectorWriter alignment_writer(alignment_wspecifier);
		Int32VectorWriter likelihood_writer(likelihood_wspecifier);

		int32 num_done = 0, num_err = 0;
		double tot_like = 0.0;
		kaldi::int64 frame_count = 0;
		for (; !feature_reader.Done(); feature_reader.Next()) {
			std::string utt = feature_reader.Key();
#ifdef SHOW_DETAIL
			std::cout << "aligning : " << utt << "\n";
#endif // SHOW_DETAIL
			const Matrix<BaseFloat> &features = feature_reader.Value();
			if (features.NumRows() == 0) {
				KALDI_WARN << "Zero-length features for utterance: " << utt;
				num_err++;
				continue;
			}
			// 如果这个sample没有block信息则跳过
			if (!block_info_reader.HasKey(utt)) {
				KALDI_WARN << "No block info found for utterance " << utt;
				num_err++;
				continue;
			}
			const std::vector<int32> &block_info = block_info_reader.Value(utt);
			// 如果某个sample的block-information不是三个【row_num col_num tol_num】，则错误数+1，跳过这个sample
			if (block_info.size() != 3) {
				KALDI_WARN << "Block information size supposed to be 3, but get "
					<< block_info.size() << " from " << utt << " instead.";
				num_err++;
				continue;
			}
			// 如果某个sample的block-information不正确【row_num*col_num！=tol_num】，则错误数+1，跳过这个sample
			if (block_info[2] != block_info[0] * block_info[1]) {
				KALDI_WARN << "Block rows number * cols number not equal to total number, sample name:" << utt
					<< "rows:" << block_info[0] << " cols:" << block_info[1] << " total:" << block_info[2];
				num_err++;
				continue;
			}
			//-------------------start decoding ...--------------------//
			std::vector<int32> blocks2states(block_info[2], 0);
			
			const std::vector<int32> &phones = topo.GetPhones(); // get sorted phones-list from topo
			std::vector<int32> phones2likelihood(phones.size(), 0); // map each phone to their likelihood, index the same as &phones
			std::vector<int32> likelihood2phones(phones.size(), 0); // store phones-list from most likely to less likely
			for (size_t i = 0; i < phones.size(); i++) {
				int32 phone_id = phones[i];
				phones2likelihood[i] = sample2phone_likelihood(trans_model, am_gmm, features, block_info, phone_id, blocks2states);
			}
			std::vector<size_t> phones_index_descend = sort_indexes(phones2likelihood); // store the index of phones from most likely to less likely
			for (size_t i = 0; i < phones.size(); i++) {
				likelihood2phones[i] = phones[phones_index_descend[i]];
			}

			int32 max_likely_phone = likelihood2phones[0];

			sample2phone_likelihood(trans_model, am_gmm, features, block_info, max_likely_phone, blocks2states, true);

			alignment_writer.Write(utt, blocks2states);
			likelihood_writer.Write(utt, likelihood2phones);
			num_done++;

			tot_like += phones2likelihood[phones_index_descend[0]];
			frame_count += block_info[2];
		}
		KALDI_LOG << "Overall log-likelihood per frame is " << (tot_like / frame_count)
			<< " over " << frame_count << " frames.";
		KALDI_LOG << "Done " << num_done << ", errors on " << num_err;
		return (num_done != 0 ? 0 : 1);
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}
