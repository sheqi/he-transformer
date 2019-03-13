//*****************************************************************************
// Copyright 2018-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#include "he_backend.hpp"
#include "ngraph/ngraph.hpp"
#include "test_util.hpp"
#include "util/all_close.hpp"
#include "util/ndarray.hpp"
#include "util/test_control.hpp"
#include "util/test_tools.hpp"

using namespace std;
using namespace ngraph;

static string s_manifest = "${MANIFEST}";

template <typename T>
class BatchNormInferenceTester {
 public:
  BatchNormInferenceTester(
      const std::unique_ptr<ngraph::runtime::he::HEBackend>& backend,
      const Shape& input_shape, element::Type etype, double epsilon)
      : m_he_backend(backend) {
    Shape channel_shape{input_shape.at(1)};

    auto Input = make_shared<op::Parameter>(etype, input_shape);
    auto Gamma = make_shared<op::Parameter>(etype, channel_shape);
    auto Beta = make_shared<op::Parameter>(etype, channel_shape);
    auto Mean = make_shared<op::Parameter>(etype, channel_shape);
    auto Variance = make_shared<op::Parameter>(etype, channel_shape);
    auto BN = make_shared<op::BatchNormInference>(Input, Gamma, Beta, Mean,
                                                  Variance, epsilon);
    m_function = make_shared<Function>(
        BN, ParameterVector{Input, Gamma, Beta, Mean, Variance});

    m_input = backend->create_cipher_tensor(etype, input_shape);
    m_gamma = backend->create_plain_tensor(etype, channel_shape);
    m_beta = backend->create_plain_tensor(etype, channel_shape);
    m_mean = backend->create_plain_tensor(etype, channel_shape);
    m_variance = backend->create_plain_tensor(etype, channel_shape);
    m_normed_input = backend->create_tensor(etype, input_shape);
  }

  bool call(const std::vector<T>& input, const std::vector<T>& gamma,
            const std::vector<T>& beta, const std::vector<T>& mean,
            const std::vector<T>& variance,
            const std::vector<T>& normed_input) {
    copy_data(m_input, input);
    copy_data(m_gamma, gamma);
    copy_data(m_beta, beta);
    copy_data(m_mean, mean);
    copy_data(m_variance, variance);
    auto handle = m_he_backend->compile(m_function);
    m_he_backend->call(handle, {m_normed_input},
                       {m_input, m_gamma, m_beta, m_mean, m_variance});
    auto res_normed_input = read_vector<T>(m_normed_input);

    for (const auto& res : res_normed_input) {
      NGRAPH_INFO << "res " << res;
    }

    return all_close(normed_input, res_normed_input);
  }

 protected:
  const std::unique_ptr<ngraph::runtime::he::HEBackend>& m_he_backend;
  std::shared_ptr<Function> m_function;
  std::shared_ptr<ngraph::runtime::Tensor> m_input;
  std::shared_ptr<ngraph::runtime::Tensor> m_gamma;
  std::shared_ptr<ngraph::runtime::Tensor> m_beta;
  std::shared_ptr<ngraph::runtime::Tensor> m_mean;
  std::shared_ptr<ngraph::runtime::Tensor> m_variance;
  std::shared_ptr<ngraph::runtime::Tensor> m_normed_input;
};

template <typename T>
class BatchNormInferenceTesterZeroEpsilon : public BatchNormInferenceTester<T> {
 public:
  // These are for documentation purposes only below
  using Input = test::NDArray<T, 2>;
  using Gamma = test::NDArray<T, 1>;
  using Beta = test::NDArray<T, 1>;
  using Mean = test::NDArray<T, 1>;
  using Variance = test::NDArray<T, 1>;
  using NormedInput = test::NDArray<T, 2>;

  BatchNormInferenceTesterZeroEpsilon(
      const std::unique_ptr<ngraph::runtime::he::HEBackend>& backend,
      element::Type etype)
      : BatchNormInferenceTester<T>(backend, Shape{2, 3}, etype, 0.0) {}

  bool test(const Input& input, const Gamma& gamma, const Beta& beta,
            const Mean& mean, const Variance& variance,
            const NormedInput& normed_input) {
    return BatchNormInferenceTester<T>::call(
        input.get_vector(), gamma.get_vector(), beta.get_vector(),
        mean.get_vector(), variance.get_vector(), normed_input.get_vector());
  }

