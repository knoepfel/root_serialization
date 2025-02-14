#include "sourceFactoryGenerator.h"

#include "RootSource.h"
#include "RepeatingRootSource.h"
#include "SerialRootSource.h"
#include "PDSSource.h"
#include "HDFSource.h"
#include "SharedPDSSource.h"
#include "EmptySource.h"
#include "ReplicatedSharedSource.h"
#include "TestProductsSource.h"

std::function<std::unique_ptr<cce::tf::SharedSourceBase>(unsigned int, unsigned long long)> 
cce::tf::sourceFactoryGenerator(std::string_view iType, std::string_view iOptions) {
  std::function<std::unique_ptr<SharedSourceBase>(unsigned int, unsigned long long)> sourceFactory;
  if( iType == "ReplicatedRootSource") {
    std::string fileName( iOptions );
    sourceFactory = [fileName](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<ReplicatedSharedSource<RootSource>>(iNLanes, iNEvents, fileName);
    };
  } else if( iType == "RepeatingRootSource") {
    std::string fileName( iOptions );
    unsigned int nUniqueEvents = 10;
    std::string branchToRead;
    auto pos = fileName.find(':');
    if(pos != std::string::npos) {
      nUniqueEvents = std::stoul(fileName.substr(pos+1));
      auto leftOptions = fileName.substr(pos+1);
      fileName = fileName.substr(0,pos);
      pos = leftOptions.find(':', 0);
      if(pos != std::string::npos) {
        branchToRead = leftOptions.substr(pos+1);
      }
    }
    sourceFactory = [fileName, nUniqueEvents, branchToRead](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<RepeatingRootSource>(fileName, nUniqueEvents, iNLanes, iNEvents, branchToRead);
    };
  } else if( iType == "SerialRootSource") {
    std::string fileName( iOptions );
    sourceFactory = [fileName](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<SerialRootSource>(iNLanes, iNEvents, fileName);
    };    
  } else if( iType == "ReplicatedPDSSource") {
    std::string fileName( iOptions );
    sourceFactory = [fileName](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<ReplicatedSharedSource<PDSSource>>(iNLanes, iNEvents, fileName);
    };
  } else if( iType == "SharedPDSSource") {
    std::string fileName( iOptions );
    sourceFactory = [fileName](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<SharedPDSSource>(iNLanes, iNEvents, fileName);
    };
  } else if( iType == "EmptySource") {
    sourceFactory = [](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<EmptySource>(iNEvents);
      };
    }
    else if( iType == "HDFSource") {
      std::string fileName( iOptions );
      sourceFactory = [fileName](unsigned int iNLanes, unsigned long long iNEvents) {
        return std::make_unique<ReplicatedSharedSource<HDFSource>>(iNLanes, iNEvents, fileName);
      };
  } else if(iType == "TestProductsSource") {
    sourceFactory = [](unsigned int iNLanes, unsigned long long iNEvents) {
      return std::make_unique<TestProductsSource>(iNLanes, iNEvents);
    };
  }
  return sourceFactory;
}
