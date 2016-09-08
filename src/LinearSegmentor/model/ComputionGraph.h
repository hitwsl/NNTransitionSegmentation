#ifndef SRC_ComputionGraph_H_
#define SRC_ComputionGraph_H_

#include "ModelParams.h"
#include "State.h"


// Each model consists of two parts, building neural graph and defining output losses.
struct ComputionGraph : Graph{

public:
	// node instances
	CStateItem start;
	vector<vector<CStateItem> > states;
	vector<NRHeap<CScoredState, CScoredState_Compare> >  beams;

	vector<CStateItem*> execStates;

	Node bucket_true, bucket_false; // for max-margin and etc., in case of _current.output is not the final output

	vector<vector<CStateItem*> > outputs; // to define loss

	dtype delta;

	// node pointers
public:
	ComputionGraph() : Graph(){
		clear();
	}

	~ComputionGraph(){
		clear();
	}

public:
	//allocate enough nodes 
	inline void initial(ModelParams& model, HyperParams& opts){		
		states.resize(opts.maxlength + 1);
		for (int idx = 0; idx < states.size(); idx++){
			states[idx].resize(opts.beam * model.actions.size());
			for (int idy = 0; idy < states[idx].size(); idy++){
				states[idx][idy].initial(model, opts);
			}
		}
		start.clear();
		start._score.val = Mat::Zero(1, 1);

		beams.resize(opts.maxlength + 1);
		for (int idx = 0; idx < states.size(); idx++){
			beams[idx].resize(opts.beam);
		}
		execStates.clear();

		bucket_true.val = Mat::Zero(1, 1);		
		bucket_false.val = Mat::Zero(1, 1);	

		delta = opts.delta;
	}

	inline void clear(){
		Graph::clear();
		beams.clear();

		clearVec(states);
		execStates.clear();
	}


public:
	// some nodes may behave different during training and decode, for example, dropout
	inline int forward(const std::vector<std::string>* pCharacters, const hash_set<string>& dicts, const vector<CAction>* goldAC = NULL){
		//first step, clear node values
		clearValue(); // compute is a must step for train, predict and cost computation

		//second step, build graph
		static vector<CStateItem*> lastStates;
		static CStateItem* pGenerator;
		static int step, offset;
		static std::vector<CAction> actions; // actions to apply for a candidate
		static CScoredState scored_action; // used rank actions
		static bool correct_action_scored;
		static bool correct_in_beam;
		static CAction answer;
		static vector<CStateItem*> states_per_step;

		lastStates.clear();
		start.setInput(pCharacters);
		lastStates.push_back(&start);

		bucket_true.val(0, 0) = 0.0;
		//bucket_false.val(0, 0) = delta;
		bucket_false.val(0, 0) = 0.0;

		step = 0;
		while (true){			
			//prepare for the next
			for (int idx = 0; idx < lastStates.size(); idx++){
				pGenerator = lastStates[idx];
				pGenerator->prepare(dicts);
			}

			offset = 0;
			answer.clear();
			correct_action_scored = false;
			if (goldAC != NULL) answer = (*goldAC)[step];
			for (int idx = 0; idx < lastStates.size(); idx++){
				pGenerator = lastStates[idx];
				pGenerator->getCandidateActions(actions);
				for (int idy = 0; idy < actions.size(); ++idy) {
					pGenerator->move(&states[step][offset], actions[idy]);
					if (pGenerator->_bGold && actions[idy] == answer){
						states[step][offset]._bGold = true;
						correct_action_scored = true;
					}
					offset++;
				}
			}

			if (goldAC != NULL && !correct_action_scored){ //training
				std::cout << "error during training, gold-standard action is filtered" << std::endl;
			}

			if (offset == 0){ // judge correctiveness
				std::cout << "error, reach no output here, please find why" << std::endl;
				for (int idx = 0; idx < pCharacters->size(); idx++) {
					std::cout << (*pCharacters)[idx] << std::endl;
				}
				std::cout << "" << std::endl;
				return -1;
			}

			//scoring and sorting
			beams[step].clear();
			states_per_step.clear();
			for (int idx = 0; idx < offset; idx++){
				states[step][idx].computeScore(this);
				if (states[step][idx]._bGold || goldAC == NULL){
					states[step][idx]._score.forward(this, &(states[step][idx]._current.output), &(bucket_true));
				}
				else{
					states[step][idx]._score.forward(this, &(states[step][idx]._current.output), &(bucket_false));
				}
				states_per_step.push_back(&(states[step][idx]));
				scored_action.item = &(states[step][idx]);
				scored_action.score = scored_action.item->_score.val(0, 0);
				beams[step].add_elem(scored_action);
				execStates.push_back(&states[step][idx]);
			}			
			beams[step].sort_elem();
			outputs.push_back(states_per_step);


			if (beams[step][0].item->IsTerminated()){
				break;
			}

			//for next step
			lastStates.clear();
			correct_in_beam = false;
			for (int idx = 0; idx < beams[step].elemsize(); idx++){
				lastStates.push_back(beams[step][idx].item);
				if (lastStates[idx]->_bGold){
					correct_in_beam = true;
				}
			}

			if (goldAC != NULL && !correct_in_beam){
				break;
			}

			step++;
		}


		//third step, define output nodes
		for (int idx = 0; idx < outputs.size(); idx++){
			for (int idy = 0; idy < outputs[idx].size(); idy++){
				exportNode(&(outputs[idx][idy]->_score));
			}
		}


		return 1;
	}

public:
	inline void clearValue(){
		Graph::clearValue();
		for (int idx = 0; idx < execStates.size(); idx++){
			execStates[idx]->clear();
		}
		clearVec(outputs);
	}

};

#endif /* SRC_ComputionGraph_H_ */