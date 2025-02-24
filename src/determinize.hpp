// Copyright (C) 2017-2019 Laboratoire de Recherche et Développement
// de l'Epita.
// Copyright (C) 2022 - present  The COLA Authors
//
// COLA is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// COLA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "kofola.hpp"
#include "types.hpp"
#include "simulation.hpp"
#include "optimizer.hpp"

namespace cola
{
    // Returns true if lhs has a smaller nesting pattern than rhs
    // If lhs and rhs are the same, return false.
    // compare backwards
    bool compare_brace(const std::vector<int> &lhs, const std::vector<int> &rhs);

    // Backward search for obtaining the nesting pattern
    // The obtained nesting pattern is in reverse order
    bool compare_node_rank(const std::vector<int> &braces, int a, int b);

    // compute the parity color for an edge
    int compute_parity_color(int min_dcc, int min_acc);

    // Compute successor of a DAC macrostate
    class dac_determinize_succ
    {
    public:
        const spot::scc_info &si_;
        // current ranking values of the DAC states
        const std::vector<state_rank> &curr_ranks_;
        // the reachable states at this level inside this SCC
        state_set &next_level_;
        // transitions
        const std::unordered_map<unsigned, std::vector<edge_label>> &det_trans_;
        // DAC number
        unsigned curr_scc_;
        // the reference to other ranking, more general than passing the tba_mstate
        std::vector<state_rank>& succ_ranks_;

        dac_determinize_succ(spot::scc_info &si, unsigned scc, const std::vector<state_rank> &curr_ranks, state_set &next_level
            , std::vector<state_rank>& succ_ranks, std::unordered_map<unsigned, std::vector<edge_label>> &det_trans)
            : si_(si), curr_scc_(scc), curr_ranks_(curr_ranks), next_level_(next_level), succ_ranks_(succ_ranks), det_trans_(det_trans)
        {
        }
        // compute the successor rankings
        void compute_succ();

        int get_color();
    };

    // Compute successor of a NAC macrostate
    class nac_determinize_succ
    {
    public:
        const spot::scc_info &si_;
        const std::vector<state_rank> &curr_ranks_;
        const std::unordered_map<unsigned, std::vector<edge_label>> &nondet_trans_;
        unsigned curr_scc_;

        state_set &next_level_;
        std::vector<state_rank> &succ_ranks_;
        std::vector<int>& succ_braces_;
        // whether we rearrange the ranks of states from accepting transitions
        bool reassign_ranks_;

        nac_determinize_succ(spot::scc_info &si, unsigned scc, const std::vector<state_rank> &curr_ranks, const std::vector<int> &curr_braces, state_set &next_level
            , std::vector<state_rank>& succ_ranks, std::vector<int>& succ_braces, std::unordered_map<unsigned, std::vector<edge_label>> &nondet_trans, bool reassign_ranks)
            : si_(si), curr_scc_(scc), curr_ranks_(curr_ranks), next_level_(next_level), succ_ranks_(succ_ranks)
            , succ_braces_(succ_braces), nondet_trans_(nondet_trans), reassign_ranks_(reassign_ranks)
        {
            assert (succ_ranks_.size() == 0 && succ_braces_.size() == 0);
            // First copy all the braces from current macrostate
            for (unsigned i = 0; i < curr_braces.size(); i++)
            {
                succ_braces_.push_back(curr_braces[i]);
            }
        }

        void compute_succ();

        int get_color();
    };

    // define the macrostate for determinization

    class tba_mstate
    {
    public:
        // SCC information
        spot::scc_info &si_;
        // nondeterministic accepting SCCs (NAC)
        // 1. NAC states point to its braces
        std::vector<std::vector<state_rank>> nac_ranks_;
        // the braces for each NAC
        std::vector<std::vector<int>> nac_braces_;

        // deterministic accepting SCCs (DAC)
        // 2. DAC states point to its labelling
        std::vector<std::vector<state_rank>> dac_ranks_;

        // Inherently weak SCCs (IWC)
        // 3. IWC states point to RANK_WEAK
        state_set weak_set_;
        // breakpoint construction for weak accepting SCCs
        state_set break_set_;