  bool test_gamma() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{2.0, 3.0, 4.0}, Beta{0.0, 0.0, 0.0}, Mean{0.0, 0.0, 0.0},
                Variance{1.0, 1.0, 1.0},
                NormedInput{{2.0, 6.0, 12.0}, {-2.0, -6.0, -12.0}});
  }

  bool test_beta() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{2.0, -2.0, 3.0}, Mean{0.0, 0.0, 0.0},
                Variance{1.0, 1.0, 1.0},
                NormedInput{{3.0, 0.0, 6.0}, {1.0, -4.0, 0.0}});
  }

  bool test_mean() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{0.0, 0.0, 0.0},
                Mean{-2.0, 2.0, -3.0}, Variance{1.0, 1.0, 1.0},
                NormedInput{{3.0, 0.0, 6.0}, {1.0, -4.0, 0.0}});
  }

  bool test_variance() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{0.0, 0.0, 0.0}, Mean{0.0, 0.0, 0.0},
                Variance{0.25, .0625, 4.0},
                NormedInput{{2.0, 8.0, 1.5}, {-2.0, -8.0, -1.5}});
  }
};

template <typename T>
class BatchNormInferenceTesterNonZeroEpsilon
    : public BatchNormInferenceTester<T> {
 public:
  // These are for documentation purposes only below
  using Input = test::NDArray<T, 2>;
  using Gamma = test::NDArray<T, 1>;
  using Beta = test::NDArray<T, 1>;
  using Mean = test::NDArray<T, 1>;
  using Variance = test::NDArray<T, 1>;
  using NormedInput = test::NDArray<T, 2>;

  BatchNormInferenceTesterNonZeroEpsilon(
      const std::unique_ptr<ngraph::runtime::he::HEBackend>& backend,
      element::Type etype)
      : BatchNormInferenceTester<T>(backend, Shape{2, 3}, etype, 0.25) {}

  bool test(const Input& input, const Gamma& gamma, const Beta& beta,
            const Mean& mean, const Variance& variance,
            const NormedInput& normed_input) {
    return BatchNormInferenceTester<T>::call(
        input.get_vector(), gamma.get_vector(), beta.get_vector(),
        mean.get_vector(), variance.get_vector(), normed_input.get_vector());
  }

  bool test_gamma() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{2.0, 3.0, 4.0}, Beta{0.0, 0.0, 0.0}, Mean{0.0, 0.0, 0.0},
                Variance{0.75, 0.75, 0.75},
                NormedInput{{2.0, 6.0, 12.0}, {-2.0, -6.0, -12.0}});
  }

  bool test_beta() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{2.0, -2.0, 3.0}, Mean{0.0, 0.0, 0.0},
                Variance{0.75, 0.75, 0.75},
                NormedInput{{3.0, 0.0, 6.0}, {1.0, -4.0, 0.0}});
  }

  bool test_mean() {
    return test(Input{{1.0, 2.0, 3.0}, {-1.0, -2.0, -3.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{0.0, 0.0, 0.0},
                Mean{-2.0, 2.0, -3.0}, Variance{0.75, 0.75, 0.75},
                NormedInput{{3.0, 0.0, 6.0}, {1.0, -4.0, 0.0}});
  }

  bool test_variance() {
    return test(Input{{3.0, 5.0, 1.0}, {-3.0, -5.0, -1.0}},
                Gamma{1.0, 1.0, 1.0}, Beta{0.0, 0.0, 0.0}, Mean{0.0, 0.0, 0.0},
                Variance{2.0, 6.0, 0.0},
                NormedInput{{2.0, 2.0, 2.0}, {-2.0, -2.0, -2.0}});
  }
};

NGRAPH_TEST(${BACKEND_NAME}, batch_norm_inference_0eps_f32) {
  auto backend = runtime::Backend::create("${BACKEND_NAME}");
  std::unique_ptr<ngraph::runtime::he::HEBackend> he_backend;
  auto he_backend_tmp = static_cast<runtime::he::HEBackend*>(backend.get());
  backend.release();
  he_backend.reset(he_backend_tmp);

  BatchNormInferenceTesterZeroEpsilon<float> bnt(he_backend, element::f32);
  EXPECT_TRUE(bnt.test_gamma()) << "Gamma test";
  EXPECT_TRUE(bnt.test_beta()) << "Beta test";
  EXPECT_TRUE(bnt.test_mean()) << "Mean test";
  EXPECT_TRUE(bnt.test_variance()) << "Variance test";
}

NGRAPH_TEST(${BACKEND_NAME}, batch_norm_inference_f32) {
  auto backend = runtime::Backend::create("${BACKEND_NAME}");
  std::unique_ptr<ngraph::runtime::he::HEBackend> he_backend;
  auto he_backend_tmp = static_cast<runtime::he::HEBackend*>(backend.get());
  backend.release();
  he_backend.reset(he_backend_tmp);

  BatchNormInferenceTesterNonZeroEpsilon<float> bnt(he_backend, element::f32);
  EXPECT_TRUE(bnt.test_gamma()) << "Gamma test";
  EXPECT_TRUE(bnt.test_beta()) << "Beta test";
  EXPECT_TRUE(bnt.test_mean()) << "Mean test";
  EXPECT_TRUE(bnt.test_variance()) << "Variance test";
}
