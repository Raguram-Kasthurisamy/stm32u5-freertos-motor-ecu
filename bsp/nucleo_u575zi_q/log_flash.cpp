// Author: Raguram Kasthurisamy

// Fault-log flash page: the last 8 KiB page of bank 2. The code runs from bank 1, so with the
// U5's dual-bank flash the CPU keeps executing (read-while-write) during erase and program.

#include "bsp/log_flash.hpp"

#include <array>
#include <cstring>

#include "bsp/hal.hpp"

extern "C" const std::uint8_t _fault_log_start[];
extern "C" const std::uint8_t _fault_log_size[];  // linker symbol: its address is the size

namespace bsp::log_flash {
namespace {

std::uint32_t base_address() { return reinterpret_cast<std::uint32_t>(_fault_log_start); }
std::size_t size_bytes() { return reinterpret_cast<std::size_t>(_fault_log_size); }

}  // namespace

std::span<const std::uint8_t> region() { return {_fault_log_start, size_bytes()}; }

bool erase() {
  const std::uint32_t address = base_address();
  FLASH_EraseInitTypeDef erase_init{};
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks = address >= (FLASH_BASE + FLASH_BANK_SIZE) ? FLASH_BANK_2 : FLASH_BANK_1;
  erase_init.Page = ((address - FLASH_BASE) % FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
  erase_init.NbPages = static_cast<std::uint32_t>(size_bytes() / FLASH_PAGE_SIZE);
  std::uint32_t page_error = 0;

  (void)HAL_FLASH_Unlock();
  const bool ok = HAL_FLASHEx_Erase(&erase_init, &page_error) == HAL_OK;
  (void)HAL_FLASH_Lock();
  (void)HAL_ICACHE_Invalidate();  // the cache may still hold the old contents
  return ok;
}

bool program(std::size_t offset, std::span<const std::uint8_t> data) {
  if (offset % kWriteUnit != 0U || data.size() % kWriteUnit != 0U ||
      offset + data.size() > size_bytes()) {
    return false;
  }
  bool ok = true;
  (void)HAL_FLASH_Unlock();
  for (std::size_t i = 0; i < data.size(); i += kWriteUnit) {
    std::array<std::uint32_t, 4> quad_word{};
    std::memcpy(quad_word.data(), data.data() + i, kWriteUnit);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                          base_address() + static_cast<std::uint32_t>(offset + i),
                          reinterpret_cast<std::uint32_t>(quad_word.data())) != HAL_OK) {
      ok = false;
      break;
    }
  }
  (void)HAL_FLASH_Lock();
  (void)HAL_ICACHE_Invalidate();
  return ok;
}

}  // namespace bsp::log_flash