        // the number of states num, default values, and number of NACs
        tba_mstate(spot::scc_info &si, unsigned num_dacs, unsigned num_nacs)
            : si_(si)
        {
            for (unsigned i = 0; i < num_dacs; i++)
            {
                dac_ranks_.emplace_back(std::vector<state_rank>());
            }
            for (unsigned i = 0; i < num_nacs; i++)
            {
                nac_braces_.emplace_back(std::vector<int>());
                nac_ranks_.emplace_back(std::vector<state_rank>());
            }
        }

        tba_mstate(const tba_mstate &other)
            : si_(other.si_)
        {
            this->break_set_.clear();
            this->break_set_.insert(other.break_set_.begin(), other.break_set_.end());
            this->weak_set_.clear();
            this->weak_set_.insert(other.weak_set_.begin(), other.weak_set_.end());

            this->dac_ranks_.clear();
            for (unsigned i = 0; i < other.dac_ranks_.size(); i++)
            {
                std::vector<state_rank> copy = other.dac_ranks_[i];
                dac_ranks_.emplace_back(copy);
            }

            this->nac_ranks_.clear();
            this->nac_braces_.clear();
            for (unsigned i = 0; i < other.nac_ranks_.size(); i++)
            {
                std::vector<int> braces = other.nac_braces_[i];
                this->nac_braces_.emplace_back(braces);
                std::vector<state_rank> copy = other.nac_ranks_[i];
                this->nac_ranks_.emplace_back(copy);
            }
        }

        tba_mstate &
        operator=(const tba_mstate &other)
        {
            this->si_ = other.si_;
            this->break_set_.clear();
            this->break_set_.insert(other.break_set_.begin(), other.break_set_.end());
            this->weak_set_.clear();
            this->weak_set_.insert(other.weak_set_.begin(), other.weak_set_.end());

            this->dac_ranks_.clear();
            for (unsigned i = 0; i < other.dac_ranks_.size(); i++)
            {
                std::vector<state_rank> copy = other.dac_ranks_[i];
                dac_ranks_.emplace_back(copy);
            }

            this->nac_ranks_.clear();
            this->nac_braces_.clear();
            for (unsigned i = 0; i < other.nac_ranks_.size(); i++)
            {
                std::vector<int> braces = other.nac_braces_[i];
                this->nac_braces_.emplace_back(braces);
                std::vector<state_rank> copy = other.nac_ranks_[i];
                this->nac_ranks_.emplace_back(copy);
            }
            return *this;
        }

        state_set
        get_reach_set() const;

        state_set
        get_weak_set() const;

        bool is_empty() const;

        size_t hash() const;

        std::vector<safra_node>
        get_safra_nodes(unsigned index) const;

        bool operator<(const tba_mstate &other) const;
        bool operator==(const tba_mstate &other) const;
    };

    struct tba_mstate_hash
    {
        size_t
        operator()(const tba_mstate &s) const noexcept
        {
            return s.hash();
        }
    };

    // by default, the number of colors for each set is even
    spot::acc_cond::acc_code
    make_parity_condition(int base, bool odd, int num_colors);

    // Divide-and-conquer determinization based on SCC decomposition
    class tba_determinize
    {
    private:
        // The source automaton
        const spot::const_twa_graph_ptr aut_;

        // SCCs information of the source automaton.
        spot::scc_info &si_;

        // Number of states in the input automaton.
        unsigned nb_states_;

        // state_simulator
        state_simulator simulator_;

        //delayed simulator
        delayed_simulation delayed_simulator_;

        // The parity automata being built.
        spot::twa_graph_ptr res_;

        spot::option_map &om_;

        // use ambiguous
        bool use_unambiguous_;
        bool use_scc_;
        bool use_stutter_;
        bool use_simulation_;

        // Association between labelling states and state numbers of the DELA
        std::unordered_map<tba_mstate, unsigned, tba_mstate_hash> rank2n_;

        // outgoing transition to its colors by each accepting SCCs (weak is the righmost)
        std::unordered_map<outgoing_trans, std::vector<int>, outgoing_trans_hash> trans2colors_;

