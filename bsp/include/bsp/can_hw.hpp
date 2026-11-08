// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "protocol/can_frame.hpp"

// CAN controller (FDCAN1 in classic CAN mode, 500 kbit/s) and its pins.
namespace bsp::can_hw {

enum class Mode : std::uint8_t {
  kNormal,
  kInternalLoopback,  // frames loop back inside the MCU: test the whole stack with no bus
};

// Called from the CAN interrupt for every accepted frame.
using RxCallback = void (*)(const protocol::CanFrame& frame);

struct BusState {
  bool bus_off;
  bool error_passive;
  bool warning;
  std::uint8_t tx_errors;  // transmit error counter (TEC)
  std::uint8_t rx_errors;  // receive error counter (REC)
};

// (Re)initializes the controller. Only frames with identifier `accept_id` are delivered.
// Uses HAL timeouts, so call it from a task (or before any FreeRTOS object exists).
bool start(Mode mode, std::uint32_t accept_id, RxCallback callback);
// Queues a frame in the hardware TX FIFO. False if the FIFO is full.
bool send(const protocol::CanFrame& frame);
[[nodiscard]] BusState bus_state();
// After bus-off the controller stays offline until software restarts it; it then waits for
// 128 x 11 recessive bits, as the CAN standard requires, before rejoining the bus.
void recover_from_bus_off();

}  // namespace bsp::can_hw
