#pragma once

#include "../deps/r2/src/common.hh"
#include "../../xutils/marshal.hh"

namespace xstore {

using namespace r2;

struct __attribute__((packed)) ReqMessage {
  // the dispatcher size
  u32 req_id;       //1. META   2. Xcache   3. ALN -> key array     4. ALN, Key -> value 
  u64 ALN;
  u8 key_idx;          //! keytype
};

struct __attribute__((packed)) MetaRsp {
  
  // for allocating space
  u32 dispatcher_sz;
  u32 total_sz;
  u32 tt_size;
};

//struct __attribute__((packed)) XcacheRsp {
//  
//  std::string first_layer; // first layer
//  std::string second_layer;        // second layer
//};

}
