/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/c/experimental/gradients/math_grad.h"

#include "tensorflow/c/eager/abstract_tensor_handle.h"
#include "tensorflow/c/eager/gradients.h"
#include "tensorflow/c/experimental/ops/array_ops.h"
#include "tensorflow/c/experimental/ops/math_ops.h"

using std::vector;
using tensorflow::ops::Conj;
using tensorflow::ops::Identity;
using tensorflow::ops::Mul;

namespace tensorflow {
namespace gradients {
namespace {

class AddGradientFunction : public GradientFunction {
 public:
  Status Compute(Context* ctx, const IncomingGradients& grad_inputs,
                 vector<AbstractTensorHandle*>* grad_outputs) override {
    grad_outputs->resize(2);
    vector<AbstractTensorHandle*> identity_outputs(1);
    // TODO(b/145674566): Handle name unification in tracing code.
    // TODO(b/161805092): Support broadcasting.
    TF_RETURN_IF_ERROR(ops::Identity(ctx->ctx, {grad_inputs[0]},
                                     absl::MakeSpan(identity_outputs),
                                     "Identity0"));
    (*grad_outputs)[0] = identity_outputs[0];
    TF_RETURN_IF_ERROR(ops::Identity(ctx->ctx, {grad_inputs[0]},
                                     absl::MakeSpan(identity_outputs),
                                     "Identity1"));
    (*grad_outputs)[1] = identity_outputs[0];
    return Status::OK();
  }
  ~AddGradientFunction() override {}
};

class ExpGradientFunction : public GradientFunction {
 public:
  explicit ExpGradientFunction(AbstractTensorHandle* exp) : exp_(exp) {
    exp->Ref();
  }
  Status Compute(Context* ctx, const IncomingGradients& grad_inputs,
                 vector<AbstractTensorHandle*>* grad_outputs) override {
    vector<AbstractTensorHandle*> conj_outputs(1);
    TF_RETURN_IF_ERROR(
        Conj(ctx->ctx, {exp_.get()}, absl::MakeSpan(conj_outputs), "ExpConj"));
    AbstractTensorHandlePtr conj_output_releaser(conj_outputs[0]);
    grad_outputs->resize(1);
    TF_RETURN_IF_ERROR(Mul(ctx->ctx, {conj_outputs[0], grad_inputs[0]},
                           absl::MakeSpan(*grad_outputs), "ExpGradMul"));
    return Status::OK();
  }
  ~ExpGradientFunction() override {}

 private:
  AbstractTensorHandlePtr exp_;
};

}  // namespace

BackwardFunction* AddRegisterer(const ForwardOperation& op) {
  auto gradient_function = new AddGradientFunction;
  // For ops with a single output, the gradient function is not called if there
  // is no incoming gradient. So we do not need to worry about creating zeros
  // grads in this case.
  auto default_gradients = new PassThroughDefaultGradients(op);
  return new BackwardFunction(gradient_function, default_gradients);
}

BackwardFunction* ExpRegisterer(const ForwardOperation& op) {
  auto gradient_function = new ExpGradientFunction(op.outputs[0]);
  // For ops with a single output, the gradient function is not called if there
  // is no incoming gradient. So we do not need to worry about creating zeros
  // grads in this case.
  auto default_gradients = new PassThroughDefaultGradients(op);
  return new BackwardFunction(gradient_function, default_gradients);
}

}  // namespace gradients
}  // namespace tensorflow
