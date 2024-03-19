#include <gflags/gflags.h>
#include <type_traits>

#include "../../deps/kvs-workload/static_loader.hh"
#include "../../deps/kvs-workload/ycsb/mod.hh"
using namespace kvs_workloads::ycsb;

#include "./schema.hh"

#include "./utils.hh"

#include "../../xcomm/tests/transport_util.hh"

#include "../../xutils/file_loader.hh"
#include "../../xutils/local_barrier.hh"
#include "../../xutils/marshal.hh"

#include "../../deps/r2/src/thread.hh"

#include "../../benchs/reporter.hh"

#include<stdio.h>
#include<iostream>
#include<cstring>
#include<sys/fcntl.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<errno.h>
#include<sys/types.h>
#include <arpa/inet.h>


// #include "./eval_c.hh"

// #include "../arc/val/cpu.hh"

using namespace test;

using namespace xstore;
//using namespace xstore::rpc;
using namespace xstore::transport;
using namespace xstore::bench;
using namespace xstore::util;

DEFINE_bool(vlen, false, "whether to use variable length value");

DEFINE_uint64(nkeys, 1000000, "Number of keys to fetch");

namespace xstore {
std::unique_ptr<XCache> cache = nullptr;
std::vector<XCacheTT> tts;
} // namespace xstore

int main(int argc, char** argv)
{

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
      std::cerr << "Socket creation error" << std::endl;
      return -1;  
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7777);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }
    
    std::cout << "init success" << std::endl;

    // 1. bootstrap the XCache
    char reply_buf[1024];

    ReqMessage Req;
    Req.req_id = 1; 

    std::string send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
    
    int res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);
    
    //! 发送消息请求META
    //! 等待返回
    recv(sock_fd, reply_buf, sizeof(reply_buf), 0);

    // parse the reply
    auto meta = ::xstore::util::Marshal<MetaRsp>::deserialize(reply_buf, 1024);

    std::cout << "first layer size: " << meta.dispatcher_sz << std::endl;
    std::cout << "second layer size: " << meta.total_sz << std::endl;
    std::cout << "TT size: " << meta.tt_size << std::endl;
    
    // std::cout << "Meta received" << std::endl;

    //! 请求first layer

    Req.req_id = 2; 
    send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
    char first_layer_buf[meta.dispatcher_sz];

    res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);
    
    // std::cout << "first layer request sent" << std::endl;
    
    recv(sock_fd, first_layer_buf, sizeof(first_layer_buf), 0);
    
    // std::cout << "first layer data received" << std::endl;
    
//    auto xcache = ::xstore::util::Marshal<XcacheRsp>::deserialize(xcache_buf, meta.total_sz);
//    
//    std::cout << "deserialized first layer" << std::endl;
//    
//    std::cout << xcache.first_layer.size() << std::endl;

    cache = std::make_unique<XCache>(
            std::string(first_layer_buf, meta.dispatcher_sz),
            std::is_same<XCache::FirstML<KK>, TNN<KK>>());
           
