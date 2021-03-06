#include "mpogen.h"
#include <assert.h>
#include <sstream>      // std::istringstream
#include <iostream>
#include <omp.h>
#include <boost/algorithm/string.hpp>

using std::cout;
using std::endl;
using std::getline;

void physical(Qshapes<Quantum>& qp, Dshapes& dp) {
  qp = {Quantum(1), Quantum(-1), Quantum(0)};
  dp = {1, 1, 2};
  // up, down, {empty, double}
}

void zero(MPO<Quantum>& mpo) {
  int nsite = mpo.size();
  Qshapes<Quantum> qzero(1, Quantum(0));
  Dshapes dzero(qzero.size(), 1);
  Qshapes<Quantum> qp;
  Dshapes dp;
  physical(qp, dp);
  for (int i = 0; i < nsite; ++i) {
    mpo[i].resize(Quantum::zero(), make_array(qzero, qp, -qp, -qzero), make_array(dzero, dp, dp, dzero));
    mpo[i] = 0;
  }
}

MPO<Quantum> create_op(const ColumnVector& c, Spin s) {
  vector<Qshapes<Quantum>> qr;
  vector<Dshapes> dr;
  int nsite = c.Nrows();
  int l0 = 1, r0 = nsite;
  /*
  for (int i = 1; i < nsite+1; ++i) {
    if (i == l0 && fabs(c(l0)) < 1e-10) {
      ++l0;
    }
    if (r0 == nsite+1-i && fabs(c(r0)) < 1e-10) {
      --r0;
    }
  }
  */
  Qshapes<Quantum> qzero, qone;
  qzero.push_back(Quantum(0));
  qone.push_back(Quantum(int(s))); // 1 for up, -1 for down
  
  for (int i = 1; i < nsite+1; ++i) {
    if (i < l0) {
      qr.push_back(qzero);
    } else if (i >= r0) {
      qr.push_back(qone);
    } else {
      qr.push_back(qzero + qone); // concatenate
    }
    Dshapes tempd(qr[i-1].size(), 1);
    dr.push_back(tempd);
  }
  Qshapes <Quantum> ql = qzero;
  Dshapes dl(ql.size(), 1);
  
  MPO<Quantum> ad(nsite);

  Qshapes<Quantum> qp;
  Dshapes dp;
  physical(qp, dp);

  auto qshape = make_array(ql, qp, -qp, -qr[0]);
  auto dshape = make_array(dl, dp, dp, dr[0]);
  ad[0].resize(Quantum::zero(), qshape, dshape);
  ad[0] = 0.;

  for (int i = 1; i < nsite; ++i) {
    qshape = make_array(qr[i-1], qp, -qp, -qr[i]);
    dshape = make_array(dr[i-1], dp, dp, dr[i]);
    ad[i].resize(Quantum::zero(), qshape, dshape);
    ad[i] = 0.;    
  }

  // now intialize ad
  DArray<4> block;
  for (int i = 0; i < nsite; ++i) {
    for (auto it = ad[i].begin(); it != ad[i].end(); ++it) {
      auto q = ad[i].qindex(ad[i].index(it->first));
      block.reference(*(it->second));
      if (q == qarray(0, 1, 1, 0) || q == qarray(0, -1, -1, 0)) {
        // the site before actually doing anything
        // and this site is singly occupied
        block(0, 0, 0, 0) = -1.;
      } else if (q == qarray(0, 0, 0, 0)) {
        // this site is doubly occupied or not occupied
        block(0, 0, 0, 0) = 1.;
        block(0, 1, 1, 0) = 1.;
      } else if (q[0] == qone[0] && q[3] == -qone[0]) {
        // the site after creating the particle
        block(0, 0, 0, 0) = 1.;
        if (q[1] == qzero[0]) {
          block(0, 1, 1, 0) = 1.;
        }
      } else if (q == qarray(0, int(s), 0, int(s))) { // ci*ai_sigma|0) (not occupied yet)
        block(0, 0, 0, 0) = c(i+1);
      } else if (q == qarray(0, 0, -int(s), int(s))) { // ci*ai_sigma|sigma') (occupied by the other spin)
        block(0, 1, 0, 0) = c(i+1) * int(s); // negative sign for spin down
      }
    }
  }
  return std::move(ad);
}

