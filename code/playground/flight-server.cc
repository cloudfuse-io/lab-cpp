#include <arrow/flight/api.h>
#include <arrow/util/logging.h>

#include <csignal>
#include <iostream>

namespace arrow {

namespace flight {

class MyFlightServer : public FlightServerBase {
  Status ListFlights(const ServerCallContext& context, const Criteria* criteria,
                     std::unique_ptr<FlightListing>* listings) override {
    std::vector<FlightInfo> flights = {FlightInfo(FlightInfo::Data{
        .schema = "",
        .descriptor = {},
        .endpoints = {},
        .total_records = 0,
        .total_bytes = 0,
    })};
    *listings = std::unique_ptr<FlightListing>(new SimpleFlightListing(flights));
    return Status::OK();
  }
};

}  // namespace flight
}  // namespace arrow

int main() {
  auto server = std::make_unique<arrow::flight::MyFlightServer>();
  // Initialize server
  arrow::flight::Location location;
  int port = 80;
  // Listen to all interfaces on a free port
  std::cout << "ForGrpcTcp 0.0.0.0:" << port << std::endl;
  ARROW_CHECK_OK(arrow::flight::Location::ForGrpcTcp("0.0.0.0", port, &location));
  arrow::flight::FlightServerOptions options(location);

  // Start the server
  std::cout << "Init" << std::endl;
  ARROW_CHECK_OK(server->Init(options));
  // Exit with a clean error code (0) on SIGTERM
  std::cout << "server->SetShutdownOnSignals" << std::endl;
  ARROW_CHECK_OK(server->SetShutdownOnSignals({SIGTERM}));

  std::cout << "Server listening on localhost:" << server->port() << std::endl;
  ARROW_CHECK_OK(server->Serve());
}