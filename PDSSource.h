#if !defined(PDSSource_h)
#define PDSSource_h

#include <string>
#include <memory>
#include <chrono>
#include <iostream>
#include <fstream>

#include "TClass.h"

#include "DataProductRetriever.h"

class PDSSource {
public:
  PDSSource(std::string const& iName, unsigned long long iNEvents);
  PDSSource(PDSSource&&) = default;
  PDSSource(PDSSource const&) = default;

  std::vector<DataProductRetriever>& dataProducts() { return dataProducts_; }

  bool gotoEvent(long iEventIndex) {
    if(iEventIndex < maxNEvents_) {
      auto start = std::chrono::high_resolution_clock::now();
      auto more = readEvent(iEventIndex);
      accumulatedTime_ += std::chrono::duration_cast<decltype(accumulatedTime_)>(std::chrono::high_resolution_clock::now() - start);
      return more;
    }
    return false;
  }

  std::chrono::microseconds accumulatedTime() const { return accumulatedTime_;}

  struct ProductInfo{
  ProductInfo(std::string iName, uint32_t iIndex) : name_(std::move(iName)), index_{iIndex} {}

    uint32_t classIndex() const {return index_;}
    std::string const& name() const { return name_;}
    
    std::string name_;
    uint32_t index_;
  };

  using buffer_iterator = std::vector<std::uint32_t>::const_iterator;
private:
  static constexpr size_t kEventHeaderSizeInWords = 5;
  uint32_t readPreamble(); //returns header buffer size in words
  std::vector<std::string> readStringsArray(buffer_iterator&, buffer_iterator);
  std::vector<std::string> readRecordNames(buffer_iterator&, buffer_iterator);
  std::vector<std::string> readTypes(buffer_iterator&, buffer_iterator);
  std::vector<ProductInfo> readProductInfo(buffer_iterator&, buffer_iterator);
  uint32_t readword();
  std::vector<uint32_t> readWords(uint32_t);
  bool readEvent(long iEventIndex); //returns true if an event was read
  bool skipToNextEvent(); //returns true if an event was skipped
  bool readEventContent();

  std::ifstream file_;
  const unsigned long long maxNEvents_;
  long presentEventIndex_ = 0;
  std::vector<DataProductRetriever> dataProducts_;
  std::vector<void**> dataBuffers_;
  std::chrono::microseconds accumulatedTime_;
};

inline uint32_t PDSSource::readword() {
  int32_t word;
  file_.read(reinterpret_cast<char*>(&word), 4);
  assert(file_.rdstate() == std::ios_base::goodbit);
  return word;
}

inline std::vector<uint32_t> PDSSource::readWords(uint32_t bufferSize) {
  std::vector<uint32_t> words(bufferSize);
  file_.read(reinterpret_cast<char*>(words.data()), 4*bufferSize);
  assert(file_.rdstate() == std::ios_base::goodbit);
  return words;  
}


inline uint32_t PDSSource::readPreamble() {
  std::array<uint32_t, 3> header;
  file_.read(reinterpret_cast<char*>(header.data()),3*4);
  assert(file_.rdstate() == std::ios_base::goodbit);

  assert(3141592*256+1 == header[0]);
  return header[2];
}

inline std::vector<std::string> PDSSource::readStringsArray(buffer_iterator& itBuffer, buffer_iterator itEnd) {
  assert(itBuffer!=itEnd);
  auto bufferSize = *(itBuffer++);

  std::vector<std::string> toReturn;
  if(bufferSize == 0) {
    return toReturn;
  }

  assert(itBuffer+bufferSize <= itEnd);

  const char* itChars = reinterpret_cast<const char*>(&(*itBuffer));
  itBuffer = itBuffer+bufferSize;

  auto itEndChars = itChars + bufferSize*4;
  auto nStrings = std::count(itChars, itEndChars,'\0');
  toReturn.reserve(nStrings);

  while(itChars != itEndChars) {
    std::string s(&(*itChars));
    if(s.empty()) {
      break;
    }
    itChars = itChars+s.size()+1;
    std::cout <<s<<std::endl;
    toReturn.emplace_back(std::move(s));
  }
  std::cout << (void*)itChars << " "<<&(*itBuffer)<<std::endl;
  assert(itChars <= reinterpret_cast<const char*>(&(*itBuffer)));
  return toReturn;
}

inline std::vector<std::string> PDSSource::readRecordNames(buffer_iterator& itBuffer, buffer_iterator itEnd) {
  return readStringsArray(itBuffer, itEnd);
}