        // maximal colors for each accepting SCCs, including DACs and NACs
        std::vector<int> max_colors_;
        std::vector<int> min_colors_;

        // States to process.
        std::deque<std::pair<tba_mstate, unsigned>> todo_;

        // Support for each state of the source automaton.
        std::vector<bdd> support_;

        // Propositions compatible with all transitions of a state.
        std::vector<bdd> compat_;

        // Whether a SCC is IWC, DAC or NAC
        std::string scc_types_;

        // State names for graphviz display
        std::vector<std::string> *names_;

        // the index of each DAC
        std::vector<unsigned> dacs_;
        // the index of each NAC
        std::vector<unsigned> nacs_;

        // Show Rank states in state name to help debug
        bool show_names_;

    private:
        // From a mmacro state, looks for a duplicate in the map before
        // creating a new state if needed.
        unsigned new_state(tba_mstate &s)
        {
            tba_mstate dup(s);
            auto p = rank2n_.emplace(dup, 0);
            if (p.second) // This is a new state
            {
                p.first->second = res_->new_state();
                if (show_names_)
                    names_->push_back(get_name(p.first->first));
                todo_.emplace_back(dup, p.first->second);
            }
            return p.first->second;
        }

        bool exists(tba_mstate &s)
        {
            return rank2n_.end() != rank2n_.find(s);
        }

        bool has_acc_iwcs()
        {
            for (unsigned i = 0; i < scc_types_.size(); i++)
            {
                // if there is an accepting weak SCC
                if ((scc_types_[i] & SCC_WEAK_TYPE) > 0 && (scc_types_[i] & SCC_ACC) > 0)
                {
                    return true;
                }
            }
            return false;
        }

        int get_nac_index(unsigned scc)
        {
            for (int idx = 0; idx < nacs_.size(); idx++)
            {
                if (nacs_[idx] == scc)
                {
                    return idx;
                }
            }
            throw std::runtime_error("tba_determinize::get_nac_index(unsigned scc): index not found");
        }

        int get_dac_index(unsigned scc)
        {
            for (int idx = 0; idx < dacs_.size(); idx++)
            {
                if (dacs_[idx] == scc)
                {
                    return idx;
                }
            }
            throw std::runtime_error("tba_determinize::get_dac_index(unsigned scc): index not found");
        }

        spot::twa_graph_ptr postprocess(spot::twa_graph_ptr aut)
        {
            spot::scc_info da(aut, spot::scc_info_options::ALL);
            // set of states -> the forest of reachability in the states.
            mstate_equiv_map set2scc;
            // record the representative of every SCC
            for (auto p = rank2n_.begin(); p != rank2n_.end(); p++)
            {
                const state_set set = p->first.get_reach_set();
                // first the set of reached states
                auto val = set2scc.emplace(set, state_set());
                val.first->second.insert(p->second);
            }
            mstate_merger merger(aut, set2scc, da, om_);
            spot::twa_graph_ptr res = merger.run();
            if (om_.get(VERBOSE_LEVEL) >= 1)
            std::cout << "The number of states reduced by mstate_merger: "
                        << (aut->num_states() - res->num_states()) << " {out of "
                        << aut->num_states() << "}" << std::endl;
            return res;
        }

        std::string get_name(const tba_mstate &ms);

        void finalize_acceptance();

        void remove(std::vector<state_rank>& nodes, state_set& to_remove);

        void merge_redundant_states(tba_mstate &ms, std::vector<state_rank>& nodes, bool nondet);

        void make_simulation_state(tba_mstate &ms);

        // compute the successor N={nondeterministic states and nonaccepting SCCs} O = {breakpoint for weak SCCs}
        // and labelling states for each SCC
        void compute_succ(const tba_mstate &ms, bdd letter, tba_mstate &nxt, std::vector<int> &color);

        void compute_stutter_succ(const tba_mstate &curr, bdd letter, tba_mstate &succ, std::vector<int> &colors);

    public:
        tba_determinize(const spot::const_twa_graph_ptr &aut, spot::scc_info &si, spot::option_map &om, std::vector<bdd> &implications);

        spot::twa_graph_ptr run();
    };
}