//    std::cout << "init first layer" << std::endl;
            
    // std::cout << "cache data received" << std::endl;

    std::cout << "first layer check: " << cache->first_layer.up_bound
                 << "; num: " << cache->first_layer.dispatch_num
                 << " total sz:" << meta.total_sz << std::endl;
                 
    //std::cout << "init first layer done" << std::endl;
    
    Req.req_id = 3; 
    send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
    char* second_layer_buf = new char[meta.total_sz];
    
    res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);

    ssize_t total_received = 0;  // 到目前为止接收到的总字节数
    ssize_t bytes_received;      // 每次调用 recv 接收到的字节数
    
    while (total_received < meta.total_sz) {
    bytes_received = recv(sock_fd, second_layer_buf + total_received, meta.total_sz - total_received, 0);

    if (bytes_received == 0) {
        break;
    } else if (bytes_received < 0) {
        perror("recv failed");
        break;
    }

    total_received += bytes_received;
}

    // int len = recv(sock_fd, second_layer_buf, sizeof(second_layer_buf), 0);

    // std::cout << "second layer length: " << len << std::endl;

    std::cout << "init second layer start:" << meta.total_sz << std::endl;

    // std::cout << "second layer raw data:" << second_layer_buf << std::endl;

    

    XCache::Sub sub;
    const usize submodel_sz = sub.serialize().size();
    // const char* cur_ptr = second_layer_buf;

    ASSERT(cache->second_layer.size() == 0);

    for (uint i = 0; i < cache->first_layer.dispatch_num; ++i) {

        // cache->second_layer.emplace_back(std::make_shared<>(std::string(cur_ptr,submodel_sz)));

        cache->emplace_one_second_model(
          std::string(second_layer_buf, submodel_sz));
        second_layer_buf += submodel_sz;

        

        // std::cout << i <<" err_max: " << cache->second_layer[i]->err_max <<
        //              "err_min: " << cache->second_layer[i]->err_min << std::endl;

        // std::cout << "init second layer " << i << " done:" << meta.total_sz << std::endl;
    }

    // std::cout << "init second layer done:" << cache->second_layer.size() << std::endl;


    // then the TT
    tts.reserve(cache->first_layer.dispatch_num);
            
    //! get TT
            
    Req.req_id = 4; 
    send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
    char* tt_buf = new char[meta.tt_size];

    res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);

    total_received = 0;

    while (total_received < meta.tt_size) {
    bytes_received = recv(sock_fd, tt_buf + total_received, meta.tt_size - total_received, 0);

    // std::cout << "receive: " << bytes_received << std::endl;

    if (bytes_received == 0) {
        break;
    } else if (bytes_received < 0) {
        perror("recv failed");
        break;
    }

    total_received += bytes_received;
}
    
   // recv(sock_fd, tt_buf, sizeof(tt_buf), 0);        
            
    // cur_ptr = tt_buf;

    for (uint i = 0; i < cache->first_layer.dispatch_num; ++i) {

      // if (i % 100 == 0) {
      //           // LOG(4) << "load :" << i << " done; total: " <<
      //           // cache->first_layer.dispatch_num;
      // }

      auto tt_n =
          ::xstore::util::Marshal<u32>::deserialize(tt_buf, sizeof(u64)); //

      const ::xstore::string_view buf((const char*)tt_buf,
                                              sizeof(u32) +
                                                tt_n * sizeof(XCacheTT::ET));
              // serialize one
      tts.emplace_back(buf);
      tt_buf += tts[i].tt_sz();
    }

    // std::cout << "serialzie TTs done:" << meta.tt_size << std::endl;

    // while(1){}

    // read test

    // YCSBCWorkloadUniform ycsb(FLAGS_nkeys);

    std::ifstream map("smap.txt");
    std::string line;

    auto i = FLAGS_nkeys;

    r2::Timer t;
    usize count = 0;
    auto key = KK(0, 0);

    while(1)
    {
        if(getline(map, line))
        {
          std::stringstream ss(line);
          float lon;
          float lat;
          ss >> lon >> lat;
          key = KK(lat, lon);
          count++;

        }

        if (count > FLAGS_nkeys) break;


        //KK key = KK(ycsb.next_key());

        // std::cout << "key: " << key << std::endl;

        const usize read_sz = DBTree::Leaf::value_start_offset();

        // 1. predict
        auto m = cache->select_sec_model(key);

        // std::cout << "second model: " << m << std::endl;



        // auto pos = cache->second_layer[m]->get_point_predict(key);
        
        // std::cout << "second model predict: " << pos << std::endl;

        auto range = cache->get_predict_range(key);

        // std::cout << "range: " << range << std::endl;

        

        // get page range
        auto ns = std::max<int>(std::get<0>(range) / kNPageKey, 0);
        auto ne = std::min<int>(static_cast<int>(tts[m].size() - 1),
                          std::get<1>(range) / kNPageKey);

        // std::cout << "[ns, ne]:" << "[" << ns << ", " << ne << "]" << std::endl;

        // std::cout << "read size" << read_sz << std::endl;
         

        char* Xnode_layer_buf = new char[read_sz];
        char* Value_buf = new char[sizeof(ValType)];

        for (auto p = ns; p <= ne; ++p) 
        {
           // std::cout << "search keys" << std::endl;
            u64 addr = tts.at(m).get_wo_incar(p);
            Req.req_id = 5; 
            Req.ALN = addr;
            send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
            
            res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);

            total_received = 0;

            while (total_received < read_sz) 
            {
              bytes_received = recv(sock_fd, Xnode_layer_buf + total_received, read_sz - total_received, 0);

              // std::cout << "receive: " << bytes_received << std::endl;

              if (bytes_received == 0) {
                  break;
              } else if (bytes_received < 0) {
                  perror("recv failed");
                  break;
              }

              total_received += bytes_received;
            }
    
            // recv(sock_fd, Xnode_layer_buf, sizeof(Xnode_layer_buf), 0);   

            NodeK* keys = reinterpret_cast<NodeK*>(Xnode_layer_buf);

            

            auto idx = keys->search(key);

             

            if (idx)
            {
              Req.req_id = 6; 
              Req.ALN = addr;
              Req.key_idx = *idx;
              send_buf = xstore::util::Marshal<ReqMessage>::serialize_to(Req);
            
              res = send(sock_fd, send_buf.c_str(), send_buf.size(), 0);
    
              int len = recv(sock_fd, Value_buf, sizeof(Value_buf), 0);

              if (len) 
              {
                // std::cout << "read finish" << std::endl;
                // i -- ;
                break;
              }

            }

            if (p == ne)
            {
               //std::cout << "no such key" << std::endl;
              // i -- ;
            }

        }

        
        
    }

    std::cout << "read test for " << 
      FLAGS_nkeys << " keys done in: " << t.passed_msec() << "msec" << std::endl;







}
//         // 2. wait for the XCache to bootstrap done
//         bar.wait();

