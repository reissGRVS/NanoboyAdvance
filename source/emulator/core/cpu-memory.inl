/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

inline std::uint32_t CPU::ReadBIOS(std::uint32_t address) {
  // TODO: clean this mess up.
  int shift = (address & 3) * 8;
  
  address &= ~3;

  if (address >= 0x4000) {
    return ReadUnused(address) >> shift;
  }
  
  if (state.r15 >= 0x4000) {
    return memory.bios_latch >> shift;
  }

  memory.bios_latch = UpdateOpenBus32(Read<std::uint32_t>(memory.bios, address));

  return memory.bios_latch >> shift;
}

inline std::uint32_t CPU::ReadUnused(std::uint32_t address) {    
  return memory.latch >> ((address & 3) * 8);
}

inline auto CPU::ReadByte(std::uint32_t address, Access access) -> std::uint8_t {
  int page = address >> 24;
  int cycles = cycles16[int(access)][page];

  switch (page) {
  case REGION_BIOS: {
    PrefetchStepRAM(cycles);
    return ReadBIOS(address);
  }
  case REGION_EWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusDup8(Read<std::uint8_t>(memory.wram, address & 0x3FFFF));
  }
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusIWRAM8(address, Read<std::uint8_t>(memory.iram, address & 0x7FFF));
  }
  case REGION_MMIO: {
    // TODO: figure out open bus behaviour.
    // We likely have to fetch the whole 32-bit IO register...
    PrefetchStepRAM(cycles);
    return ReadMMIO(address);
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusDup8(Read<std::uint8_t>(ppu.pram, address & 0x3FF));
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000)
      address &= ~0x8000;
    return UpdateOpenBusDup8(Read<std::uint8_t>(ppu.vram, address));
  }
  case REGION_OAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(ppu.oam, address & 0x3FF)) >> ((address & 3) * 8);
  }
  case REGION_ROM_W0_L:
  case REGION_ROM_W0_H:
  case REGION_ROM_W1_L:
  case REGION_ROM_W1_H:
  case REGION_ROM_W2_L:
  case REGION_ROM_W2_H: {
    if ((address & 0x1FFFF) != 0) {
      PrefetchStepROM(address, cycles);
    } else {
      PrefetchStepROM(address, cycles16[int(Access::Nonsequential)][page]);
    }
    address &= memory.rom.mask;
    if (address >= memory.rom.size) {
      return UpdateOpenBusDup8((address / 2) >> ((address & 1) * 8));
    }
    return UpdateOpenBusDup8(Read<std::uint8_t>(memory.rom.data.get(), address));
  }
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    // TODO: confirm that the bus duplicates the 8-bit byte for SRAM/FLASH as well.
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (IsGPIOAccess(address) && memory.rom.gpio->IsReadable()) {
      return UpdateOpenBusDup8(memory.rom.gpio->Read(address));
    }
    if (memory.rom.backup_sram) {
      return UpdateOpenBusDup8(memory.rom.backup_sram->Read(address));
    }
    return 0;
  }  
  default: {
    PrefetchStepRAM(cycles);
    return ReadUnused(address);
  }
  }
}

inline auto CPU::ReadHalf(std::uint32_t address, Access access) -> std::uint16_t {
  int page = address >> 24;
  int cycles = cycles16[int(access)][page];

  if (page != REGION_SRAM_1 && page != REGION_SRAM_2) {
    address &= ~1;
  }
  
  switch (page) {
  case REGION_BIOS: {
    PrefetchStepRAM(cycles);
    return ReadBIOS(address);
  }
  case REGION_EWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusDup16(Read<std::uint16_t>(memory.wram, address & 0x3FFFF));
  }
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusIWRAM16(address, Read<std::uint16_t>(memory.iram, address & 0x7FFF));
  }
  case REGION_MMIO: {
    // TODO: figure out open bus behaviour.
    // We likely have to fetch the whole 32-bit IO register...
    PrefetchStepRAM(cycles);
    return  ReadMMIO(address + 0) |
           (ReadMMIO(address + 1) << 8);
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBusDup16(Read<std::uint16_t>(ppu.pram, address & 0x3FF));
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000)
      address &= ~0x8000;
    return UpdateOpenBusDup16(Read<std::uint16_t>(ppu.vram, address));
  }
  case REGION_OAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(ppu.oam, address & 0x3FF)) >> ((address & 3) * 8);
  } 
  /* 0x0DXXXXXX may be used to read/write from EEPROM */
  case REGION_ROM_W2_H: {
    PrefetchStepROM(address, cycles);
    /* Must check if this is an EEPROM access or ordinary ROM mirror read. */
    if (IsEEPROMAccess(address)) {
      /* TODO: this is not a very nice way to do this. */
      if (!dma.IsRunning()) {
        return UpdateOpenBusDup16(1);
      }
      return UpdateOpenBusDup16(memory.rom.backup_eeprom->Read(address));
    }
    [[fallthrough]];
  }
  case REGION_ROM_W0_L:
  case REGION_ROM_W0_H:
  case REGION_ROM_W1_L:
  case REGION_ROM_W1_H:
  case REGION_ROM_W2_L: {
    if ((address & 0x1FFFF) != 0) {
      PrefetchStepROM(address, cycles);
    } else {
      PrefetchStepROM(address, cycles16[int(Access::Nonsequential)][page]);
    }
    address &= memory.rom.mask;
    if (IsGPIOAccess(address) && memory.rom.gpio->IsReadable()) {
      return UpdateOpenBusDup16(memory.rom.gpio->Read(address));
    }
    if (address >= memory.rom.size) {
      return UpdateOpenBusDup16(address / 2);
    }
    return UpdateOpenBusDup16(Read<std::uint16_t>(memory.rom.data.get(), address));
  }
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    // TODO: confirm that the bus duplicates the 16-bit word for SRAM/FLASH as well.
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (memory.rom.backup_sram) {
      return UpdateOpenBusDup16(memory.rom.backup_sram->Read(address) * 0x0101);
    }
    return 0;
  }
  default: {
    PrefetchStepRAM(cycles);
    return ReadUnused(address);
  }
  }
}

