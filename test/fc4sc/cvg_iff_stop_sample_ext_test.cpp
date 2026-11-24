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
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "fc4sc.hpp"
#include "gtest/gtest.h"

namespace {

class cvg_iff_sample_cg : public covergroup {
public:
  CG_CONS(cvg_iff_sample_cg) {};

  int a = 0;
  int b = 0;
  bool en = true;

  CG_IFF(en && (a >= 0));
  SAMPLE_CG(a, b);

  COVERPOINT(int, cvp_a, a) {
    bin<int>("zero", 0),
    bin<int>("one", 1),
    illegal_bin<int>("bad", -1)
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("ten", 10),
    bin<int>("twenty", 20)
  };

  cross<int, int> axb = cross<int, int>(this, "axb", &cvp_a, &cvp_b);
};

class cvg_stop_indep_cg : public covergroup {
public:
  CG_CONS(cvg_stop_indep_cg) {};

  int a = 0;
  int b = 0;
  SAMPLE_CG(a, b);

  COVERPOINT(int, cvp_a, a) {
    bin<int>("lo", interval(0, 3)),
    bin<int>("hi", interval(2, 5))
  };

  COVERPOINT(int, cvp_b, b) {
    bin<int>("x", 0),
    bin<int>("y", 1)
  };

  cross<int, int> axb = cross<int, int>(this, "axb", &cvp_a, &cvp_b);
};

using typed_int_int_cg = fc4sc::typed_covergroup<int, int>;

class typed_cg_impl : public typed_int_int_cg {
public:
  CG_EXTEND(typed_cg_impl, typed_int_int_cg) {};

  int a = 0;
  int b = 0;

  void sample(int va, int vb) override {
    a = va;
    b = vb;
    covergroup::sample();
  }

  COVERPOINT(int, cvp_a, a) { bin<int>("v1", 1) };
  COVERPOINT(int, cvp_b, b) { bin<int>("v2", 2) };
};

class nested_helper_cg : public covergroup {
public:
  CG_CONS(nested_helper_cg) {};
  int v = 0;
  SAMPLE_CG(v);
  COVERPOINT(int, cvp_v, v) { bin<int>("one", 1) };
};

class base_ext_cg : public covergroup {
public:
  CG_CONS(base_ext_cg, int thresh = 10), threshold(thresh) {
    option().at_least = 1;
    type_option().distribute_first = true;
  }

  int threshold = 10;
  int x = 0;
  bool base_gate = true;

  CG_IFF(base_gate);
  SAMPLE_CG(x);

  COVERPOINT(int, cvp_x, x) {
    bin<int>("low", interval(0, 3)),
    bin<int>("high", interval(4, 7))
  };

  nested_helper_cg nested{"", __FILE__, __LINE__,
                          this->get_cvg_data()->parent_scp->cntxt};
};

class derived_ext_cg : public base_ext_cg {
public:
  CG_EXTEND_BASE(derived_ext_cg, base_ext_cg, (thresh), int thresh = 20, int mode = 1),
    extra_mode(mode) {}

  int extra_mode = 1;
  int y = 0;
  bool derived_gate = true;

  CG_IFF(derived_gate);
  SAMPLE_CG(x, y);

  COVERPOINT(int, cvp_y, y) {
    bin<int>("y0", 0),
    bin<int>("y1", 1)
  };

  cross<int, int> x_cross_y = cross<int, int>(this, "x_cross_y", &cvp_x, &cvp_y);
};

class grandchild_ext_cg : public derived_ext_cg {
public:
  CG_EXTEND(grandchild_ext_cg, derived_ext_cg) {}
  int z = 0;
  SAMPLE_CG(x, y, z);
  COVERPOINT(int, cvp_z, z) { bin<int>("z1", 1) };
};

class decl_def_base_cg : public covergroup {
public:
  CG_CONS_DECL(decl_def_base_cg, int v = 1);
  int a = 0;
  COVERPOINT(int, cvp_a, a) { bin<int>("one", 1) };
};

CG_CONS_DEF(decl_def_base_cg, int v) { a = v; }

class decl_def_derived_cg : public decl_def_base_cg {
public:
  CG_EXTEND_DECL(decl_def_derived_cg, decl_def_base_cg, int v = 1, int w = 2);
  int b = 0;
  COVERPOINT(int, cvp_b, b) { bin<int>("two", 2) };
};

CG_EXTEND_BASE_DEF(decl_def_derived_cg, decl_def_base_cg, (v), int v, int w) { b = w; }

class ext_scope : public fc4sc::scope {
public:
  SCOPE_DECL(ext_scope)

