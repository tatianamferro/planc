/* Copyright Ramakrishnan Kannan 2018 */

#ifndef NTF_NTFNES_HPP_
#define NTF_NTFNES_HPP_

#include "ntf/auntf.hpp"

namespace planc {
class NTFNES : public AUNTF {
 private:
  NCPFactors m_prox_t;  // Proximal Term (H_*)
  NCPFactors m_acc_t;   // Acceleration term (Y)
  NCPFactors m_grad_t;  // Gradient Term (\nabla_f(Y))
  MAT modified_gram;
  double delta1;
  double delta2;

 protected:
  inline double get_lambda(double L, double mu) {
    double q = L / mu;
    double lambda = 0.0;

    if (q > 1e6)
      lambda = 10 * mu;
    else if (q > 1e3)
      lambda = mu;
    else
      lambda = mu / 10;

    return lambda;
  }

  inline double get_alpha(double alpha, double q) {
    /* Solves the quadratic equation
      x^2 + (\alpha^2 -q)x - \alpha^2 = 0
    */
    double a, b, c, D, x;
    a = 1.0;
    b = alpha * alpha - q;
    c = -alpha * alpha;
    D = b * b - 4 * a * c;
    x = (-b + sqrt(D)) / 2;
    return x;
  }

  bool stop_iter(const int mode) {
    bool stop = false;
    double absmax, amin;

    absmax = (arma::abs(m_grad_t.factor(mode) % m_acc_t.factor(mode))).max();
    amin = (m_grad_t.factor(mode)).min();

    if (absmax <= delta1 && amin >= -delta2) stop = true;

    return stop;
  }

  MAT update(const int mode) {
    double L, mu, lambda, q, alpha, alpha_prev, beta;
    MAT Ht(m_prox_t.factor(mode));
    MAT Htprev = Ht;
    m_acc_t.set(mode, Ht);
    modified_gram = this->gram_without_one;

    VEC eigval = arma::eig_sym(modified_gram);
    L = eigval.max();
    mu = eigval.min();
    lambda = get_lambda(L, mu);
    modified_gram.diag() += lambda;

    q = (mu + lambda) / (L + lambda);

    MAT modified_local_mttkrp_t =
        (-1 * lambda) * m_acc_t.factor(mode) - this->ncp_mttkrp_t[mode];

    alpha = 1;
    alpha_prev = 1;
    beta = 1;

    while (true) {
      m_grad_t.set(mode, modified_local_mttkrp_t +
                             (modified_gram * m_acc_t.factor(mode)));
      if (stop_iter(mode)) break;

      Htprev = Ht;
      Ht = m_acc_t.factor(mode) - ((1 / (L + lambda)) * m_grad_t.factor(mode));
      fixNumericalError<MAT>(&Ht, EPSILON_1EMINUS16);
      Ht.for_each([](MAT::elem_type &val) { val = val > 0.0 ? val : 0.0; });
      alpha_prev = alpha;
      alpha = get_alpha(alpha_prev, q);
      beta =
          (alpha_prev * (1 - alpha_prev)) / (alpha + alpha_prev * alpha_prev);
      m_acc_t.set(mode, Ht + beta * (Ht - Htprev));
    }

    m_prox_t.set(mode, Ht);
    return Ht;
  }

 public:
  NTFNES(const Tensor &i_tensor, const int i_k, algotype i_algo)
      : AUNTF(i_tensor, i_k, i_algo),
        m_prox_t(i_tensor.dimensions(), i_k, true),
        m_acc_t(i_tensor.dimensions(), i_k, true),
        m_grad_t(i_tensor.dimensions(), i_k, true) {
    m_prox_t.zeros();
    m_acc_t.zeros();
    m_grad_t.zeros();
    modified_gram.zeros(i_k, i_k);
    delta1 = 1e-2;
    delta2 = 1e-2;
  }
};  // class NTFNES
}  // namespace planc

#endif  // NTF_NTFNES_HPP_