inline auto CPU::ReadWord(std::uint32_t address, Access access) -> std::uint32_t {
  int page = address >> 24;
  int cycles = cycles32[int(access)][page];

  if (page != REGION_SRAM_1 && page != REGION_SRAM_2) {
    address &= ~3;
  }
  
  switch (page) {
  case REGION_BIOS: {
    PrefetchStepRAM(cycles);
    return ReadBIOS(address);
  }
  case REGION_EWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(memory.wram, address & 0x3FFFF));
  }
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(memory.iram, address & 0x7FFF ));
  }
  case REGION_MMIO: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(ReadMMIO(address + 0) |
                          (ReadMMIO(address + 1) << 8 ) |
                          (ReadMMIO(address + 2) << 16) |
                          (ReadMMIO(address + 3) << 24));
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(ppu.pram, address & 0x3FF));
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000)
      address &= ~0x8000;
    return UpdateOpenBus32(Read<std::uint32_t>(ppu.vram, address));
  }
  case REGION_OAM: {
    PrefetchStepRAM(cycles);
    return UpdateOpenBus32(Read<std::uint32_t>(ppu.oam, address & 0x3FF));
  }
  case REGION_ROM_W0_L:
  case REGION_ROM_W0_H:
  case REGION_ROM_W1_L:
  case REGION_ROM_W1_H:
  case REGION_ROM_W2_L:
  case REGION_ROM_W2_H: {
    if ((address & 0x1FFFF) != 0) {
      PrefetchStepROM(address, cycles);
    } else {
      PrefetchStepROM(address, cycles32[int(Access::Nonsequential)][page]);
    }
    address &= memory.rom.mask;
    if (IsGPIOAccess(address) && memory.rom.gpio->IsReadable()) {
      return UpdateOpenBus32(memory.rom.gpio->Read(address + 0) |
                            (memory.rom.gpio->Read(address + 2) << 16));
    }
    if (address >= memory.rom.size) {
      return UpdateOpenBus32((((address + 0) / 2) & 0xFFFF) |
                             (((address + 2) / 2) << 16));
    }
    return UpdateOpenBus32(Read<std::uint32_t>(memory.rom.data.get(), address));
  }
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (memory.rom.backup_sram) {
      return UpdateOpenBus32(memory.rom.backup_sram->Read(address) * 0x01010101);
    }
    return UpdateOpenBus32(0);
  }
  default: {
    PrefetchStepRAM(cycles);
    return ReadUnused(address);
  }
  }
}

inline void CPU::WriteByte(std::uint32_t address, std::uint8_t value, Access access) {
  int page = address >> 24;
  int cycles = cycles16[int(access)][page];

  switch (page) {
  case REGION_EWRAM:
    PrefetchStepRAM(cycles);
    Write<std::uint8_t>(memory.wram, address & 0x3FFFF, value);
    break;
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint8_t>(memory.iram, address & 0x7FFF,  value);
    break;
  }
  case REGION_MMIO: {
    PrefetchStepRAM(cycles);
    WriteMMIO(address, value & 0xFF);
    break;
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint16_t>(ppu.pram, address & 0x3FF, value * 0x0101);
    break;
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000) {
      address &= ~0x8000;
    }
    if (address >= (ppu.mmio.dispcnt.mode >= 3 ? 0x14000 : 0x10000)) {
      break;
    }
    Write<std::uint16_t>(ppu.vram, address, value * 0x0101);
    break;
  }
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (memory.rom.backup_sram) {
      memory.rom.backup_sram->Write(address, value);
    }
    break;
  }
  }
}

