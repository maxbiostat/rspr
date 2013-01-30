/*******************************************************************************
lgt.h

Utilities for inferring LGT events.

Copyright 2012 Chris Whidden
whidden@cs.dal.ca
http://kiwi.cs.dal.ca/Software/RSPR
Dec 14, 2012

This file is part of rspr.

rspr is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

rspr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with rspr.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

#ifndef LGT
#define LGT

//#define DEBUG_LGT

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <climits>
#include <vector>
#include <map>
#include <set>
#include <list>
#include "Forest.h"
#include "ClusterForest.h"
#include "LCA.h"
#include "ClusterInstance.h"
#include "SiblingPair.h"
#include "UndoMachine.h"


using namespace std;

void add_transfers(vector<vector<int> > *transfer_counts, Node *super_tree,
		vector<Node *> *gene_trees);
void add_transfers(vector<vector<int> > *transfer_counts, Forest *F1,
		Forest *F2, Forest *MAF1, Forest *MAF2);
Node *find_best_target(Node *source, Forest *AF);
Node *find_best_target(Node *source, Node *target, Node **best_target);


void add_transfers(vector<vector<int> > *transfer_counts, Node *super_tree,
		vector<Node *> *gene_trees) {
	#pragma omp parallel for
	for(int i = 0; i < gene_trees->size(); i++) {
		Forest MAF1;
		Forest MAF2;
		Forest F1 = Forest(super_tree);
		Forest F2 = Forest((*gene_trees)[i]);
		if (sync_twins(&F1,&F2)) {
			int distance = rSPR_branch_and_bound_simple_clustering(F1.get_component(0), F2.get_component(0), &MAF1, &MAF2);
			expand_contracted_nodes(&MAF1);
			expand_contracted_nodes(&MAF2);
#ifdef DEBUG_LGT			
			cout << i << ": " << distance << endl;
			cout << "\tT1: "; F1.print_components();
			cout << "\tT2: "; F2.print_components();
			cout << "\tF1: "; MAF1.print_components_with_edge_pre_interval();
			cout << "\tF2: "; MAF2.print_components_with_edge_pre_interval();
#endif
			sync_af_twins(&MAF1, &MAF2);
			add_transfers(transfer_counts, &F1, &F2, &MAF1, &MAF2);
		}
	}
}

void add_transfers(vector<vector<int> > *transfer_counts, Forest *F1,
		Forest *F2, Forest *MAF1, Forest *MAF2) {
	int start = 1;
	if (MAF2->contains_rho())
		start = 0;
	for(int i = start; i < MAF2->num_components(); i++) {
		Node *F2_source = MAF2->get_component(i);
		if (F2_source->str() == "p")
			continue;
		Node *F2_target = find_best_target(F2_source, MAF2);
#ifdef DEBUG_LGT			
		cout << "\tF2s: " << F2_source->str_edge_pre_interval_subtree() << endl;
		if (F2_target != NULL)
			cout << "\tF2t: " << F2_target->str_edge_pre_interval_subtree() << endl;
		else
			cout << "\tF2t: p" << endl;
#endif
		Node *F1_source = F2_source->get_twin();
		Node *F1_target;
		if (F2_target != NULL)
			F1_target = F2_target->get_twin();
		else
			F1_target = F1->get_component(0);
#ifdef DEBUG_LGT			
		cout << "\tF1s: " << F1_source->str_edge_pre_interval_subtree() << endl;
		if (F2_target != NULL)
			cout << "\tF1t: " << F1_target->str_edge_pre_interval_subtree() << endl;
		else
			cout << "\tF1t: p" << endl;
		cout << endl;
#endif
		string a = F1_source->get_name();
		string b = F1_target->get_name();
		if ((a == "5" || a == "18" || a == "(5,18)")
				&& (b == "5" || b == "18" || b == "(5,18)"))
			cout << "FOO: " << a << "\t" << b << endl;

		#pragma omp atomic
		(*transfer_counts)[F1_source->get_preorder_number()][F1_target->get_preorder_number()]++;
		// do we want to check that the move is valid here?
	}
					// TODO: identify a valid move (if any) for each component
					// loop over the components of MAF2
							// map source to destination in MAF2
							// map source to MAF1
							// map dest to MAF1
							// check if move is valid
							// ? map to full trees? automatic with the pre numbers?
							// store in a list/vector? counts?
					// TODO: make this functiony (reusable)

				// output ideas
				// 1 list of transfers with freqs
				// 2 some kind of processing to make a network
				// 3 using input groups of interest map highways between those
				// 4 groups

}

Node *find_best_target(Node *source, Forest *AF) {
	Node *best_target = NULL;
	for(int i = 0; i < AF->num_components(); i++) {
		Node *target = AF->get_component(i);
		if (source != target)
			find_best_target(source, target, &best_target);
	}
	return best_target;
}

Node *find_best_target(Node *source, Node *target, Node **best_target) {
	if (target->get_edge_pre_start() <= source->get_preorder_number()
			&& target->get_edge_pre_end() >= source->get_preorder_number()
			&& (*best_target == NULL || target->get_edge_pre_start() >
					(*best_target)->get_edge_pre_start())) {
		*best_target = target;
	}
	list<Node *>::const_iterator c;
	for(c = target->get_children().begin();
			c != target->get_children().end(); c++) {
		find_best_target(source, *c, best_target);
	}
}

void add_lcas_to_groups(vector<int> *pre_to_group, Node *subtree) {
	list<Node *>::const_iterator c;
	int group = -1;
	for(c = subtree->get_children().begin();
			c != subtree->get_children().end(); c++) {
		add_lcas_to_groups(pre_to_group, *c);
		int child_group = (*pre_to_group)[(*c)->get_preorder_number()];
		if (group == -1)
			group = child_group;
		else if (group != child_group)
			group = -2;
	}
	if (group >= 0)
		(*pre_to_group)[subtree->get_preorder_number()] = group;
}

#endif
