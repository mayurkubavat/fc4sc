/******************************************************************************

   Copyright 2026 Mayur Kubavat

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

******************************************************************************/

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#include "fc4sc.hpp"
#include "fc4sc_crave.hpp"
#include "gtest/gtest.h"

namespace {

class uvm_bus_cg : public covergroup {
public:
  CG_CONS(uvm_bus_cg) {};

  unsigned int addr = 0;
  unsigned int mode = 0;

  COVERPOINT(unsigned int, addr_cvp, addr) {
    bin<unsigned int>("low", interval(0u, 3u)),
    bin<unsigned int>("mid", interval(4u, 7u)),
    bin<unsigned int>("high", interval(8u, 11u))
  };

  COVERPOINT(unsigned int, mode_cvp, mode) {
    bin<unsigned int>("rd", 0u),
    bin<unsigned int>("wr", 1u)
  };

  cross<unsigned int, unsigned int> am_crs = cross<unsigned int, unsigned int>(
      this, "am_crs", &addr_cvp, &mode_cvp);
};

class uvm_bus_item : public uvm_randomized_sequence_item {
public:
  crave::crv_variable<unsigned int> addr;
  crave::crv_variable<unsigned int> mode;

  crave::crv_constraint c_dom {
    addr() <= 15u,
    mode() <= 3u
  };

  UVM_OBJECT_UTILS(uvm_bus_item);

  uvm_bus_item(crave::crv_object_name name = "uvm_bus_item")
      : uvm_randomized_sequence_item(name) {
    addr = 0;
    mode = 0;
  }

  uvm_bus_item(const uvm_bus_item& rhs)
      : uvm_randomized_sequence_item(rhs.get_name()) {
    *this = rhs;
  }

  uvm_bus_item& operator=(const uvm_bus_item& rhs) {
    if (this != &rhs) {
      uvm::uvm_sequence_item::operator=(rhs);
      addr = static_cast<unsigned int>(rhs.addr);
      mode = static_cast<unsigned int>(rhs.mode);
    }
    return *this;
  }
};

class cdv_uvm_seq : public uvm::uvm_sequence<uvm_bus_item> {
public:
  uvm_bus_cg* cov = nullptr;

  UVM_OBJECT_UTILS(cdv_uvm_seq);

  cdv_uvm_seq(const std::string& name = "cdv_uvm_seq")
      : uvm::uvm_sequence<uvm_bus_item>(name) {}

  void body() override {
    for (int i = 0; i < 24 && cov->get_inst_coverage() < 100.0; ++i) {
      uvm_bus_item* req = nullptr;
      UVM_DO_WITH(req, (
          fc4sc::hole_constraint(req->addr(), cov->addr_cvp),
          fc4sc::hole_constraint(req->mode(), cov->mode_cvp),
          fc4sc::cross_hole_constraint(cov->am_crs, req->addr(), req->mode(), 64)
      ));
    }
  }
};

class cdv_uvm_driver : public uvm::uvm_driver<uvm_bus_item> {
public:
  uvm_bus_cg* cov = nullptr;
  int tx_count = 0;

  UVM_COMPONENT_UTILS(cdv_uvm_driver);

  cdv_uvm_driver(uvm::uvm_component_name name)
      : uvm::uvm_driver<uvm_bus_item>(name) {}

  void run_phase(uvm::uvm_phase& phase) override {
    (void)phase;
    for (;;) {
      uvm_bus_item req;
      seq_item_port->get_next_item(req);
      cov->addr = static_cast<unsigned int>(req.addr);
      cov->mode = static_cast<unsigned int>(req.mode);
      cov->sample();
      ++tx_count;
      seq_item_port->item_done();
    }
  }
};

class cdv_uvm_test : public uvm::uvm_test {
public:
  uvm::uvm_sequencer<uvm_bus_item>* seqr = nullptr;
  cdv_uvm_driver* drv = nullptr;
  uvm_bus_cg* cov = nullptr;

  UVM_COMPONENT_UTILS(cdv_uvm_test);

  cdv_uvm_test(uvm::uvm_component_name name)
      : uvm::uvm_test(name) {}

  void build_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::build_phase(phase);
    uvm::uvm_config_db<uvm_bus_cg*>::get(this, "", "cov", cov);
    seqr = new uvm::uvm_sequencer<uvm_bus_item>("seqr");
    drv = cdv_uvm_driver::type_id::create("drv", this);
  }

  void connect_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::connect_phase(phase);
    drv->seq_item_port.connect(seqr->seq_item_export);
  }

  void run_phase(uvm::uvm_phase& phase) override {
    phase.raise_objection(this);
    drv->cov = cov;
    cdv_uvm_seq* seq = cdv_uvm_seq::type_id::create("seq");
    seq->cov = cov;
    seq->start(seqr);
    phase.drop_objection(this);
  }

  void check_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::check_phase(phase);
    if (drv->tx_count > 6) {
      UVM_ERROR("TX_CNT", "Expected <= 6 transactions to reach 100% coverage");
    }
  }
};

} // namespace

TEST(crave2uvm_cdv, uvm_do_with_reaches_100_percent_coverage) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    uvm_bus_cg cov("uvm_bus_cg", __FILE__, __LINE__, cntxt);
    uvm::uvm_config_db<uvm_bus_cg*>::set(nullptr, "*", "cov", &cov);
    uvm::run_test("cdv_uvm_test");

    EXPECT_EQ(cov.addr_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cov.mode_cvp.get_inst_coverage(), 100.0);
    EXPECT_EQ(cov.am_crs.get_inst_coverage(), 100.0);
    EXPECT_EQ(cov.get_inst_coverage(), 100.0);
    EXPECT_EQ(uvm::uvm_report_server::get_server()->get_severity_count(uvm::UVM_ERROR), 0u);
  }
  fc4sc::global::delete_context(cntxt);
}

int sc_main(int ac, char* av[]) {
  crave::set_global_seed(9876);
  testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}
