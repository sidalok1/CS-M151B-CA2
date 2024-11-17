// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

#include <iostream>
#include <ostream>
#include <cmath>
#include <bitset>
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
#define HISTORY_LENGTH	75
#define GHISTORY_LENGTH	14
#define TABLE_BITS	18
#define PERCEPTRON_BITS 18
#define PERCEPTRON_MASK ((1 << PERCEPTRON_BITS) - 1)
	my_update u;
	branch_info bi;
	unsigned long long history; //Global hist
	unsigned int ghistory; //For gshare
	bitset<HISTORY_LENGTH> histBits;
	unsigned char tab[1<<TABLE_BITS]; //Local hist
	unsigned char choice = 0; //Decision mechanism
	int pweights[1<<PERCEPTRON_BITS][HISTORY_LENGTH]; //ANN weights
	int gshare_weight = 0;
	const unsigned int thresh = floor((1.93 * (HISTORY_LENGTH + 1)) + 14) ; // https://www.cs.utexas.edu/~lin/papers/hpca01.pdf

	my_predictor (void) : history(0), ghistory(0) {
		memset (tab, 0, sizeof (tab));
		memset (pweights, 0, sizeof (pweights));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			//Gshare table index
			u.index = //Hash
				  (ghistory << (TABLE_BITS - GHISTORY_LENGTH))
				^ (b.address & ((1<<TABLE_BITS)-1));

			u.w_index =
					(b.address & PERCEPTRON_MASK)
					^ ((b.address << PERCEPTRON_BITS) & PERCEPTRON_MASK);



			u.sum = tab[u.index]>>2 ? gshare_weight : -gshare_weight;

			histBits = bitset<HISTORY_LENGTH>(history);

			for (auto i = 0; i < HISTORY_LENGTH; i++) {
				u.sum += histBits[i] ? pweights[u.w_index][i] : - pweights[u.w_index][i];
			}
			const bool decision = (choice>>1) ? (u.sum > 0) : tab[u.index]>>2;
			u.direction_prediction (decision);
		} else { //Unconditional jump
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		my_update* mu = (my_update*)u;
		if (bi.br_flags & BR_CONDITIONAL) {

			if (mu->sum > 0 == taken) {
				//Update choice mechanism. There is a bias towards the perceptron
				if (choice < 3) {
					choice++;
				} else if (choice > 0) {
					choice--;
				}
			}

			if (taken != mu->direction_prediction() or abs(mu->sum) <= thresh) {
				//Update the weights for the ANN
				if (tab[mu->index]>>2 == taken) {
					gshare_weight++;
				} else {
					gshare_weight--;
				}
				for (auto i = 0; i < HISTORY_LENGTH; i++) {
					if (histBits[i] == taken) {
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
				if (*c < 7) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}

			history <<= 1; //Global history
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
			ghistory <<= 1; //Global history for gshare
			ghistory |= taken;
			ghistory &= (1<<GHISTORY_LENGTH)-1;
		}
	}
};
