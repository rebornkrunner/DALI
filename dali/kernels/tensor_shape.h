// Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
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

#ifndef DALI_KERNELS_TENSOR_SHAPE_H_
#define DALI_KERNELS_TENSOR_SHAPE_H_

#include <array>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <utility>
#include <vector>
#include "dali/core/span.h"
#include "dali/core/util.h"
#include "dali/core/small_vector.h"
#include "dali/core/dev_array.h"
#include "dali/core/cuda_utils.h"

namespace dali {


namespace kernels {

constexpr int DynamicDimensions = -1;
constexpr int InferDimensions = -2;

namespace detail {

template <int ndim1, int ndim2>
struct is_compatible_ndim : std::integral_constant<bool,
  ndim1 == ndim2 || ndim1 == DynamicDimensions || ndim2 == DynamicDimensions> {};

template <int ndim1, int ndim2>
struct check_compatible_ndim {
  static_assert(is_compatible_ndim<ndim1, ndim2>::value, "Incompatible number of dimensions\n."
    "Must be equal or at least one side must be DynamicDimensions");
};

}  // namespace detail


template <typename T>
struct compile_time_size_impl : std::integral_constant<int, DynamicDimensions> {};

template <typename T>
using compile_time_size = compile_time_size_impl<
  std::remove_cv_t<
    std::remove_reference_t<T>
  >>;

template <typename T, size_t N>
struct compile_time_size_impl<T[N]> : std::integral_constant<int, N> {};

template <typename T, size_t N>
struct compile_time_size_impl<std::array<T, N>> : std::integral_constant<int, N> {};

template <typename T, size_t N>
struct compile_time_size_impl<DeviceArray<T, N>> : std::integral_constant<int, N> {};

/**
 * @brief Class representing shape of a Tensor
 *
 * Static shapes do not allocate additional memory as they are backed by static array
 * @tparam ndim Either non-negative integer representing static number of dimensions
 *         or DynamicDimensions.
 */
template <int ndim = DynamicDimensions>
struct TensorShape;

template <int N>
struct compile_time_size_impl<TensorShape<N>> : std::integral_constant<int, N> {};

/**
 * @brief Base class for TensorShape containing common code for iterators and operator[]
 * @tparam Container - the data structure in which the sizes are stored
 * @tparam ndim - number of dimensions
 */
template <typename Container, int ndim>
struct TensorShapeBase {
  using container_type = Container;
  using value_type = typename container_type::value_type;
  using size_type = int;
  using reference = value_type &;
  using const_reference = const value_type &;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;

  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV reference operator[](int d) { return shape[d]; }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV const_reference operator[](int d) const { return shape[d]; }

  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV iterator begin() noexcept { return shape.begin(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV iterator end() noexcept { return shape.end(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV const_iterator begin() const noexcept { return shape.begin(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV const_iterator end() const noexcept { return shape.end(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV const_iterator cbegin() const noexcept { return shape.cbegin(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV const_iterator cend() const noexcept { return shape.cend(); }

  /**
   * @brief Returns number of dimensions in this shape
   */
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV size_type size() const noexcept { return shape.size(); }
  /**
   * @brief Returns number of dimensions in this shape
   */
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV size_type sample_dim() const noexcept { return shape.size(); }
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV constexpr bool empty() const noexcept { return size() == 0; }

  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV volume_t<value_type> num_elements() const {
    return volume(shape);
  }

  Container shape;
  static constexpr int static_ndim = ndim;

  /**
   * @brief Returns a static subshape consisting of first other_ndim dimensions (outer dimensions)
   * [1, 2, 3, 4].first<2>() -> [1, 2]
   */
  DALI_NO_EXEC_CHECK
  template <int other_ndim>
  DALI_HOST_DEV
  TensorShape<other_ndim> first() const;
  /**
   * @brief Returns a static subshape consisting of last other_ndim dimensions (inner dimensions)
   * [1, 2, 3, 4].last<2>() -> [3, 4]
   */
  DALI_NO_EXEC_CHECK
  template <int other_ndim>
  DALI_HOST_DEV
  TensorShape<other_ndim> last() const;

  /**
   * @brief Returns a dynamic subshape consisting of first count dimensions (outer dimensions)
   * [1, 2, 3, 4].first(2) -> [1, 2]
   */
  TensorShape<DynamicDimensions> first(int count) const;
  /**
   * @brief Returns a dynamic subshape consisting of last count dimensions (inner dimensions)
   * [1, 2, 3, 4].last(2) -> [3, 4]
   */
  TensorShape<DynamicDimensions> last(int count) const;

 protected:
  // Disallow instantiation of Base class

  // Zero-fill the shape for Container=DeviceArray<int64_t> with shape{}
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV TensorShapeBase() : shape{} {}
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV TensorShapeBase(const Container &c) : shape(c) {}        // NOLINT
  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV TensorShapeBase(Container &&c) : shape(cuda_move(c)) {}  // NOLINT
};

using DynamicTensorShapeContainer = SmallVector<int64_t, 6>;

/**
 * @brief Dynamic TensorShape can be constructed from any Static TensorShape
 */
template <>
struct TensorShape<DynamicDimensions>
    : public TensorShapeBase<DynamicTensorShapeContainer, DynamicDimensions> {
  using Base = TensorShapeBase<DynamicTensorShapeContainer, DynamicDimensions>;

  TensorShape(const std::vector<int64_t> &s)  // NOLINT
  : Base(DynamicTensorShapeContainer(s.data(), s.size())) {}
  TensorShape(const DynamicTensorShapeContainer &s) : Base(s) {}  // NOLINT

  template <size_t N>
  TensorShape(const std::array<int64_t, N> &s)  // NOLINT
      : Base(typename Base::container_type(s.begin(), s.end())) {}

  template <size_t N>
  TensorShape(const DeviceArray<int64_t, N> &s)  // NOLINT
      : Base(typename Base::container_type(s.begin(), s.end())) {}

  template <typename... Ts,
            typename = std::enable_if_t<
              all_of<std::is_convertible<Ts, int64_t>::value...>::value>>
  TensorShape(int64_t i0, Ts... s)  // NOLINT
      : Base(typename Base::container_type{i0, int64_t{s}...}) {}

  template <typename It,
            typename = std::enable_if_t<
              std::is_same<typename std::iterator_traits<It>::value_type, int64_t>::value>>
  TensorShape(It first, It last)
      : Base(typename Base::container_type{first, last}) {}

  TensorShape() = default;
  TensorShape(const TensorShape &) = default;
  TensorShape(TensorShape &&) = default;
  TensorShape &operator=(const TensorShape &other) = default;
  TensorShape &operator=(TensorShape &&other) = default;

  template <int other_ndim>
  TensorShape(const TensorShape<other_ndim> &other)
      : Base(typename Base::container_type(other.shape.begin(), other.shape.end())) {}

  template <int other_ndim>
  TensorShape &operator=(const TensorShape<other_ndim> &other) {
    shape = Base::container_type(other.shape.begin(), other.shape.end());
    return *this;
  }

  /**
   * @brief Convert to static shape
   * Behaviour is undefined for other_ndim != dim()
   */
  template <int other_ndim>
  TensorShape<other_ndim> to_static() const {
    static_assert(other_ndim != DynamicDimensions,
                  "Conversion to static only allowed for static shape");
    assert(size() == other_ndim);
    TensorShape<other_ndim> shape;
    for (int i = 0; i < other_ndim; i++) {
      shape[i] = (*this)[i];
    }
    return shape;
  }

  void resize(typename Base::size_type count) { shape.resize(count); }
};

template <int ndim>
struct TensorShape : public TensorShapeBase<DeviceArray<int64_t, ndim>, ndim> {
  using Base = TensorShapeBase<DeviceArray<int64_t, ndim>, ndim>;

  TensorShape(const std::array<int64_t, ndim> &s) : Base(s) {}  // NOLINT
  DALI_HOST_DEV TensorShape(const DeviceArray<int64_t, ndim> &s) : Base(s) {}  // NOLINT
  // Base class constructor will zero-initialize array
  DALI_HOST_DEV
  TensorShape() {}
  // We allow only explicit operations on TensorShape static dim
  TensorShape(const TensorShape &) = default;
  TensorShape &operator=(const TensorShape &other) = default;

  template <typename... Ts>
  DALI_HOST_DEV TensorShape(int64_t i0, Ts... s)  // NOLINT
      : Base(typename Base::container_type{i0, int64_t{s}...}) {
    static_assert(sizeof...(Ts) == ndim - 1, "Number of shapes passed must match ndim");
  }

  template <int other_ndim>
  DALI_HOST_DEV TensorShape<other_ndim> to_static() const {
    static_assert(other_ndim == ndim, "Cannot convert to other static ndim");
    return *this;
  }

  DALI_NO_EXEC_CHECK
  DALI_HOST_DEV void resize(typename Base::size_type count) {
    assert(count == ndim && "Not supported for count other than statically defined");
  }

  static_assert(ndim >= 0, "TensorShape dimension should not be negative");
};

template <typename Container, int ndim>
template <int other_ndim>
TensorShape<other_ndim> TensorShapeBase<Container, ndim>::first() const {
  static_assert(other_ndim <= ndim || ndim == DynamicDimensions,
                "Number of dimensions in subshape must be between 0 and size()");
  static_assert(other_ndim != DynamicDimensions, "This function can produce only static shapes");
  assert(0 <= other_ndim && other_ndim <= size() &&
         "Number of dimensions in subshape must be between 0 and size()");
  TensorShape<other_ndim> result;
  for (int i = 0; i < other_ndim; i++) {
    result[i] = (*this)[i];
  }
  return result;
}

template <typename Container, int ndim>
template <int other_ndim>
TensorShape<other_ndim> TensorShapeBase<Container, ndim>::last() const {
  static_assert(other_ndim <= ndim || ndim == DynamicDimensions,
                "Number of dimensions in subshape must be between 0 and size()");
  static_assert(other_ndim != DynamicDimensions, "This function can produce only static shapes");
  assert(0 <= other_ndim && other_ndim <= size() &&
         "Number of dimensions in subshape must be between 0 and size()");
  TensorShape<other_ndim> result;
  int start_offset = size() - other_ndim;
  for (int i = 0; i < other_ndim; i++) {
    result[i] = (*this)[start_offset + i];
  }
  return result;
}

template <typename Container, int ndim>
TensorShape<DynamicDimensions> TensorShapeBase<Container, ndim>::first(int count) const {
  assert(0 <= count && count <= size() &&
         "Number of dimensions in subshape must be between 0 and size()");
  TensorShape<DynamicDimensions> result;
  result.resize(count);
  for (int i = 0; i < count; i++) {
    result[i] = (*this)[i];
  }
  return result;
}

template <typename Container, int ndim>
TensorShape<DynamicDimensions> TensorShapeBase<Container, ndim>::last(int count) const {
  assert(0 <= count && count <= size() &&
         "Number of dimensions in subshape must be between 0 and size()");
  TensorShape<DynamicDimensions> result;
  result.resize(count);
  int start_offset = size() - count;
  for (int i = 0; i < count; i++) {
    result[i] = (*this)[start_offset + i];
  }
  return result;
}

/**
 * @brief Checks if both shapes have the same number of dimensions and all of them are equal
 */
DALI_NO_EXEC_CHECK
template <int left_ndim, int right_ndim>
DALI_HOST_DEV bool operator==(const TensorShape<left_ndim> &left,
                              const TensorShape<right_ndim> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  int size = left.size();
  for (int i = 0; i < size; i++) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

DALI_NO_EXEC_CHECK
template <int left_ndim, int right_ndim>
DALI_HOST_DEV bool operator!=(const TensorShape<left_ndim> &left,
                              const TensorShape<right_ndim> &right) {
  return !(left == right);
}

DALI_HOST_DEV
constexpr int shape_cat_ndim(int left_ndim, int right_ndim) {
  return (left_ndim == DynamicDimensions || right_ndim == DynamicDimensions)
             ? DynamicDimensions
             : left_ndim + right_ndim;
}

/**
 * @brief Concatenate shapes
 * @return TensorShape<shape_cat_ndim(left_ndim, right_ndim)> Static shape if both of arguments
 *         are static, otherwise dynamic
 */
DALI_NO_EXEC_CHECK
template <int left_ndim, int right_ndim>
DALI_HOST_DEV
TensorShape<shape_cat_ndim(left_ndim, right_ndim)> shape_cat(const TensorShape<left_ndim> &left,
                                                             const TensorShape<right_ndim> &right) {
  TensorShape<shape_cat_ndim(left_ndim, right_ndim)> result;
  int total_size = left.size() + right.size();
  result.resize(total_size);
  for (int i = 0; i < left.size(); i++) {
    result[i] = left[i];
  }
  for (int i = 0; i < right.size(); i++) {
    result[left.size() + i] = right[i];
  }
  return result;
}

/**
 * @brief Appends a scalar to a shape
 */
DALI_NO_EXEC_CHECK
template <int ndim, int out_dim = ndim == DynamicDimensions ? ndim : ndim + 1>
DALI_HOST_DEV
TensorShape<out_dim> shape_cat(const TensorShape<ndim> &left, int64_t right) {
  TensorShape<out_dim> result;
  result.resize(left.size() + 1);
  for (int i = 0; i < left.size(); i++) {
    result[i] = left[i];
  }
  result[left.size()] = right;
  return result;
}

/**
 * @brief Prepends a scalar to a shape
 */
DALI_NO_EXEC_CHECK
template <int ndim, int out_dim = ndim == DynamicDimensions ? ndim : ndim + 1>
DALI_HOST_DEV
TensorShape<out_dim> shape_cat(int64_t left, const TensorShape<ndim> &right) {
  TensorShape<out_dim> result;
  result.resize(right.size() + 1);
  result[0] = left;
  for (int i = 0; i < right.size(); i++) {
    result[i+1] = right[i];
  }
  return result;
}

/**
 * @brief Flatten list of shapes into contigous vector
 */
template <int sample_ndim>
std::enable_if_t<sample_ndim != DynamicDimensions, std::vector<int64_t>>
flatten_shapes(const std::vector<TensorShape<sample_ndim>> &shapes) {
  std::vector<int64_t> result;
  result.resize(sample_ndim * shapes.size());
  for (size_t sample = 0; sample < shapes.size(); sample++) {
    for (int axis = 0; axis < sample_ndim; axis++) {
      result[sample * sample_ndim + axis] = shapes[sample][axis];
    }
  }
  return result;
}

/**
 * @brief Get the dim from list of shapes that have uniform dimensions.
 * @return 0 if list is empty, otherwise dim of first element
 */
template <typename T>
std::enable_if_t<std::is_same<T, TensorShape<DynamicDimensions>>::value ||
                 std::is_same<T, std::vector<int64_t>>::value,
                 int>
get_dim_from_uniform(const std::vector<T> &shapes) {
  if (shapes.empty()) {
    return 0;
  }
  return shapes[0].size();
}

template <typename T>
std::enable_if_t<std::is_same<T, TensorShape<DynamicDimensions>>::value ||
                 std::is_same<T, std::vector<int64_t>>::value,
                 std::vector<int64_t>>
flatten_shapes(const std::vector<T> &shapes) {
  std::vector<int64_t> result;
  int uniform_sample_ndim = get_dim_from_uniform(shapes);
  result.resize(uniform_sample_ndim * shapes.size());
  for (size_t sample = 0; sample < shapes.size(); sample++) {
    assert(static_cast<size_t>(shapes[sample].size()) == static_cast<size_t>(uniform_sample_ndim));
    for (int axis = 0; axis < uniform_sample_ndim; axis++) {
      result[sample * uniform_sample_ndim + axis] = shapes[sample][axis];
    }
  }
  return result;
}

static int get_dim_from_uniform(std::initializer_list<std::vector<int64_t>> shapes) {
  return get_dim_from_uniform(std::vector<std::vector<int64_t>>(shapes));
}

static std::vector<int64_t> flatten_shapes(std::initializer_list<std::vector<int64_t>> shapes) {
  return flatten_shapes(std::vector<std::vector<int64_t>>(shapes));
}

/**
 * @brief List of TensorShapes stored as contigous vector.
 *        All shapes have the same number of dimensions
 *
 * @tparam sample_ndim Either non-negative integer representing static number of dimensions
 *         or DynamicDimensions.
 */
template <int sample_ndim = DynamicDimensions>
struct TensorListShape;

/**
 * @tparam Derived - actual class of an object (CRTP)
 * @tparam sample_dim - number of dimensions of each sample in the list
 */
template <typename Derived, int sample_ndim>
struct TensorListShapeBase {
  /**
   * @brief Returns a static subshape list consisting of first other_ndim dimensions
   *        (outer dimensions) for each sample
   */
  template <int other_ndim>
  TensorListShape<other_ndim> first() const;
  /**
   * @brief Returns a static subshape list consisting of last other_ndim dimensions
   *        (inner dimensions) for each sample
   */
  template <int other_ndim>
  TensorListShape<other_ndim> last() const;

  /**
   * @brief Returns a dynamic subshape list consisting of first count dimensions
   *        (outer dimensions) for each sample
   */
  TensorListShape<DynamicDimensions> first(int count) const;
  /**
   * @brief Returns a dynamic subshape list consisting of last count dimensions
   *        (inner dimensions) for each sample
   */
  TensorListShape<DynamicDimensions> last(int count) const;

  /**
   * @brief Return a span containing the shape of `sample`
   */
  span<int64_t, sample_ndim == DynamicDimensions ?
                dynamic_extent : span_extent_t(sample_ndim)>
  tensor_shape_span(int64_t sample) {
    return {&shapes[sample * sample_dim()], span_extent_t(sample_dim())};
  }

  span<const int64_t, sample_ndim == DynamicDimensions ?
                      dynamic_extent : span_extent_t(sample_ndim)>
  tensor_shape_span(int64_t sample) const {
    return {&shapes[sample * sample_dim()], span_extent_t(sample_dim())};
  }

  /**
   * @brief Return the TensorShape for given `sample`
   *
   * @tparam tensor_ndim Should be equal sample_dim() or DynamicDimensions to obtain either static
   *         or dynamic TensorShape
   */
  template <int tensor_ndim = sample_ndim>
  TensorShape<tensor_ndim> tensor_shape(int64_t sample) const {
    static_assert(tensor_ndim == sample_ndim || sample_ndim == DynamicDimensions
                  || tensor_ndim == DynamicDimensions, "Cannot convert to other static ndim");
    if (tensor_ndim != DynamicDimensions) {
      assert(tensor_ndim == sample_dim() && "Cannot convert to other ndim");
    }
    TensorShape<tensor_ndim> out;
    out.resize(sample_dim());
    int64_t base = sample_dim() * sample;
    for (int i = 0; i < sample_dim(); i++) {
      out[i] = shapes[base + i];
    }
    return out;
  }

  // Resolves ambiguity when used with brace-enclosed initializer list
  void set_tensor_shape(int64_t sample, const TensorShape<sample_ndim> &sample_shape) {
    set_tensor_shape<TensorShape<sample_ndim>>(sample, sample_shape);
  }

  /**
   * @brief Set a TensorShape for `sample`
   */
  template <typename SampleShape>
  void set_tensor_shape(int64_t sample, const SampleShape &sample_shape) {
    detail::check_compatible_ndim<sample_ndim, compile_time_size<SampleShape>::value>();
    assert(static_cast<int>(dali::size(sample_shape)) == static_cast<int>(sample_dim()));
    assert(sample >= 0 && sample < nsamples && "Sample index out of range");
    int64_t base = sample_dim() * sample;
    for (int i = 0; i < sample_dim(); i++) {
      shapes[base + i] = sample_shape[i];
    }
  }

  std::vector<int64_t> shapes;

  decltype(shapes.data()) data() { return shapes.data(); }
  constexpr bool empty() const { return size() == 0; }
  constexpr int size() const { return nsamples; }
  constexpr int num_samples() const { return size(); }

  ptrdiff_t num_elements() const {
    ptrdiff_t n = 0;
    for (int i = 0; i < num_samples(); i++) {
      n += volume(tensor_shape_span(i));
    }
    return n;
  }

  template <typename SampleShape>
  static Derived make_uniform(int num_samples, const SampleShape &ss) {
    if (num_samples < 0)
      return {};

    Derived ret;
    int dim = dali::size(ss);
    ret.resize(num_samples, dim);

    if (num_samples == 0) {
      return ret;
    }

    // copy the sample shape to the first entry
    auto it = std::begin(ss);
    for (int j = 0; j < dim; j++)
      ret.shapes[j] = *it++;

    // repeat first sample shape over the entire array
    int n = ret.shapes.size();
    for (int k = dim; k < n; k++) {
      ret.shapes[k] = ret.shapes[k - dim];  // this will periodically repeat items 0..dim-1
    }

    return ret;
  }

  void resize(int num_samples) {
    nsamples = num_samples;
    shapes.resize(num_samples * sample_dim());
  }

  void resize(int num_samples, int sample_dim) {
    nsamples = num_samples;
    set_sample_dim(sample_dim);
    shapes.resize(num_samples * sample_dim);
  }

 protected:
  int sample_dim() const { return static_cast<const Derived *>(this)->sample_dim(); }
  void set_sample_dim(int dim) { static_cast<Derived *>(this)->set_sample_dim(dim); }
  TensorListShapeBase() = default;
  TensorListShapeBase(const std::vector<int64_t> &shapes, int num_samples)
      : shapes(shapes), nsamples(num_samples) {}
  TensorListShapeBase(std::vector<int64_t> &&shapes, int num_samples)
      : shapes(std::move(shapes)), nsamples(num_samples) {}

  int nsamples = 0;
};

template <>
struct TensorListShape<DynamicDimensions>
    : TensorListShapeBase<TensorListShape<DynamicDimensions>, DynamicDimensions> {
  using Base = TensorListShapeBase<TensorListShape<DynamicDimensions>, DynamicDimensions>;

  TensorListShape() : Base() {}

  TensorListShape(const TensorListShape &) = default;
  TensorListShape(TensorListShape &&) = default;

  template <int other_sample_ndim>
  TensorListShape(const TensorListShape<other_sample_ndim> &other)
      : Base(other.shapes, other.size()), ndim(other.sample_dim()) {}

  template <int other_sample_ndim>
  TensorListShape(TensorListShape<other_sample_ndim> &&other)
      : Base(std::move(other.shapes), other.size()), ndim(other.sample_dim()) {
    other.nsamples = 0;
  }

  explicit TensorListShape(int num_samples) : Base() {
    resize(num_samples);
  }

  explicit TensorListShape(int num_samples, int sample_dim) : Base()  {
    resize(num_samples, sample_dim);
  }

  TensorListShape(const std::vector<std::vector<int64_t>> &sample_shapes)  // NOLINT
      : Base(flatten_shapes(sample_shapes), sample_shapes.size()),
        ndim(get_dim_from_uniform(sample_shapes)) {}

  TensorListShape(const std::vector<TensorShape<DynamicDimensions>> &sample_shapes)  // NOLINT
      : Base(flatten_shapes(sample_shapes), sample_shapes.size()),
        ndim(get_dim_from_uniform(sample_shapes)) {}

  // Constructor disambiguating brace initialization
  TensorListShape(std::initializer_list<std::vector<int64_t>> sample_shapes)  // NOLINT
      : Base(flatten_shapes(sample_shapes), sample_shapes.size()),
        ndim(get_dim_from_uniform(sample_shapes)) {}

  TensorListShape(const std::vector<int64_t> &shapes, int sample_dim)
      : Base(shapes, shapes.size() / sample_dim), ndim(sample_dim) {}

  TensorListShape(std::vector<int64_t> &&shapes, int sample_dim)
      : Base(std::move(shapes), shapes.size() / sample_dim), ndim(sample_dim) {}

  TensorListShape(const std::vector<int64_t> &shapes, int num_samples, int sample_dim)
      : Base(shapes, num_samples), ndim(sample_dim) {
    assert(num_samples == static_cast<int>(shapes.size()) / sample_dim);
  }

  TensorListShape(std::vector<int64_t> &&shapes, int num_samples, int sample_dim)
      : Base(std::move(shapes), num_samples), ndim(sample_dim) {}

  TensorListShape &operator=(const TensorListShape &) = default;
  TensorListShape &operator=(TensorListShape &&other) {
    shapes = std::move(other.shapes);
    nsamples = other.size();
    ndim = other.sample_dim();
    other.nsamples = 0;
    return *this;
  }

  template <int other_sample_ndim>
  TensorListShape &operator=(const TensorListShape<other_sample_ndim> &other) {
    shapes = other.shapes;
    nsamples = other.size();
    ndim = other.sample_dim();
    return *this;
  }

  template <int other_sample_ndim>
  TensorListShape &operator=(TensorListShape<other_sample_ndim> &&other) {
    shapes = std::move(other.shapes);
    nsamples = other.size();
    ndim = other.sample_dim();
    other.nsamples = 0;
    return *this;
  }

  /**
   * @brief Return a dynamic TensorShape for `sample`
   */
  TensorShape<DynamicDimensions> operator[](int64_t sample) const {
    return tensor_shape<DynamicDimensions>(sample);
  }

  int sample_dim() const { return ndim; }

  int ndim = 0;
  using Base::shapes;

  /**
   * @brief Convert to static TensorListShape
   *
   * Behaviour is undefined for other_ndim != sample_dim()
   * @tparam other_ndim must be equal sample_dim()
   */
  template <int other_ndim>
  TensorListShape<other_ndim> to_static() const & {
    static_assert(other_ndim != DynamicDimensions,
                  "Conversion to static only allowed for static shape");
    assert(sample_dim() == other_ndim && "Cannot convert to other ndim");
    return { shapes, size(), other_ndim };
  }

  template <int other_ndim>
  TensorListShape<other_ndim> to_static() && {
    static_assert(other_ndim != DynamicDimensions,
                  "Conversion to static only allowed for static shape");
    assert(sample_dim() == other_ndim && "Cannot convert to other ndim");
    return { std::move(shapes), size(), other_ndim };
  }

 private:
  void set_sample_dim(int dim) { this->ndim = dim; }

  friend struct TensorListShapeBase<TensorListShape<DynamicDimensions>, DynamicDimensions>;
};

template <int sample_ndim>
struct TensorListShape : TensorListShapeBase<TensorListShape<sample_ndim>, sample_ndim> {
  using Base = TensorListShapeBase<TensorListShape<sample_ndim>, sample_ndim>;

  TensorListShape() = default;
  TensorListShape(const TensorListShape &) = default;
  TensorListShape(TensorListShape &&other) : Base(std::move(other.shapes), other.nsamples) {
    other.nsamples = 0;
  }

  explicit TensorListShape(int num_samples) : Base() {
    Base::resize(num_samples);
  }

  explicit TensorListShape(int num_samples, int sample_dim) : Base()  {
    assert(sample_dim == sample_ndim);
    Base::resize(num_samples, sample_dim);
  }

  TensorListShape(const std::vector<TensorShape<sample_ndim>> &sample_shapes)  // NOLINT
      : Base(flatten_shapes(sample_shapes), sample_shapes.size()) {}

  TensorListShape(const std::vector<int64_t> &shapes, int sample_dim)
      : Base(shapes, shapes.size() / sample_dim) {
    assert(sample_dim == sample_ndim);
  }

  TensorListShape(std::vector<int64_t> &&shapes, int sample_dim)
      : Base(std::move(shapes), shapes.size() / sample_dim) {
    assert(sample_dim == sample_ndim);
  }

  TensorListShape(const std::vector<int64_t> &shapes, int num_samples, int sample_dim)
      : Base(shapes, num_samples) {
    assert(sample_dim == sample_ndim);
    assert(num_samples == static_cast<int>(shapes.size()) / sample_dim);
  }

  TensorListShape(std::vector<int64_t> &&shapes, int num_samples, int sample_dim)
      : Base(std::move(shapes), num_samples) {
    assert(sample_dim == sample_ndim);
  }

  TensorListShape &operator=(const TensorListShape &) = default;
  TensorListShape &operator=(TensorListShape &&other) {
    shapes = std::move(other.shapes);
    nsamples = other.size();
    other.nsamples = 0;
    return *this;
  }

  /**
   * @brief Return a static TensorShape for `sample`
   */
  TensorShape<sample_ndim> operator[](int64_t sample) const {
    TensorShape<sample_ndim> result;
    int64_t base = sample_dim() * sample;
    for (int i = 0; i < sample_dim(); i++) {
      result[i] = shapes[base + i];
    }
    return result;
  }

  constexpr int sample_dim() const noexcept { return sample_ndim; }

  using Base::shapes;
  using Base::nsamples;

  template <int other_ndim>
  TensorListShape<other_ndim> to_static() const & {
    static_assert(other_ndim == sample_ndim, "Cannot convert to other static ndim");
    return { shapes, this->size(), other_ndim };
  }

  template <int other_ndim>
  TensorListShape<other_ndim> to_static() && {
    static_assert(other_ndim == sample_ndim, "Cannot convert to other static ndim");
    return { std::move(shapes), nsamples, other_ndim };
  }

 private:
  void set_sample_dim(int dim) {
    assert(dim == sample_ndim && "Cannot change number of dimensions");
  }

  friend struct TensorListShapeBase<TensorListShape<sample_ndim>, sample_ndim>;
};


template <typename Derived, int sample_ndim>
template <int other_ndim>
TensorListShape<other_ndim> TensorListShapeBase<Derived, sample_ndim>::first() const {
  static_assert(other_ndim <= sample_ndim || sample_ndim == DynamicDimensions,
                "Number of dimensions in subshape must be between 0 and sample_dim()");
  static_assert(other_ndim != DynamicDimensions, "This function can produce only static shapes");
  assert(0 <= other_ndim && other_ndim <= sample_dim() &&
         "Number of dimensions in subshape must be between 0 and sample_dim()");
  TensorListShape<other_ndim> result;
  result.resize(size());
  for (int sample = 0; sample < size(); sample++) {
    for (int d = 0; d < other_ndim; d++) {
      result.shapes[sample * other_ndim + d] = shapes[sample * sample_dim() + d];
    }
  }
  return result;
}

template <typename Derived, int sample_ndim>
template <int other_ndim>
TensorListShape<other_ndim> TensorListShapeBase<Derived, sample_ndim>::last() const {
  static_assert(other_ndim <= sample_ndim || sample_ndim == DynamicDimensions,
                "Number of dimensions in subshape must be between 0 and sample_dim()");
  static_assert(other_ndim != DynamicDimensions, "This function can produce only static shapes");
  assert(0 <= other_ndim && other_ndim <= sample_dim() &&
         "Number of dimensions in subshape must be between 0 and sample_dim()");
  TensorListShape<other_ndim> result;
  result.resize(size());
  int start_offset = sample_dim() - other_ndim;
  for (int sample = 0; sample < size(); sample++) {
    for (int d = 0; d < other_ndim; d++) {
      result.shapes[sample * other_ndim + d] = shapes[sample * sample_dim() + start_offset + d];
    }
  }
  return result;
}

template <typename Derived, int sample_ndim>
TensorListShape<DynamicDimensions>
TensorListShapeBase<Derived, sample_ndim>::first(int count) const {
  assert(0 <= count && count <= sample_dim() &&
         "Number of dimensions in subshape must be between 0 and sample_dim()");
  TensorListShape<DynamicDimensions> result;
  result.resize(size(), count);
  for (int sample = 0; sample < size(); sample++) {
    for (int d = 0; d < count; d++) {
      result.shapes[sample * count + d] = shapes[sample * sample_dim() + d];
    }
  }
  return result;
}

template <typename Derived, int sample_ndim>
TensorListShape<DynamicDimensions>
TensorListShapeBase<Derived, sample_ndim>::last(int count) const {
  assert(0 <= count && count <= sample_dim() &&
         "Number of dimensions in subshape must be between 0 and sample_dim()");
  TensorListShape<DynamicDimensions> result;
  result.resize(size(), count);
  int start_offset = sample_dim() - count;
  for (int sample = 0; sample < size(); sample++) {
    for (int d = 0; d < count; d++) {
      result.shapes[sample * count + d] = shapes[sample * sample_dim() + start_offset + d];
    }
  }
  return result;
}

template <int left_ndim, int right_ndim>
bool operator==(const TensorListShape<left_ndim> &left, const TensorListShape<right_ndim> &right) {
  detail::check_compatible_ndim<left_ndim, right_ndim>();
  if (left.sample_dim() != right.sample_dim()) {
    return false;
  }
  if (left.num_samples() != right.num_samples()) {
    return false;
  }
  return left.shapes == right.shapes;
}

template <int left_ndim, int right_ndim>
bool operator!=(const TensorListShape<left_ndim> &left, const TensorListShape<right_ndim> &right) {
  detail::check_compatible_ndim<left_ndim, right_ndim>();
  return !(left == right);
}

/**
 * @brief Calculate pointers for Tensors stored in contigous buffer whose shapes
 *        are described by tls. Offsets are calculated as number of elements of each tensor.
 */
template <int sample_ndim, typename T>
void calculate_pointers(std::vector<T*> &pointers, T *base,
                        const TensorListShape<sample_ndim> &tls) {
  pointers.resize(tls.size());
  pointers[0] = base;
  for (int i = 0; i < tls.size() - 1; i++) {
    auto sample_shape_span = tls.tensor_shape_span(i);
    pointers[i + 1] = pointers[i] + volume(sample_shape_span);
  }
}

/**
 * @brief Calculate pointers for Tensors stored in contigous buffer whose shapes
 *        are described by tls. Offsets are calculated as number of elements of each tensor.
 */
template <int sample_ndim, typename T>
std::vector<T *> calculate_pointers(T *base, const TensorListShape<sample_ndim> &tls) {
  std::vector<T *> pointers;
  calculate_pointers(pointers, base, tls);
  return pointers;
}

/**
 * @brief Checks if all TensorShapes stored in `tls` have the same sizes
 */
template <int ndim>
bool is_uniform(const TensorListShape<ndim> &tls) {
  if (!tls.size()) {
    return true;  // empty is uniform
  }
  auto first_span = tls.tensor_shape_span(0);
  for (int i = 1; i < tls.size(); i++) {
    if (first_span != tls.tensor_shape_span(i)) {
      return false;
    }
  }
  return true;
}

template <int out_dim, int in_dim>
TensorShape<out_dim> convert_dim(const TensorShape<in_dim> &in) {
  static_assert(out_dim == DynamicDimensions || in_dim == DynamicDimensions ||
                in_dim == out_dim, "Incompatible number of dimensions"
                " - must be equal or at least one side must be dynamic");
  TensorShape<out_dim> out;
  out.resize(in.size());
  for (int i = 0; i < out.size(); i++)
    out[i] = in[i];
  return out;
}

template <int out_dim, int in_dim>
TensorShape<out_dim> convert_dim(TensorShape<in_dim> &&in) {
  return convert_dim<out_dim>(in);
}

template <>  // provide a trivial move when not actually converting
inline TensorShape<DynamicDimensions>
convert_dim<DynamicDimensions, DynamicDimensions>(TensorShape<DynamicDimensions> &&in) {
  return std::move(in);
}

template <int out_dim, int in_dim>
std::enable_if_t<(out_dim != DynamicDimensions), TensorListShape<out_dim>>
convert_dim(const TensorListShape<in_dim> &in) {
  static_assert(out_dim == DynamicDimensions || in_dim == DynamicDimensions ||
                in_dim == out_dim, "Incompatible number of dimensions"
                " - must be equal or at least one side must be dynamic");
  TensorListShape<out_dim> out = in.template to_static<out_dim>();
  return out;
}

template <int out_dim, int in_dim>
std::enable_if_t<(out_dim == DynamicDimensions), TensorListShape<out_dim>>
convert_dim(const TensorListShape<in_dim> &in) {
  return in;  // use implicit conversion
}


template <int out_dim, int in_dim>
std::enable_if_t<(out_dim != DynamicDimensions), TensorListShape<out_dim>>
convert_dim(TensorListShape<in_dim> &&in) {
  static_assert(out_dim == DynamicDimensions || in_dim == DynamicDimensions ||
                in_dim == out_dim, "Incompatible number of dimensions"
                " - must be equal or at least one side must be dynamic");
  TensorListShape<out_dim> out = std::move(in).template to_static<out_dim>();
  return out;
}

template <int out_dim, int in_dim>
std::enable_if_t<(out_dim == DynamicDimensions), TensorListShape<out_dim>>
convert_dim(TensorListShape<in_dim> &&in) {
  return std::move(in);  // use implicit conversion
}

template <int ndim = InferDimensions,
  typename SampleShape,
  int inferred = compile_time_size<SampleShape>::value,
  int ret_dim = (ndim == InferDimensions) ? inferred : ndim
> TensorListShape<ret_dim> uniform_list_shape(int num_samples, const SampleShape &sample_shape) {
  return TensorListShape<ret_dim>::make_uniform(num_samples, sample_shape);
}


template <int ndim = DynamicDimensions, typename T>
TensorListShape<ndim> uniform_list_shape(int num_samples, std::initializer_list<T> sample_shape) {
  return TensorListShape<ndim>::make_uniform(num_samples, sample_shape);
}

}  // namespace kernels
}  // namespace dali

#endif  // DALI_KERNELS_TENSOR_SHAPE_H_