  class base_sc_cg : public covergroup {
  public:
    CG_SCOPED_CONS(base_sc_cg, ext_scope) {}
    int a = 0;
    COVERPOINT(int, cvp_a, a) { bin<int>("a0", 0) };
  };

  class drv_sc_cg : public base_sc_cg {
  public:
    CG_SCOPED_EXTEND(drv_sc_cg, base_sc_cg, ext_scope) {}
    int b = 0;
    COVERPOINT(int, cvp_b, b) { bin<int>("b1", 1) };
  };

  ext_scope(std::string name_, const char *auto_inst_file_name = "", int auto_line = 1,
            fc4sc::global* cntxt = fc4sc::global::getter())
    : fc4sc::scope(name_, __FILE__, __LINE__, auto_inst_file_name, auto_line, cntxt),
      CG_SCOPED_INST(drv) {}

  drv_sc_cg drv;
};

class inner_base_cg : public covergroup {
public:
  CG_CONS(inner_base_cg) {}
  int v = 0;
  SAMPLE_CG(v);
  COVERPOINT(int, cvp_v, v) { bin<int>("v0", 0) };
};

class inner_drv_cg : public inner_base_cg {
public:
  CG_EXTEND(inner_drv_cg, inner_base_cg) {}
  int w = 0;
  SAMPLE_CG(v, w);
  COVERPOINT(int, cvp_w, w) { bin<int>("w1", 1) };
};

class outer_base_with_nested_ext_cg : public covergroup {
public:
  CG_CONS(outer_base_with_nested_ext_cg) {}
  int a = 0;
  COVERPOINT(int, cvp_a, a) { bin<int>("a0", 0) };
  inner_drv_cg nested{"", __FILE__, __LINE__,
                      this->get_cvg_data()->parent_scp->cntxt};
  inner_drv_cg nested2{"", __FILE__, __LINE__,
                       this->get_cvg_data()->parent_scp->cntxt};
};

class outer_mid_with_plain_cg : public outer_base_with_nested_ext_cg {
public:
  CG_EXTEND(outer_mid_with_plain_cg, outer_base_with_nested_ext_cg) {}
  nested_helper_cg plain_in_mid{"", __FILE__, __LINE__,
                                this->get_cvg_data()->parent_scp->cntxt};
};

class outer_drv_with_nested_ext_cg : public outer_mid_with_plain_cg {
public:
  CG_EXTEND(outer_drv_with_nested_ext_cg, outer_mid_with_plain_cg) {}
  int b = 0;
  COVERPOINT(int, cvp_b, b) { bin<int>("b1", 1) };
};

} // namespace