MPO<Quantum> anni_op(const ColumnVector& c, Spin s) {
  vector<Qshapes<Quantum>> qr;
  vector<Dshapes> dr;
  int nsite = c.Nrows();  
  int l0 = 1, r0 = nsite;
  /*
  for (int i = 1; i < nsite+1; ++i) {
    if (i == l0 && fabs(c(l0)) < 1e-10) {
      ++l0;
    }
    if (r0 == nsite+1-i && fabs(c(r0)) < 1e-10) {
      --r0;
    }
  }
  */
  Qshapes<Quantum> qzero, qone;
  qzero.push_back(Quantum(0));
  qone.push_back(Quantum(-int(s))); // 1 for up, -1 for down
  
  for (int i = 1; i < nsite+1; ++i) {
    if (i < l0) {
      qr.push_back(qzero);
    } else if (i >= r0) {
      qr.push_back(qone);
    } else {
      qr.push_back(qzero + qone);
    }
    Dshapes tempd(qr[i-1].size(), 1);
    dr.push_back(tempd);
  }
  Qshapes <Quantum> ql = qzero;
  Dshapes dl(ql.size(), 1);
  
  MPO<Quantum> ad(nsite);

  Qshapes<Quantum> qp;
  Dshapes dp;
  physical(qp, dp);

  auto qshape = make_array(ql, qp, -qp, -qr[0]);
  auto dshape = make_array(dl, dp, dp, dr[0]);
  ad[0].resize(Quantum::zero(), qshape, dshape);
  ad[0] = 0.;
  
  for (int i = 1; i < nsite; ++i) {
    qshape = make_array(qr[i-1], qp, -qp, -qr[i]);
    dshape = make_array(dr[i-1], dp, dp, dr[i]);
    ad[i].resize(Quantum::zero(), qshape, dshape);
    ad[i] = 0.;
  }

  // now intialize ad
  DArray<4> block;
  for (int i = 0; i < nsite; ++i) {
    for (auto it = ad[i].begin(); it != ad[i].end(); ++it) {
      auto q = ad[i].qindex(ad[i].index(it->first));
      block.reference(*(it->second));
      if (q == qarray(0, 1, 1, 0) || q == qarray(0, -1, -1, 0)) {
        // the site before actually doing anything
        // and this site is singly occupied
        block(0, 0, 0, 0) = -1.;
      } else if (q == qarray(0, 0, 0, 0)) {
        // this site is doubly occupied or not occupied
        block(0, 0, 0, 0) = 1.;
        block(0, 1, 1, 0) = 1.;
      } else if (q[0] == qone[0] && q[3] == -qone[0]) {
        // the site after annihilating the particle
        block(0, 0, 0, 0) = 1.;
        if (q[1] == qzero[0]) {
          block(0, 1, 1, 0) = 1.;
        }
      } else if (q == qarray(0, -int(s), 0, -int(s))) { // ci*ai_sigma|ud) (fully occupied)
        block(0, 0, 1, 0) = c(i+1) * int(s);
      } else if (q == qarray(0, 0, int(s), -int(s))) { // ci*ai_sigma|sigma) (only singly occupied by the right spin)
        block(0, 0, 0, 0) = c(i+1); // negative sign for spin down
      }
    }
  }
  return std::move(ad);
}

Matrix rdm(const MPS<Quantum>& A, Spin s) {
  int nsite = A.size();
  vector<MPO<Quantum>> a(nsite);
  SymmetricMatrix rdm1(nsite);
  ColumnVector coef(nsite);
  for (int i = 0; i < nsite; ++i) {
    coef = 0.;
    coef(i+1) = 1.;
    a[i] = anni_op(coef, s);
  }
  vector <MPS<Quantum>> temp(nsite);
  // now build 1rdm
  #pragma omp parallel default(none) shared(A, rdm1, a, temp, nsite)
  {
  # pragma omp for schedule(guided, 1)
  for (int i = 0; i < nsite; ++i) {
    temp[i] = a[i] * A;
  }
  # pragma omp barrier
  # pragma omp for schedule(dynamic, 1)
  for (int i = 0; i < nsite; ++i) {
    for (int j = i; j < nsite; ++j) {
      rdm1(i+1, j+1) = dot(MPS_DIRECTION::Left, temp[j], temp[i]);
    }
  }
  }
  return std::move(rdm1);
}

