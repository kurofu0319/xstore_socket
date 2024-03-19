#pragma once

#include "../utils.hh"
#include "../schema.hh"

#include "../../xcomm/src/rpc/mod.hh"
#include "../../xcomm/src/transport/rdma_ud_t.hh"

#include<bits/stdc++.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/types.h>

namespace xstore {

    extern DBTree db;
    extern std::unique_ptr<XCache> cache;
    extern std::vector<XCacheTT> tts;	     // vector<vector<u64>>

    auto MetaCallback()
    {
        ASSERT(cache != nullptr);
        
        std::string first_layer = cache->first_layer.serialize();
        std::string second_layer ;
        
        for (const auto& model : cache->second_layer) {
   	 	    std::string model_serialized = model->serialize();
    	 	second_layer += model_serialized;
        }
        
        u32 tt_size = 0;
        
        for(auto tt : tts)
		    tt_size += tt.tt_sz();

        MetaRsp meta = {
            .dispatcher_sz = static_cast<u32>(first_layer.size()),
            .total_sz = static_cast<u32>(second_layer.size()),
            .tt_size = tt_size,
        };
        
        std::string raw_meta = xstore::util::Marshal<MetaRsp>::serialize_to(meta);
        
        return raw_meta;
    }

//    auto XcacheCallback()
//    {
//        XcacheRsp xcache;
//        {
//            if (std::is_same<XCache::FirstML<KK>, TNN<KK>>()) {
//                xcache.first_layer = cache->first_layer.serialize_to_file("xfirst");
//            } else {
//                xcache.first_layer = cache->first_layer.serialize();
//            }
//        }

//        std::string second_layer ;
//        
//        for (const auto& model : cache->second_layer) {
//   	 	std::string model_serialized = model->serialize();
//    	 	second_layer += model_serialized;
//        }
//        
//        xcache.second_layer = second_layer;
//        
//        std::string raw_meta = xstore::util::Marshal<XcacheRsp>::serialize_to(xcache);

//        return raw_meta;

//    }
    
    
    auto FirstCallback()
    {
    	std::string dispatcher = "";
    
	if (std::is_same<XCache::FirstML<KK>, TNN<KK>>()) {
                dispatcher = cache->first_layer.serialize_to_file("xfirst");
            } else {
                dispatcher = cache->first_layer.serialize();  // meta:dispatcher_num, up_bound + model
            }
	return dispatcher;
    }
    
    auto SecondCallback()
    {
    	std::string SubModels = "";
        for (const auto& model : cache->second_layer) {
   	 	    std::string model_serialized = model->serialize();	// err_max + err_min + max + ml
    	 	SubModels += model_serialized;

            // std::cout << model_serialized << std::endl;
        }
        // std::cout << SubModels.size() << std::endl;
	return SubModels;
    }
    
   auto TTCallback()
   {
   	std::string TT = "";
       for (auto tt : tts) {
  	 	std::string entries = tt.serialize();	// entries.size(u32) + entries
   	 	TT += entries;
       }
	return TT;
   }

   auto KeysCallback(u64 addr)
   {
        std::string key_array = "";

        DBTree::Leaf* node = reinterpret_cast<DBTree::Leaf*>(addr);

        auto keys = node->keys;

        key_array = xstore::util::Marshal<NodeK>::serialize_to(keys);

        return key_array;
   }

   auto ValueCallback(u64 addr, u64 idx)
   {
        std::string value = "";

        DBTree::Leaf* node = reinterpret_cast<DBTree::Leaf*>(addr);

        auto val = node->get_value(idx);

        value = std::to_string(*val);

        return value;

   }
    
    
    
    
    
    
    
}