TEST(covergroup_iff, guard_and_sample_cg_interaction) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cvg_iff_sample_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_TRUE(cg.has_iff());
    EXPECT_TRUE(cg.eval_iff());

    // When iff evaluates false (even via newly passed SAMPLE_CG args a=-1),
    // sample() must silently skip: no stderr warning, no illegal throw, no hits/misses.
    ::testing::internal::CaptureStderr();
    EXPECT_NO_THROW(cg.sample(-1, 10));
    std::string err = ::testing::internal::GetCapturedStderr();
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ(cg.cvp_a.get_misses(), 0u);
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 0u);

    // en = false via pointer/callable
    cg.en = false;
    cg.sample(0, 10);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 0u);

    cg.en = true;
    cg.sample(0, 10);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 1u);

    // set_iff(bool*) and clear_iff
    bool flag = false;
    cg.set_iff(&flag);
    cg.sample(1, 20);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(1), 0u);

    cg.add_iff([]() { return true; });
    cg.sample(1, 20);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(1), 0u);

    cg.clear_iff();
    EXPECT_FALSE(cg.has_iff());
    cg.sample(1, 20);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(1), 1u);
    EXPECT_EQ(cg.cvp_a.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.cvp_b.get_inst_coverage(), 100.0);
    EXPECT_EQ(cg.axb.get_inst_coverage(), 50.0);
    cg.sample(0, 20);
    cg.sample(1, 10);
    EXPECT_EQ(cg.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(covergroup_stop_start, independent_collect_state) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    cvg_stop_indep_cg cg("cg", __FILE__, __LINE__, cntxt);
    EXPECT_TRUE(cg.is_collecting());
    EXPECT_TRUE(cg.cvp_a.is_collecting());
    EXPECT_TRUE(cg.cvp_b.is_collecting());

    // Stop coverpoint a, then stop & start covergroup: cvp_a must stay stopped!
    cg.cvp_a.stop();
    EXPECT_FALSE(cg.cvp_a.is_collecting());
    cg.stop();
    EXPECT_FALSE(cg.is_collecting());
    EXPECT_FALSE(cg.cvp_b.is_collecting()); // parent stopped -> effective is_collecting false

    // Sample while cg stopped with detect_overlap enabled -> still warns once, 0 hits
    cg.option().detect_overlap = true;
    ::testing::internal::CaptureStderr();
    cg.sample(2, 0);
    std::string err = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("overlap"), std::string::npos) << err;
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 0u);
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 0u);

    // Even direct cvp_b.sample() while cg is stopped must not sample
    cg.cvp_b.start();
    cg.b = 0;
    cg.cvp_b.sample();
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 0u);

    // Start cg: cvp_b resumes, cvp_a remains independently stopped!
    cg.start();
    EXPECT_TRUE(cg.is_collecting());
    EXPECT_FALSE(cg.cvp_a.is_collecting());
    EXPECT_TRUE(cg.cvp_b.is_collecting());

    cg.sample(1, 0);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 0u);
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 1u);
    EXPECT_TRUE(cg.axb.get_cross_bins().empty());

    // Start cvp_a: both active
    cg.cvp_a.start();
    cg.sample(1, 0);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 1u);
    EXPECT_FALSE(cg.axb.get_cross_bins().empty());
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(covergroup_sample_args, bind_sample_args_dynamic_and_typed) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    // typed_covergroup polymorphic handle
    typed_cg_impl tcg("tcg", __FILE__, __LINE__, cntxt);
    fc4sc::typed_covergroup<int, int>* base_ptr = &tcg;
    base_ptr->sample(1, 2);
    EXPECT_EQ(tcg.get_inst_coverage(), 100.0);

    // covergroup::bind_sample_args + sample_with
    cvg_iff_sample_cg cg("cg", __FILE__, __LINE__, cntxt);
    cg.bind_sample_args(cg.a, cg.b);
    cg.sample_with(0, 10);
    EXPECT_EQ(cg.cvp_a.get_bin_hit_count(0), 1u);
    EXPECT_EQ(cg.cvp_b.get_bin_hit_count(0), 1u);

    // dynamic_covergroup parameterized sample(Args...)
    fc4sc::dynamic_covergroup_factory fac("dyn_param_cg", __FILE__, __LINE__);
    int da = 0, db = 0;
    auto ca = fac.create_coverpoint<int()>("da", [&]() { return da; });
    ca.create_bin("one", 1);
    auto cb = fac.create_coverpoint<int()>("db", [&]() { return db; });
    cb.create_bin("two", 2);
    fac.set_iff([&]() { return da >= 0; });

    fc4sc::dynamic_covergroup dinst(fac, "dinst", __FILE__, __LINE__, cntxt);
    ca.bind_sample(dinst);
    cb.bind_sample(dinst);
    dinst.bind_sample_args(da, db);
    dinst.sample(-1, 2); // blocked by factory iff
    EXPECT_EQ(dinst.get_inst_coverage(), 0.0);
    dinst.sample(1, 2);
    EXPECT_EQ(dinst.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt);
}