Matrix kappa(const MPS<Quantum>& A, bool symm) {
  int nsite = A.size();
  vector <MPO<Quantum>> a_u(nsite), a_d(nsite);
  ColumnVector coef(nsite);  
  for (int i = 0; i < nsite; ++i) {
    coef = 0.;
    coef(i+1) = 1.;
    a_u[i] = create_op(coef, Spin::Up);
    a_d[i] = anni_op(coef, Spin::Down);
  }
  vector <MPS<Quantum>> temp_u(nsite), temp_d(nsite);    
  # pragma omp parallel for default(none) shared(A, a_u, a_d, temp_u, temp_d, nsite) schedule(guided, 1)
  for (int i = 0; i < nsite; ++i) {
    temp_u[i] = a_u[i] * A;
    temp_d[i] = a_d[i] * A;
  }

  if (symm) {
    SymmetricMatrix kappa1(nsite);
    # pragma omp parallel for default(none) shared(kappa1, temp_u, temp_d, nsite) schedule(dynamic, 1)
    for (int i = 0; i < nsite; ++i) {
      for (int j = i; j < nsite; ++j) {
        kappa1(i+1, j+1) = dot(MPS_DIRECTION::Left, temp_d[j], temp_u[i]);
      }
    }
    return std::move(kappa1);    
  } else {
    Matrix kappa1(nsite, nsite);
    # pragma omp parallel for default(none) shared(kappa1, temp_u, temp_d, nsite) schedule(guided, 1)    
    for (int i = 0; i < nsite; ++i) {
      for (int j = 0; j < nsite; ++j) {
        kappa1(i+1, j+1) = dot(MPS_DIRECTION::Left, temp_d[j], temp_u[i]);
      }
    }
    return std::move(kappa1);    
  }
}

MPOGen_Hubbard_BCS::MPOGen_Hubbard_BCS(const char* m_input): MPOGen(m_input) {
  // open input file
  std::ifstream in(input.c_str());
  in >> nsite >> ntei >> U;
  H0.ReSize(nsite, nsite);
  D0.ReSize(nsite, nsite);
  f.resize(ntei);
  g.resize(ntei);
  d.resize(ntei);
  // read H0
  read_matrix(H0, "h0", in);
  assert((H0.t()-H0).NormFrobenius() < 1e-12);
  // read D0
  read_matrix(D0, "d0", in);
  assert((D0.t()-D0).NormFrobenius() < 1e-12);
  // read f,d,g
  for (int i = 0; i < ntei; ++i) {
    // read f
    f[i].ReSize(nsite, nsite);
    read_matrix(f[i], "f(site "+std::to_string(i)+")", in);
    // read g
    g[i].ReSize(nsite, nsite);
    read_matrix(g[i], "g(site "+std::to_string(i)+")", in);
    // read d
    d[i].ReSize(nsite, nsite);
    read_matrix(d[i], "d(site "+std::to_string(i)+")", in);
  }
  in.close();
}

void MPOGen_Hubbard_BCS::read_matrix(Matrix& A, string name, std::ifstream& in) {
  string line = "";
  while (line[0] != '!') {
    getline(in, line);
  }
  assert(line.substr(0, name.length()+1).compare("!"+name) == 0);
  for (int i = 0; i < nsite; ++i) {
    for (int j = 0; j < nsite; ++j) {
      in >> A(i+1, j+1);
    }
  }
}

