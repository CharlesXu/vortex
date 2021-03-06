#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "util.h"
#include "instr.h"
#include "core.h"

using namespace vortex;

Warp::Warp(Core *core, Word id)
    : id_(id)
    , core_(core)
    , pc_(0x80000000)
    , shadowPc_(0)
    , activeThreads_(0)
    , shadowActiveThreads_(0)
    , shadowReg_(core_->arch().getNumRegs())
    , VLEN_(1024)
    , interruptEnable_(true)
    , shadowInterruptEnable_(false)
    , supervisorMode_(true)
    , shadowSupervisorMode_(false)
    , spawned_(false)
    , steps_(0)
    , insts_(0)
    , loads_(0)
    , stores_(0) {
  D(3, "Creating a new thread with PC: " << std::hex << pc_);
  /* Build the register file. */
  Word regNum(0);
  for (Word j = 0; j < core_->arch().getNumThreads(); ++j) {
    regFile_.push_back(std::vector<Reg<Word>>(0));
    for (Word i = 0; i < core_->arch().getNumRegs(); ++i) {
      regFile_[j].push_back(Reg<Word>(id, regNum++));
    }

    bool act = false;
    if (j == 0)
      act = true;
    tmask_.push_back(act);
    shadowTmask_.push_back(act);
  }

  for (Word i = 0; i < (1 << 12); i++) {
    csrs_.push_back(Reg<uint16_t>(id, regNum++));
  }

  /* Set initial register contents. */
  regFile_[0][0] = (core_->arch().getNumThreads() << (core_->arch().getWordSize() * 8 / 2)) | id;
}

void Warp::step(trace_inst_t *trace_inst) {
  Size fetchPos(0);
  Size decPos;
  Size wordSize(core_->arch().getWordSize());
  std::vector<Byte> fetchBuffer(wordSize);

  if (activeThreads_ == 0)
    return;

  ++steps_;

  D(3, "current PC=0x" << std::hex << pc_);

  // std::cout << "pc: " << std::hex << pc << "\n";
  trace_inst->pc = pc_;

  /* Fetch and decode. */
  if (wordSize < sizeof(pc_))
    pc_ &= ((1ll << (wordSize * 8)) - 1);
    
  unsigned fetchSize = 4;
  fetchBuffer.resize(fetchSize);
  Word fetched = core_->mem().fetch(pc_ + fetchPos, supervisorMode_);
  writeWord(fetchBuffer, fetchPos, fetchSize, fetched);

  decPos = 0;
  std::shared_ptr<Instr> instr = core_->decoder().decode(fetchBuffer, decPos, trace_inst);

  // Update pc
  pc_ += decPos;

  // Execute
  this->execute(*instr, trace_inst);

  // At Debug Level 3, print debug info after each instruction.
  D(3, "Register state:");
  for (unsigned i = 0; i < regFile_[0].size(); ++i) {
    D_RAW("  %r" << std::setfill('0') << std::setw(2) << std::dec << i << ':');
    for (unsigned j = 0; j < (activeThreads_); ++j)
      D_RAW(' ' << std::setfill('0') << std::setw(8) << std::hex << regFile_[j][i] << std::setfill(' ') << ' ');
    D_RAW('(' << shadowReg_[i] << ')' << std::endl);
  }

  DPH(3, "Thread mask:");
  for (unsigned i = 0; i < tmask_.size(); ++i)
    DPN(3, " " << tmask_[i]);
  DPN(3, "\n");
}

bool Warp::interrupt(Word r0) {
  if (!interruptEnable_)
    return false;

  shadowActiveThreads_   = activeThreads_;
  shadowTmask_           = tmask_;
  shadowInterruptEnable_ = interruptEnable_; /* For traps. */
  shadowSupervisorMode_  = supervisorMode_;

  for (Word i = 0; i < regFile_[0].size(); ++i)
    shadowReg_[i] = regFile_[0][i];

  for (Word i = 0; i < regFile_.size(); ++i)
    tmask_[i] = 1;

  shadowPc_ = pc_;
  activeThreads_ = 1;
  interruptEnable_ = false;
  supervisorMode_ = true;
  regFile_[0][0] = r0;
  pc_ = core_->interruptEntry();

  return true;
}

void Warp::printStats() const {
  std::cout << "Steps : " << steps_ << std::endl
            << "Insts : " << insts_ << std::endl
            << "Loads : " << loads_ << std::endl
            << "Stores: " << stores_ << std::endl;
}