TEST(covergroup_inheritance, static_multilevel_scoped_and_dynamic_extend) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    base_ext_cg b("b", __FILE__, __LINE__, cntxt, 15);
    derived_ext_cg d("d", __FILE__, __LINE__, cntxt, 25, 3);
    grandchild_ext_cg g("g", __FILE__, __LINE__, cntxt);

    EXPECT_EQ(b.type_name(), "base_ext_cg");
    EXPECT_EQ(d.type_name(), "derived_ext_cg");
    EXPECT_EQ(g.type_name(), "grandchild_ext_cg");
    EXPECT_EQ(b.nested.type_name(), "nested_helper_cg");
    EXPECT_EQ(d.nested.type_name(), "nested_helper_cg");
    EXPECT_EQ(d.threshold, 25);
    EXPECT_EQ(d.extra_mode, 3);
    EXPECT_TRUE(d.cvp_x.type_option().distribute_first);

    // Base and derived type coverages are completely isolated
    b.sample(1);
    EXPECT_EQ(b.get_coverage(), 50.0);
    EXPECT_EQ(d.get_coverage(), 0.0);
    EXPECT_EQ(g.get_coverage(), 0.0);

    // Derived inherits base CG_IFF and base SAMPLE_CG(x) alongside SAMPLE_CG(x, y)
    d.base_gate = false;
    d.sample(1, 0);
    EXPECT_EQ(d.get_inst_coverage(), 0.0);

    d.base_gate = true;
    d.derived_gate = false;
    d.sample(1, 0);
    EXPECT_EQ(d.get_inst_coverage(), 0.0);

    d.derived_gate = true;
    d.sample(1, 0);
    d.sample(5, 1);
    EXPECT_EQ(d.cvp_x.get_inst_coverage(), 100.0);
    EXPECT_EQ(d.cvp_y.get_inst_coverage(), 100.0);
    EXPECT_EQ(d.x_cross_y.get_inst_coverage(), 50.0);
    // Base still 50%
    EXPECT_EQ(b.get_coverage(), 50.0);

    g.sample(1, 0, 1);
    EXPECT_EQ(g.cvp_z.get_inst_coverage(), 100.0);

    // Decl/def separation macros
    decl_def_derived_cg dd("dd", __FILE__, __LINE__, cntxt, 1, 2);
    EXPECT_EQ(dd.type_name(), "decl_def_derived_cg");
    dd.sample();
    EXPECT_EQ(dd.get_inst_coverage(), 100.0);

    // Scoped inheritance
    ext_scope scp("scp", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(scp.drv.type_name(), "drv_sc_cg");
    scp.drv.b = 1;
    scp.drv.sample();
    EXPECT_EQ(scp.drv.get_inst_coverage(), 100.0);

    // Dynamic factory extend()
    fc4sc::dynamic_covergroup_factory base_fac("dyn_base", __FILE__, __LINE__);
    int bx = 0, by = 0;
    auto fbx = base_fac.create_coverpoint<int()>("bx", [&]() { return bx; });
    fbx.create_bin("b0", 0);
    fbx.create_bin("b1", 1);
    base_fac.set_iff([&]() { return bx >= 0; });

    fc4sc::dynamic_covergroup_factory drv_fac("dyn_drv", __FILE__, __LINE__);
    drv_fac.extend(base_fac);
    auto fby = drv_fac.create_coverpoint<int()>("by", [&]() { return by; });
    fby.create_bin("y0", 0);
    auto* base_bx_ptr = drv_fac.get_coverpoint_factory_ptr<int>("bx");
    ASSERT_NE(base_bx_ptr, nullptr);
    drv_fac.add_cross_by_name<int, int>("bx_x_by", {"bx", "by"},
                                        default_cross_bin("rest"));

    fc4sc::dynamic_covergroup d_inst(drv_fac, "d_inst", __FILE__, __LINE__, cntxt);
    fbx.bind_sample(d_inst);
    fby.bind_sample(d_inst);
    bx = -1; by = 0;
    d_inst.sample();
    EXPECT_EQ(d_inst.get_inst_coverage(), 0.0);
    bx = 0; by = 0;
    d_inst.sample();
    EXPECT_GT(d_inst.get_inst_coverage(), 0.0);

    xml_printer::coverage_save("cvg_inheritance_test.xml", cntxt);
  }
  fc4sc::global::delete_context(cntxt);

  // XML round-trip for inherited covergroups
  auto cntxt2 = fc4sc::global::create_new_context();
  {
    base_ext_cg b2("b", __FILE__, __LINE__, cntxt2, 15);
    derived_ext_cg d2("d", __FILE__, __LINE__, cntxt2, 25, 3);
    uint64_t loaded = fc4sc::load_coverage_db("cvg_inheritance_test.xml", cntxt2);
    EXPECT_GT(loaded, 0u);
    EXPECT_EQ(b2.get_inst_coverage(), 50.0);
    EXPECT_EQ(d2.cvp_x.get_inst_coverage(), 100.0);
    EXPECT_EQ(d2.cvp_y.get_inst_coverage(), 100.0);
  }
  fc4sc::global::delete_context(cntxt2);
}