const MPO<Quantum> MPOGen_Hubbard_BCS::generate(int M) {
  MPO<Quantum> H(nsite);
  zero(H);
  // first add the H0 part
  for (int i = 0; i < nsite; ++i) {
    ColumnVector temp(nsite);
    temp = 0.;
    temp(i+1) = 1.;
    axpy(1., create_op(temp, Spin::Up) * anni_op(H0.Row(i+1).t(), Spin::Up), H);
    axpy(1., create_op(temp, Spin::Down) * anni_op(H0.Row(i+1).t(), Spin::Down), H);
    compress(H, MPS_DIRECTION::Left, 0);
    compress(H, MPS_DIRECTION::Right, 0);
  }
  // then Delta0 part
  if (D0.NormFrobenius() > 1e-10) {
    for (int i = 0; i < nsite; ++i) {
      ColumnVector temp(nsite);
      temp = 0.;
      temp(i+1) = 1.;
      axpy(1., create_op(temp, Spin::Up) * create_op(D0.Row(i+1).t(), Spin::Down), H);
      axpy(1., anni_op(temp, Spin::Down) * anni_op(D0.Column(i+1), Spin::Up), H);
      compress(H, MPS_DIRECTION::Left, 0);
      compress(H, MPS_DIRECTION::Right, 0);
    }
  }
  // last, the interaction part
  for (int t = 0; t < ntei; ++t) {
    MPO<Quantum> op1(nsite), op2(nsite);
    zero(op1);
    zero(op2);
    for (int i = 0; i < nsite; ++i) {
      ColumnVector temp(nsite);
      temp = 0.;
      temp(i+1) = 1.;
      // op1
      axpy(1., create_op(temp, Spin::Up) * anni_op(f[t].Row(i+1).t(), Spin::Up), op1);
      axpy(1., create_op(temp, Spin::Down) * anni_op(g[t].Row(i+1).t(), Spin::Down), op1);
      axpy(1., create_op(temp, Spin::Up) * create_op(d[t].Row(i+1).t(), Spin::Down), op1);
      axpy(1., anni_op(temp, Spin::Down) * anni_op(d[t].Column(i+1), Spin::Up), op1);
      // op2
      axpy(1., create_op(temp, Spin::Up) * anni_op(g[t].Row(i+1).t(), Spin::Up), op2);
      axpy(1., create_op(temp, Spin::Down) * anni_op(f[t].Row(i+1).t(), Spin::Down), op2);
      axpy(1., create_op(temp, Spin::Up) * create_op(d[t].Column(i+1), Spin::Down), op2);
      axpy(1., anni_op(temp, Spin::Down) * anni_op(d[t].Row(i+1).t(), Spin::Up), op2);
      compress(op1, MPS_DIRECTION::Left, 0);
      compress(op1, MPS_DIRECTION::Right, 0);
      compress(op2, MPS_DIRECTION::Left, 0);
      compress(op2, MPS_DIRECTION::Right, 0);      
    }
    axpy(U, op1 * op2, H);
    compress(H, MPS_DIRECTION::Left, 0);
    compress(H, MPS_DIRECTION::Right, 0);
  }

  return std::move(H);
}

MPOGen_Hubbard_UBCS::MPOGen_Hubbard_UBCS(const char* m_input): MPOGen(m_input) {
  // open input file
  std::ifstream in(input.c_str());
  in >> nsite >> ntei >> U;
  H0a.ReSize(nsite, nsite);
  H0b.ReSize(nsite, nsite);  
  D0.ReSize(nsite, nsite);
  fa.resize(ntei);
  ga.resize(ntei);
  da.resize(ntei);
  fb.resize(ntei);
  gb.resize(ntei);
  db.resize(ntei);
  // read H0
  read_matrix(H0a, "h0a", in);
  assert((H0a.t()-H0a).NormFrobenius() < 1e-12);
  read_matrix(H0b, "h0b", in);
  assert((H0b.t()-H0b).NormFrobenius() < 1e-12);
  // read D0
  read_matrix(D0, "d0", in);
  // read f,d,g
  for (int i = 0; i < ntei; ++i) {
    // read fa
    fa[i].ReSize(nsite, nsite);
    read_matrix(fa[i], "f(site "+std::to_string(i)+")a", in);
    // read ga
    ga[i].ReSize(nsite, nsite);
    read_matrix(ga[i], "g(site "+std::to_string(i)+")a", in);
    // read da
    da[i].ReSize(nsite, nsite);
    read_matrix(da[i], "d(site "+std::to_string(i)+")a", in);
    // read fb
    fb[i].ReSize(nsite, nsite);
    read_matrix(fb[i], "f(site "+std::to_string(i)+")b", in);
    // read gb
    gb[i].ReSize(nsite, nsite);
    read_matrix(gb[i], "g(site "+std::to_string(i)+")b", in);
    // read db
    db[i].ReSize(nsite, nsite);
    read_matrix(db[i], "d(site "+std::to_string(i)+")b", in);
  }
  in.close();
}

