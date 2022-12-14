// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: pipeline.proto
#ifndef GRPC_pipeline_2eproto__INCLUDED
#define GRPC_pipeline_2eproto__INCLUDED

#include "pipeline.pb.h"

#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/method_handler_impl.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/impl/codegen/rpc_method.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/impl/codegen/stub_options.h>
#include <grpcpp/impl/codegen/sync_stream.h>

namespace grpc {
class CompletionQueue;
class Channel;
class ServerCompletionQueue;
class ServerContext;
}  // namespace grpc

namespace Lmm {

class PipelineService final {
 public:
  static constexpr char const* service_full_name() {
    return "Lmm.PipelineService";
  }
  class StubInterface {
   public:
    virtual ~StubInterface() {}
    virtual ::grpc::Status GetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::Lmm::PipelinesInfo* response) = 0;
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>> AsyncGetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>>(AsyncGetInfoRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>> PrepareAsyncGetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>>(PrepareAsyncGetInfoRaw(context, request, cq));
    }
    virtual ::grpc::Status GetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::Lmm::QueueInfo* response) = 0;
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>> AsyncGetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>>(AsyncGetQueueInfoRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>> PrepareAsyncGetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>>(PrepareAsyncGetQueueInfoRaw(context, request, cq));
    }
  private:
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>* AsyncGetInfoRaw(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::PipelinesInfo>* PrepareAsyncGetInfoRaw(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>* AsyncGetQueueInfoRaw(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::Lmm::QueueInfo>* PrepareAsyncGetQueueInfoRaw(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) = 0;
  };
  class Stub final : public StubInterface {
   public:
    Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel);
    ::grpc::Status GetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::Lmm::PipelinesInfo* response) override;
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>> AsyncGetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>>(AsyncGetInfoRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>> PrepareAsyncGetInfo(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>>(PrepareAsyncGetInfoRaw(context, request, cq));
    }
    ::grpc::Status GetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::Lmm::QueueInfo* response) override;
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>> AsyncGetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>>(AsyncGetQueueInfoRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>> PrepareAsyncGetQueueInfo(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>>(PrepareAsyncGetQueueInfoRaw(context, request, cq));
    }

   private:
    std::shared_ptr< ::grpc::ChannelInterface> channel_;
    ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>* AsyncGetInfoRaw(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientAsyncResponseReader< ::Lmm::PipelinesInfo>* PrepareAsyncGetInfoRaw(::grpc::ClientContext* context, const ::Lmm::GenericQ& request, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>* AsyncGetQueueInfoRaw(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientAsyncResponseReader< ::Lmm::QueueInfo>* PrepareAsyncGetQueueInfoRaw(::grpc::ClientContext* context, const ::Lmm::QueueInfoQ& request, ::grpc::CompletionQueue* cq) override;
    const ::grpc::internal::RpcMethod rpcmethod_GetInfo_;
    const ::grpc::internal::RpcMethod rpcmethod_GetQueueInfo_;
  };
  static std::unique_ptr<Stub> NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options = ::grpc::StubOptions());

  class Service : public ::grpc::Service {
   public:
    Service();
    virtual ~Service();
    virtual ::grpc::Status GetInfo(::grpc::ServerContext* context, const ::Lmm::GenericQ* request, ::Lmm::PipelinesInfo* response);
    virtual ::grpc::Status GetQueueInfo(::grpc::ServerContext* context, const ::Lmm::QueueInfoQ* request, ::Lmm::QueueInfo* response);
  };
  template <class BaseClass>
  class WithAsyncMethod_GetInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithAsyncMethod_GetInfo() {
      ::grpc::Service::MarkMethodAsync(0);
    }
    ~WithAsyncMethod_GetInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status GetInfo(::grpc::ServerContext* context, const ::Lmm::GenericQ* request, ::Lmm::PipelinesInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestGetInfo(::grpc::ServerContext* context, ::Lmm::GenericQ* request, ::grpc::ServerAsyncResponseWriter< ::Lmm::PipelinesInfo>* response, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncUnary(0, context, request, response, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithAsyncMethod_GetQueueInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithAsyncMethod_GetQueueInfo() {
      ::grpc::Service::MarkMethodAsync(1);
    }
    ~WithAsyncMethod_GetQueueInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status GetQueueInfo(::grpc::ServerContext* context, const ::Lmm::QueueInfoQ* request, ::Lmm::QueueInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestGetQueueInfo(::grpc::ServerContext* context, ::Lmm::QueueInfoQ* request, ::grpc::ServerAsyncResponseWriter< ::Lmm::QueueInfo>* response, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncUnary(1, context, request, response, new_call_cq, notification_cq, tag);
    }
  };
  typedef WithAsyncMethod_GetInfo<WithAsyncMethod_GetQueueInfo<Service > > AsyncService;
  template <class BaseClass>
  class WithGenericMethod_GetInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithGenericMethod_GetInfo() {
      ::grpc::Service::MarkMethodGeneric(0);
    }
    ~WithGenericMethod_GetInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status GetInfo(::grpc::ServerContext* context, const ::Lmm::GenericQ* request, ::Lmm::PipelinesInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
  };
  template <class BaseClass>
  class WithGenericMethod_GetQueueInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithGenericMethod_GetQueueInfo() {
      ::grpc::Service::MarkMethodGeneric(1);
    }
    ~WithGenericMethod_GetQueueInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status GetQueueInfo(::grpc::ServerContext* context, const ::Lmm::QueueInfoQ* request, ::Lmm::QueueInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
  };
  template <class BaseClass>
  class WithStreamedUnaryMethod_GetInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithStreamedUnaryMethod_GetInfo() {
      ::grpc::Service::MarkMethodStreamed(0,
        new ::grpc::internal::StreamedUnaryHandler< ::Lmm::GenericQ, ::Lmm::PipelinesInfo>(std::bind(&WithStreamedUnaryMethod_GetInfo<BaseClass>::StreamedGetInfo, this, std::placeholders::_1, std::placeholders::_2)));
    }
    ~WithStreamedUnaryMethod_GetInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable regular version of this method
    ::grpc::Status GetInfo(::grpc::ServerContext* context, const ::Lmm::GenericQ* request, ::Lmm::PipelinesInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    // replace default version of method with streamed unary
    virtual ::grpc::Status StreamedGetInfo(::grpc::ServerContext* context, ::grpc::ServerUnaryStreamer< ::Lmm::GenericQ,::Lmm::PipelinesInfo>* server_unary_streamer) = 0;
  };
  template <class BaseClass>
  class WithStreamedUnaryMethod_GetQueueInfo : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service *service) {}
   public:
    WithStreamedUnaryMethod_GetQueueInfo() {
      ::grpc::Service::MarkMethodStreamed(1,
        new ::grpc::internal::StreamedUnaryHandler< ::Lmm::QueueInfoQ, ::Lmm::QueueInfo>(std::bind(&WithStreamedUnaryMethod_GetQueueInfo<BaseClass>::StreamedGetQueueInfo, this, std::placeholders::_1, std::placeholders::_2)));
    }
    ~WithStreamedUnaryMethod_GetQueueInfo() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable regular version of this method
    ::grpc::Status GetQueueInfo(::grpc::ServerContext* context, const ::Lmm::QueueInfoQ* request, ::Lmm::QueueInfo* response) final override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    // replace default version of method with streamed unary
    virtual ::grpc::Status StreamedGetQueueInfo(::grpc::ServerContext* context, ::grpc::ServerUnaryStreamer< ::Lmm::QueueInfoQ,::Lmm::QueueInfo>* server_unary_streamer) = 0;
  };
  typedef WithStreamedUnaryMethod_GetInfo<WithStreamedUnaryMethod_GetQueueInfo<Service > > StreamedUnaryService;
  typedef Service SplitStreamedService;
  typedef WithStreamedUnaryMethod_GetInfo<WithStreamedUnaryMethod_GetQueueInfo<Service > > StreamedService;
};

}  // namespace Lmm


#endif  // GRPC_pipeline_2eproto__INCLUDED