inline std::vector<std::string> PDSSource::readTypes(buffer_iterator& itBuffer, buffer_iterator itEnd) {
  return readStringsArray(itBuffer, itEnd);
}

namespace {
  inline size_t bytesToWords(size_t nBytes) {
    return nBytes/4 + ( (nBytes % 4) == 0 ? 0 : 1);
  }
}

inline std::vector<PDSSource::ProductInfo> PDSSource::readProductInfo(buffer_iterator& itBuffer, buffer_iterator itEnd) {
  assert(itBuffer != itEnd);
  //should be a loop over records, but we only have 1 for now
  auto nDataProducts = *(itBuffer++);
  std::vector<ProductInfo> info;
  info.reserve(nDataProducts);

  for(int i=0; i< nDataProducts; ++i) {
    assert(itBuffer < itEnd);
    auto classIndex = *(itBuffer++);
    assert(itBuffer < itEnd);

    const char* itChars = reinterpret_cast<const char*>(&(*itBuffer));
    std::string name(itChars);
    itBuffer = itBuffer + bytesToWords(name.size()+1);
    assert(itBuffer <= itEnd);
    std::cout <<name <<" "<<classIndex<<std::endl;
    info.emplace_back(std::move(name), classIndex);
  }

  return info;
}


inline bool PDSSource::readEvent(long iEventIndex) {
  while(iEventIndex != presentEventIndex_) {
    auto skipped = skipToNextEvent();
    if(not skipped) {return false;}
  }
  return readEventContent();
}

inline bool PDSSource::readEventContent() {
  std::cout <<"readEventContent"<<std::endl;
  if( file_.rdstate() & std::ios_base::eofbit) {
    return false;
  }
  std::array<uint32_t, kEventHeaderSizeInWords+1> headerBuffer;
  file_.read(reinterpret_cast<char*>(headerBuffer.data()), (kEventHeaderSizeInWords+1)*4);
  if( file_.rdstate() & std::ios_base::eofbit) {
    return false;
  }
  assert(file_.rdstate() == std::ios_base::goodbit);

  int32_t bufferSize = headerBuffer[kEventHeaderSizeInWords];

  std::vector<uint32_t> buffer = readWords(bufferSize+1);

  int32_t crossCheckBufferSize = buffer[bufferSize];
  std::cout <<bufferSize<<" "<<crossCheckBufferSize<<std::endl;
  assert(crossCheckBufferSize == bufferSize);

  ++presentEventIndex_;
  return true;
}

inline bool PDSSource::skipToNextEvent() {
  file_.seekg(kEventHeaderSizeInWords*4, std::ios_base::cur);
  if( file_.rdstate() & std::ios_base::eofbit) {
    return false;
  }
  assert(file_.rdstate() == std::ios_base::goodbit);

  int32_t bufferSize = readword();

  file_.seekg(bufferSize*4,std::ios_base::cur);
  assert(file_.rdstate() == std::ios_base::goodbit);

  int32_t crossCheckBufferSize = readword();
  assert(crossCheckBufferSize == bufferSize);

  ++presentEventIndex_;
  return true;
}


inline PDSSource::PDSSource(std::string const& iName, unsigned long long iNEvents) :
  file_{iName, std::ios_base::binary},
  maxNEvents_{iNEvents},
  accumulatedTime_{std::chrono::microseconds::zero()}
{
  auto bufferSize = readPreamble();
  
  //1 word beyond the buffer is the crosscheck value
  std::vector<uint32_t> buffer = readWords(bufferSize+1);
  auto itBuffer = buffer.cbegin();
  auto itEnd = buffer.cend();

  (void) readRecordNames(itBuffer, itEnd);
  auto types = readTypes(itBuffer, itEnd);
  //non-top level types
  readTypes(itBuffer, itEnd);
  auto productInfo = readProductInfo(itBuffer, itEnd);
  assert(itBuffer != itEnd);
  assert(itBuffer+1 == itEnd);
  std::cout <<*itBuffer <<" "<<bufferSize<<std::endl;
  assert(*itBuffer == bufferSize);

  dataProducts_.reserve(productInfo.size());
  dataBuffers_.resize(productInfo.size(), nullptr);
  size_t index =0;
  for(auto const& pi : productInfo) {
    
    TClass* cls = TClass::GetClass(types[pi.classIndex()].c_str());
    assert(cls);
    dataProducts_.emplace_back(dataBuffers_[index++],
                               pi.name(),
                               cls);
  }
}
#endif