void MPOGen_Hubbard_UBCS::read_matrix(Matrix& A, string name, std::ifstream& in) {
  string line = "";
  while (line[0] != '!') {
    getline(in, line);
  }
  assert(line.substr(0, name.length()+1).compare("!"+name) == 0);
  for (int i = 0; i < nsite; ++i) {
    for (int j = 0; j < nsite; ++j) {
      in >> A(i+1, j+1);
    }
  }
}

const MPO<Quantum> MPOGen_Hubbard_UBCS::generate(int M) {
  MPO<Quantum> H(nsite);
  zero(H);
  // first add the H0 part
  for (int i = 0; i < nsite; ++i) {
    ColumnVector temp(nsite);
    temp = 0.;
    temp(i+1) = 1.;
    axpy(1., create_op(temp, Spin::Up) * anni_op(H0a.Row(i+1).t(), Spin::Up), H);
    axpy(1., create_op(temp, Spin::Down) * anni_op(H0b.Row(i+1).t(), Spin::Down), H);
    compress(H, MPS_DIRECTION::Left, 0);
    compress(H, MPS_DIRECTION::Right, 0);
  }
  // then Delta0 part
  if (D0.NormFrobenius() > 1e-10) {
    for (int i = 0; i < nsite; ++i) {
      ColumnVector temp(nsite);
      temp = 0.;
      temp(i+1) = 1.;
      axpy(1., create_op(temp, Spin::Up) * create_op(D0.Row(i+1).t(), Spin::Down), H);
      axpy(1., anni_op(temp, Spin::Down) * anni_op(D0.Column(i+1), Spin::Up), H);
      compress(H, MPS_DIRECTION::Left, 0);
      compress(H, MPS_DIRECTION::Right, 0);
    }
  }
  // last, the interaction part
  if (fabs(U) > 1e-8) {
    for (int t = 0; t < ntei; ++t) {
      MPO<Quantum> op1(nsite), op2(nsite);
      zero(op1);
      zero(op2);
      for (int i = 0; i < nsite; ++i) {
        ColumnVector temp(nsite);
        temp = 0.;
        temp(i+1) = 1.;
        // op1
        axpy(1., create_op(temp, Spin::Up) * anni_op(fa[t].Row(i+1).t(), Spin::Up), op1);
        axpy(1., create_op(temp, Spin::Down) * anni_op(ga[t].Row(i+1).t(), Spin::Down), op1);
        axpy(1., create_op(temp, Spin::Up) * create_op(da[t].Row(i+1).t(), Spin::Down), op1);
        axpy(1., anni_op(temp, Spin::Down) * anni_op(da[t].Column(i+1), Spin::Up), op1);
        // op2
        axpy(1., create_op(temp, Spin::Down) * anni_op(fb[t].Row(i+1).t(), Spin::Down), op2);
        axpy(1., create_op(temp, Spin::Up) * anni_op(gb[t].Row(i+1).t(), Spin::Up), op2);
        axpy(1., create_op(temp, Spin::Down) * create_op(db[t].Row(i+1).t(), Spin::Up), op2);
        axpy(1., anni_op(temp, Spin::Up) * anni_op(db[t].Column(i+1), Spin::Down), op2);
        compress(op1, MPS_DIRECTION::Left, 0);
        compress(op1, MPS_DIRECTION::Right, 0);
        compress(op2, MPS_DIRECTION::Left, 0);
        compress(op2, MPS_DIRECTION::Right, 0);      
      }
      axpy(U, op1 * op2, H);
      compress(H, MPS_DIRECTION::Left, 0);
      compress(H, MPS_DIRECTION::Right, 0);
    }
  }
  return std::move(H);
}