TEST(covergroup_inheritance, nested_cg_extend_and_dynamic_factory_type_option_and_iff) {
  auto cntxt = fc4sc::global::create_new_context();
  {
    // Bug 4: nested CG_EXTEND members inside derived covergroup's base/mid classes
    outer_drv_with_nested_ext_cg od("od", __FILE__, __LINE__, cntxt);
    EXPECT_EQ(od.type_name(), "outer_drv_with_nested_ext_cg");
    EXPECT_EQ(od.nested.type_name(), "inner_drv_cg");
    EXPECT_EQ(od.nested2.type_name(), "inner_drv_cg");
    EXPECT_EQ(od.plain_in_mid.type_name(), "nested_helper_cg");

    // Bug 5: dynamic_coverpoint_factory type_option (distribute_first, weight, goal, comment)
    // survives dynamic_covergroup_factory::extend() and dynamic_covergroup instantiation
    int vx = 0, vy = 0;
    fc4sc::dynamic_covergroup_factory bf("dyn_opt_base", __FILE__, __LINE__);
    auto fdf = bf.create_coverpoint<int()>("df", [&]() { return vx; });
    bin_array<int>("ba", 2, interval(0, 5))
        .with([](int x) { return x <= 2 || x == 5; })
        .add_to_cvp(*fdf.get_cvp_fact());
    fdf.get_cvp_fact()->type_option().distribute_first = true;
    fdf.get_cvp_fact()->type_option().weight = 7;
    fdf.get_cvp_fact()->type_option().goal = 85;
    fdf.get_cvp_fact()->type_option().comment = "dyn_df_comment";

    auto fplain = bf.create_coverpoint<int()>("plain", [&]() { return vy; });
    bin_array<int>("bp", 2, interval(0, 5))
        .with([](int y) { return y <= 2 || y == 5; })
        .add_to_cvp(*fplain.get_cvp_fact());

    bool base_en = true;
    bool drv_en = true;
    bf.set_iff(&base_en, "base_en");

    // Minor 6a: derived set_iff BEFORE extend() composes with base iff_cond
    fc4sc::dynamic_covergroup_factory df("dyn_opt_drv", __FILE__, __LINE__);
    df.set_iff(&drv_en, "drv_en");
    df.extend(bf);

    // Minor 6a: duplicate-name throw on extend() preserves df.cvps (strong exception safety)
    fc4sc::dynamic_covergroup_factory dup_fac("dyn_dup", __FILE__, __LINE__);
    dup_fac.create_coverpoint<int()>("df", [&]() { return vx; });
    const size_t cvp_count_before = df.cvps.size();
    EXPECT_THROW(df.extend(dup_fac), std::invalid_argument);
    EXPECT_EQ(df.cvps.size(), cvp_count_before);

    fc4sc::dynamic_covergroup dg(df, "dg", __FILE__, __LINE__, cntxt);
    fdf.bind_sample(dg);
    fplain.bind_sample(dg);

    EXPECT_TRUE(dg.get_coverpoint("df").type_option().distribute_first);
    EXPECT_EQ(dg.get_coverpoint("df").type_option().weight, 7u);
    EXPECT_EQ(dg.get_coverpoint("df").type_option().goal, 85u);
    EXPECT_EQ(dg.get_coverpoint("df").type_option().comment, "dyn_df_comment");
    // Non-overridden coverpoint still inherits dg.type_option().distribute_first
    dg.type_option().distribute_first = true;
    EXPECT_TRUE(dg.get_coverpoint("plain").type_option().distribute_first);

    // Check composed iff: base_en=false blocks sample even when drv_en=true
    base_en = false; drv_en = true;
    vx = 2; vy = 2;
    dg.sample();
    auto df_bins = static_cast<fc4sc::coverpoint_base&>(dg.get_coverpoint("df")).get_bins_base();
    EXPECT_EQ(df_bins[0]->get_hitcount(), 0u);
    EXPECT_EQ(df_bins[1]->get_hitcount(), 0u);

    base_en = true; drv_en = false;
    dg.sample();
    EXPECT_EQ(df_bins[0]->get_hitcount(), 0u);

    // With distribute_first=true, bin_array(2, [0:5]) splits [0..2], [3..5], then filters:
    // ba[0] holds {0,1,2} (hits on vx=2), ba[1] holds {5}.
    drv_en = true;
    dg.sample();
    EXPECT_EQ(df_bins[0]->get_hitcount(), 1u);
    EXPECT_EQ(df_bins[1]->get_hitcount(), 0u);
  }
  fc4sc::global::delete_context(cntxt);
}
