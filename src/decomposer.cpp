// Copyright (C) 2022  The COLA Authors
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

#include "decomposer.hpp"

#include <set>
#include <vector>
#include <functional>
#include <queue>

#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/postproc.hh>


namespace cola
{

std::vector<spot::twa_graph_ptr>
decomposer::run(bool nonacc_pred, bool merge_iwa, bool merge_det)
{
    std::vector<spot::twa_graph_ptr> result;
    spot::scc_info si(nba_, spot::scc_info_options::ALL);

    std::vector<bool> reach_sccs = find_scc_paths(si);

    struct pair_compare
    {
        // reverse order
        bool operator()(std::pair<unsigned, unsigned> scc_i, std::pair<unsigned, unsigned> scc_j) const
        {
            return scc_i.second <= scc_j.second;
        }
    };
    // first deal with SCCs with large numbers
    // SCC with more states gets priority to decompose
    std::priority_queue<std::pair<unsigned, unsigned>, std::vector<std::pair<unsigned, unsigned>>, pair_compare> scclist;
    std::set<unsigned> iw_sccs;
    std::set<unsigned> det_sccs;
    auto scc_types = get_scc_types(si);
    for (unsigned sc = 0; sc < si.scc_count(); sc ++)
    {
        if (is_accepting_weakscc(scc_types, sc))
        {
            iw_sccs.insert(sc);
        } else if (is_accepting_detscc(scc_types, sc))
        {
            det_sccs.insert(sc);
        }
        
        // only care about accepting scc
        if (! si.is_accepting_scc(sc)) continue;
        scclist.push(std::make_pair(sc, si.states_of(sc).size()));
    }
    
    bool iwa_done = false;
    bool det_done = false;
    while(scclist.size() > 0)
    {
        unsigned scc_i = scclist.top().first;
        scclist.pop();
        
        std::set<unsigned> sccs;
        sccs.insert(scc_i);

        if (merge_iwa and not iwa_done and is_accepting_weakscc(scc_types, scc_i))
        {
            sccs = iw_sccs;
            iwa_done = true;
        }
        else if (merge_iwa and iwa_done and is_accepting_weakscc(scc_types, scc_i))
        {
            continue;
        }

        if (merge_det and not det_done and is_accepting_detscc(scc_types, scc_i))
        {
            sccs = det_sccs;
            det_done = true;
        }
        else if (merge_det and det_done and is_accepting_detscc(scc_types, scc_i))
        {
            continue;
        }

        spot::twa_graph_ptr aut = make_twa_with_scc(si, sccs, reach_sccs, nonacc_pred);
        result.push_back(aut);

        -- num_nbas_;
        if(num_nbas_ == 1 && scclist.size() > 0)
        {
            // put rest together
            break;
        }
    }

    std::set<unsigned> remaining_sccs;
    while(scclist.size() > 0)
    {
        unsigned scc_i = scclist.top().first;
        scclist.pop();
        remaining_sccs.insert(scc_i);
    }
    if (! remaining_sccs.empty())
    {
        spot::twa_graph_ptr aut = make_twa_with_scc(si, remaining_sccs, reach_sccs, nonacc_pred, merge_iwa, merge_det);
        result.push_back(aut);
    }
    return result;
}

spot::twa_graph_ptr
decomposer::make_twa_with_scc(spot::scc_info& si, std::set<unsigned> sccs, std::vector<bool>& reach_sccs, bool nonacc_pred, bool merge_iwa, bool merge_det)
{
    assert(! sccs.empty());

    auto scc_reach = [&si, &reach_sccs](unsigned s, unsigned t) -> bool
    {
      return s == t || (reach_sccs[t + si.scc_count() * s]);
    };
    
    // now construct new DPAs
    spot::twa_graph_ptr res = spot::make_twa_graph(nba_->get_dict());
    res->copy_ap_of(nba_);
    res->prop_copy(nba_,
                   {
                       false,        // state based
                       nba_->prop_inherently_weak().is_true(),        // inherently_weak
                       false, false, // deterministic
                       nba_->prop_complete().is_true(),         // complete
                       nba_->prop_stutter_invariant().is_true()         // stutter inv
                   });
    for (unsigned s = 0; s < nba_->num_states(); s++)
    {
      res->new_state();
    }
    auto is_good_scc = [&scc_reach, &sccs](unsigned scc_i) -> bool
    {
        for(auto scc : sccs)
        {
            if (scc == scc_i) return true;
            if (scc_reach(scc_i, scc))
            {
                return true;
            }
        }
        return false;
    };
    for (auto &t : nba_->edges())
    {
      // out going transition for t.src
      unsigned scc_src = si.scc_of(t.src);
      unsigned scc_dst = si.scc_of(t.dst);
      if (is_good_scc(scc_src) && is_good_scc(scc_dst))
      {
        //   if(sccs.find(scc_src) != sccs.end() && sccs.find(scc_dst) != sccs.end())
            
            if (not nonacc_pred)
                res->new_edge(t.src, t.dst, t.cond, t.acc);
            else 
            {
                if (sccs.find(scc_src) != sccs.end() and sccs.find(scc_dst) != sccs.end())
                    res->new_edge(t.src, t.dst, t.cond, t.acc);
                else
                    res->new_edge(t.src, t.dst, t.cond);
            }
      }
    }
    // names
    res->set_init_state(nba_->get_init_state_number());
    // now acceptance condition
    res->set_buchi();
    // remove unreachable macrostates
    spot::postprocessor p;
    p.set_type(spot::postprocessor::Buchi);
    res = p.run(res);
    return res;
}

}

