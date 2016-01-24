/*
 * Copyright (C) 2014-2016 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file mocus.cc
/// Implementation of the MOCUS algorithm.
/// The algorithm assumes
/// that the graph is in negation normal form.
///
/// A ZBDD data structure is employed to store and extract
/// intermediate (containing gates)
/// and final (basic events only) cut sets
/// upon cut set generation.

#include "mocus.h"

#include "logger.h"

namespace scram {

Mocus::Mocus(const BooleanGraph* fault_tree, const Settings& settings)
      : constant_graph_(false),
        graph_(fault_tree),
        kSettings_(settings) {
  IGatePtr top = fault_tree->root();
  if (top->IsConstant() || top->type() == kNullGate) {
    constant_graph_ = true;
    zbdd_ = std::unique_ptr<Zbdd>(new Zbdd(fault_tree, settings));
    zbdd_->Analyze();
  }
}

void Mocus::Analyze() {
  BLOG(DEBUG2, constant_graph_) << "Graph is constant. No analysis!";
  if (constant_graph_) return;

  CLOCK(mcs_time);
  LOG(DEBUG2) << "Start minimal cut set generation.";
  zbdd_ = Mocus::AnalyzeModule(graph_->root());
  LOG(DEBUG2) << "Delegating cut set minimization to ZBDD.";
  zbdd_->Analyze();
  LOG(DEBUG2) << "Minimal cut sets found in " << DUR(mcs_time);
}

const std::vector<std::vector<int>>& Mocus::products() const {
  assert(zbdd_ && "Analysis is not done.");
  return zbdd_->products();
}

std::unique_ptr<zbdd::CutSetContainer>
Mocus::AnalyzeModule(const IGatePtr& gate) noexcept {
  assert(gate->IsModule() && "Expected only module gates.");
  CLOCK(gen_time);
  LOG(DEBUG3) << "Finding cut sets from module: G" << gate->index();
  std::unordered_map<int, IGatePtr> gates;
  gates.insert(gate->gate_args().begin(), gate->gate_args().end());

  std::unique_ptr<zbdd::CutSetContainer> container(
      new zbdd::CutSetContainer(kSettings_, graph_->basic_events().size()));
  container->Merge(container->ConvertGate(gate));
  int next_gate = container->GetNextGate();
  while (next_gate) {
    LOG(DEBUG5) << "Expanding gate G" << next_gate;
    IGatePtr inter_gate = gates.find(next_gate)->second;
    gates.insert(inter_gate->gate_args().begin(),
                 inter_gate->gate_args().end());
    container->Merge(container->ExpandGate(
        container->ConvertGate(inter_gate),
        container->ExtractIntermediateCutSets(next_gate)));
    next_gate = container->GetNextGate();
  }
  container->Minimize();
  if (!graph_->coherent()) {
    container->EliminateComplements();
    container->Minimize();
  }
  for (int module : container->GatherModules()) {
    container->JoinModule(module,
                          Mocus::AnalyzeModule(gates.find(module)->second));
  }
  container->EliminateConstantModules();
  container->Minimize();
  LOG(DEBUG4) << "G" << gate->index()
              << " cut set generation time: " << DUR(gen_time);
  return container;
}

}  // namespace scram