//         YCSBCWorkloadUniform ycsb(
//           FLAGS_nkeys, 0xdeadbeaf + thread_id + FLAGS_client_name * 73);
//         ::kvs_workloads::StaticLoader other(
//           all_keys, 0xdeadbeaf + thread_id + FLAGS_client_name * 73);

//         worker_id = thread_id;

//         for (uint i = 0; i < FLAGS_coros; ++i) {
//           ssched.spawn([&statics,
//                         &total_processed,
//                         &sender,
//                         &rc,
//                         &alloc1,
//                         &ycsb,
//                         &other,
//                         &rpc,
//                         lkey,
//                         send_buf,
//                         thread_id](R2_ASYNC) {
//             char reply_buf[1024];
//             char* my_buf = reinterpret_cast<char*>(
//               std::get<0>(alloc1.alloc_one(kMaxBatch * 128).value()));

//             r2::Timer t;

//             while (running) {
//               r2::compile_fence();
//               // const auto key = XKey(ycsb.next_key());
//               KK key = KK(other.next_key());

//               if (!FLAGS_vlen) {

//                 auto res = core_eval(key,
//                                      rc,
//                                      rpc,
//                                      sender,
//                                      my_buf,
//                                      statics[thread_id],
//                                      R2_ASYNC_WAIT);

//                 if (thread_id == 0 && R2_COR_ID() == 1) {
//                   statics[0].set_lat(t.passed_msec());
//                   t.reset();
//                 }

//               } else {
//                 auto res = core_eval_v(key, rc, my_buf, R2_ASYNC_WAIT);
//                 // TODO: how to check the value's correctness?
//               }
//               statics[thread_id].increment();
//             }

//             // LOG(4) << "coros: " << R2_COR_ID() << " exit";

//             if (R2_COR_ID() == FLAGS_coros) {
//               R2_STOP();
//             }
//             R2_RET;
//           });
//         }
//         ssched.run();
//         if (thread_id == 0) {
//           LOG(4) << "after run, total processed: " << total_processed
//                  << " at client: " << thread_id;
//         }

//         if (FLAGS_client_name == 1 && worker_id == 0) {
//           error_cdf.finalize();
//           LOG(4) << "Error data: "
//                  << "[average: " << error_cdf.others.average
//                  << ", min: " << error_cdf.others.min
//                  << ", max: " << error_cdf.others.max;
//           LOG(4) << error_cdf.dump_as_np_data() << "\n";
//         }

//         return 0;
//       })));
//   }

//   for (auto& w : workers) {
//     w->start();
//   }

//   bar.wait();
//   Reporter::report_thpt(
//     statics, 20, std::to_string(FLAGS_client_name) + ".xstorelog");
//   running = false;

//   for (auto& w : workers) {
//     w->join();
//   }

//   LOG(4) << "Data distribution client finishes";
//   return 0;
// }
