#include "RootSource.h"
#include <iostream>
#include "TBranch.h"
#include "TTree.h"
#include "TFile.h"
#include "TClass.h"

RootSource::RootSource(std::string const& iName, unsigned long long iNEvents) :
  SourceBase(iNEvents),
  file_{TFile::Open(iName.c_str())}
{
  events_ = file_->Get<TTree>("Events");
  auto l = events_->GetListOfBranches();

  const std::string eventAuxiliaryBranchName{"EventAuxiliary"}; 

  dataProducts_.reserve(l->GetEntriesFast());
  branches_.reserve(l->GetEntriesFast());
  for( int i=0; i< l->GetEntriesFast(); ++i) {
    auto b = dynamic_cast<TBranch*>((*l)[i]);
    //std::cout<<b->GetName()<<std::endl;
    //std::cout<<b->GetClassName()<<std::endl;
    b->SetupAddresses();
    TClass* class_ptr=nullptr;
    EDataType type;
    b->GetExpectedType(class_ptr,type);

    dataProducts_.emplace_back(i,
			       reinterpret_cast<void**>(b->GetAddress()),
                               b->GetName(),
                               class_ptr,
			       &delayedReader_);
    branches_.emplace_back(b);
    if(eventAuxiliaryBranchName == dataProducts_.back().name()) {
      eventAuxReader_ = EventAuxReader(dataProducts_.back().address());
    }
  }
  if(not eventAuxReader_) {
    eventAuxReader_ = EventAuxReader(nullptr);
  }
}

long RootSource::numberOfEvents() { 
  return events_->GetEntriesFast();
}

bool RootSource::readEvent(long iEventIndex) {
  if(iEventIndex<numberOfEvents()) {
    auto it = dataProducts_.begin();
    for(auto b: branches_) {
      (it++)->setSize( b->GetEntry(iEventIndex) );
    }
    return true;
  }
  return false;
}
