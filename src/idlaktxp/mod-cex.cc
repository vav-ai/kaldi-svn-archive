// idlaktxp/mod-cex.cc

// Copyright 2012 CereProc Ltd.  (Author: Matthew Aylett)

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
//

#include "./txpmodule.h"

namespace kaldi {

TxpCex::TxpCex(const std::string &tpdb, const std::string &configf)
    : TxpModule("cex", tpdb, configf) {
  cexspec_.Init(config_, std::string("cex"), std::string("default"));
  cexspec_.Parse(tpdb.c_str());
}

TxpCex::~TxpCex() {
}

bool TxpCex::Process(pugi::xml_document* input) {
  //int modellen;
  std::string* model = new std::string();
  //modellen = cexspec_.MaxFeatureSize() + 1;
  //model =  new char[modellen];
  cexspec_.AddPauseNodes(input);
  pugi::xpath_node_set tks =
      input->document_element().select_nodes("//phon");
  tks.sort();
  TxpCexspecContext context(*input,  cexspec_.GetPauseHandling());
  kaldi::int32 i = 0;
  for (pugi::xpath_node_set::const_iterator it = tks.begin();
       it != tks.end();
       ++it, i++, context.Next()) {
    pugi::xml_node phon = (*it).node();
    //memset(model, 0, modellen);
    model->clear();
    cexspec_.ExtractFeatures(context, model);
    phon.text() = model->c_str();
  }
  delete model;
  // should set to error status
  return true;
}

bool TxpCex::IsSptPauseHandling() {
  if (cexspec_.GetPauseHandling() == CEXSPECPAU_HANDLER_SPURT) return true;
  return false;
}

}  // namespace kaldi