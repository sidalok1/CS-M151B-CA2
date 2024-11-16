// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

#include <iostream>
#include <ostream>
#include <cmath>
using namespace std;
class my_update : public branch_update {
public:
	unsigned int index;
	unsigned int w_index;
	bool target_hit = false;
	int sum = 0;
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	32
#define TABLE_BITS	15
#define BTB_BITS 8
#define PERCEPTRON_BITS 16
#define PERCEPTRON_MASK ((1 << PERCEPTRON_BITS) - 1)
	my_update u;
	branch_info bi;
	unsigned int history; //Global hist
	unsigned char tab[1<<TABLE_BITS]; //Local hist
	unsigned int btb[1<<BTB_BITS][2]; //branch target map
	int pweights[1<<PERCEPTRON_BITS][HISTORY_LENGTH];

	const unsigned int thresh = floor(1.93 * HISTORY_LENGTH) + 14; // https://www.cs.utexas.edu/~lin/papers/hpca01.pdf

	my_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
		memset (btb, 0, sizeof (btb));
		memset (pweights, 0, sizeof (pweights));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			//Gshare table index
			u.index = //Hash
				  (history << (TABLE_BITS - HISTORY_LENGTH))
				^ (b.address & ((1<<TABLE_BITS)-1));

			u.w_index =
					(b.address & PERCEPTRON_MASK)
					^ ((b.address << PERCEPTRON_BITS) & PERCEPTRON_MASK);
			u.sum = 0;
			for (auto i = 0; i < HISTORY_LENGTH; i++) {
				unsigned int hist_bit = (history >> i) & 1;
				if (hist_bit == 1) {
					u.sum += pweights[u.w_index][i];
				} else {
					u.sum -= pweights[u.w_index][i];
				}
			}

			u.direction_prediction (u.sum >= 0);
		} else { //Unconditional jump
			u.direction_prediction (true);
		}
		if (btb[b.address & ((1<<BTB_BITS)-1)][0] == (b.address & ~((1<<BTB_BITS)-1))) {
			u.target_hit = true;
			u.target_prediction (btb[b.address & ((1<<BTB_BITS)-1)][1]);
			//cerr << "HIT: " << u.target_prediction() << endl;
		} else {
			u.target_hit = false;
		}
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		my_update* mu = (my_update*)u;
		if (bi.br_flags & BR_CONDITIONAL) {

			if (taken != mu->direction_prediction() or abs(mu->sum) < thresh) {
				for (auto i = 0; i < HISTORY_LENGTH; i++) {
					if (((history >> i) & 1) == taken) {
						pweights[mu->w_index][i]++;
					}
					else {
						pweights[mu->w_index][i]--;
					}
					//cerr << i << ": " << pweights[mu->w_index][i] << " ";
				}
				//cerr << endl;
			}

			unsigned char *c = &tab[mu->index]; //Pointer to local history
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1; //Global history
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
		if (mu->target_hit == false or btb[bi.address & ((1<<BTB_BITS)-1)][1] != target) {
			btb[bi.address & ((1<<BTB_BITS)-1)][0] = bi.address & ~((1<<BTB_BITS)-1);
			btb[bi.address & ((1<<BTB_BITS)-1)][1] = target;
		}
	}
};
