// #include "../../xutils/local_barrier.hh"

#include <gflags/gflags.h>

DEFINE_uint64(nkeys, 100000, "Number of keys to load");
DEFINE_uint64(alloc_mem_m,
              64,
              "The size of memory to register (in size of MB).");
DEFINE_uint64(ncheck_model, 100, "Number of models to check");

#include "../../deps/r2/src/timer.hh"

#include "../../../xutils/huge_region.hh"

#include "../../deps/r2/src/logging.hh"

#include "../utils.hh"

#include "./server_functions.hh"

#include "./db.hh"

using namespace xstore::util;
using namespace xstore;
//DEFINE_int64(port, 8888, "Server listener (UDP) port.");



using namespace rdmaio;
using namespace rdmaio::rmem;


int main(int argc, char** argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    const usize MB = 1024 * 1024;
    auto mem = HugeRegion::create(FLAGS_alloc_mem_m * 1024 * 1024L).value();
    auto estimated_leaves = FLAGS_nkeys / (kNPageKey / 2) + 16;
    u64 total_sz = sizeof(DBTree::Leaf) * estimated_leaves;
    
    ASSERT(mem->sz > total_sz);

    xalloc = new XAlloc<sizeof(DBTree::Leaf)>((char*)mem->start_ptr(), total_sz);
    db.init_pre_alloced_leaf(*xalloc);

    //! 加载数据
    r2::Timer t;
    ::xstore::load_map(FLAGS_nkeys);
    std::cout << "load Map dataset in :" << t.passed_msec() << " msecs" << std::endl;

    //! 训练Xtree
    ::xstore::train_db("");
    std::cout << "Train dataset in :" << t.passed_msec() << "msecs" << std::endl;
    
    // check the xcache
  for (uint i = 0; i < FLAGS_ncheck_model; ++i) {
    if (cache->second_layer.at(i)->max != 0) {
      R2_LOG(0) << "model x : " << i
             << " error:" << cache->second_layer.at(i)->total_error()
             << "; total: " << cache->second_layer.at(i)->max;
    }
  }

      int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7777);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return -1;
    }

    listen(sock_fd, 3);

    int new_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if ((new_socket = accept(sock_fd, (struct sockaddr *)&client_addr, &client_len))<0) {
        std::cerr << "Accept failed" << std::endl;
        return -1;
    }
    
    std::cout << "init success" << std::endl;
    
    while(1){

        char buf[128] = "";
        std::string Res = "";
        int recv_len = recv(new_socket, buf, sizeof(buf), 0);

        auto Req = ::xstore::util::Marshal<ReqMessage>::deserialize(buf, 128);

        if (Req.req_id == 1)
            Res = MetaCallback();
    
        else if (Req.req_id == 2)
            Res = FirstCallback();
            
        else if (Req.req_id == 3)
            Res = SecondCallback();

        else if (Req.req_id == 4)
            Res = TTCallback();

        else if (Req.req_id == 5)
            Res = KeysCallback(Req.ALN);

        else if (Req.req_id == 6)
            Res = ValueCallback(Req.ALN, Req.key_idx);


        send(new_socket, Res.c_str(), Res.size(), 0);
    }

    return 0;

}