inline void CPU::WriteHalf(std::uint32_t address, std::uint16_t value, Access access) {
  int page = address >> 24;
  int cycles = cycles16[int(access)][page];

  if (page != REGION_SRAM_1 && page != REGION_SRAM_2) {
    address &= ~1;
  }

  switch (page) {
  case REGION_EWRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint16_t>(memory.wram, address & 0x3FFFF, value);
    break;
  }
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint16_t>(memory.iram, address & 0x7FFF,  value);
    break;
  }
  case REGION_MMIO: {
    PrefetchStepRAM(cycles);
    WriteMMIO(address + 0, (value >> 0) & 0xFF);
    WriteMMIO(address + 1, (value >> 8) & 0xFF);
    break;
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint16_t>(ppu.pram, address & 0x3FF, value);
    break;
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000) {
      address &= ~0x8000;
    }
    Write<std::uint16_t>(ppu.vram, address, value);
    break;
  }
  case REGION_OAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint16_t>(ppu.oam, address & 0x3FF, value);
    break;
  }
  case REGION_ROM_W0_L: case REGION_ROM_W0_H:
  case REGION_ROM_W1_L: case REGION_ROM_W1_H:
  case REGION_ROM_W2_L: {
    PrefetchStepROM(address, cycles);
    address &= 0x1FFFFFF;
    if (IsGPIOAccess(address)) {
      memory.rom.gpio->Write(address + 0, value & 0xFF);
      memory.rom.gpio->Write(address + 1, value >> 8);
    }
    break;
  }
  case REGION_ROM_W2_H: {
    PrefetchStepROM(address, cycles);
    if (IsEEPROMAccess(address)) {
      /* TODO: this is not a very nice way to do this. */
      if (!dma.IsRunning()) {
        break;
      }
      memory.rom.backup_eeprom->Write(address, value);
      break;
    }
    address &= 0x1FFFFFF;
    if (IsGPIOAccess(address)) {
      memory.rom.gpio->Write(address, value & 0xFF);
      break;
    }
    break;
  }  
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (memory.rom.backup_sram) {
      memory.rom.backup_sram->Write(address, value >> ((address & 1) * 8));
    }
    break;
  }
  }
}

inline void CPU::WriteWord(std::uint32_t address, std::uint32_t value, Access access) {
  int page = address >> 24;
  int cycles = cycles32[int(access)][page];

  if (page != REGION_SRAM_1 && page != REGION_SRAM_2) {
    address &= ~3;
  }

  UpdateOpenBus32(value);

  switch (page) {
  case REGION_EWRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint32_t>(memory.wram, address & 0x3FFFF, value);
    break;
  }
  case REGION_IWRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint32_t>(memory.iram, address & 0x7FFF,  value);
    break;
  }
  case REGION_MMIO: {
    PrefetchStepRAM(cycles);
    WriteMMIO(address + 0, (value >>  0) & 0xFF);
    WriteMMIO(address + 1, (value >>  8) & 0xFF);
    WriteMMIO(address + 2, (value >> 16) & 0xFF);
    WriteMMIO(address + 3, (value >> 24) & 0xFF);
    break;
  }
  case REGION_PRAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint32_t>(ppu.pram, address & 0x3FF, value);
    break;
  }
  case REGION_VRAM: {
    PrefetchStepRAM(cycles);
    address &= 0x1FFFF;
    if (address >= 0x18000) {
      address &= ~0x8000;
    }
    Write<std::uint32_t>(ppu.vram, address, value);
    break;
  }
  case REGION_OAM: {
    PrefetchStepRAM(cycles);
    Write<std::uint32_t>(ppu.oam, address & 0x3FF, value);
    break;
  }
  case REGION_ROM_W0_L: case REGION_ROM_W0_H:
  case REGION_ROM_W1_L: case REGION_ROM_W1_H:
  case REGION_ROM_W2_L: case REGION_ROM_W2_H: {
    PrefetchStepROM(address, cycles);
    address &= 0x1FFFFFF;
    if (IsGPIOAccess(address)) {
      memory.rom.gpio->Write(address + 0, (value >>  0) & 0xFF);
      memory.rom.gpio->Write(address + 2, (value >> 16) & 0xFF);
    }
    break;
  }
  case REGION_SRAM_1:
  case REGION_SRAM_2: {
    PrefetchStepROM(address, cycles);
    address &= 0x0EFFFFFF;
    if (memory.rom.backup_sram) {
      memory.rom.backup_sram->Write(address, value >> ((address & 3) * 8));
    }
    break;
  }
  }
}
